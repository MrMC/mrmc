/*
 *      Copyright (C) 2017 Team MrMC
 *      https://github.com/MrMC
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MrMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "EmbyServices.h"

#include "EmbyUtils.h"
#include "EmbyClient.h"
#include "Application.h"
#include "URL.h"
#include "Util.h"
#include "GUIUserMessages.h"
#include "dialogs/GUIDialogBusy.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogProgress.h"
#include "dialogs/GUIDialogSelect.h"
#include "filesystem/DirectoryCache.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/GUIWindowManager.h"
#include "interfaces/AnnouncementManager.h"
#include "network/Network.h"
#include "network/DNSNameCache.h"
#include "network/GUIDialogNetworkSetup.h"
#include "network/WakeOnAccess.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/md5.h"
#include "utils/sha1.hpp"
#include "utils/StringUtils.h"
#include "utils/StringHasher.h"
#include "utils/SystemInfo.h"
#include "utils/JobManager.h"

#include "utils/SystemInfo.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/Variant.h"

using namespace ANNOUNCEMENT;

static bool IsInSubNet(CURL url)
{
  bool rtn = false;
  CNetworkInterface* iface = g_application.getNetwork().GetFirstConnectedInterface();
  in_addr_t localMask = ntohl(inet_addr(iface->GetCurrentNetmask().c_str()));
  in_addr_t testAddress = ntohl(inet_addr(url.GetHostName().c_str()));
  in_addr_t localAddress = ntohl(inet_addr(iface->GetCurrentIPAddress().c_str()));

  in_addr_t temp1 = testAddress & localMask;
  in_addr_t temp2 = localAddress & localMask;

  if (temp1 == temp2)
  {
    // now make sure it is a emby server
    rtn = CEmbyUtils::GetIdentity(url, 1);
  }
#if defined(EMBY_DEBUG_VERBOSE)
  char buffer[256];
  std::string temp1IpAddress;
  if (inet_neta(temp1, buffer, sizeof(buffer)))
    temp1IpAddress = buffer;
  std::string temp2IpAddress;
  if (inet_neta(temp2, buffer, sizeof(buffer)))
    temp2IpAddress = buffer;
  CLog::Log(LOGDEBUG, "IsInSubNet = yes(%d), testAddress(%s), localAddress(%s)", rtn, temp1IpAddress.c_str(), temp2IpAddress.c_str());
#endif
  return rtn;
}

class CEmbyServiceJob: public CJob
{
public:
  CEmbyServiceJob(double currentTime, std::string strFunction)
  : m_function(strFunction)
  , m_currentTime(currentTime)
  {
  }
  virtual ~CEmbyServiceJob()
  {
  }
  virtual bool DoWork()
  {
    if (m_function == "FoundNewClient")
    {
      CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
      g_windowManager.SendThreadMessage(msg);

      // announce that we have a emby client and that recently added should be updated
      ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "UpdateRecentlyAdded");
      ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::AudioLibrary, "xbmc", "UpdateRecentlyAdded");
    }
    return true;
  }
  virtual bool operator==(const CJob *job) const
  {
    return true;
  }
private:
  std::string    m_function;
  double         m_currentTime;
};


CEmbyServices::CEmbyServices()
: CThread("EmbyServices")
, m_playState(MediaServicesPlayerState::stopped)
, m_hasClients(false)
{
  // register our redacted protocol options with CURL
  // we do not want these exposed in mrmc.log.
  if (!CURL::HasProtocolOptionsRedacted(EmbyApiKeyHeader))
    CURL::SetProtocolOptionsRedacted(EmbyApiKeyHeader, "EMBYTOKEN");

  CAnnouncementManager::GetInstance().AddAnnouncer(this);
}

CEmbyServices::~CEmbyServices()
{
  CAnnouncementManager::GetInstance().RemoveAnnouncer(this);

  if (IsRunning())
    Stop();

  CancelJobs();
}

CEmbyServices& CEmbyServices::GetInstance()
{
  static CEmbyServices sEmbyServices;
  return sEmbyServices;
}

void CEmbyServices::Start()
{
  CSingleLock lock(m_critical);
  if (IsRunning())
    Stop();
  CThread::Create();
}

void CEmbyServices::Stop()
{
  CSingleLock lock(m_critical);
  if (IsRunning())
  {
    m_bStop = true;
    m_processSleep.Set();
    StopThread();
  }

  g_directoryCache.Clear();
  CSingleLock lock2(m_clients_lock);
  m_clients.clear();
  m_playState = MediaServicesPlayerState::stopped;
  m_hasClients = false;
}

bool CEmbyServices::IsActive()
{
  return IsRunning();
}

bool CEmbyServices::IsEnabled()
{
  return (!CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYACESSTOKEN).empty());
}

bool CEmbyServices::HasClients() const
{
  return m_hasClients;
}

void CEmbyServices::GetClients(std::vector<CEmbyClientPtr> &clients) const
{
  CSingleLock lock(m_clients_lock);
  clients = m_clients;
}

CEmbyClientPtr CEmbyServices::FindClient(const std::string &path)
{
  CURL url(path);
  CSingleLock lock(m_clients_lock);
  for (const auto &client : m_clients)
  {
    if (client->IsSameClientHostName(url))
      return client;
  }

  return nullptr;
}

CEmbyClientPtr CEmbyServices::FindClient(const CEmbyClient *testclient)
{
  CSingleLock lock(m_clients_lock);
  for (const auto &client : m_clients)
  {
    if (testclient == client.get())
      return client;
  }

  return nullptr;
}

void CEmbyServices::OnSettingAction(const CSetting *setting)
{
  if (setting == nullptr)
    return;

  bool startThread = false;
  std::string strMessage;
  std::string strSignIn = g_localizeStrings.Get(2115);
  std::string strSignOut = g_localizeStrings.Get(2116);
  const std::string& settingId = setting->GetId();
  if (settingId == CSettings::SETTING_SERVICES_EMBYSIGNIN)
  {
    if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNIN) == strSignIn)
    {
      CURL curl(m_serverURL);
      curl.SetProtocol("emby");
      std::string path = curl.Get();
      if (CGUIDialogNetworkSetup::ShowAndGetNetworkAddress(path))
      {
        CURL curl2(path);
        if (!curl2.GetHostName().empty() && !curl2.GetUserName().empty())
        {
          if (AuthenticateByName(curl2))
          {
            // never save the password
            curl2.SetPassword("");
            m_serverURL = curl2.Get();
            // change prompt to 'sign-out'
            CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYSIGNIN, strSignOut);
            CLog::Log(LOGDEBUG, "CEmbyServices:OnSettingAction manual sign-in ok");
            startThread = true;
          }
          else
          {
            strMessage = "Could not get authToken via manual sign-in";
            CLog::Log(LOGERROR, "CEmbyServices: %s", strMessage.c_str());
          }
        }
        else
        {
          // opps, nuke'em all
          CLog::Log(LOGDEBUG, "CEmbyServices:OnSettingAction host/user are empty");
          m_userId.clear();
          m_accessToken.clear();
        }
      }
    }
    else
    {
      // prompt is 'sign-out'
      // clear authToken and change prompt to 'sign-in'
      m_userId.clear();
      m_accessToken.clear();
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYSIGNIN, strSignIn);
      CLog::Log(LOGDEBUG, "CEmbyServices:OnSettingAction sign-out ok");
    }
    SetUserSettings();

    if (startThread)
      Start();
    else
      Stop();
  }
  else if (settingId == CSettings::SETTING_SERVICES_EMBYSIGNINPIN)
  {
    if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN) == strSignIn)
    {
      if (PostSignInPinCode())
      {
        // change prompt to 'sign-out'
        CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN, strSignOut);
        CLog::Log(LOGDEBUG, "CEmbyServices:OnSettingAction pin sign-in ok");
        startThread = true;
      }
      else
      {
        std::string strMessage = "Could not get authToken via pin request sign-in";
        CLog::Log(LOGERROR, "CEmbyServices: %s", strMessage.c_str());
      }
    }
    else
    {
      // prompt is 'sign-out'
      // clear authToken and change prompt to 'sign-in'
      m_userId.clear();
      m_accessToken.clear();
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN, strSignIn);
      CLog::Log(LOGDEBUG, "CEmbyServices:OnSettingAction sign-out ok");
    }
    SetUserSettings();

    if (startThread)
      Start();
    else
    {
      if (!strMessage.empty())
        CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Emby Services", strMessage, 3000, true);
      Stop();
    }
  }
  else if (settingId == CSettings::SETTING_SERVICES_EMBYSERVERSOURCES)
  {
    CFileItemList embyServers;
    EmbyServerInfoVector serverInfos;

    std::vector<CVariant> embySavedSources;
    embySavedSources = CSettings::GetInstance().GetList(CSettings::SETTING_SERVICES_EMBYSAVEDSOURCES);

    serverInfos = GetConnectServerList(m_userId, m_accessToken);
    if (!serverInfos.empty())
    {
      for (auto &serverInfo : serverInfos)
      {
        CFileItemPtr embyServer(new CFileItem());
        // set m_bIsFolder to true to indicate we are a folder
        embyServer->m_bIsFolder = true;
        embyServer->SetProperty("title", serverInfo.ServerName);
        embyServer->SetProperty("id", serverInfo.ServerId);
        embyServer->SetLabel(serverInfo.ServerName);
        //embyServer->SetIconImage();
        //embyServer->SetArt("thumb", );
        // search saved sources for a match by server id.
        for (auto &embySavedSource : embySavedSources)
        {
          std::string sourceId = embySavedSource.asString();
          if (sourceId == "None" || sourceId == serverInfo.ServerId)
            embyServer->Select(true);
        }
        embyServers.Add(embyServer);
      }
    }

    CGUIDialogSelect *dialog = (CGUIDialogSelect*)g_windowManager.GetWindow(WINDOW_DIALOG_SELECT);
    if (dialog == NULL)
      return;

    dialog->Reset();
    dialog->SetHeading("Choose Emby Servers");
    dialog->SetItems(embyServers);
    dialog->SetMultiSelection(true);
    dialog->SetUseDetails(true);
    dialog->Open();

    if (!dialog->IsConfirmed())
      return;

    embySavedSources.clear();
    for (int i : dialog->GetSelectedItems())
      embySavedSources.push_back(CVariant(serverInfos[i].ServerId));
    CSettings::GetInstance().SetList(CSettings::SETTING_SERVICES_EMBYSAVEDSOURCES, embySavedSources);

    if (embySavedSources.size() == serverInfos.size())
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYSERVERSOURCES, "All Sources");
    else if (embySavedSources.size() == 0)
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYSERVERSOURCES, "None");
    else
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYSERVERSOURCES, "Selected Sources");

    Start();
  }
}

void CEmbyServices::Announce(AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  if ((flag & AnnouncementFlag::Player) && strcmp(sender, "xbmc") == 0)
  {
    using namespace StringHasher;
    switch(mkhash(message))
    {
      case "OnPlay"_mkhash:
        m_playState = MediaServicesPlayerState::playing;
        break;
      case "OnPause"_mkhash:
        m_playState = MediaServicesPlayerState::paused;
        break;
      case "OnStop"_mkhash:
        m_playState = MediaServicesPlayerState::stopped;
        break;
      default:
        break;
    }
  }
  else if ((flag & AnnouncementFlag::Other) && strcmp(sender, "emby") == 0)
  {
    if (strcmp(message, "UpdateLibrary") == 0)
    {
      std::string content = data["MediaServicesContent"].asString();
      std::string clientId = data["MediaServicesClientID"].asString();
      for (const auto &client : m_clients)
      {
        if (client->GetUuid() == clientId)
          client->UpdateLibrary(content);
      }
    }
    else if (strcmp(message, "ReloadProfiles") == 0)
    {
      // restart if we MrMC profiles has changed
      Stop();
      Start();
    }
  }
}

void CEmbyServices::OnSettingChanged(const CSetting *setting)
{
  // All Emby settings so far
  /*
  static const std::string SETTING_SERVICES_EMBYSIGNIN;
  static const std::string SETTING_SERVICES_EMBYUSERID;
  static const std::string SETTING_SERVICES_EMBYSERVERURL;
  static const std::string SETTING_SERVICES_EMBYACESSTOKEN;

  static const std::string SETTING_SERVICES_EMBYSIGNINPIN;
  static const std::string SETTING_SERVICES_EMBYUPDATEMINS;
  */

  if (setting == NULL)
    return;
}

void CEmbyServices::SetUserSettings()
{
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYUSERID, m_userId);
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYSERVERURL, m_serverURL);
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_EMBYACESSTOKEN, m_accessToken);
  CSettings::GetInstance().Save();
}

void CEmbyServices::GetUserSettings()
{
  m_userId = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYUSERID);
  m_serverURL  = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSERVERURL);
  m_accessToken = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYACESSTOKEN);
}

void CEmbyServices::Process()
{
  CLog::Log(LOGDEBUG, "CEmbyServices::Process bgn");
  SetPriority(THREAD_PRIORITY_BELOW_NORMAL);

  GetUserSettings();

  bool signInByPin, signInByManual;
  std::string strSignOut = g_localizeStrings.Get(2116);
    // if set to strSignOut, we are signed in by pin
  signInByPin = (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN) == strSignOut);
  // if set to strSignOut, we are signed in by user/pass
  signInByManual = (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNIN) == strSignOut);

  while (!m_bStop)
  {
    if (g_sysinfo.HasInternet())
    {
      CLog::Log(LOGDEBUG, "CEmbyServices::Process has gateway1");
      break;
    }
    if (signInByPin)
    {
      std::string ip;
      if (CDNSNameCache::Lookup("connect.mediabrowser.tv", ip))
      {
        in_addr_t embydotcom = inet_addr(ip.c_str());
        if (g_application.getNetwork().PingHost(embydotcom, 0, 1000))
        {
          CLog::Log(LOGDEBUG, "CEmbyServices::Process has gateway2");
          break;
        }
      }
    }
    m_processSleep.WaitMSec(250);
    m_processSleep.Reset();
  }

  int serviceTimeoutSeconds = 5;
  if (signInByManual)
  {
    if (!m_accessToken.empty() && !m_userId.empty())
    {
      CURL curl(m_serverURL);
      CWakeOnAccess::GetInstance().WakeUpHost(curl.GetHostName(), "Emby Server");
      GetEmbyLocalServers(m_serverURL, m_userId, m_accessToken);
      serviceTimeoutSeconds = 60 * 15;
    }
  }
  else if (signInByPin)
  {
    if (!m_accessToken.empty() && !m_userId.empty())
    {
      // fetch our saved emby servers sources
      std::vector<CVariant> embySavedSources;
      embySavedSources = CSettings::GetInstance().GetList(CSettings::SETTING_SERVICES_EMBYSAVEDSOURCES);
      // fetch a list of emby servers from emby
      EmbyServerInfoVector embySources;
      embySources = GetConnectServerList(m_userId, m_accessToken);

      EmbyServerInfoVector servers;
      for (auto &embySource : embySources)
      {
        for (auto &embySavedSource : embySavedSources)
        {
          std::string sourceId = embySavedSource.asString();
          // if matches, use this emby server
          if (sourceId == "None" || sourceId == embySource.ServerId)
            servers.push_back(embySource);
        }
      }
      if (!servers.empty())
      {
        // create a clients and assign them to a server.
        for (const auto &server : servers)
        {
          CEmbyClientPtr client(new CEmbyClient());
          if (client->Init(server))
          {
            if (AddClient(client))
            {
              CURL curl(server.ServerURL);
              CWakeOnAccess::GetInstance().WakeUpHost(curl.GetHostName(), "Emby Server");
              CLog::Log(LOGNOTICE, "CEmbyServices::CheckEmbyServers Server found %s", client->GetServerName().c_str());
            }
            else if (GetClient(client->GetUuid()) == nullptr)
            {
              // lost client
              CLog::Log(LOGNOTICE, "CEmbyServices::CheckEmbyServers Server was lost %s", client->GetServerName().c_str());
            }
          }
        }
      }
      serviceTimeoutSeconds = 60 * 15;
    }
  }

  while (!m_bStop)
  {
    m_processSleep.WaitMSec(250);
    m_processSleep.Reset();
  }

  CLog::Log(LOGDEBUG, "CEmbyServices::Process end");
}

bool CEmbyServices::AuthenticateByName(const CURL& url)
{
  XFILE::CCurlFile emby;
  emby.SetRequestHeader("Cache-Control", "no-cache");
  emby.SetRequestHeader("Content-Type", "application/json");
  CEmbyUtils::PrepareApiCall("", "", emby);

  std::string password = url.GetPassWord();
  uuids::sha1 sha1;
  sha1.process_bytes(password.c_str(), password.size());

  unsigned int hash[5];
  sha1.get_digest(hash);

  std::string passwordSha1;
  for (const auto hashPart : hash)
    passwordSha1 += StringUtils::Format("%08x", hashPart);

  std::string passwordMd5 = XBMC::XBMC_MD5::GetMD5(password);

  CVariant body;
  body["Username"] = url.GetUserName();
  body["password"] = passwordSha1;
  body["passwordMd5"] = passwordMd5;
  std::string requestBody;
  if (!CJSONVariantWriter::Write(body, requestBody, true))
    return false;

  CURL curl("emby/Users/AuthenticateByName");
  curl.SetPort(url.GetPort());
  if (url.GetProtocol() == "embys")
    curl.SetProtocol("https");
  else
    curl.SetProtocol("http");
  curl.SetHostName(url.GetHostName());

  std::string path = curl.Get();
  std::string response;
  if (!emby.Post(path, requestBody, response) || response.empty())
  {
    std::string strMessage = "Could not connect to retreive EmbyToken";
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Emby Services", strMessage, 3000, true);
    CLog::Log(LOGERROR, "CEmbyServices:AuthenticateByName failed %d, %s", emby.GetResponseCode(), response.c_str());
    return false;
  }

  CVariant responseObj;
  if (!CJSONVariantParser::Parse(response, responseObj))
    return false;
  if (!responseObj.isObject() ||
      !responseObj.isMember("AccessToken") ||
      !responseObj.isMember("User") ||
      !responseObj["User"].isMember("Id"))
    return false;

  m_userId = responseObj["User"]["Id"].asString();
  m_accessToken = responseObj["AccessToken"].asString();

  return !m_accessToken.empty() && !m_userId.empty();
}

EmbyServerInfo CEmbyServices::GetEmbyLocalServerInfo(const std::string url)
{
  EmbyServerInfo serverInfo;

  XFILE::CCurlFile emby;
  emby.SetRequestHeader("Cache-Control", "no-cache");
  emby.SetRequestHeader("Content-Type", "application/json");

  CURL curl(url);
  curl.SetFileName("emby/system/info/public");
  bool useHttps = curl.GetProtocol() == "embys";
  if (useHttps)
    curl.SetProtocol("https");
  else
    curl.SetProtocol("http");
  // do not need user/pass for server info
  curl.SetUserName("");
  curl.SetPassword("");

  std::string path = curl.Get();
  std::string response;
  if (!emby.Get(path, response) || response.empty())
  {
    std::string strMessage = "Could not connect to retreive EmbyServerInfo";
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Emby Services", strMessage, 3000, true);
    CLog::Log(LOGERROR, "CEmbyServices:GetEmbyServerInfo failed %d, %s", emby.GetResponseCode(), response.c_str());
    return serverInfo;
  }

  static const std::string ServerPropertyId = "Id";
  static const std::string ServerPropertyName = "ServerName";
  static const std::string ServerPropertyVersion = "Version";
  static const std::string ServerPropertyWanAddress = "WanAddress";
  static const std::string ServerPropertyLocalAddress = "LocalAddress";
  static const std::string ServerPropertyOperatingSystem = "OperatingSystem";
  CVariant responseObj;
  if (!CJSONVariantParser::Parse(response, responseObj))
    return serverInfo;
  if (!responseObj.isObject() ||
      !responseObj.isMember(ServerPropertyId) ||
      !responseObj.isMember(ServerPropertyName) ||
      !responseObj.isMember(ServerPropertyVersion) ||
      !responseObj.isMember(ServerPropertyWanAddress) ||
      !responseObj.isMember(ServerPropertyLocalAddress) ||
      !responseObj.isMember(ServerPropertyOperatingSystem))
    return serverInfo;

  serverInfo.UserId = m_userId;
  serverInfo.AccessToken = m_accessToken;
  // servers found by broadcast are always local ("Linked")
  serverInfo.UserType= "Linked";
  serverInfo.ServerId = responseObj[ServerPropertyId].asString();
  serverInfo.ServerURL = curl.GetWithoutFilename();
  serverInfo.ServerName = responseObj[ServerPropertyName].asString();
  serverInfo.WanAddress = responseObj[ServerPropertyWanAddress].asString();
  serverInfo.LocalAddress = responseObj[ServerPropertyLocalAddress].asString();
  return serverInfo;
}

bool CEmbyServices::GetEmbyLocalServers(const std::string &serverURL, const std::string &userId, const std::string &accessToken)
{
  bool rtn = false;

  std::vector<CEmbyClientPtr> clientsFound;

  EmbyServerInfo embyServerInfo = GetEmbyLocalServerInfo(serverURL);
  if (!embyServerInfo.ServerId.empty())
  {
    embyServerInfo.UserId = userId;
    embyServerInfo.AccessToken = accessToken;
    CEmbyClientPtr client(new CEmbyClient());
    if (client->Init(embyServerInfo))
    {
      if (AddClient(client))
      {
        CLog::Log(LOGNOTICE, "CEmbyServices::CheckEmbyServers Server found %s", client->GetServerName().c_str());
      }
      else if (GetClient(client->GetUuid()) == nullptr)
      {
        // lost client
        CLog::Log(LOGNOTICE, "CEmbyServices::CheckEmbyServers Server was lost %s", client->GetServerName().c_str());
      }
    }
  }
  return rtn;
}

bool CEmbyServices::PostSignInPinCode()
{
  // on return, show user m_signInByPinCode so they can enter it at https://emby.media/pin
  bool rtn = false;
  std::string strMessage;

  XFILE::CCurlFile curlfile;
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");

  CURL curl("https://connect.mediabrowser.tv");
  curl.SetFileName("service/pin");
  curl.SetOption("format", "json");

  CVariant data;
  data["deviceId"] = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_UUID);
  std::string jsonBody;
  if (!CJSONVariantWriter::Write(data, jsonBody, false))
    return rtn;
  std::string response;
  if (curlfile.Post(curl.Get(), jsonBody, response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CEmbyServices:FetchSignInPin %s", response.c_str());
#endif
    CVariant reply;
    if (!CJSONVariantParser::Parse(response, reply))
      return rtn;
    if (reply.isObject() && reply.isMember("Pin"))
    {
      m_signInByPinCode = reply["Pin"].asString();
      if (m_signInByPinCode.empty())
        strMessage = "Failed to get Pin Code";
      rtn = !m_signInByPinCode.empty();
    }

    CGUIDialogProgress *waitPinReplyDialog;
    waitPinReplyDialog = (CGUIDialogProgress*)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
    waitPinReplyDialog->SetHeading(g_localizeStrings.Get(2115));
    waitPinReplyDialog->SetLine(0, g_localizeStrings.Get(2117));
    std::string prompt = g_localizeStrings.Get(2118) + m_signInByPinCode;
    waitPinReplyDialog->SetLine(1, prompt);

    waitPinReplyDialog->Open();
    waitPinReplyDialog->ShowProgressBar(true);

    CStopWatch dieTimer;
    dieTimer.StartZero();
    int timeToDie = 60 * 5;

    CStopWatch pingTimer;
    pingTimer.StartZero();

    m_userId.clear();
    m_accessToken.clear();
    while (!waitPinReplyDialog->IsCanceled())
    {
      waitPinReplyDialog->SetPercentage(int(float(dieTimer.GetElapsedSeconds())/float(timeToDie)*100));
      waitPinReplyDialog->Progress();
      if (pingTimer.GetElapsedSeconds() > 1)
      {
        // wait for user to run and enter pin code
        // at https://emby.media/pin
        if (GetSignInByPinReply())
          break;
        pingTimer.Reset();
        m_processSleep.WaitMSec(250);
        m_processSleep.Reset();
      }

      if (dieTimer.GetElapsedSeconds() > timeToDie)
      {
        rtn = false;
        break;
      }
    }
    waitPinReplyDialog->Close();

    if (m_accessToken.empty())
    {
      strMessage = "Error extracting AcessToken";
      CLog::Log(LOGERROR, "CEmbyServices::PostSignInPinCode failed to get authToken");
      m_signInByPinCode = "";
      rtn = false;
    }
  }
  else
  {
    strMessage = "Could not connect to retreive AuthToken";
    CLog::Log(LOGERROR, "CEmbyServices:FetchSignInPin failed %s", response.c_str());
  }
  if (!rtn)
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Emby Services", strMessage, 3000, true);
  return rtn;
}

bool CEmbyServices::GetSignInByPinReply()
{
  // repeat called until we timeout or get authToken
  bool rtn = false;
  std::string strMessage;

  XFILE::CCurlFile curlfile;
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");

  CURL curl("https://connect.mediabrowser.tv");
  curl.SetFileName("service/pin");
  curl.SetOption("format", "json");
  curl.SetOption("pin", m_signInByPinCode);
  curl.SetOption("deviceId", CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_UUID));

  std::string response;
  if (curlfile.Get(curl.Get(), response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CEmbyServices:WaitForSignInByPin %s", response.c_str());
#endif
    CVariant reply;
    if (!CJSONVariantParser::Parse(response, reply))
      return rtn;
    if (reply.isObject() && reply.isMember("IsConfirmed") && reply["IsConfirmed"].asString() == "true")
    {
      std::string pin = reply["Pin"].asString();
      std::string deviceId = reply["DeviceId"].asString();
      std::string id = reply["Id"].asString();
      //std::string isConfirmed = reply["IsConfirmed"].asString();
      //std::string isExpired = reply["IsExpired"].asString();
      //std::string accessToken = reply["AccessToken"].asString();
      if (!deviceId.empty() && !pin.empty())
        rtn = AuthenticatePinReply(deviceId, pin);
    }
  }

  if (!rtn)
  {
    CLog::Log(LOGERROR, "CEmbyServices:WaitForSignInByPin failed %s", response.c_str());
  }
  return rtn;
}

bool CEmbyServices::AuthenticatePinReply(const std::string &deviceId, const std::string &pin)
{
  bool rtn = false;

  XFILE::CCurlFile curlfile;
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");

  CURL curl("https://connect.mediabrowser.tv");
  curl.SetFileName("service/pin/authenticate");
  curl.SetOption("format", "json");

  CVariant data;
  data["pin"] = pin;
  data["deviceId"] = deviceId;
  std::string jsondata;
  if (!CJSONVariantWriter::Write(data, jsondata, false))
    return rtn;
  std::string response;
  if (curlfile.Post(curl.Get(), jsondata, response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CEmbyServices:AuthenticatePinReply %s", response.c_str());
#endif
    CVariant reply;
    if (!CJSONVariantParser::Parse(response, reply))
      return rtn;
    if (reply.isObject() && reply.isMember("AccessToken"))
    {
      // pin connects are parsed as UserId/AccessToken
      // user/pass connects are parsed as ConnectUserId/ConnectAccessToken
      const std::string connectUserId = reply["UserId"].asString();
      const std::string connectAccessToken = reply["AccessToken"].asString();
      EmbyServerInfoVector servers;
      servers = GetConnectServerList(connectUserId, connectAccessToken);
      if (!servers.empty())
      {
        m_userId = connectUserId;
        m_accessToken = connectAccessToken;
        rtn = true;
      }
    }
  }
  return rtn;
}

EmbyServerInfoVector CEmbyServices::GetConnectServerList(const std::string &connectUserId, const std::string &connectAccessToken)
{
  EmbyServerInfoVector servers;

  CGUIDialogBusy *busyDialog = (CGUIDialogBusy*)g_windowManager.GetWindow(WINDOW_DIALOG_BUSY);
  if (busyDialog)
    busyDialog->Open();
  
  XFILE::CCurlFile curlfile;
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");

  CURL curl("https://connect.emby.media");
  curl.SetFileName("service/servers");
  curl.SetOption("format", "json");
  curl.SetOption("userId", connectUserId);
  curl.SetProtocolOptions("&X-Connect-UserToken=" + connectAccessToken);

  std::string response;
  if (curlfile.Get(curl.Get(), response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CEmbyServices:GetConnectServerList %s", response.c_str());
#endif
    CVariant vservers;
    if (!CJSONVariantParser::Parse(response, vservers))
    {
      if (busyDialog)
        busyDialog->Close();
      return servers;
    }
    if (vservers.isArray())
    {
      for (auto serverObjectIt = vservers.begin_array(); serverObjectIt != vservers.end_array(); ++serverObjectIt)
      {
        const auto server = *serverObjectIt;
        EmbyServerInfo serverInfo;
        serverInfo.UserId = connectUserId;
        serverInfo.AccessToken = connectAccessToken;

        serverInfo.UserType= server["UserType"].asString();
        serverInfo.ServerId = server["SystemId"].asString();
        serverInfo.AccessKey= server["AccessKey"].asString();
        serverInfo.ServerName= server["Name"].asString();
        serverInfo.WanAddress= server["Url"].asString();
        serverInfo.LocalAddress= server["LocalAddress"].asString();
        if (IsInSubNet(CURL(serverInfo.LocalAddress)))
          serverInfo.ServerURL= serverInfo.LocalAddress;
        else
          serverInfo.ServerURL= serverInfo.WanAddress;
        if (ExchangeAccessKeyForAccessToken(serverInfo))
          servers.push_back(serverInfo);
      }
    }
  }
  if (busyDialog)
    busyDialog->Close();
  return servers;
}

bool CEmbyServices::ExchangeAccessKeyForAccessToken(EmbyServerInfo &connectServerInfo)
{
  bool rtn = false;

  XFILE::CCurlFile curlfile;
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");
  CEmbyUtils::PrepareApiCall(connectServerInfo.UserId, connectServerInfo.AccessKey, curlfile);

  CURL curl(connectServerInfo.ServerURL);
  curl.SetFileName("emby/Connect/Exchange");
  curl.SetOption("format", "json");
  curl.SetOption("ConnectUserId", connectServerInfo.UserId);

  std::string response;
  if (curlfile.Get(curl.Get(), response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CEmbyServices:ExchangeAccessKeyForAccessToken %s", response.c_str());
#endif
    CVariant reply;
    if (!CJSONVariantParser::Parse(response, reply))
      return rtn;
    if (reply.isObject() && reply.isMember("AccessToken"))
    {
      connectServerInfo.UserId = reply["LocalUserId"].asString();
      connectServerInfo.AccessToken = reply["AccessToken"].asString();
      rtn = true;
    }
  }
  return rtn;
}


CEmbyClientPtr CEmbyServices::GetClient(std::string uuid)
{
  CSingleLock lock(m_clients_lock);
  for (const auto &client : m_clients)
  {
    if (client->GetUuid() == uuid)
      return client;
  }

  return nullptr;
}

bool CEmbyServices::ClientIsLocal(std::string path)
{
  CSingleLock lock(m_clients_lock);
  for (const auto &client : m_clients)
  {
    if (StringUtils::StartsWithNoCase(client->GetUrl(), path))
      return client->IsLocal();
  }

  return false;
}

bool CEmbyServices::AddClient(CEmbyClientPtr foundClient)
{
  CSingleLock lock(m_clients_lock);
  for (const auto &client : m_clients)
  {
    // do not add existing clients
    if (client->GetUuid() == foundClient->GetUuid())
      return false;
  }

  // only add new clients that are present
  if (foundClient->GetPresence() && foundClient->FetchViews())
  {
    m_clients.push_back(foundClient);
    m_hasClients = !m_clients.empty();
    AddJob(new CEmbyServiceJob(0, "FoundNewClient"));
    return true;
  }

  return false;
}

bool CEmbyServices::RemoveClient(CEmbyClientPtr lostClient)
{
  CSingleLock lock(m_clients_lock);
  for (const auto &client : m_clients)
  {
    if (client->GetUuid() == lostClient->GetUuid())
    {
      // this is silly but can not figure out how to erase
      // just given 'client' :)
      m_clients.erase(std::find(m_clients.begin(), m_clients.end(), client));
      m_hasClients = !m_clients.empty();

      // client is gone, remove it from any gui lists here.
      CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
      g_windowManager.SendThreadMessage(msg);
      return true;
    }
  }

  return false;
}

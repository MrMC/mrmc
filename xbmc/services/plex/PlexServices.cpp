/*
 *      Copyright (C) 2016 Team MrMC
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

#include "PlexServices.h"

#include "Application.h"
#include "URL.h"
#include "Util.h"
#include "GUIUserMessages.h"
#include "cores/VideoRenderers/RenderManager.h"
#include "cores/VideoRenderers/RenderCapture.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogNumeric.h"
#include "dialogs/GUIDialogProgress.h"
#include "filesystem/CurlFile.h"
#include "filesystem/DirectoryCache.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/GUIWindowManager.h"
#include "interfaces/AnnouncementManager.h"
#include "network/Network.h"
#include "network/Socket.h"
#include "settings/lib/Setting.h"
#include "settings/Settings.h"
#include "profiles/dialogs/GUIDialogLockSettings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/StringHasher.h"
#include "utils/JobManager.h"

#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"
#include "utils/XMLUtils.h"

#include "PlexUtils.h"
#include "PlexClient.h"

#include <regex>

using namespace ANNOUNCEMENT;

static const int NS_PLEX_MEDIA_SERVER_PORT(32414);
static const std::string NS_BROADCAST_ADDR("239.0.0.250");
static const std::string NS_SEARCH_MSG("M-SEARCH * HTTP/1.1\r\n");
static const std::string NS_PLEXTV_URL("https://plex.tv");

class CPlexServiceJob: public CJob
{
public:
  CPlexServiceJob(double currentTime, std::string strFunction)
  : m_function(strFunction)
  , m_currentTime(currentTime)
  {
  }
  virtual ~CPlexServiceJob()
  {
  }
  virtual bool DoWork()
  {
    if (m_function == "UpdateLibraries")
    {
      CLog::Log(LOGNOTICE, "CPlexServiceJob: UpdateLibraries");
      CPlexServices::GetInstance().UpdateLibraries(true);
    }
    else if (m_function == "FoundNewClient")
    {
      CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
      g_windowManager.SendThreadMessage(msg);

      // announce that we have a plex client and that recently added should be updated
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


CPlexServices::CPlexServices()
: CThread("PlexServices")
, m_gdmListener(nullptr)
, m_updateMins(0)
, m_playState(PlexServicePlayerState::stopped)
, m_hasClients(false)
{
  // register our redacted protocol options with CURL
  // we do not want these exposed in mrmc.log.
  if (!CURL::HasProtocolOptionsRedacted("X-Plex-Token"))
    CURL::SetProtocolOptionsRedacted("X-Plex-Token", "PLEXTOKEN");

  CAnnouncementManager::GetInstance().AddAnnouncer(this);
}

CPlexServices::~CPlexServices()
{
  CAnnouncementManager::GetInstance().RemoveAnnouncer(this);

  if (IsRunning())
    Stop();

  CancelJobs();
  SAFE_DELETE(m_gdmListener);
}

CPlexServices& CPlexServices::GetInstance()
{
  static CPlexServices sPlexServices;
  return sPlexServices;
}

void CPlexServices::Start()
{
  CSingleLock lock(m_critical);
  if (IsRunning())
    Stop();
  CThread::Create();
}

void CPlexServices::Stop()
{
  CSingleLock lock(m_critical);
  if (IsRunning())
  {
    m_bStop = true;
    m_processSleep.Set();
    StopThread();
  }

  g_directoryCache.Clear();
  CSingleLock lock2(m_criticalClients);
  m_clients.clear();
}

bool CPlexServices::IsActive()
{
  return IsRunning();
}

bool CPlexServices::HasClients() const
{
  return m_hasClients;
}

void CPlexServices::GetClients(std::vector<CPlexClientPtr> &clients) const
{
  CSingleLock lock(m_criticalClients);
  clients = m_clients;
}

CPlexClientPtr CPlexServices::FindClient(const std::string &path)
{
  CURL url(path);
  CSingleLock lock(m_criticalClients);
  for (const auto &client : m_clients)
  {
    if (client->IsSameClientHostName(url))
      return client;
  }

  return nullptr;
}

void CPlexServices::OnSettingAction(const CSetting *setting)
{
  if (setting == nullptr)
    return;

  bool startThread = false;
  std::string strMessage;
  std::string strSignIn = g_localizeStrings.Get(1240);
  std::string strSignOut = g_localizeStrings.Get(1241);
  const std::string& settingId = setting->GetId();
  if (settingId == CSettings::SETTING_SERVICES_PLEXSIGNIN)
  {
    if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNIN) == strSignIn)
    {
      // prompt is 'sign-in'
      std::string user;
      std::string pass;
      std::string module;
      bool saveDetails = false;
      if (CGUIDialogLockSettings::ShowAndGetUserAndPassword(user, pass, module, &saveDetails, true))
      {
        if (!user.empty() && !pass.empty())
        {
          if (GetPlexToken(user, pass))
          {
            // change prompt to 'sign-out'
            CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXSIGNIN, strSignOut);
            CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXHOMEUSER, m_myHomeUser);
            CLog::Log(LOGDEBUG, "CPlexServices:OnSettingAction manual sign-in ok");
            startThread = true;
          }
          else
          {
            strMessage = "Could not get authToken via manual sign-in";
            CLog::Log(LOGERROR, "CPlexServices: %s", strMessage.c_str());
          }
        }
        else
        {
          // opps, nuke'em all
          CLog::Log(LOGDEBUG, "CPlexServices:OnSettingAction user/pass are empty");
          m_authToken.clear();
        }
      }
    }
    else
    {
      // prompt is 'sign-out'
      // clear authToken and change prompt to 'sign-in'
      m_authToken.clear();
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXSIGNIN, strSignIn);
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXHOMEUSER, "");
      CLog::Log(LOGDEBUG, "CPlexServices:OnSettingAction sign-out ok");
    }
    SetUserSettings();

    if (startThread || m_useGDMServer)
      Start();
    else
      Stop();

  }
  else if (settingId == CSettings::SETTING_SERVICES_PLEXSIGNINPIN)
  {
    if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN) == strSignIn)
    {
      if (GetSignInPinCode())
      {
        // change prompt to 'sign-out'
        CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN, strSignOut);
        CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXHOMEUSER, m_myHomeUser);
        CLog::Log(LOGDEBUG, "CPlexServices:OnSettingAction pin sign-in ok");
        startThread = true;
      }
      else
      {
        std::string strMessage = "Could not get authToken via pin request sign-in";
        CLog::Log(LOGERROR, "CPlexServices: %s", strMessage.c_str());
      }
    }
    else
    {
      // prompt is 'sign-out'
      // clear authToken and change prompt to 'sign-in'
      m_authToken.clear();
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN, strSignIn);
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXHOMEUSER, "");
      CLog::Log(LOGDEBUG, "CPlexServices:OnSettingAction sign-out ok");
    }
    SetUserSettings();


    if (startThread || m_useGDMServer)
      Start();
    else
    {
      if (!strMessage.empty())
        CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Plex Services", strMessage, 3000, true);
      Stop();
    }
  }
  else if (settingId == CSettings::SETTING_SERVICES_PLEXHOMEUSER)
  {
    // user must be in 'sign-in' state so check for 'sign-out' label
    if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNIN) == strSignOut ||
        CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN) == strSignOut)
    {
      std::string homeUserName;
      if (GetMyHomeUsers(homeUserName))
      {
        m_myHomeUser = homeUserName;
        CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXHOMEUSER, m_myHomeUser);
        SetUserSettings();
        CSingleLock lock(m_criticalClients);
        m_clients.clear();
        Start();
      }
    }
  }
}

void CPlexServices::Announce(AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  if ((flag & AnnouncementFlag::Player) && strcmp(sender, "xbmc") == 0)
  {
    using namespace StringHasher;
    switch(mkhash(message))
    {
      case "OnPlay"_mkhash:
        m_playState = PlexServicePlayerState::playing;
        break;
      case "OnPause"_mkhash:
        m_playState = PlexServicePlayerState::paused;
        break;
      case "OnStop"_mkhash:
        m_playState = PlexServicePlayerState::stopped;
        break;
      default:
        break;
    }
  }
  else if ((flag & AnnouncementFlag::Other) && strcmp(sender, "plex") == 0)
  {
    if (strcmp(message, "UpdateLibrary") == 0)
    {
      AddJob(new CPlexServiceJob(0, "UpdateLibraries"));
    }
  }
}

void CPlexServices::OnSettingChanged(const CSetting *setting)
{
  // All Plex settings so far
  /*
  static const std::string SETTING_SERVICES_PLEXSIGNIN;
  static const std::string SETTING_SERVICES_PLEXSIGNINPIN;
  static const std::string SETTING_SERVICES_PLEXHOMEUSER;
  static const std::string SETTING_SERVICES_PLEXGDMSERVER;
  static const std::string SETTING_SERVICES_PLEXMYPLEXAUTH;
  */

  if (setting == NULL)
    return;

  const std::string& settingId = setting->GetId();
  if (settingId == CSettings::SETTING_SERVICES_PLEXGDMSERVER)
  {
    m_useGDMServer = CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_PLEXGDMSERVER);
    // start or stop the service
    if (m_useGDMServer || MyPlexSignedIn())
      Start();
    else
      Stop();
  }
  else if (settingId == CSettings::SETTING_SERVICES_PLEXUPDATEMINS)
  {
    int oldUpdateMins = m_updateMins;
    m_updateMins = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_PLEXUPDATEMINS);
    if (IsRunning())
    {
      if (oldUpdateMins > 0 && m_updateMins == 0)
      {
        // switch to no caching
        g_directoryCache.Clear();
      }
      if (m_playState == PlexServicePlayerState::stopped)
      {
        CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
        g_windowManager.SendThreadMessage(msg);
      }
    }
  }
}

void CPlexServices::SetUserSettings()
{
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_PLEXMYPLEXAUTH, m_authToken);
  CSettings::GetInstance().Save();
}

void CPlexServices::GetUserSettings()
{
  m_authToken  = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXMYPLEXAUTH);
  m_updateMins  = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_PLEXUPDATEMINS);
  m_useGDMServer = CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_PLEXGDMSERVER);
}

void CPlexServices::UpdateLibraries(bool forced)
{
  CSingleLock lock(m_criticalClients);
  bool clearDirCache = false;
  for (const auto &client : m_clients)
  {
    client->ParseSections(PlexSectionParsing::checkSection);
    if (forced || client->NeedUpdate())
    {
      client->ParseSections(PlexSectionParsing::updateSection);
      clearDirCache = true;
    }
  }
  if (clearDirCache)
  {
    g_directoryCache.Clear();
    if (m_playState == PlexServicePlayerState::stopped)
    {
      CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
      g_windowManager.SendThreadMessage(msg);
    }
  }
}

bool CPlexServices::MyPlexSignedIn()
{
  return !m_authToken.empty();
}

void CPlexServices::Process()
{
  SetPriority(THREAD_PRIORITY_BELOW_NORMAL);

  GetUserSettings();

  while (!m_bStop)
  {
    CNetworkInterface* iface = g_application.getNetwork().GetFirstConnectedInterface();
    if (iface && iface->IsConnected())
    {
      in_addr_t router = inet_addr(iface->GetCurrentDefaultGateway().c_str());
      if (router != INADDR_NONE && g_application.getNetwork().PingHost(router, 0, 1000))
        break;
    }

    m_processSleep.WaitMSec(250);
    m_processSleep.Reset();
  }

  int plextvTimeoutSeconds = 5;

  // try plex.tv first
  if (MyPlexSignedIn())
  {
    bool includeHttps = true;
    GetMyPlexServers(includeHttps);
    plextvTimeoutSeconds = 60 * 15;
  }
  // the via GDM
  CheckForGDMServers();

  CStopWatch gdmTimer, plextvTimer, checkUpdatesTimer;
  gdmTimer.StartZero();
  plextvTimer.StartZero();
  checkUpdatesTimer.StartZero();
  while (!m_bStop)
  {
    // check for services every N seconds
    if (plextvTimer.GetElapsedSeconds() > plextvTimeoutSeconds)
    {
      // try plex.tv
      if (MyPlexSignedIn())
      {
        if (m_playState == PlexServicePlayerState::stopped)
        {
          // if we get back servers, then
          // reduce the initial polling time
          bool foundSomething = false;
          foundSomething = GetMyPlexServers(true);
          //foundSomething = foundSomething || GetMyPlexServers(false);
          if (foundSomething)
            plextvTimeoutSeconds = 60 * 15;
        }
      }
      plextvTimer.Reset();
    }

    if (gdmTimer.GetElapsedSeconds() > 5)
    {
      if (m_playState == PlexServicePlayerState::stopped)
        CheckForGDMServers();
      gdmTimer.Reset();
    }

    if (m_updateMins > 0 && (checkUpdatesTimer.GetElapsedSeconds() > (60 * m_updateMins)))
    {
      UpdateLibraries(false);
      checkUpdatesTimer.Reset();
    }

    m_processSleep.WaitMSec(250);
    m_processSleep.Reset();
  }

  if (m_gdmListener)
  {
    // before deleting listener, fetch and delete any sockets it uses.
    SOCKETS::CUDPSocket *socket = (SOCKETS::CUDPSocket*)m_gdmListener->GetFirstSocket();
    // we should not have to do the close,
    // delete 'should' do it.
    socket->Close();
    SAFE_DELETE(socket);
    SAFE_DELETE(m_gdmListener);
  }
}

bool CPlexServices::GetPlexToken(std::string user, std::string pass)
{
  bool rtn = false;
  XFILE::CCurlFile plex;
  CPlexUtils::GetDefaultHeaders(plex);

  CURL url(NS_PLEXTV_URL + "/users/sign_in.json");
  url.SetUserName(user);
  url.SetPassword(pass);

  std::string strResponse;
  std::string strPostData;
  if (plex.Post(url.Get(), strPostData, strResponse))
  {
    //CLog::Log(LOGDEBUG, "CPlexServices: myPlex %s", strResponse.c_str());

    CVariant reply;
    reply = CJSONVariantParser::Parse((const unsigned char*)strResponse.c_str(), strResponse.size());

    CVariant user = reply["user"];
    m_authToken = user["authentication_token"].asString();

    std::string homeUserName;
    if (GetMyHomeUsers(homeUserName))
      m_myHomeUser = homeUserName;

    rtn = true;
  }
  else
  {
    std::string strMessage = "Could not connect to retreive PlexToken";
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Plex Services", strMessage, 3000, true);
    CLog::Log(LOGERROR, "CPlexServices:FetchPlexToken failed %s", strResponse.c_str());
  }

  return rtn;
}

bool CPlexServices::GetMyPlexServers(bool includeHttps)
{
  bool rtn = false;

  std::vector<CPlexClientPtr> clientsFound;

  XFILE::CCurlFile plex;
  plex.SetBufferSize(32768*10);

  CPlexUtils::GetDefaultHeaders(plex);
  if (MyPlexSignedIn())
    plex.SetRequestHeader("X-Plex-Token", m_authToken);

  std::string strResponse;
  CURL url(NS_PLEXTV_URL);
  if (includeHttps)
    url.SetFileName("pms/resources?includeHttps=1");
  else
    url.SetFileName("pms/resources?includeHttps=0");

  if (plex.Get(url.Get(), strResponse))
  {
#if defined(PLEX_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CPlexServices:GetMyPlexServers %d, %s", includeHttps, strResponse.c_str());
#endif
    TiXmlDocument xml;
    xml.Parse(strResponse.c_str());

    TiXmlElement* MediaContainer = xml.RootElement();
    if (MediaContainer)
    {
      const TiXmlElement* DeviceNode = MediaContainer->FirstChildElement("Device");
      while (DeviceNode)
      {
        std::string provides = XMLUtils::GetAttribute(DeviceNode, "provides");
        if (provides == "server")
        {
          CPlexClientPtr client(new CPlexClient());
          if (client->Init(DeviceNode))
          {
            clientsFound.push_back(client);
            // always return true if we find anything
            rtn = true;
          }
        }
        DeviceNode = DeviceNode->NextSiblingElement("Device");
      }
    }
  }
  else
  {
    // 401 Unauthorized
    //if (plex.GetResponseCode() == 401)
    //  m_authToken.clear();

    std::string strMessage = "Error getting Plex servers";
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Plex Services", strMessage, 3000, true);
    CLog::Log(LOGDEBUG, "CPlexServices:FetchMyPlexServers failed %s", strResponse.c_str());
    return false;
  }

  std::vector<CPlexClientPtr> lostClients;
  if (!clientsFound.empty())
  {
    for (const auto &client : clientsFound)
    {
      if (AddClient(client))
      {
        // new client
        CLog::Log(LOGNOTICE, "CPlexServices: Server found via plex.tv %s", client->GetServerName().c_str());
      }
      else if (GetClient(client->GetUuid()) == nullptr)
      {
        // lost client
        lostClients.push_back(client);
        CLog::Log(LOGNOTICE, "CPlexServices: Server was lost %s", client->GetServerName().c_str());
      }
      else if (UpdateClient(client))
      {
        // client exists and something changed
        CLog::Log(LOGNOTICE, "CPlexServices: Server presence changed %s", client->GetServerName().c_str());
      }
    }
    AddJob(new CPlexServiceJob(0, "FoundNewClient"));
  }

  if (!lostClients.empty())
  {
    for (const auto &lostclient : lostClients)
      RemoveClient(lostclient);
  }

  return rtn;
}

bool CPlexServices::GetSignInPinCode()
{
  // on return, show user m_signInByPinCode so they can enter it at https://plex.tv/pin

  bool rtn = false;

  std::string id;
  std::string code;
  std::string clientid;
  CDateTime   expiresAt;

  XFILE::CCurlFile plex;
  // use a lower default timeout
  plex.SetTimeout(3);
  CPlexUtils::GetDefaultHeaders(plex);

  CURL url(NS_PLEXTV_URL + "/pins.xml");

  std::string strResponse;
  std::string strMessage;
  if (plex.Post(url.Get(), "", strResponse))
  {
#if defined(PLEX_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CPlexServices:FetchSignInPin %s", strResponse.c_str());
#endif

    TiXmlDocument xml;
    xml.Parse(strResponse.c_str());

    TiXmlElement* pinNode = xml.RootElement();
    if (pinNode)
    {
      for (TiXmlElement *elem = pinNode->FirstChildElement(); elem; elem = elem->NextSiblingElement())
      {
        if (elem->GetText() == nullptr)
          continue;

        if (elem->ValueStr() == "id")
          id = elem->GetText();
        else if (elem->ValueStr() == "code")
          code = elem->GetText();
        else if (elem->ValueStr() == "client-identifier")
          clientid = elem->GetText();
        else if (elem->ValueStr() == "expires-at")
        {
          std::string date = elem->GetText();
          date = std::regex_replace(date, std::regex("T"), " ");
          date = std::regex_replace(date, std::regex("Z"), "");
          expiresAt.SetFromDBDateTime(date);
        }
      }
      m_signInByPinId = id;
      m_signInByPinCode = code;
      strMessage = "Failed to get ID or Code";
      rtn = !m_signInByPinId.empty() && !m_signInByPinCode.empty();
    }

    CGUIDialogProgress *waitPinReplyDialog;
    waitPinReplyDialog = (CGUIDialogProgress*)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
    waitPinReplyDialog->SetHeading(g_localizeStrings.Get(1246));
    waitPinReplyDialog->SetLine(0, g_localizeStrings.Get(1248));
    std::string prompt = g_localizeStrings.Get(1249) + code;
    waitPinReplyDialog->SetLine(1, prompt);

    waitPinReplyDialog->Open();
    waitPinReplyDialog->ShowProgressBar(false);

    CStopWatch dieTimer;
    dieTimer.StartZero();
    int timeToDie = 60 * 5;

    CStopWatch pingTimer;
    pingTimer.StartZero();

    m_authToken.clear();
    while (!waitPinReplyDialog->IsCanceled())
    {
      waitPinReplyDialog->SetPercentage(int(float(dieTimer.GetElapsedSeconds())/float(timeToDie)*100));

      if (pingTimer.GetElapsedSeconds() > 1)
      {
        // wait for user to run and enter pin code
        // at https://plex.tv/pin
        if (GetSignInByPinReply())
          break;
        pingTimer.Reset();
      }

      if (dieTimer.GetElapsedSeconds() > timeToDie)
      {
        rtn = false;
        break;
      }
      waitPinReplyDialog->Progress();
    }
    waitPinReplyDialog->Close();

    if (m_authToken.empty())
    {
      strMessage = "Error extracting AuthToken";
      CLog::Log(LOGERROR, "CPlexServices:FetchSignInPin failed to get authToken");
      rtn = false;
    }
    else
    {
      std::string homeUserName;
      if (GetMyHomeUsers(homeUserName))
      {
        m_myHomeUser = homeUserName;
        rtn = true;
      }
      else
        rtn = false;
    }
  }
  else
  {
    strMessage = "Could not connect to retreive AuthToken";
    CLog::Log(LOGERROR, "CPlexServices:FetchSignInPin failed %s", strResponse.c_str());
  }
  if (!rtn)
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Plex Services", strMessage, 3000, true);
  return rtn;
}

bool CPlexServices::GetSignInByPinReply()
{
  // repeat called until we timeout or get authToken
  bool rtn = false;
  std::string strMessage;
  XFILE::CCurlFile plex;
  CPlexUtils::GetDefaultHeaders(plex);

  std::string path = NS_PLEXTV_URL + "/pins/" + m_signInByPinId + ".xml";
  CURL url(path);

  std::string strResponse;
  if (plex.Get(url.Get(), strResponse))
  {
#if defined(PLEX_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CPlexServices:WaitForSignInByPin %s", strResponse.c_str());
#endif
    TiXmlDocument xml;
    xml.Parse(strResponse.c_str());

    TiXmlElement* pinNode = xml.RootElement();
    if (pinNode)
    {
      for (TiXmlElement *elem = pinNode->FirstChildElement(); elem; elem = elem->NextSiblingElement())
      {
        if (elem->GetText() == nullptr)
          continue;

        if (elem->ValueStr() == "auth_token")
          m_authToken = elem->GetText();
      }
      rtn = !m_authToken.empty();
    }
  }
  else
  {
    CLog::Log(LOGERROR, "CPlexServices:WaitForSignInByPin failed %s", strResponse.c_str());
  }
  return rtn;
}

void CPlexServices::CheckForGDMServers()
{
  if (m_useGDMServer)
  {
    if (!m_gdmListener)
    {
      SOCKETS::CUDPSocket *socket = SOCKETS::CSocketFactory::CreateUDPSocket();
      if (socket)
      {
        SOCKETS::CAddress my_addr;
        CNetworkInterface* iface = g_application.getNetwork().GetFirstConnectedInterface();
        my_addr.SetAddress(iface->GetCurrentIPAddress().c_str());
        if (!socket->Bind(my_addr, NS_PLEX_MEDIA_SERVER_PORT, 0))
        {
          CLog::Log(LOGERROR, "CPlexServices:CheckforGDMServers Could not listen on port %d", NS_PLEX_MEDIA_SERVER_PORT);
          SAFE_DELETE(m_gdmListener);
          m_useGDMServer = false;
          return;
        }

        if (socket)
        {
          socket->SetBroadCast(true);
          // create and add our socket to the 'select' listener
          m_gdmListener = new SOCKETS::CSocketListener();
          m_gdmListener->AddSocket(socket);
        }
      }
      else
      {
        CLog::Log(LOGERROR, "CPlexServices:CheckforGDMServers Could not create socket for GDM");
        m_useGDMServer = false;
        return;
      }
    }

    SOCKETS::CUDPSocket *socket = (SOCKETS::CUDPSocket*)m_gdmListener->GetFirstSocket();
    if (socket)
    {
      SOCKETS::CAddress discoverAddress;
      discoverAddress.SetAddress(NS_BROADCAST_ADDR.c_str(), NS_PLEX_MEDIA_SERVER_PORT);
      std::string discoverMessage = NS_SEARCH_MSG;
      int packetSize = socket->SendTo(discoverAddress, discoverMessage.length(), discoverMessage.c_str());
      if (packetSize < 0)
        CLog::Log(LOGERROR, "CPlexServices:CPlexServices:CheckforGDMServers discover send failed");
    }

    bool foundNewClient = false;
    // listen for GDM reply until we timeout
    if (socket && m_gdmListener->Listen(250))
    {
      char buffer[1024] = {0};
      SOCKETS::CAddress sender;
      int packetSize = socket->Read(sender, 1024, buffer);
      if (packetSize > -1)
      {
        std::string buf(buffer, packetSize);
        if (buf.find("200 OK") != std::string::npos)
        {
          CPlexClientPtr client(new CPlexClient());
          if (client->Init(buf, sender.Address()))
          {
            if (AddClient(client))
            {
              CLog::Log(LOGNOTICE, "CPlexServices:CheckforGDMServers Server found via GDM %s", client->GetServerName().c_str());
            }
            else if (GetClient(client->GetUuid()) == nullptr)
            {
              // lost client
              CLog::Log(LOGNOTICE, "CPlexServices:CheckforGDMServers Server was lost %s", client->GetServerName().c_str());
            }
            else if (UpdateClient(client))
            {
              // client exists and something changed
              CLog::Log(LOGNOTICE, "CPlexServices:CheckforGDMServers presence changed %s", client->GetServerName().c_str());
            }
          }
        }
      }
    }
    if (foundNewClient)
      AddJob(new CPlexServiceJob(0, "FoundNewClient"));
  }
}

CPlexClientPtr CPlexServices::GetClient(std::string uuid)
{
  CSingleLock lock(m_criticalClients);
  for (const auto &client : m_clients)
  {
    if (client->GetUuid() == uuid)
      return client;
  }

  return nullptr;
}

bool CPlexServices::AddClient(CPlexClientPtr foundClient)
{
  CSingleLock lock(m_criticalClients);
  for (const auto &client : m_clients)
  {
    // do not add existing clients
    if (client->GetUuid() == foundClient->GetUuid())
      return false;
  }

  // only add new clients that are present
  if (foundClient->GetPresence() && foundClient->ParseSections(PlexSectionParsing::newSection))
  {
    m_clients.push_back(foundClient);
    m_hasClients = !m_clients.empty();
    return true;
  }

  return false;
}

bool CPlexServices::RemoveClient(CPlexClientPtr lostClient)
{
  CSingleLock lock(m_criticalClients);
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

bool CPlexServices::UpdateClient(CPlexClientPtr updateClient)
{
  CSingleLock lock(m_criticalClients);
  for (const auto &client : m_clients)
  {
    if (client->GetUuid() == updateClient->GetUuid())
    {
      // client needs updating
      if (client->GetPresence() != updateClient->GetPresence())
      {
        client->SetPresence(updateClient->GetPresence());
        // update any gui lists here.
        for (const auto &item : client->GetSectionItems())
        {
          std::string title = client->FindSectionTitle(item->GetPath());
          if (!title.empty())
          {
            item->SetLabel(client->FormatContentTitle(title));
            CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_ITEM, 0, item);
            g_windowManager.SendThreadMessage(msg);
          }
        }
        return true;
      }
      // no need to look further but an update was not needed
      return false;
    }
  }

  return false;
}

bool CPlexServices::GetMyHomeUsers(std::string &homeUserName)
{
  bool rtn = false;

  std::string strMessage;
  XFILE::CCurlFile plex;
  plex.SetBufferSize(32768*10);
  CPlexUtils::GetDefaultHeaders(plex);
  if (MyPlexSignedIn())
    plex.SetRequestHeader("X-Plex-Token", m_authToken);

  std::string strResponse;
  CURL url(NS_PLEXTV_URL + "/api/home/users");
  if (plex.Get(url.Get(), strResponse))
  {
#if defined(PLEX_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CPlexServices:GetMyHomeUsers %s", strResponse.c_str());
#endif

    TiXmlDocument xml;
    CFileItemList plexUsers;
    xml.Parse(strResponse.c_str());

    TiXmlElement* MediaContainer = xml.RootElement();
    if (MediaContainer)
    {
      std::string users = XMLUtils::GetAttribute(MediaContainer, "size");
      if (atoi(users.c_str()) == 1)
      {
        // if we only have one user show the name of it
        const TiXmlElement* UserNode = MediaContainer->FirstChildElement("User");
        homeUserName = XMLUtils::GetAttribute(UserNode, "title");
        return true;
      }
      else if (atoi(users.c_str()) > 1)
      {
        const TiXmlElement* UserNode = MediaContainer->FirstChildElement("User");
        while (UserNode)
        {
          CFileItemPtr plexUser(new CFileItem());
          // set m_bIsFolder to true to indicate we are tvshow list
          plexUser->m_bIsFolder = true;
          plexUser->SetProperty("title", XMLUtils::GetAttribute(UserNode, "title"));
          plexUser->SetProperty("uuid", XMLUtils::GetAttribute(UserNode, "uuid"));
          plexUser->SetProperty("id", XMLUtils::GetAttribute(UserNode, "id"));
          plexUser->SetProperty("protected", XMLUtils::GetAttribute(UserNode, "protected"));
          plexUser->SetLabel(XMLUtils::GetAttribute(UserNode, "title"));
          plexUser->SetIconImage(XMLUtils::GetAttribute(UserNode, "thumb"));
          plexUser->SetArt("thumb", XMLUtils::GetAttribute(UserNode, "thumb"));
          plexUsers.Add(plexUser);
          UserNode = UserNode->NextSiblingElement("User");
        }
      }
      else
      {
        return false;
      }
    }

    CGUIDialogSelect *dialog = (CGUIDialogSelect*)g_windowManager.GetWindow(WINDOW_DIALOG_SELECT);
    if (dialog == NULL)
      return false;

    dialog->Reset();
    dialog->SetHeading("Choose User");
    dialog->SetItems(plexUsers);
    dialog->SetMultiSelection(false);
    dialog->SetUseDetails(true);
    dialog->Open();

    if (!dialog->IsConfirmed())
      return false;

    const CFileItemPtr item = dialog->GetSelectedItem();

    if (item == NULL || !item->HasProperty("id"))
      return false;

    std::string pinUrl = "/switch";
    if (item->GetProperty("protected").asBoolean())
    {
      std::string pin;
      if( !CGUIDialogNumeric::ShowAndGetNumber(pin, "Enter pin") )
        return false;
      pinUrl = "/switch?pin=" + pin;
    }

    XFILE::CCurlFile plex;
    CPlexUtils::GetDefaultHeaders(plex);
    if (MyPlexSignedIn())
      plex.SetRequestHeader("X-Plex-Token", m_authToken);

    std::string id = item->GetProperty("id").asString();
    CURL url(NS_PLEXTV_URL + "/api/home/users/" + id + pinUrl);

    CPlexUtils::GetDefaultHeaders(plex);
    std::string strResponse;
    plex.Post(url.Get(), "", strResponse);

    TiXmlDocument xml1;
    xml1.Parse(strResponse.c_str());

    TiXmlElement* userContainer = xml1.RootElement();
    if (userContainer)
    {
      m_authToken = XMLUtils::GetAttribute(userContainer, "authToken");
      homeUserName = XMLUtils::GetAttribute(userContainer, "title");
      rtn = !homeUserName.empty() && !m_authToken.empty();
    }
    else
    {
      strMessage = "Couldn't get home users";
    }
  }
  else
  {
    strMessage = "Could not connect to retreive Home users";
    CLog::Log(LOGDEBUG, "CPlexServices:GetMyHomeUsers failed %s", strResponse.c_str());
  }

  if (!rtn)
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Plex Services", strMessage, 3000, true);
  return rtn;
}

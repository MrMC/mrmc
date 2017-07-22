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

#include "TraktServices.h"

#include "Application.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogProgress.h"
#include "dialogs/GUIDialogYesNo.h"
#include "filesystem/ZipFile.h"
#include "guilib/GUIWindowManager.h"
#include "interfaces/AnnouncementManager.h"
#include "interfaces/json-rpc/JSONUtils.h"
#include "settings/lib/Setting.h"
#include "settings/Settings.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/log.h"
#include "utils/StringHasher.h"
#include "utils/SystemInfo.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"

#include "video/VideoInfoTag.h"
#include "video/VideoDatabase.h"

#include "services/emby/EmbyClient.h"
#include "services/emby/EmbyServices.h"

static char chars[] = "'/!";
void removeCharsFromString( std::string &str)
{
  for ( unsigned int i = 0; i < strlen(chars); ++i ) {
    str.erase( remove(str.begin(), str.end(), chars[i]), str.end() );
  }
}

using namespace ANNOUNCEMENT;

static const std::string NS_TRAKT_CLIENTID("fdf32abb08db6b163f31cb6be8b06ede301b4d883b7c050f88efcd82ca9a2dbc");
static const std::string NS_TRAKT_CLIENTSECRET("0cb37612e9c2fcdcb22f1dc7504465ebc34c155256c52597c0cd524bc082c7c7");

class CTraktServiceJob: public CJob
{
public:
  CTraktServiceJob(CFileItem &item, double percentage, std::string strFunction)
  : m_item(item)
  , m_function(strFunction)
  , m_percentage(percentage)
  {
  }
  CTraktServiceJob(std::string strFunction)
  : m_item(*new CFileItem)
  , m_function(strFunction)
  , m_percentage(0)
  {
  }
  virtual ~CTraktServiceJob()
  {
  }
  virtual bool DoWork()
  {
    using namespace StringHasher;
    switch(mkhash(m_function.c_str()))
    {
      case "OnPlay"_mkhash:
        CLog::Log(LOGDEBUG, "CTraktServiceJob::OnPlay currentTime = %f", m_currentTime);
        CTraktServices::ReportProgress(m_item, "start", m_percentage);
        break;
      case "OnSeek"_mkhash:
        // Trakt API only as start/pause/stop. It is unclear what
        // to do about if you are seeking, others seem to just do
        // start again. We can too.
        CLog::Log(LOGDEBUG, "CTraktServiceJob::OnSeek currentTime = %f", m_currentTime);
        CTraktServices::ReportProgress(m_item, "start", m_percentage);
        break;
      case "OnPause"_mkhash:
        CLog::Log(LOGDEBUG, "CTraktServiceJob::OnPause currentTime = %f", m_currentTime);
        CTraktServices::ReportProgress(m_item, "pause", m_percentage);
        break;
      case "TraktSetStopped"_mkhash:
        CTraktServices::ReportProgress(m_item, "stop", m_percentage);
        break;
      case "TraktSetWatched"_mkhash:
        CTraktServices::SetItemWatchedJob(m_item, true);
        break;
      case "TraktSetUnWatched"_mkhash:
        CTraktServices::SetItemWatchedJob(m_item, false);
        break;
      case "TraktPushWatched"_mkhash:
        CTraktServices::PushWatchedStatus();
        break;
      case "TraktPullWatched"_mkhash:
        CTraktServices::PullWatchedStatus();
        break;
      default:
        return false;
    }
    return true;
  }
  virtual bool operator==(const CJob *job) const
  {
    if (strcmp(job->GetType(),GetType()) == 0)
    {
      const CTraktServiceJob* rjob = dynamic_cast<const CTraktServiceJob*>(job);
      if (rjob)
      {
        return m_function == rjob->m_function;
      }
    }
    return false;
  }

private:
  CFileItem   m_item;
  std::string m_function;
  double      m_percentage;
  double      m_currentTime;
};

CTraktServices::CTraktServices()
{
  CAnnouncementManager::GetInstance().AddAnnouncer(this);
}

CTraktServices::~CTraktServices()
{
  CAnnouncementManager::GetInstance().RemoveAnnouncer(this);
}

CTraktServices& CTraktServices::GetInstance()
{
  static CTraktServices sTraktServices;
  return sTraktServices;
}

bool CTraktServices::IsEnabled()
{
  return !CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_TRAKTACESSTOKEN).empty();
}

void CTraktServices::Announce(AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  if (!IsEnabled())
    return;

  if ((flag & AnnouncementFlag::Player) && strcmp(sender, "xbmc") == 0)
  {
    double percentage;
    CFileItem &item = g_application.CurrentFileItem();
    using namespace StringHasher;
    switch(mkhash(message))
    {
      // Announce of "OnStop" has wrong timestamp, so we pick it up from
      // CApplication::SaveFileState which calls our SaveFileState
      case "OnPlay"_mkhash:
        // "OnPlay" could be playback startup, or after pause. We really
        // cannot tell from the Announce message or passed CVariant data.
        // So we track the state internally and fetch the correct play time.
        // Note: m_resumePoint is ONLY good for playback startup.
        if (GetPlayState(item) == MediaServicesPlayerState::paused)
          percentage = 100.0 * g_application.GetTime() / g_application.GetTotalTime();
        else
        {
          percentage = 0;
          if (item.GetVideoInfoTag()->m_resumePoint.totalTimeInSeconds > 0)
            percentage = 100.0 * item.GetVideoInfoTag()->m_resumePoint.timeInSeconds / item.GetVideoInfoTag()->m_resumePoint.totalTimeInSeconds;
        }
        if (percentage < 0.0)
          percentage = 0.0;
        SetPlayState(item, MediaServicesPlayerState::playing);
        CLog::Log(LOGDEBUG, "CTraktServiceJob::Announce OnPlay currentSeconds = %f", percentage);
        AddJob(new CTraktServiceJob(item, percentage, message));
        break;
      case "OnPause"_mkhash:
        SetPlayState(item, MediaServicesPlayerState::paused);
        percentage = 100.0 * g_application.GetTime() / g_application.GetTotalTime();
        CLog::Log(LOGDEBUG, "CTraktServiceJob::Announce OnPause currentSeconds = %f", percentage);
        AddJob(new CTraktServiceJob(item, percentage, message));
        break;
      case "OnSeek"_mkhash:
        // Ahh, finally someone actually gives us the right playtime.
        percentage = 100.0 * JSONRPC::CJSONUtils::TimeObjectToMilliseconds(data["player"]["time"]) / g_application.GetTotalTime();
        CLog::Log(LOGDEBUG, "CTraktServiceJob::Announce OnSeek currentSeconds = %f", percentage);
        AddJob(new CTraktServiceJob(item, percentage, message));
        break;
      case "OnStop"_mkhash:
        SetPlayState(item, MediaServicesPlayerState::stopped);
        break;
      default:
        break;
    }
  }
  else if ((flag & AnnouncementFlag::GUI) && strcmp(sender, "xbmc") == 0)
  {
    if (std::string(message) == "OnCreated")
    {
      GetUserSettings();
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTPULLWATCHED, "Idle");
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTPUSHWATCHED, "Idle");
      m_canSync = IsEnabled();
    }
  }
  else if ((flag & AnnouncementFlag::Other) && strcmp(sender, "trakt") == 0)
  {
    if (strcmp(message, "ReloadProfiles") == 0)
    {
      // restart if MrMC profiles has changed
      GetUserSettings();
    }
  }
  else if ((flag & AnnouncementFlag::VideoLibrary) && strcmp(sender, "xbmc") == 0)
  {
    if (strcmp(message, "OnScanFinished") == 0 && m_canSync)
    {
      int traktSetting = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_TRAKTACTIONAFTERLIBRARYUPDATE);
      if (traktSetting == 1)
      {
        m_canSync = false;
        AddJob(new CTraktServiceJob("TraktPushWatched"));
      }
      else if (traktSetting == 2)
      {
        m_canSync = false;
        AddJob(new CTraktServiceJob("TraktPullWatched"));
      }
    }
  }
}

void CTraktServices::OnSettingAction(const CSetting *setting)
{
  /*
   static const std::string SETTING_SERVICES_TRAKTSIGNINPIN;
   static const std::string SETTING_SERVICES_TRAKTPULLWATCHED;
   static const std::string SETTING_SERVICES_TRAKTPUSHWATCHED;
   static const std::string SETTING_SERVICES_TRAKTACESSTOKEN;
   static const std::string SETTING_SERVICES_TRAKTACESSREFRESHTOKEN;
   static const std::string SETTING_SERVICES_TRAKTACESSTOKENVALIDITY;
   */

  if (setting == nullptr)
    return;

  bool startThread = false;
  std::string strMessage;
  std::string strSignIn = g_localizeStrings.Get(1240);
  std::string strSignOut = g_localizeStrings.Get(1241);
  const std::string& settingId = setting->GetId();

  if (settingId == CSettings::SETTING_SERVICES_TRAKTSIGNINPIN)
  {
    if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_TRAKTSIGNINPIN) == strSignIn)
    {
      if (GetSignInPinCode())
      {
        // change prompt to 'sign-out'
        CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTSIGNINPIN, strSignOut);
        CLog::Log(LOGDEBUG, "CTraktServices:OnSettingAction pin sign-in ok");
        startThread = true;
        m_canSync = true;
      }
      else
      {
        std::string strMessage = "Could not get authToken via pin request sign-in";
        CLog::Log(LOGERROR, "CTraktServices: %s", strMessage.c_str());
        m_canSync = false;
      }
    }
    else
    {
      // prompt is 'sign-out'
      // clear authToken and change prompt to 'sign-in'
      m_authToken.clear();
      m_authTokenValidity = 0;
      m_refreshAuthToken.clear();
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTSIGNINPIN, strSignIn);
      CLog::Log(LOGDEBUG, "CTraktServices:OnSettingAction sign-out ok");
      m_canSync = false;
    }
    SetUserSettings();
  }
  else if (settingId == CSettings::SETTING_SERVICES_TRAKTPULLWATCHED ||
           settingId == CSettings::SETTING_SERVICES_TRAKTPUSHWATCHED)
  {
    if (!m_canSync)
      return;

    CGUIDialogYesNo* pDialog = (CGUIDialogYesNo*)g_windowManager.GetWindow(WINDOW_DIALOG_YES_NO);
    if (!pDialog) return;

    std::string line1 = (settingId == CSettings::SETTING_SERVICES_TRAKTPULLWATCHED) ? g_localizeStrings.Get(36633):g_localizeStrings.Get(36635);
    pDialog->SetHeading(CVariant{"Trakt.tv"});
    pDialog->SetLine(1, CVariant{line1});
    pDialog->SetLine(2, CVariant{36637});
    pDialog->Open();

    if (!pDialog->IsConfirmed())
      return;

    if (settingId == CSettings::SETTING_SERVICES_TRAKTPULLWATCHED)
    {
      m_canSync = false;
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTPULLWATCHED, "Running");
      AddJob(new CTraktServiceJob("TraktPullWatched"));
    }
    else
    {
      m_canSync = false;
      CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTPUSHWATCHED, "Running");
      AddJob(new CTraktServiceJob("TraktPushWatched"));
    }
  }
}

void CTraktServices::OnSettingChanged(const CSetting *setting)
{
  // All Trakt settings so far
  /*
  static const std::string SETTING_SERVICES_TRAKTSIGNINPIN;
  static const std::string SETTING_SERVICES_TRAKTACESSTOKEN;
  static const std::string SETTING_SERVICES_TRAKTACESSTOKENVALIDITY;
  */

  if (setting == NULL)
    return;
}

void CTraktServices::SetUserSettings()
{
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTACESSTOKEN, m_authToken);
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTACESSREFRESHTOKEN, m_refreshAuthToken);
  CSettings::GetInstance().SetInt(CSettings::SETTING_SERVICES_TRAKTACESSTOKENVALIDITY, m_authTokenValidity);
  CSettings::GetInstance().Save();
}

void CTraktServices::GetUserSettings()
{
  m_authToken  = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_TRAKTACESSTOKEN);
  m_refreshAuthToken  = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_TRAKTACESSREFRESHTOKEN);
  m_authTokenValidity  = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_TRAKTACESSTOKENVALIDITY);
}

bool CTraktServices::MyTraktSignedIn()
{
  return !m_authToken.empty();
}

bool CTraktServices::GetSignInPinCode()
{
  // on return, show user m_signInByPinCode so they can enter it at https://emby.media/pin
  bool rtn = false;
  std::string strMessage;

  XFILE::CCurlFile curlfile;
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");

  CURL curl("https://trakt.tv");
  curl.SetFileName("oauth/device/code");
  curl.SetOption("format", "json");

  CVariant data;
  data["client_id"] = NS_TRAKT_CLIENTID;
  std::string jsonBody;
  if (!CJSONVariantWriter::Write(data, jsonBody, false))
    return rtn;
  std::string response;
  if (curlfile.Post(curl.Get(), jsonBody, response))
  {
#if defined(TRAKT_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CTraktServices:FetchSignInPin %s", response.c_str());
#endif
    CVariant reply;
    std::string verification_url;
    std::string user_code;
    int expires_in;
    int interval;
    if (!CJSONVariantParser::Parse(response, reply))
      return rtn;

    if (!reply.isObject() && !reply.isMember("user_code"))
      return rtn;
 
    m_deviceCode = reply["device_code"].asString();
    verification_url = reply["verification_url"].asString();
    expires_in = reply["expires_in"].asInteger();
    interval = reply["interval"].asInteger();
    user_code = reply["user_code"].asString();

    CGUIDialogProgress *waitPinReplyDialog;
    waitPinReplyDialog = (CGUIDialogProgress*)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
    waitPinReplyDialog->SetHeading(g_localizeStrings.Get(2115));
    waitPinReplyDialog->SetLine(0, g_localizeStrings.Get(2117));
    std::string prompt = verification_url + g_localizeStrings.Get(2119) + user_code;
    waitPinReplyDialog->SetLine(1, prompt);

    waitPinReplyDialog->Open();
    waitPinReplyDialog->ShowProgressBar(true);

    CStopWatch dieTimer;
    dieTimer.StartZero();

    CStopWatch pingTimer;
    pingTimer.StartZero();

    m_authToken.clear();
    while (!waitPinReplyDialog->IsCanceled())
    {
      waitPinReplyDialog->SetPercentage(int(float(dieTimer.GetElapsedSeconds())/float(expires_in)*100));
      if (pingTimer.GetElapsedSeconds() > interval)
      {
        // wait for user to run and enter pin code
        rtn = GetSignInByPinReply();
        if (rtn)
          break;
        pingTimer.Reset();
        m_processSleep.WaitMSec(250);
        m_processSleep.Reset();
      }

      if (dieTimer.GetElapsedSeconds() > expires_in)
      {
        rtn = false;
        break;
      }
      waitPinReplyDialog->Progress();
    }
    waitPinReplyDialog->Close();

    if (m_authToken.empty())
    {
      strMessage = "Error extracting AcessToken";
      CLog::Log(LOGERROR, "CTraktServices::FetchSignInPin failed to get authToken");
      rtn = false;
    }
  }
  else
  {
    strMessage = "Could not connect to retreive AuthToken";
    CLog::Log(LOGERROR, "CTraktServices::FetchSignInPin failed %s", response.c_str());
  }
  if (!rtn)
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, "Emby Services", strMessage, 3000, true);
  return rtn;
}

bool CTraktServices::GetSignInByPinReply()
{
  bool rtn = false;

  XFILE::CCurlFile curlfile;
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");
  curlfile.SetSilent(true);

  CURL curl("https://trakt.tv");
  curl.SetFileName("oauth/device/token");
  curl.SetOption("format", "json");

  CVariant data;
  data["code"] = m_deviceCode;
  data["client_id"] = NS_TRAKT_CLIENTID;
  data["client_secret"] = NS_TRAKT_CLIENTSECRET;
  std::string jsondata;
  if (!CJSONVariantWriter::Write(data, jsondata, false))
    return rtn;
  std::string response;
  if (curlfile.Post(curl.Get(), jsondata, response))
  {
#if defined(TRAKT_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CTraktServices:AuthenticatePinReply %s", response.c_str());
#endif
    CVariant reply;
    if (!CJSONVariantParser::Parse(response, reply))
      return rtn;
    if (reply.isObject() && reply.isMember("access_token"))
    {
      m_authToken = reply["access_token"].asString();
      m_refreshAuthToken = reply["refresh_token"].asString();
      m_authTokenValidity = reply["created_at"].asInteger() + reply["expires_in"].asInteger();
      SetUserSettings();
      rtn = true;
    }
  }
  return rtn;
}

const MediaServicesPlayerState CTraktServices::GetPlayState(CFileItem &item)
{
  // will create a new playstate if nothing matches
  CSingleLock lock(m_playStatesLock);
  std::string path = item.GetPath();
  for (const auto &playState : m_playStates)
  {
    if (path == playState.path)
    {
      return playState.state;
    }
  }
  TraktPlayState newPlayState;
  newPlayState.path = item.GetPath();
  m_playStates.push_back(newPlayState);
  return newPlayState.state;
}

void CTraktServices::SetPlayState(CFileItem &item, const MediaServicesPlayerState &state)
{
  // will erase existing playstate if matches and state is stopped
  CSingleLock lock(m_playStatesLock);
  std::string path = item.GetPath();
  for (auto playStateIt = m_playStates.begin(); playStateIt != m_playStates.end() ; ++playStateIt)
  {
    if (path == playStateIt->path)
    {
      if (state == MediaServicesPlayerState::stopped)
        m_playStates.erase(playStateIt);
      else
        playStateIt->state = state;
      break;
    }
  }
}

void CTraktServices::SetItemWatchedJob(CFileItem &item, bool watched, bool setLastWatched)
{
  CVariant data;
  CDateTime now = CDateTime::GetCurrentDateTime();
  CDateTime lastPlayed = item.GetVideoInfoTag()->m_lastPlayed;

  if (setLastWatched && lastPlayed.IsValid())
    now = lastPlayed;

  if (item.HasVideoInfoTag() && item.GetVideoInfoTag()->m_type == MediaTypeMovie)
  {
    std::string timenow = now.GetAsW3CDateTime(true);
    CVariant movie;

    movie["title"]      = item.GetVideoInfoTag()->m_strTitle;
    movie["year"]       = item.GetVideoInfoTag()->GetYear();
    movie["watched_at"] = now.GetAsW3CDateTime(true);
    movie["ids"]        = ParseIds(item.GetVideoInfoTag()->GetUniqueIDs(), item.GetVideoInfoTag()->m_type);
    data["movies"].push_back(movie);
  }
  else if (item.HasVideoInfoTag() &&
           (item.GetVideoInfoTag()->m_type == MediaTypeTvShow ||
            item.GetVideoInfoTag()->m_type == MediaTypeSeason ||
            item.GetVideoInfoTag()->m_type == MediaTypeEpisode))
  {
    CFileItem showItem;
    if (!item.IsMediaServiceBased())
    {
      CVideoDatabase videodatabase;
      if (!videodatabase.Open())
        return;
      
      std::string basePath = StringUtils::Format("videodb://tvshows/titles/%i/%i/%i",item.GetVideoInfoTag()->m_iIdShow, item.GetVideoInfoTag()->m_iSeason, item.GetVideoInfoTag()->m_iDbId);
      videodatabase.GetTvShowInfo(basePath, *showItem.GetVideoInfoTag(), item.GetVideoInfoTag()->m_iIdShow);
      videodatabase.Close();
    }
    else
    {
      if (item.HasProperty("PlexItem"))
      {
           // plex dosnt have any tvdb or imdb IDs so we just send the show name and the season
          // less reliable but seems to work
      }
      else if (item.HasProperty("EmbyItem"))
      {
        CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(item.GetPath());
        if (client && client->GetPresence())
        {
          CVariant paramsseries;
          std::string seriesId = item.GetProperty("EmbySeriesID").asString();
          paramsseries = client->FetchItemById(seriesId);
          CVariant paramsProvID = paramsseries["Items"][0]["ProviderIds"];
          if (paramsProvID.isObject())
          {
            for (auto it = paramsProvID.begin_map(); it != paramsProvID.end_map(); ++it)
            {
              std::string strFirst = it->first;
              StringUtils::ToLower(strFirst);
              showItem.GetVideoInfoTag()->SetUniqueID(it->second.asString(),strFirst);
            }
          }
        }
      }
    }
    CVariant show;
    show["title"]      = item.GetVideoInfoTag()->m_strShowTitle;
    show["year"]       = item.GetVideoInfoTag()->GetYear();

    if (item.GetVideoInfoTag()->m_type == MediaTypeTvShow)
      show["watched_at"] = now.GetAsW3CDateTime(true);

    show["ids"]        = ParseIds(showItem.GetVideoInfoTag()->GetUniqueIDs(), item.GetVideoInfoTag()->m_type);

    if (item.GetVideoInfoTag()->m_type == MediaTypeSeason || item.GetVideoInfoTag()->m_type == MediaTypeEpisode)
    {
      CVariant season;
      season["number"] = item.GetVideoInfoTag()->m_iSeason;

      if (item.GetVideoInfoTag()->m_type == MediaTypeEpisode)
      {
        CVariant episode;
        episode["watched_at"] = now.GetAsW3CDateTime(true);
        episode["number"]        = item.GetVideoInfoTag()->m_iEpisode;
        season["episodes"].push_back(episode);
      }
      else
        season["watched_at"] = now.GetAsW3CDateTime(true);
      show["seasons"].push_back(season);
    }
    data["shows"].push_back(show);
  }

  std::string unwatched = watched ? "":"/remove";
  // send it to server
  ServerChat("https://api.trakt.tv/sync/history" + unwatched,data);
}

void CTraktServices::SetItemWatched(CFileItem &item)
{
  AddJob(new CTraktServiceJob(item, 100, "TraktSetWatched"));
}

void CTraktServices::SetItemUnWatched(CFileItem &item)
{
  AddJob(new CTraktServiceJob(item, 0, "TraktSetUnWatched"));
}

void CTraktServices::ReportProgress(CFileItem &item, const std::string &status, double percentage)
{
  // if we are music, do not report
  if (item.IsAudio() || item.IsPVR())
    return;

  if (!status.empty())
  {
    CLog::Log(LOGDEBUG, "CTraktServices::ReportProgress status = %s, percentage = %f", status.c_str(), percentage);
    CVariant data;
    if (item.HasVideoInfoTag() && item.GetVideoInfoTag()->m_type == MediaTypeEpisode)
    {
      /// https://api.trakt.tv/shows/top-gear/seasons/24
      // to get a list of episodes, there we will find what we need

      std::string showName = item.GetVideoInfoTag()->m_strShowTitle;
      removeCharsFromString(showName);
      StringUtils::Replace(showName, " ", "-");
      std::string episodesUrl = StringUtils::Format("https://api.trakt.tv/shows/%s/seasons/%i",
        showName.c_str(), item.GetVideoInfoTag()->m_iSeason);
      CVariant episodes = GetTraktCVariant(episodesUrl);

      if (episodes.isArray())
      {
        for (auto it = episodes.begin_array(); it != episodes.end_array(); ++it)
        {
          CVariant &episodeItem = *it;
          if (episodeItem["number"].asInteger() == item.GetVideoInfoTag()->m_iEpisode)
          {
            data["episode"] = episodeItem;
            break;
          }
        }
      }
      data["progress"] = percentage;
      data["app_version"] = CSysInfo::GetVersion();
      data["app_date"] = CSysInfo::GetBuildDate();
    }
    else if (item.HasVideoInfoTag() && item.GetVideoInfoTag()->m_type == MediaTypeMovie)
    {
      data["movie"]["title"] = item.GetVideoInfoTag()->m_strTitle;
      data["movie"]["year"] = item.GetVideoInfoTag()->GetYear();
      data["movie"]["ids"] = ParseIds(item.GetVideoInfoTag()->GetUniqueIDs(), item.GetVideoInfoTag()->m_type);
      data["progress"] = percentage;
      data["app_version"] = CSysInfo::GetVersion();
      data["app_date"] = CSysInfo::GetBuildDate();
    }

    if (!data.isNull())
    {
      // now that we have "data" talk to trakt server
      // If the progress is above 80%, the video will be scrobbled
      // and we will get a 409 on stop. Ignore it :)
      ServerChat("https://api.trakt.tv/scrobble/" + status, data);
    }
  }
}

void CTraktServices::SaveFileState(CFileItem &item, double currentTime, double totalTime)
{
  double percentage = 100.0 * currentTime / totalTime;
  if (isnan(percentage))
    percentage = 0;
  AddJob(new CTraktServiceJob(item, percentage, "TraktSetStopped"));
}

CVariant CTraktServices::ParseIds(const std::map<std::string, std::string> &Ids, const std::string &type)
{
  CVariant variantIDs;
  for (std::map<std::string, std::string>::const_iterator i = Ids.begin(); i != Ids.end(); ++i)
  {
    if (i->first == "unknown")
    {
      std::string id = i->second;
      if (StringUtils::StartsWithNoCase(id, "tt"))
        variantIDs["imdb"] = id;
      else if (isdigit(*id.c_str()) && type == MediaTypeMovie)
        variantIDs["tmdb"] = atoi(id.c_str());
      else if (isdigit(*id.c_str()) && (type == MediaTypeEpisode || type == MediaTypeSeason || type == MediaTypeTvShow))
        variantIDs["tvdb"] = atoi(id.c_str());
      else
        variantIDs["slug"] = id;
    }
    else
    {
      variantIDs[i->first] = i->second;
    }
  }
  return variantIDs;
}

void CTraktServices::PullWatchedStatus()
{
  CLog::Log(LOGDEBUG, "CTraktServices::PullWatchedStatus() - Started");
  CVideoDatabase videodb;

  if (!videodb.Open())
    return;

  if (!videodb.HasContent())
  {
    videodb.Close();
    return;
  }

  CFileItemList items;
  CVideoDatabase::Filter filterMov;
  filterMov.AppendWhere("playCount IS NULL OR playCount < 1");
  videodb.GetMoviesByWhere("videodb://movies/titles/", filterMov, items);

  CVariant data = GetTraktCVariant("https://api.trakt.tv/sync/watched/movies");
  if (data.isArray() && items.Size() > 0)
  {
    for (auto it = data.begin_array(); it != data.end_array(); ++it)
    {
      const CVariant &movieItem = *it;
      for (int i = 0; i < items.Size(); ++i)
      {
        if (items[i]->GetVideoInfoTag() == nullptr)
          continue;

        const std::string itemDefault = items[i]->GetVideoInfoTag()->GetUniqueID();
        if (itemDefault.empty())
          continue;

        CLog::Log(LOGDEBUG, "CTraktServices::PullWatchedStatus() - Matching movie [%s] to {%s} {%s}",
                  itemDefault.c_str(),
                  movieItem["movie"]["ids"]["tmdb"].asString().c_str(),
                  movieItem["movie"]["ids"]["imdb"].asString().c_str());

        if (itemDefault == movieItem["movie"]["ids"]["tmdb"].asString() ||
            itemDefault == movieItem["movie"]["ids"]["imdb"].asString())
        {
          CLog::Log(LOGDEBUG, "CTraktServices::PullWatchedStatus() - Matched movie [%s]", items[i]->GetVideoInfoTag()->m_strTitle.c_str());
          CDateTime date;
          date.SetFromW3CDateTime(movieItem["last_watched_at"].asString());
          videodb.ClearBookMarksOfFile(items[i]->GetVideoInfoTag()->GetPath(), CBookmark::RESUME);
          videodb.SetPlayCount(*items[i], movieItem["plays"].asInteger(), date);
        }
      }
    }
  }
  items.Clear();

  // get tvshows
  CVideoDatabase::Filter filter;
  videodb.GetTvShowsByWhere("videodb://tvshows/titles/", filter, items);

  CVariant dataShows = GetTraktCVariant("https://api.trakt.tv/sync/watched/shows");
  if (dataShows.isArray() && items.Size() > 0)
  {
    for (auto it = dataShows.begin_array(); it != dataShows.end_array(); ++it)
    {
      const CVariant &movieItem = *it;
      for (int i = 0; i < items.Size(); ++i)
      {
        if (items[i]->GetVideoInfoTag() == nullptr)
          continue;

        const std::string itemDefault = items[i]->GetVideoInfoTag()->GetUniqueID();
        if (itemDefault.empty())
          continue;

        if ((itemDefault == movieItem["show"]["ids"]["tvdb"].asString() ||
             itemDefault == movieItem["show"]["ids"]["tmdb"].asString() ||
             itemDefault == movieItem["show"]["ids"]["imdb"].asString()) &&
             items[i]->GetVideoInfoTag()->GetYear() == movieItem["show"]["year"].asInteger())
        {
          CFileItemList episodes;
          std::string strFilter = StringUtils::Format("idShow=%i AND playCount IS NULL OR playCount < 1",
            items[i]->GetVideoInfoTag()->m_iDbId);
          CVideoDatabase::Filter filterEpisodes;
          filterEpisodes.AppendWhere(strFilter);
          videodb.GetEpisodesByWhere("videodb://tvshows/titles/", filterEpisodes, episodes);
          for (int i = 0; i < episodes.Size(); ++i)
          {
            CVariant dataSeasons = movieItem["seasons"];
            for (auto is = dataSeasons.begin_array(); is != dataSeasons.end_array(); ++is)
            {
              const CVariant &seasonItem = *is;
              if (seasonItem["number"].asInteger() == episodes[i]->GetVideoInfoTag()->m_iSeason)
              {
                CVariant dataEpisodes = seasonItem["episodes"];
                for (auto ie = dataEpisodes.begin_array(); ie != dataEpisodes.end_array(); ++ie)
                {
                  const CVariant &episodeItem = *ie;
                  if (episodeItem["number"].asInteger() == episodes[i]->GetVideoInfoTag()->m_iEpisode)
                  {
                    CDateTime date;
                    date.SetFromW3CDateTime(episodeItem["last_watched_at"].asString());
                    videodb.ClearBookMarksOfFile(episodes[i]->GetVideoInfoTag()->GetPath(), CBookmark::RESUME);
                    videodb.SetPlayCount(*episodes[i], episodeItem["plays"].asInteger() , date);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  // close database
  videodb.CommitTransaction();
  videodb.Close();
  CLog::Log(LOGDEBUG, "CTraktServices::PullWatchedStatus() - Done");
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTPULLWATCHED, "Idle");
  GetInstance().m_canSync = true;
}

void CTraktServices::PushWatchedStatus()
{
  CLog::Log(LOGDEBUG, "CTraktServices::PushWatchedStatus() - Started");

  CVariant data;
  CVideoDatabase videodb;

  if (!videodb.Open())
    return;

  if (!videodb.HasContent())
    return;

  CFileItemList movieItems;
  CVideoDatabase::Filter filterMov;
  filterMov.AppendWhere("playCount > 0");
  videodb.GetMoviesByWhere("videodb://movies/titles/", filterMov, movieItems);

  if (movieItems.Size() > 0)
  {
    // get all watched movies on trakt.tv, we will compare to that later
    CVariant watchedMovies = GetTraktCVariant("https://api.trakt.tv/sync/watched/movies");
    for (int i = 0; i < movieItems.Size(); ++i)
    {
      if (movieItems[i]->GetVideoInfoTag() == nullptr)
        continue;

      if (!CompareMovieToHistory(*movieItems[i], watchedMovies))
      {
        CDateTime date = movieItems[i]->GetVideoInfoTag()->m_lastPlayed;
        std::string timenow = date.GetAsW3CDateTime(true);
        CVariant movie;
        movie["title"]      = movieItems[i]->GetVideoInfoTag()->m_strTitle;
        movie["year"]       = movieItems[i]->GetVideoInfoTag()->GetYear();
        movie["watched_at"] = timenow;
        movie["ids"]        = ParseIds(movieItems[i]->GetVideoInfoTag()->GetUniqueIDs(), movieItems[i]->GetVideoInfoTag()->m_type);
        data["movies"].push_back(movie);
      }
    }
  }

  CFileItemList showItems;
  CVideoDatabase::Filter filterShows;
  filterShows.AppendWhere("totalCount IS NOT NULL AND totalCount > 0 AND watchedcount > 0");
  videodb.GetTvShowsByWhere("videodb://tvshows/titles/", filterShows, showItems);

  if (showItems.Size() > 0)
  {
    // get all watched episodes on trakt.tv, we will compare to that later
    CVariant watchedShows = GetTraktCVariant("https://api.trakt.tv/sync/watched/shows");

    for (int i = 0; i < showItems.Size(); ++i)
    {
      if (showItems[i]->GetVideoInfoTag() == nullptr)
        continue;

      CFileItem showItem;
      std::string basePath = StringUtils::Format("videodb://tvshows/titles");
      videodb.GetTvShowInfo(basePath, *showItem.GetVideoInfoTag(), showItems[i]->GetVideoInfoTag()->m_iDbId);

      CVariant show;
      show["title"] = showItems[i]->GetVideoInfoTag()->m_strShowTitle;
      show["year"]  = showItems[i]->GetVideoInfoTag()->GetYear();
      show["ids"]   = ParseIds(showItem.GetVideoInfoTag()->GetUniqueIDs(), showItems[i]->GetVideoInfoTag()->m_type);

      std::string strFilter = StringUtils::Format("idShow=%i AND playCount IS NOT NULL AND playCount > 0",
        showItems[i]->GetVideoInfoTag()->m_iDbId);
      CFileItemList episodeItems;
      CVideoDatabase::Filter filterEpisodes;
      filterEpisodes.AppendWhere(strFilter);
      videodb.GetEpisodesByWhere("videodb://tvshows/titles/", filterEpisodes, episodeItems);

      if (episodeItems.Size() > 0)
      {
        int episodesAdded = 0;
        for (int i = 0; i < episodeItems.Size(); ++i)
        {
          if (episodeItems[i]->GetVideoInfoTag() == nullptr)
            continue;

          if (!CompareEpisodeToHistory(*episodeItems[i], watchedShows, showItem))
          {
            CVariant season;
            season["number"] = episodeItems[i]->GetVideoInfoTag()->m_iSeason;
            
            CDateTime date = episodeItems[i]->GetVideoInfoTag()->m_lastPlayed;
            std::string timePlayed = date.GetAsW3CDateTime(true);
            
            CVariant episode;
            episode["watched_at"] = timePlayed;
            episode["number"]     = episodeItems[i]->GetVideoInfoTag()->m_iEpisode;
            // add episode to season
            season["episodes"].push_back(episode);
            //add season to show
            show["seasons"].push_back(season);
            episodesAdded++;
          }
        }
        if (episodesAdded > 0)
          data["shows"].push_back(show);
      }
    }
  }
  ServerChat("https://api.trakt.tv/sync/history", data);
  CLog::Log(LOGDEBUG, "CTraktServices::PushWatchedStatus() - Done");
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTPUSHWATCHED, "Idle");
  GetInstance().m_canSync = true;
}

bool CTraktServices::CompareMovieToHistory(CFileItem &mItem, const CVariant &watchedMovies)
{
  const std::string itemDefault = mItem.GetVideoInfoTag()->GetUniqueID();
  if (itemDefault.empty())
    return false;

  // it will return true if it finds a match, same imdb/tmdb id
  for (auto it = watchedMovies.begin_array(); it != watchedMovies.end_array(); it++)
  {
    const CVariant &movieItem = *it;
    if (itemDefault == movieItem["movie"]["ids"]["tmdb"].asString() ||
        itemDefault == movieItem["movie"]["ids"]["imdb"].asString())
    {
      return true;
    }
  }
  return false;
}

bool CTraktServices::CompareEpisodeToHistory(CFileItem &eItem, const CVariant &watchedShows, CFileItem &sItem)
{
  int season  = eItem.GetVideoInfoTag()->m_iSeason;
  int episode = eItem.GetVideoInfoTag()->m_iEpisode;
  std::string defaultID = sItem.GetVideoInfoTag()->GetUniqueID();
  if (defaultID.empty())
    return false;

  // it will return true if it finds a match, same imdb/tvdb id
  for (auto it = watchedShows.begin_array(); it != watchedShows.end_array(); ++it)
  {
    const CVariant &showItem = *it;
    if (defaultID == showItem["show"]["ids"]["tvdb"].asString() ||
        defaultID == showItem["show"]["ids"]["imdb"].asString() ||
        defaultID == showItem["show"]["ids"]["imdb"].asString())
    {
      const CVariant &seasonItems = showItem["seasons"];
      for (auto is = seasonItems.begin_array(); is != seasonItems.end_array(); ++is)
      {
        const CVariant &seasonItem = *is;
        if (season == seasonItem["number"].asInteger())
        {
          const CVariant &episodesItems = seasonItem["episodes"];
          for (auto ie = episodesItems.begin_array(); ie != episodesItems.end_array(); ++ie)
          {
            const CVariant &episodeItem = *ie;
            if (episode == episodeItem["number"].asInteger())
            {
              // episode match, return true so we dont report it to server
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

CVariant CTraktServices::GetTraktCVariant(const std::string &url)
{
  // check if token expired
  CTraktServices::GetInstance().CheckAccessToken();

  XFILE::CCurlFile curlfile;
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");
  curlfile.SetRequestHeader("Accept-Encoding", "gzip");
  curlfile.SetRequestHeader("trakt-api-version", "2");
  curlfile.SetRequestHeader("trakt-api-key", NS_TRAKT_CLIENTID);
  curlfile.SetRequestHeader("Authorization", "Bearer " + CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_TRAKTACESSTOKEN));

  CURL curl(url);
  // this is key to get back gzip encoded content
  curl.SetProtocolOption("seekable", "0");
  // we always want json back
  curl.SetProtocolOptions(curl.GetProtocolOptions() + "&format=json");
  std::string response;
  if (curlfile.Get(curl.Get(), response))
  {
    if (curlfile.GetContentEncoding() == "gzip")
    {
      std::string buffer;
      if (XFILE::CZipFile::DecompressGzip(response, buffer))
        response = std::move(buffer);
      else
        return CVariant(CVariant::VariantTypeNull);
    }
#if defined(TRAKT_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CTraktServices::GetTraktCVariant %s", curl.Get().c_str());
    CLog::Log(LOGDEBUG, "CTraktServices::GetTraktCVariant - response %s", response.c_str());
#endif
    CVariant resultObject;
    if (CJSONVariantParser::Parse(response, resultObject))
    {
      if (resultObject.isObject() || resultObject.isArray())
        return resultObject;
    }
  }
  return CVariant(CVariant::VariantTypeNull);
}

void CTraktServices::ServerChat(const std::string &url, const CVariant &data)
{
  // check if token expired
  CTraktServices::GetInstance().CheckAccessToken();

  XFILE::CCurlFile curlfile;
  curlfile.SetSilent(true);
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");
  curlfile.SetRequestHeader("trakt-api-version", "2");
  curlfile.SetRequestHeader("trakt-api-key", NS_TRAKT_CLIENTID);
  curlfile.SetRequestHeader("Authorization", "Bearer " + CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_TRAKTACESSTOKEN));

  CURL curl(url);
  
  std::string jsondata;
  if (!CJSONVariantWriter::Write(data, jsondata, false))
    return;
  std::string response;
  if (curlfile.Post(curl.Get(), jsondata, response))
  {
#if defined(TRAKT_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CTraktServices::ServerChat %s", curl.Get().c_str());
    CLog::Log(LOGDEBUG, "CTraktServices::ServerChat - response %s", response.c_str());
#endif
  }
  else
  {
    // If the same item was just scrobbled, a 409 HTTP status code
    // will returned to avoid scrobbling a duplicate
    if (curlfile.GetResponseCode() != 409)
      CLog::Log(LOGDEBUG, "CTraktServices::ServerChat - failed, response %s", response.c_str());
  }

}

void CTraktServices::RefreshAccessToken()
{
  https://api.trakt.tv/oauth/token

  XFILE::CCurlFile curlfile;
  curlfile.SetSilent(true);
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");
  curlfile.SetRequestHeader("trakt-api-version", "2");
  curlfile.SetRequestHeader("trakt-api-key", NS_TRAKT_CLIENTID);
  curlfile.SetRequestHeader("Authorization", "Bearer " + CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_TRAKTACESSTOKEN));

  CURL curl("https://api.trakt.tv/oauth/token");

  CVariant data;
  data["refresh_token"] = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_TRAKTACESSREFRESHTOKEN);
  data["client_id"] = NS_TRAKT_CLIENTID;
  data["client_secret"] = NS_TRAKT_CLIENTSECRET;
  data["redirect_uri"] = "urn:ietf:wg:oauth:2.0:oob";
  data["grant_type"] = "refresh_token";
  std::string jsondata;
  if (!CJSONVariantWriter::Write(data, jsondata, false))
    return;
  std::string response;
  if (curlfile.Post(curl.Get(), jsondata, response))
  {
#if defined(TRAKT_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CTraktServices::RefreshAccessToken() %s", curl.Get().c_str());
    CLog::Log(LOGDEBUG, "CTraktServices::RefreshAccessToken() - response %s", response.c_str());
#endif
    CVariant reply;
    if (!CJSONVariantParser::Parse(response, reply))
      return;
    if (reply.isObject() && reply.isMember("access_token"))
    {
      m_authToken = reply["access_token"].asString();
      m_refreshAuthToken = reply["refresh_token"].asString();
      m_authTokenValidity = reply["created_at"].asInteger() + reply["expires_in"].asInteger();
      SetUserSettings();
    }
  }
}

void CTraktServices::CheckAccessToken()
{
  CDateTime now = CDateTime::GetUTCDateTime();
  time_t tNow = 0;
  time_t tExpiry = (time_t)CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_TRAKTACESSTOKENVALIDITY);
  now.GetAsTime(tNow);
  if (tNow > tExpiry)
  {
    RefreshAccessToken();
    CLog::Log(LOGDEBUG, "CTraktServices::CheckAccessToken() refreshed");
  }
}

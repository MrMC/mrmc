/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "GUIUserMessages.h"
#include "GUIWindowHome.h"
#include "GUIInfoManager.h"
#include "input/Key.h"
#include "guilib/WindowIDs.h"
#include "utils/JobManager.h"
#include "utils/HomeShelfJob.h"
#include "interfaces/AnnouncementManager.h"
#include "utils/log.h"
#include "settings/AdvancedSettings.h"
#include "utils/Variant.h"
#include "guilib/GUIWindowManager.h"
#include "Application.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogSelect.h"
#include "guilib/LocalizeStrings.h"
#include "settings/Settings.h"
#include "playlists/PlayList.h"
#include "messaging/ApplicationMessenger.h"
#include "services/ServicesManager.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "music/MusicDatabase.h"
#include "filesystem/CloudUtils.h"
#include "filesystem/StackDirectory.h"
#include "filesystem/MultiPathDirectory.h"
#include "utils/Base64URL.h"
#include "guiinfo/GUIInfoLabels.h"
#include "pvr/PVRManager.h"
#include "interfaces/builtins/Builtins.h"
#include "addons/AddonManager.h"
#include "settings/SkinSettings.h"
#include "profiles/ProfilesManager.h"
#include "Util.h"
#include "filesystem/Directory.h"
#include "settings/MediaSourceSettings.h"

#define CONTROL_HOMESHELFMOVIESRA         8000
#define CONTROL_HOMESHELFTVSHOWSRA        8001
#define CONTROL_HOMESHELFMUSICALBUMS      8002
#define CONTROL_HOMESHELFMUSICVIDEOS      8003
#define CONTROL_HOMESHELFMUSICSONGS       8004
#define CONTROL_HOMESHELFMOVIESPR         8010
#define CONTROL_HOMESHELFTVSHOWSPR        8011
#define CONTROL_HOMESHELFCONTINUEWATCHING 8020

#define CONTROL_HOME_LIST              9000
#define CONTROL_SERVER_BUTTON          4000
#define CONTROL_FAVOURITES_BUTTON      4001
#define CONTROL_SETTINGS_BUTTON        4002
#define CONTROL_EXTENSIONS_BUTTON      4003
#define CONTROL_PROFILES_BUTTON        4004

using namespace ANNOUNCEMENT;

CGUIWindowHome::CGUIWindowHome(void) : CGUIWindow(WINDOW_HOME, "Home.xml"),
                                       m_HomeShelfRunningId(-1),
                                       m_cumulativeUpdateFlag(0),
                                       m_countBackCalled(0)
{
  m_firstRun = true;
  m_updateHS = (Audio | Video);
  m_loadType = KEEP_IN_MEMORY;

  m_HomeShelfTVRA = new CFileItemList;
  m_HomeShelfTVPR = new CFileItemList;
  m_HomeShelfMoviesRA = new CFileItemList;
  m_HomeShelfMoviesPR = new CFileItemList;
  m_HomeShelfMusicAlbums = new CFileItemList;
  m_HomeShelfMusicSongs = new CFileItemList;
  m_HomeShelfMusicVideos = new CFileItemList;
  m_HomeShelfContinueWatching = new CFileItemList;

  CAnnouncementManager::GetInstance().AddAnnouncer(this);
  m_buttonSections = new CFileItemList;
}

CGUIWindowHome::~CGUIWindowHome(void)
{
  CAnnouncementManager::GetInstance().RemoveAnnouncer(this);
}

bool CGUIWindowHome::OnAction(const CAction &action)
{
  static unsigned int min_hold_time = 1000;
  if (action.GetID() == ACTION_NAV_BACK)
  {
    if (action.GetHoldTime() < min_hold_time && g_application.m_pPlayer->IsPlaying())
    {
      g_application.SwitchToFullScreen();
      return true;
    }
    if (CSettings::GetInstance().GetBool(CSettings::SETTING_LOOKANDFEEL_MINIMIZEFROMHOME))
    {
      CLog::Log(LOGDEBUG, "CGUIWindowHome::OnBack - %d", m_countBackCalled);
      if (!m_countBackCalled)
      {
        m_countBackCalled++;
        CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, g_localizeStrings.Get(36554).c_str(), "", 1000, false);
        return false;
      }
      else
      {
        m_countBackCalled = 0;
        g_application.Minimize();
        return true;
      }
    }
  }

  m_countBackCalled = 0;
  return CGUIWindow::OnAction(action);
}

void CGUIWindowHome::OnInitWindow()
{
  m_triggerRA = true;
  m_updateHS = (Audio | Video);
  if (!CServicesManager::GetInstance().HasServices())
    m_firstRun = false;

  SetupServices();

  if (!m_firstRun && !g_application.IsPlayingSplash())
    AddHomeShelfJobs( m_updateHS );
  else
    m_firstRun = false;

  CGUIWindow::OnInitWindow();
}

void CGUIWindowHome::OnDeinitWindow(int nextWindowID)
{
  CJobManager::GetInstance().CancelJob(m_HomeShelfRunningId);
  m_HomeShelfRunningId = -1;

  CGUIWindow::OnDeinitWindow(nextWindowID);
}

void CGUIWindowHome::Announce(AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  if (flag & AnnouncementFlag::PVR && strcmp(message, "HomeScreenUpdate") == 0)
  {
    if (g_infoManager.GetBool(PVR_HAS_RADIO_CHANNELS) ||
        g_infoManager.GetBool(PVR_HAS_TV_CHANNELS))
    {
      SetupServices();
    }
    else
    {
      CJobManager::GetInstance().AddJob(new CHomeButtonJob(), NULL);
    }
    return;
  }
  // we are only interested in library changes
  if ((flag & (VideoLibrary | AudioLibrary)) == 0)
    return;

  if (data.isMember("transaction") && data["transaction"].asBoolean())
    return;

  if (strcmp(message, "OnScanStarted") == 0 ||
      strcmp(message, "OnCleanStarted") == 0 ||
      strcmp(message, "OnUpdate") == 0)
    return;

  CLog::Log(LOGDEBUG, "CGUIWindowHome::Announce, type: %i, from %s, message %s",(int)flag, sender, message);

  if (strcmp(message, "UpdateRecentlyAdded") == 0)
  {
    if (!data.isMember("uuid"))
      m_triggerRA = true;

    std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
    if (serverUUID == data["uuid"].asString())
    {
      m_triggerRA = true;
    }
  }

  int ra_flag = 0;

  if (m_triggerRA)
  {
    m_triggerRA = false;
    if (flag & VideoLibrary)
      ra_flag |= Video;
    if (flag & AudioLibrary)
      ra_flag |= Audio;

    CGUIMessage reload(GUI_MSG_NOTIFY_ALL, GetID(), 0, GUI_MSG_REFRESH_THUMBS, ra_flag);
    g_windowManager.SendThreadMessage(reload, GetID());
    SetupServices();
  }
}

void CGUIWindowHome::AddHomeShelfJobs(int flag)
{
  CSingleLock lockMe(*this);
  if (m_HomeShelfRunningId == -1)
  {
    flag |= m_cumulativeUpdateFlag; // add the flags from previous calls to AddHomeShelfJob

    m_cumulativeUpdateFlag = 0; // now taken care of in flag.
                                // reset this since we're going to execute a job

    if (flag)
      m_HomeShelfRunningId = CJobManager::GetInstance().AddJob(new CHomeShelfJob(flag), this);
  }
  else
    // since we're going to skip a job, mark that one came in and ...
    m_cumulativeUpdateFlag |= flag; // this will be used later

  m_updateHS = 0;
}

void CGUIWindowHome::OnJobComplete(unsigned int jobID, bool success, CJob *job)
{

  int jobFlag = ((CHomeShelfJob *)job)->GetFlag();

  if (jobFlag & Video)
  {
    CSingleLock lock(m_critsection);
    {
      // these can alter the gui lists and cause renderer crashing
      // if gui lists are yanked out from under rendering. Needs lock.
      CSingleLock lock(g_graphicsContext);

      ((CHomeShelfJob *)job)->UpdateTvItemsRA(m_HomeShelfTVRA);
      ((CHomeShelfJob *)job)->UpdateTvItemsPR(m_HomeShelfTVPR);
      ((CHomeShelfJob *)job)->UpdateMovieItemsRA(m_HomeShelfMoviesRA);
      ((CHomeShelfJob *)job)->UpdateMovieItemsPR(m_HomeShelfMoviesPR);
      ((CHomeShelfJob *)job)->UpdateContinueWatchingItems(m_HomeShelfContinueWatching);
    }

    CGUIMessage messageTVRA(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFTVSHOWSRA, 0, 0, m_HomeShelfTVRA);
    g_windowManager.SendThreadMessage(messageTVRA);


    CGUIMessage messageTVPR(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFTVSHOWSPR, 0, 0, m_HomeShelfTVPR);
    g_windowManager.SendThreadMessage(messageTVPR);


    CGUIMessage messageMovieRA(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMOVIESRA, 0, 0, m_HomeShelfMoviesRA);
    g_windowManager.SendThreadMessage(messageMovieRA);


    CGUIMessage messageMoviePR(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMOVIESPR, 0, 0, m_HomeShelfMoviesPR);
    g_windowManager.SendThreadMessage(messageMoviePR);

    CGUIMessage messageContinueWatching(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFCONTINUEWATCHING, 0, 0, m_HomeShelfContinueWatching);
    g_windowManager.SendThreadMessage(messageContinueWatching);
  }

  if (jobFlag & Audio)
  {
    CSingleLock lock(m_critsection);

    ((CHomeShelfJob *)job)->UpdateMusicAlbumItems(m_HomeShelfMusicAlbums);
    CGUIMessage messageAlbums(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMUSICALBUMS, 0, 0, m_HomeShelfMusicAlbums);
    g_windowManager.SendThreadMessage(messageAlbums);
  }

  int flag = 0;
  {
    CSingleLock lockMe(*this);

    // the job is finished.
    // did one come in in the meantime?
    flag = m_cumulativeUpdateFlag;
    m_HomeShelfRunningId = -1; /// we're done.
  }

  if (flag)
    AddHomeShelfJobs(0 /* the flag will be set inside AddHomeShelfJobs via m_cumulativeUpdateFlag */ );
}

bool CGUIWindowHome::OnMessage(CGUIMessage& message)
{
  switch ( message.GetMessage() )
  {
  case GUI_MSG_SETUP_HOME_SERVICES:
    SetupServices();
    break;
  case GUI_MSG_NOTIFY_ALL:
  {
    if (message.GetParam1() == GUI_MSG_REFRESH_THUMBS)
    {
      int updateRA = (message.GetSenderId() == GetID()) ? message.GetParam2() : (Video | Audio);

      if (IsActive())
        AddHomeShelfJobs(updateRA);
      else
        m_updateHS |= updateRA;
    }
    else if (message.GetParam1() == GUI_MSG_UPDATE_ITEM && message.GetItem())
    {
      CFileItemPtr newItem = std::dynamic_pointer_cast<CFileItem>(message.GetItem());
      if (newItem && IsActive())
      {
        CSingleLock lock(m_critsection);
        if (newItem->GetVideoInfoTag()->m_type == MediaTypeMovie)
        {
          m_HomeShelfMoviesRA->UpdateItem(newItem.get());
          m_HomeShelfMoviesPR->UpdateItem(newItem.get());
        }
        else
        {
          m_HomeShelfTVRA->UpdateItem(newItem.get());
          m_HomeShelfTVPR->UpdateItem(newItem.get());
        }
      }
    }
    break;
  }
  case GUI_MSG_CLICKED:
  {
    int iControl = message.GetSenderId();
    bool playAction = (message.GetParam1() == ACTION_PLAYER_PLAYPAUSE ||
                       message.GetParam1() == ACTION_PLAYER_PLAY);
    bool selectAction = (message.GetParam1() == ACTION_SELECT_ITEM ||
                         message.GetParam1() == ACTION_MOUSE_LEFT_CLICK ||
                         playAction
                         );

    VideoSelectAction clickSelectAction = (VideoSelectAction)CSettings::GetInstance().GetInt(CSettings::SETTING_MYVIDEOS_SELECTACTION);
    if (selectAction && iControl == CONTROL_HOMESHELFMOVIESRA)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOMESHELFMOVIESRA);
      OnMessage(msg);

      CSingleLock lock(m_critsection);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_HomeShelfMoviesRA->Size())
      {
        CFileItemPtr itemPtr = m_HomeShelfMoviesRA->Get(item);
        OnClickHomeShelfItem(*itemPtr, (playAction ? SELECT_ACTION_PLAY : clickSelectAction));
      }
      return true;
    }
    if (selectAction && iControl == CONTROL_HOMESHELFMOVIESPR)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOMESHELFMOVIESPR);
      OnMessage(msg);

      CSingleLock lock(m_critsection);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_HomeShelfMoviesPR->Size())
      {
        CFileItemPtr itemPtr = m_HomeShelfMoviesPR->Get(item);
        OnClickHomeShelfItem(*itemPtr, (playAction ? SELECT_ACTION_PLAY : clickSelectAction));
      }
      return true;
    }
    else if (selectAction && iControl == CONTROL_HOMESHELFTVSHOWSRA)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOMESHELFTVSHOWSRA);
      OnMessage(msg);

      CSingleLock lock(m_critsection);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_HomeShelfTVRA->Size())
      {
        CFileItemPtr itemPtr = m_HomeShelfTVRA->Get(item);
        OnClickHomeShelfItem(*itemPtr, (playAction ? SELECT_ACTION_PLAY : clickSelectAction));
      }
      return true;
    }
    else if (selectAction && iControl == CONTROL_HOMESHELFTVSHOWSPR)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOMESHELFTVSHOWSPR);
      OnMessage(msg);

      CSingleLock lock(m_critsection);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_HomeShelfTVPR->Size())
      {
        CFileItemPtr itemPtr = m_HomeShelfTVPR->Get(item);
        OnClickHomeShelfItem(*itemPtr, (playAction ? SELECT_ACTION_PLAY : clickSelectAction));
      }
      return true;
    }
    else if (selectAction && iControl == CONTROL_HOMESHELFCONTINUEWATCHING)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOMESHELFCONTINUEWATCHING);
      OnMessage(msg);

      CSingleLock lock(m_critsection);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_HomeShelfContinueWatching->Size())
      {
        CFileItemPtr itemPtr = m_HomeShelfContinueWatching->Get(item);
        OnClickHomeShelfItem(*itemPtr, (playAction ? SELECT_ACTION_PLAY : clickSelectAction));
      }
      return true;
    }
    else if (selectAction && iControl == CONTROL_HOMESHELFMUSICALBUMS)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOMESHELFMUSICALBUMS);
      OnMessage(msg);

      CSingleLock lock(m_critsection);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_HomeShelfMusicAlbums->Size())
      {
        CFileItemPtr itemPtr = m_HomeShelfMusicAlbums->Get(item);
        OnClickHomeShelfItem(*itemPtr, SELECT_ACTION_PLAY);
      }
      return true;
    }
    else if (selectAction && iControl == CONTROL_HOME_LIST)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOME_LIST);
      OnMessage(msg);

      int item = msg.GetParam1();
      CFileItem selectedItem(*new CFileItem(*m_buttonSections->Get(item)));
      CBuiltins::GetInstance().Execute(selectedItem.GetPath());
      return true;
    }
    else if (iControl == CONTROL_SERVER_BUTTON)
    {
      // ask the user what to do
      std::string selectedUuid = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
      CGUIDialogSelect* selectDialog = static_cast<CGUIDialogSelect*>(g_windowManager.GetWindow(WINDOW_DIALOG_SELECT));
      selectDialog->Reset();
      selectDialog->SetHeading(g_localizeStrings.Get(33051) + " " + g_localizeStrings.Get(706));
      int counter = 0;
      int selected = 0;

      // Add MrMC Database library button
      CFileItem item("MrMC");
      item.SetProperty("type", "mrmc");
      item.SetProperty("uuid", "mrmc");
      selectDialog->Add(item);
      if (selectedUuid == "mrmc")
        selected = counter;
      counter++;

      // Add all Plex server buttons
      std::vector<CPlexClientPtr>  plexClients;
      CPlexServices::GetInstance().GetClients(plexClients);
      for (const auto &client : plexClients)
      {
        CFileItem item("Plex - " + client->GetServerName());
        item.SetProperty("type", "plex");
        item.SetProperty("uuid", client->GetUuid());
        selectDialog->Add(item);
        if (selectedUuid == client->GetUuid())
          selected = counter;
        counter++;
      }

      // Add all Emby server buttons
      std::vector<CEmbyClientPtr>  embyClients;
      CEmbyServices::GetInstance().GetClients(embyClients);
      for (const auto &client : embyClients)
      {
        CFileItem item("Emby - " + client->GetServerName());
        item.SetProperty("type", "emby");
        item.SetProperty("uuid", client->GetUuid());
        selectDialog->Add(item);
        if (selectedUuid == client->GetUuid())
          selected = counter;
        counter++;
      }

      selectDialog->SetSelected(selected);
      selectDialog->EnableButton(false, 0);
      selectDialog->Open();

      // check if the user has chosen one of the results
      const CFileItemPtr selectedItem = selectDialog->GetSelectedFileItem();
      if (selectedItem->HasProperty("type") &&
          selectedItem->HasProperty("uuid") &&
          selectedItem->GetProperty("uuid").asString() != selectedUuid )
      {
        std::string type = selectedItem->GetProperty("type").asString();
        std::string uuid = selectedItem->GetProperty("uuid").asString();
        CSettings::GetInstance().SetString(CSettings::SETTING_GENERAL_SERVER_TYPE,type);
        CSettings::GetInstance().SetString(CSettings::SETTING_GENERAL_SERVER_UUID,uuid);
        CSettings::GetInstance().Save();
        SetupServices();
        CVariant data(CVariant::VariantTypeObject);
        data["uuid"] = uuid;
        ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "UpdateRecentlyAdded",data);
        ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::AudioLibrary, "xbmc", "UpdateRecentlyAdded",data);
        ClearHomeShelfItems();
      }
      return true;
    }
    else if (iControl == CONTROL_PROFILES_BUTTON)
    {
      std::string uuid = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
      std::string type = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
      if (type == "mrmc" || type == "emby" || type.empty())
      {
        g_windowManager.ActivateWindow(WINDOW_LOGIN_SCREEN);
        std::string strLabel = CProfilesManager::GetInstance().GetCurrentProfile().getName();
        std::string thumb = CProfilesManager::GetInstance().GetCurrentProfile().getThumb();
        if (thumb.empty())
          thumb = "unknown-user.png";
        SET_CONTROL_VISIBLE(CONTROL_PROFILES_BUTTON);
        SET_CONTROL_LABEL_THREAD_SAFE(CONTROL_PROFILES_BUTTON , strLabel);
        SET_CONTROL_LABEL2_THREAD_SAFE(CONTROL_PROFILES_BUTTON , thumb);
      }
      else if (type == "plex")
      {
        CPlexClientPtr plexClient = CPlexServices::GetInstance().GetClient(uuid);
        if (plexClient)
        {
          std::string strLabel = CPlexServices::GetInstance().PickHomeUser();
          if (!strLabel.empty())
          {
            std::string thumb = CPlexServices::GetInstance().GetHomeUserThumb();
            if (thumb.empty())
              thumb = "unknown-user.png";

            ClearHomeShelfItems();
            SetupServices();
            std::string text = StringUtils::Format(g_localizeStrings.Get(1256).c_str(),"Plex");
            CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, text, "", 3000, true);
            SET_CONTROL_VISIBLE(CONTROL_PROFILES_BUTTON);
            SET_CONTROL_LABEL_THREAD_SAFE(CONTROL_PROFILES_BUTTON , strLabel);
            SET_CONTROL_LABEL2_THREAD_SAFE(CONTROL_PROFILES_BUTTON , thumb);
          }
        }
      }
    }
    break;
  }
  default:
    break;
  }

  return CGUIWindow::OnMessage(message);
}

bool CGUIWindowHome::OnClickHomeShelfItem(CFileItem itemPtr, int action)
{
  switch (action)
  {
    case SELECT_ACTION_CHOOSE:
    {
      CContextButtons choices;

      if (itemPtr.IsVideoDb())
      {
        std::string itemPath(itemPtr.GetPath());
        itemPath = itemPtr.GetVideoInfoTag()->m_strFileNameAndPath;
        if (URIUtils::IsStack(itemPath) && CFileItem(XFILE::CStackDirectory::GetFirstStackedFile(itemPath),false).IsDiscImage())
          choices.Add(SELECT_ACTION_PLAYPART, 20324); // Play Part
          }

      choices.Add(SELECT_ACTION_PLAY, 208);   // Play
      choices.Add(SELECT_ACTION_INFO, 22081); // Info
      int value = CGUIDialogContextMenu::ShowAndGetChoice(choices);
      if (value < 0)
        return true;

      return OnClickHomeShelfItem(itemPtr, value);
    }
      break;
    case SELECT_ACTION_INFO:
      KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_SHOW_VIDEO_INFO, -1, -1,  static_cast<void*>(new CFileItem(itemPtr)));
      return true;
    case SELECT_ACTION_PLAY:
    default:
      PlayHomeShelfItem(itemPtr);
      break;
  }
  return false;
}

bool CGUIWindowHome::PlayHomeShelfItem(CFileItem itemPtr)
{
  // play media
  if (itemPtr.IsAudio())
  {
    CFileItemList items;

    // if we are Service based, get it from there... if not, check music database
    if (itemPtr.IsMediaServiceBased())
      CServicesManager::GetInstance().GetAlbumSongs(itemPtr, items);
    else
    {
      CMusicDatabase musicdatabase;
      if (!musicdatabase.Open())
        return false;
      musicdatabase.GetItems(itemPtr.GetPath(),items);
      musicdatabase.Close();
    }
    g_playlistPlayer.Reset();
    g_playlistPlayer.SetCurrentPlaylist(PLAYLIST_MUSIC);
    PLAYLIST::CPlayList& playlist = g_playlistPlayer.GetPlaylist(PLAYLIST_MUSIC);
    playlist.Clear();
    playlist.Add(items);
    // play full album, starting with first song...
    g_playlistPlayer.Play(0);
    // activate visualisation screen
    g_windowManager.ActivateWindow(WINDOW_VISUALISATION);
  }
  else
  {
    std::string resumeString = CGUIWindowVideoBase::GetResumeString(itemPtr);
    if (!resumeString.empty())
    {
      CContextButtons choices;
      choices.Add(SELECT_ACTION_RESUME, resumeString);
      choices.Add(SELECT_ACTION_PLAY, 12021);   // Start from beginning
      int value = CGUIDialogContextMenu::ShowAndGetChoice(choices);
      if (value < 0)
        return false;
      if (value == SELECT_ACTION_RESUME)
        itemPtr.m_lStartOffset = STARTOFFSET_RESUME;
    }

    if (itemPtr.IsMediaServiceBased())
    {
      if (!CServicesManager::GetInstance().GetResolutions(itemPtr))
        return false;
      CServicesManager::GetInstance().GetURL(itemPtr);
    }
    else if (itemPtr.IsCloud())
    {
      CCloudUtils::GetInstance().GetURL(itemPtr);
    }

    g_playlistPlayer.Reset();
    g_playlistPlayer.SetCurrentPlaylist(PLAYLIST_VIDEO);
    PLAYLIST::CPlayList& playlist = g_playlistPlayer.GetPlaylist(PLAYLIST_VIDEO);
    playlist.Clear();
    CFileItemPtr movieItem(new CFileItem(itemPtr));
    playlist.Add(movieItem);

    // play movie...
    g_playlistPlayer.Play(0);
  }
  return true;
}

void CGUIWindowHome::SetupServices()
{
  if (!IsActive())
  {
    CLog::Log(LOGDEBUG, "CGUIWindowHome::SetupServices() - !IsActive()");
    return;
  }
  // we cannot be diddling gui unless we are on main thread.
  if (!g_application.IsCurrentThread())
  {
    CLog::Log(LOGDEBUG, "CGUIWindowHome::SetupServices() - !g_application.IsCurrentThread()");
    CGUIMessage msg(GUI_MSG_SETUP_HOME_SERVICES, 0, 0, 0);
    g_windowManager.SendThreadMessage(msg, GetID());
    return;
  }

  // idea here is that if button 4000 exists in the home screen skin is compatible
  // with new Server layouts on home
  const CGUIControl *btnServers = GetControl(CONTROL_SERVER_BUTTON);
  if (!btnServers)
  {
    CLog::Log(LOGDEBUG, "CGUIWindowHome::SetupServices() - No server button");
    return;
  }

  // Always show server button as we might loose the server and "not have services"
  //  if (CServicesManager::GetInstance().HasServices())
  SET_CONTROL_VISIBLE(CONTROL_SERVER_BUTTON);

  CSingleLock lock(m_critsection);
  m_buttonSections->ClearItems();
  std::string strLabel;
  std::string strThumb;
  std::string serverType = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);

  CLog::Log(LOGDEBUG, "CGUIWindowHome::SetupServices() - serverType(%s) ,  serverUUID(%s)",serverType.c_str(),serverUUID.c_str());
  if (serverType == "plex")
  {
    if (CPlexServices::GetInstance().HasClients())
    {
      CPlexClientPtr plexClient = CPlexServices::GetInstance().GetClient(serverUUID);
      if (serverUUID.empty())
      {
        if (plexClient == nullptr)
        {
          // this is only triggered when we first sign in plex as the server has not been selected yet, we pick the first one
          plexClient = CPlexServices::GetInstance().GetFirstClient();
          if (plexClient)
            CSettings::GetInstance().SetString(CSettings::SETTING_GENERAL_SERVER_UUID,plexClient->GetUuid());
        }
      }
      if (plexClient)
      {
        AddPlexSection(plexClient);
        SET_CONTROL_LABEL_THREAD_SAFE(CONTROL_SERVER_BUTTON , plexClient->GetServerName());
        strLabel = CPlexServices::GetInstance().GetHomeUserName();
        strThumb = CPlexServices::GetInstance().GetHomeUserThumb();
        SET_CONTROL_LABEL_THREAD_SAFE(CONTROL_PROFILES_BUTTON , strLabel);
        SET_CONTROL_LABEL2_THREAD_SAFE(CONTROL_PROFILES_BUTTON , strThumb);
      }
      SET_CONTROL_VISIBLE(CONTROL_PROFILES_BUTTON);
    }
  }
  else if (serverType == "emby")
  {
    if (CEmbyServices::GetInstance().HasClients())
    {
      CEmbyClientPtr embyClient = CEmbyServices::GetInstance().GetClient(serverUUID);
      if (serverUUID.empty())
      {
        if (embyClient == nullptr)
        {
          // this is only triggered when we first sign in emby as the server has not been selected yet, we pick the first one
          embyClient = CEmbyServices::GetInstance().GetFirstClient();
          if (embyClient)
            CSettings::GetInstance().SetString(CSettings::SETTING_GENERAL_SERVER_UUID,embyClient->GetUuid());
        }
      }
      if (embyClient)
      {
        AddEmbySection(embyClient);
        SET_CONTROL_LABEL_THREAD_SAFE(CONTROL_SERVER_BUTTON , embyClient->GetServerName());
      }
    }
    SET_CONTROL_VISIBLE(CONTROL_PROFILES_BUTTON);
  }
  else if (serverType == "mrmc" || serverType.empty())
  {
    SetupMrMCHomeButtons();
  }

  SetupStaticHomeButtons();
  CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOME_LIST);
  OnMessage(msg);

  int item = msg.GetParam1();

  CGUIMessage message(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOME_LIST, item, 0, m_buttonSections);
  g_windowManager.SendThreadMessage(message);
}

void CGUIWindowHome::SetupStaticHomeButtons()
{
  CLog::Log(LOGDEBUG, "CGUIWindowHome::SetupStaticHomeButtons()");

  bool hasLiveTv = (g_infoManager.GetBool(PVR_HAS_TV_CHANNELS) &&
                    !g_SkinInfo->GetSkinSettingBool("HomeMenuNoTVButton"));
  bool hasRadio = (g_infoManager.GetBool(PVR_HAS_RADIO_CHANNELS) &&
                    !g_SkinInfo->GetSkinSettingBool("HomeMenuNoRadioButton"));

  const CGUIControl *btnFavourites = GetControl(CONTROL_FAVOURITES_BUTTON);
  const CGUIControl *btnSettings = GetControl(CONTROL_SETTINGS_BUTTON);
  const CGUIControl *btnExtensions = GetControl(CONTROL_EXTENSIONS_BUTTON);
  bool showFavourites = (!btnFavourites && !g_SkinInfo->GetSkinSettingBool("HomeMenuNoFavButton"));
  bool showExtensions = (!btnExtensions && ADDON::CAddonMgr::GetInstance().HasExtensions()&&
                         !g_SkinInfo->GetSkinSettingBool("HomeMenuNoAddonsButton"));
  bool showMediaSources = !g_SkinInfo->GetSkinSettingBool("HomeMenuNoMediaSourceButton");

  HomeButton button;
  ButtonProperty property;
  CFileItemPtr ptrButton;

  // LiveTV Button
  if (hasLiveTv)
  {
    button.label = g_localizeStrings.Get(19020);
    if (CSettings::GetInstance().GetBool(CSettings::SETTING_PVRMENU_TVMENUTOGUIDE))
      button.onclick = "ActivateWindow(TvGuide)";
    else
      button.onclick = "ActivateWindow(TVChannels)";
    // type
    property.name = "type";
    property.value = "livetv";
    button.properties.push_back(property);
    // menu_id
    property.name = "menu_id";
    property.value = "$NUMBER[12000]";
    button.properties.push_back(property);
    // id
    property.name = "id";
    property.value = "livetv";
    button.properties.push_back(property);
    // submenu
    property.name = "submenu";
    property.value = true;
    button.properties.push_back(property);
    ptrButton = MakeButton(button);
    m_buttonSections->Add(ptrButton);
  }

  // Radio Button
  if (hasRadio)
  {
    button.label = g_localizeStrings.Get(19021);
    button.onclick = "ActivateWindow(RadioChannels)";
    // type
    property.name = "type";
    property.value = "radio";
    button.properties.push_back(property);
    // menu_id
    property.name = "menu_id";
    property.value = "$NUMBER[13000]";
    button.properties.push_back(property);
    // id
    property.name = "id";
    property.value = "radio";
    button.properties.push_back(property);
    // submenu
    property.name = "submenu";
    property.value = true;
    button.properties.push_back(property);
    ptrButton = MakeButton(button);
    m_buttonSections->Add(ptrButton);
  }

  // Favourites Button
  if (showFavourites)
  {
    button.label = g_localizeStrings.Get(10134);
    button.onclick = "ActivateWindow(favourites)";
    // type
    property.name = "type";
    property.value = "favorites";
    button.properties.push_back(property);
    // menu_id
    property.name = "menu_id";
    property.value = "$NUMBER[14000]";
    button.properties.push_back(property);
    // id
    property.name = "id";
    property.value = "favorites";
    button.properties.push_back(property);
    // submenu
    property.name = "submenu";
    property.value = false;
    button.properties.push_back(property);

    ptrButton = MakeButton(button);
    m_buttonSections->Add(ptrButton);
  }

  // MediaSources Button
  if (showMediaSources)
  {
    button.label = g_localizeStrings.Get(20094);
    button.onclick = "ActivateWindow(MediaSources,mediasources://,return)";
    // type
    property.name = "type";
    property.value = "sources";
    button.properties.push_back(property);
    // menu_id
    property.name = "menu_id";
    property.value = "$NUMBER[11000]";
    button.properties.push_back(property);
    // id
    property.name = "id";
    property.value = "video";
    button.properties.push_back(property);
    // submenu
    property.name = "submenu";
    property.value = false;
    button.properties.push_back(property);

    ptrButton = MakeButton(button);
    m_buttonSections->Add(ptrButton);
  }

  VECSOURCES *videoSources = CMediaSourceSettings::GetInstance().GetSources("video");
  VECSOURCES *musicSources = CMediaSourceSettings::GetInstance().GetSources("music");
  if (videoSources->size() > 0 || musicSources->size() > 0)
  {
    for (unsigned int i = 0;i < videoSources->size();++i)
    {
      CMediaSource source = (*videoSources)[i];
      if (!source.m_showOnHome)
        continue;
      button.label = source.strName;
      button.onclick = "ActivateWindow(Videos," + source.strPath + ",return)";
      // type
      property.name = "type";
      property.value = "source";
      button.properties.push_back(property);
      // menu_id
      property.name = "menu_id";
      property.value = "$NUMBER[19500]";
      button.properties.push_back(property);
      // id
      property.name = "id";
      property.value = "source";
      button.properties.push_back(property);
      // submenu
      property.name = "submenu";
      property.value = false;
      button.properties.push_back(property);

      ptrButton = MakeButton(button);
      m_buttonSections->Add(ptrButton);
    }

    for (unsigned int i = 0;i < musicSources->size();++i)
    {
      CMediaSource source = (*musicSources)[i];
      if (!source.m_showOnHome)
        continue;
      button.label = source.strName;
      button.onclick = "ActivateWindow(Music," + source.strPath + ",return)";
      // type
      property.name = "type";
      property.value = "source";
      button.properties.push_back(property);
      // menu_id
      property.name = "menu_id";
      property.value = "$NUMBER[19500]";
      button.properties.push_back(property);
      // id
      property.name = "id";
      property.value = "source";
      button.properties.push_back(property);
      // submenu
      property.name = "submenu";
      property.value = false;
      button.properties.push_back(property);

      ptrButton = MakeButton(button);
      m_buttonSections->Add(ptrButton);
    }
  }

  // Extensions Button
  if (showExtensions)
  {
    button.label = g_localizeStrings.Get(24001);
    button.onclick = "ActivateWindow(Programs,Addons,return)";
    // type
    property.name = "type";
    property.value = "extensions";
    button.properties.push_back(property);
    // menu_id
    property.name = "menu_id";
    property.value = "$NUMBER[19000]";
    button.properties.push_back(property);
    // id
    property.name = "id";
    property.value = "addons";
    button.properties.push_back(property);
    // submenu
    property.name = "submenu";
    property.value = false;
    button.properties.push_back(property);

    ptrButton = MakeButton(button);
    m_buttonSections->Add(ptrButton);
  }
  // Settings Button
  if (!btnSettings)
  {
    button.label = g_localizeStrings.Get(10004);
    button.onclick = "ActivateWindow(settings)";
    // type
    property.name = "type";
    property.value = "system";
    button.properties.push_back(property);
    // menu_id
    property.name = "menu_id";
    property.value = "$NUMBER[14000]";
    button.properties.push_back(property);
    // id
    property.name = "id";
    property.value = "system";
    button.properties.push_back(property);
    // submenu
    property.name = "submenu";
    property.value = true;
    button.properties.push_back(property);

    ptrButton = MakeButton(button);
    m_buttonSections->Add(ptrButton);
  }

  // QUIT Button
  if (!g_infoManager.GetBool(SYSTEM_PLATFORM_DARWIN_TVOS) && !g_infoManager.GetBool(SYSTEM_PLATFORM_DARWIN_IOS))
  {
    button.label = g_localizeStrings.Get(13009);
    button.onclick = "ActivateWindow(shutdownmenu)";
    // type
    property.name = "type";
    property.value = "quit";
    button.properties.push_back(property);
    // id
    property.name = "id";
    property.value = "quit";
    button.properties.push_back(property);
    // submenu
    property.name = "submenu";
    property.value = false;
    button.properties.push_back(property);

    ptrButton = MakeButton(button);
    m_buttonSections->Add(ptrButton);
  }
}

void CGUIWindowHome::SetupMrMCHomeButtons()
{
  bool hasVideoDB = g_infoManager.GetLibraryBool(LIBRARY_HAS_VIDEO);
  bool hasPictures = (g_infoManager.GetLibraryBool(LIBRARY_HAS_PICTURES) &&
                      !g_SkinInfo->GetSkinSettingBool("HomeMenuNoPicturesButton"));

  HomeButton button;
  ButtonProperty property;
  CFileItemPtr ptrButton;

  //  CFileItemList* staticSections = new CFileItemList;
  if (CProfilesManager::GetInstance().GetNumberOfProfiles() > 1)
  {
    SET_CONTROL_VISIBLE(CONTROL_PROFILES_BUTTON);
    std::string strLabel = CProfilesManager::GetInstance().GetCurrentProfile().getName();
    std::string thumb = CProfilesManager::GetInstance().GetCurrentProfile().getThumb();
    if (thumb.empty())
      thumb = "unknown-user.png";
    SET_CONTROL_LABEL_THREAD_SAFE(CONTROL_PROFILES_BUTTON , strLabel);
    SET_CONTROL_LABEL2_THREAD_SAFE(CONTROL_PROFILES_BUTTON , thumb);
  }
  else
    SET_CONTROL_HIDDEN(CONTROL_PROFILES_BUTTON);

  SET_CONTROL_VISIBLE(CONTROL_SERVER_BUTTON);
  SET_CONTROL_LABEL_THREAD_SAFE(CONTROL_SERVER_BUTTON , "MrMC");

  if (hasVideoDB || hasPictures)
  {
    bool hasMovies = (g_infoManager.GetLibraryBool(LIBRARY_HAS_MOVIES) &&
                      !g_SkinInfo->GetSkinSettingBool("HomeMenuNoMovieButton"));
    bool hasTvShows = (g_infoManager.GetLibraryBool(LIBRARY_HAS_TVSHOWS) &&
                       !g_SkinInfo->GetSkinSettingBool("HomeMenuNoTVShowButton"));
    bool hasMusic = (g_infoManager.GetLibraryBool(LIBRARY_HAS_MUSIC) &&
                     !g_SkinInfo->GetSkinSettingBool("HomeMenuNoMusicButton"));
    bool hasMusicVideos = (g_infoManager.GetLibraryBool(LIBRARY_HAS_MUSICVIDEOS) &&
                           !g_SkinInfo->GetSkinSettingBool("HomeMenuNoMusicVideoButton"));

    int flatten = CSettings::GetInstance().GetBool(CSettings::SETTING_MYVIDEOS_FLATTEN);

    // Movies Button
    if (hasMovies)
    {
      button.label = g_localizeStrings.Get(342);
      button.onclick = flatten ? "ActivateWindow(Videos,MovieTitlesLocal,return)" :
      "ActivateWindow(Videos,movies,return)";
      // type
      property.name = "type";
      property.value = "movies";
      button.properties.push_back(property);
      // menu_id
      property.name = "menu_id";
      property.value = "$NUMBER[5000]";
      button.properties.push_back(property);
      // id
      property.name = "id";
      property.value = "movies";
      button.properties.push_back(property);
      // submenu
      property.name = "submenu";
      property.value = flatten;
      button.properties.push_back(property);
      ptrButton = MakeButton(button);
      m_buttonSections->Add(ptrButton);
    }

    // TVShows Button
    if (hasTvShows)
    {
      button.label = g_localizeStrings.Get(20343);
      button.onclick = flatten ?  "ActivateWindow(Videos,TVShowTitlesLocal,return)" :
      "ActivateWindow(Videos,tvshows,return)";
      // type
      property.name = "type";
      property.value = "tvshows";
      button.properties.push_back(property);
      // menu_id
      property.name = "menu_id";
      property.value = "$NUMBER[6000]";
      button.properties.push_back(property);
      // id
      property.name = "id";
      property.value = "tvshows";
      button.properties.push_back(property);
      // submenu
      property.name = "submenu";
      property.value = flatten;
      button.properties.push_back(property);
      ptrButton = MakeButton(button);
      m_buttonSections->Add(ptrButton);
    }

    // Music Button
    if (hasMusic)
    {
      button.label = g_localizeStrings.Get(2);
      button.onclick = "ActivateWindow(Music,rootLocal,return)";
      // type
      property.name = "type";
      property.value = "music";
      button.properties.push_back(property);
      // menu_id
      property.name = "menu_id";
      property.value = "$NUMBER[7000]";
      button.properties.push_back(property);
      // id
      property.name = "id";
      property.value = "music";
      button.properties.push_back(property);
      // submenu
      property.name = "submenu";
      property.value = true;
      button.properties.push_back(property);
      ptrButton = MakeButton(button);
      m_buttonSections->Add(ptrButton);
    }

    // MusicVideos Button
    if (hasMusicVideos)
    {
      button.label = g_localizeStrings.Get(20389);
      button.onclick = "ActivateWindow(Videos,musicvideos,return)";
      // type
      property.name = "type";
      property.value = "videos";
      button.properties.push_back(property);
      // menu_id
      property.name = "menu_id";
      property.value = "$NUMBER[16000]";
      button.properties.push_back(property);
      // id
      property.name = "id";
      property.value = "musicvideos";
      button.properties.push_back(property);

      ptrButton = MakeButton(button);
      m_buttonSections->Add(ptrButton);
    }

    // Pictures Button
    if (hasPictures)
    {
      button.label = g_localizeStrings.Get(1);
      button.onclick = "ActivateWindow(Pictures)";
      // type
      property.name = "type";
      property.value = "pictures";
      button.properties.push_back(property);
      // menu_id
      property.name = "menu_id";
      property.value = "$NUMBER[4000]";
      button.properties.push_back(property);
      // id
      property.name = "id";
      property.value = "pictures";
      button.properties.push_back(property);

      ptrButton = MakeButton(button);
      m_buttonSections->Add(ptrButton);
    }
  }

  /// below seems to be tho only way for find out if we have any playlists setup
  CFileItemList dummy;
  std::set<std::string> vec;
  vec.insert(CUtil::MusicPlaylistsLocation());
  vec.insert(CUtil::VideoPlaylistsLocation());
  std::string strPlaylistPaths = XFILE::CMultiPathDirectory::ConstructMultiPath(vec);
  CURL curl(strPlaylistPaths);
  XFILE::CDirectory::GetDirectory(curl, dummy);
  // Playlists Button
  if (dummy.Size() > 0)
  {
    button.label = g_localizeStrings.Get(136);
    button.onclick = "ActivateWindow(MediaSources,mediasources://playlists/,return)";
    // type
    property.name = "type";
    property.value = "playlists";
    button.properties.push_back(property);
    // menu_id
    property.name = "menu_id";
    property.value = "$NUMBER[17000]";
    button.properties.push_back(property);
    // id
    property.name = "id";
    property.value = "playlists";
    button.properties.push_back(property);
    // submenu
    property.name = "submenu";
    property.value = false;
    button.properties.push_back(property);

    ptrButton = MakeButton(button);
    m_buttonSections->Add(ptrButton);
  }
}

void CGUIWindowHome::ClearHomeShelfItems()
{
  CSingleLock lock(m_critsection);

  CFileItemList* tempClearItems  = new CFileItemList;
  CGUIMessage messageTVRA(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFTVSHOWSRA, 0, 0, tempClearItems);
  g_windowManager.SendThreadMessage(messageTVRA);

  CGUIMessage messageTVPR(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFTVSHOWSPR, 0, 0, tempClearItems);
  g_windowManager.SendThreadMessage(messageTVPR);

  CGUIMessage messageMovieRA(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMOVIESRA, 0, 0, tempClearItems);
  g_windowManager.SendThreadMessage(messageMovieRA);


  CGUIMessage messageMoviePR(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMOVIESPR, 0, 0, tempClearItems);
  g_windowManager.SendThreadMessage(messageMoviePR);


  CGUIMessage messageAlbums(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMUSICALBUMS, 0, 0, tempClearItems);
  g_windowManager.SendThreadMessage(messageAlbums);

  CGUIMessage messageContinue(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFCONTINUEWATCHING, 0, 0, tempClearItems);
  g_windowManager.SendThreadMessage(messageContinue);

}

CFileItemPtr CGUIWindowHome::MakeButton(HomeButton button)
{
  CFileItemPtr item(new CFileItem());
  item->SetLabel(button.label);
  for (const auto &property : button.properties)
  {
    // add all properties
    item->SetProperty(property.name,property.value);
  }
  item->SetPath(button.onclick);

  return item;
}


std::vector<PlexSectionsContent> CGUIWindowHome::GetPlexSections(CPlexClientPtr client)
{
  std::vector<PlexSectionsContent> sections;
  std::vector<PlexSectionsContent> contents = client->GetMovieContent();
  sections.insert(sections.begin(), contents.begin(),contents.end());
  contents = client->GetTvContent();
  sections.insert(sections.begin(), contents.begin(),contents.end());
  contents = client->GetArtistContent();
  sections.insert(sections.begin(), contents.begin(),contents.end());
//  contents = client->GetPhotoContent();
//  sections.insert(sections.begin(), contents.begin(),contents.end());
  return sections;
}

void CGUIWindowHome::AddPlexSection(CPlexClientPtr client)
{
  std::vector<PlexSectionsContent> contents = GetPlexSections(client);
  std::vector<PlexSectionsContent> playlists = client->GetPlaylistContent();

  if (playlists.size() > 0)
  {
    CFileItemPtr item(new CFileItem());
    item->SetLabel(g_localizeStrings.Get(136));
    item->SetLabel2("Plex-" + client->GetServerName());
    CURL curl(client->GetUrl());
    curl.SetProtocol(client->GetProtocol());
    curl.SetFileName("mediasources://plexplaylists/");
    item->SetPath("ActivateWindow(MediaSources,mediasources://plexplaylists/,return)");
    item->SetProperty("service",true);
    item->SetProperty("servicetype","plex");
    item->SetProperty("id","playlists");
    item->SetProperty("type","playlists");
    item->SetProperty("base64url",Base64URL::Encode(curl.Get()));
    item->SetProperty("submenu",CSettings::GetInstance().GetBool(CSettings::SETTING_MYVIDEOS_FLATTEN));
    m_buttonSections->AddFront(item,0);
  }
  for (const auto &content : contents)
  {
    CFileItemPtr item(new CFileItem());
    item->SetLabel(content.title);
    item->SetLabel2("Plex-" + client->GetServerName());
    CURL curl(client->GetUrl());
    curl.SetProtocol(client->GetProtocol());

    item->SetProperty("service",true);
    item->SetProperty("servicetype","plex");
    std::string strAction;
    std::string filename;
    std::string videoFilters;
    std::string musicFilters;
    std::string videoSufix = "all";
    std::string musicSufix = "all";
    if (!CSettings::GetInstance().GetBool(CSettings::SETTING_MYVIDEOS_FLATTEN))
    {
      videoFilters = "filters/";
      videoSufix = "";
    }
    if (!CSettings::GetInstance().GetBool(CSettings::SETTING_MYMUSIC_FLATTEN))
    {
      musicFilters = "filters/";
      musicSufix = "";
    }
    if (content.type == "movie")
    {
      filename = StringUtils::Format("%s/%s", content.section.c_str(), videoSufix.c_str());
      curl.SetFileName(filename);
      strAction = "plex://movies/titles/" + videoFilters + Base64URL::Encode(curl.Get());
      item->SetPath("ActivateWindow(Videos," + strAction +  ",return)");
      item->SetProperty("type","Movies");
    }
    else if (content.type == "show")
    {
      filename = StringUtils::Format("%s/%s", content.section.c_str(), videoSufix.c_str());
      curl.SetFileName(filename);
      strAction = "plex://tvshows/titles/"  + videoFilters + Base64URL::Encode(curl.Get());
      item->SetPath("ActivateWindow(Videos," + strAction +  ",return)");
      item->SetProperty("type","TvShows");
    }
    else if (content.type == "artist")
    {
      filename = StringUtils::Format("%s/%s", content.section.c_str(), musicSufix.c_str());
      curl.SetFileName(filename);
      strAction = "plex://music/root/"  + musicFilters + Base64URL::Encode(curl.Get());
      item->SetPath("ActivateWindow(Music," + strAction +  ",return)");
      item->SetProperty("type","Music");
    }
    item->SetProperty("base64url",Base64URL::Encode(curl.Get()));
    item->SetProperty("submenu",CSettings::GetInstance().GetBool(CSettings::SETTING_MYVIDEOS_FLATTEN));
    m_buttonSections->AddFront(item,0);
  }
}

std::vector<EmbyViewInfo> CGUIWindowHome::GetEmbySections(CEmbyClientPtr client)
{
  std::vector<EmbyViewInfo> sections;
  std::vector<EmbyViewInfo> contents = client->GetViewInfoForMovieContent();
  sections.insert(sections.begin(), contents.begin(),contents.end());
  contents = client->GetViewInfoForTVShowContent();
  sections.insert(sections.begin(), contents.begin(),contents.end());
  contents = client->GetViewInfoForMusicContent();
  sections.insert(sections.begin(), contents.begin(),contents.end());
  return sections;
}

void CGUIWindowHome::AddEmbySection(CEmbyClientPtr client)
{
  std::vector<EmbyViewInfo> contents = GetEmbySections(client);
  for (const auto &content : contents)
  {
    CFileItemPtr item(new CFileItem());
    item->SetLabel(content.name);
    item->SetLabel2("Emby-" + client->GetServerName());
    CURL curl(client->GetUrl());
    curl.SetProtocol(client->GetProtocol());
    curl.SetFileName(content.prefix);
    item->SetProperty("service",true);
    item->SetProperty("servicetype","emby");
    item->SetProperty("base64url",Base64URL::Encode(curl.Get()));
    std::string strAction;
    if (content.mediaType == "movies")
    {
      strAction = "emby://movies/titles/" + Base64URL::Encode(curl.Get());
      item->SetProperty("type","Movies");
      item->SetProperty("submenu",true);
    }
    else if (content.mediaType == "tvshows")
    {
      strAction = "emby://tvshows/titles/" + Base64URL::Encode(curl.Get());
      item->SetProperty("type","TvShows");
      item->SetProperty("submenu",true);
    }
    else if (content.mediaType == "music")
    {
      strAction = "emby://music/root/" + Base64URL::Encode(curl.Get());
      item->SetProperty("type","Music");
      item->SetProperty("submenu",true);
    }
    item->SetPath("ActivateWindow(Videos," + strAction +  ",return)");
    m_buttonSections->AddFront(item,0);
  }
}

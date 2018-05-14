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
#include "guilib/LocalizeStrings.h"
#include "settings/Settings.h"
#include "playlists/PlayList.h"
#include "messaging/ApplicationMessenger.h"
#include "services/ServicesManager.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "music/MusicDatabase.h"
#include "filesystem/CloudUtils.h"
#include "filesystem/StackDirectory.h"

#define CONTROL_HOMESHELFMOVIESRA      8000
#define CONTROL_HOMESHELFTVSHOWSRA     8001
#define CONTROL_HOMESHELFMUSICALBUMS   8002
#define CONTROL_HOMESHELFMUSICVIDEOS   8003
#define CONTROL_HOMESHELFMUSICSONGS    8004
#define CONTROL_HOMESHELFMOVIESPR      8010
#define CONTROL_HOMESHELFTVSHOWSPR     8011


using namespace ANNOUNCEMENT;

CGUIWindowHome::CGUIWindowHome(void) : CGUIWindow(WINDOW_HOME, "Home.xml"), 
                                       m_HomeShelfRunning(false),
                                       m_cumulativeUpdateFlag(0),
                                       m_countBackCalled(0)
{
  m_firstRun = true;
  m_updateHS = (Audio | Video | Totals);
  m_loadType = KEEP_IN_MEMORY;
  
  m_HomeShelfTVRA = new CFileItemList;
  m_HomeShelfTVPR = new CFileItemList;
  m_HomeShelfMoviesRA = new CFileItemList;
  m_HomeShelfMoviesPR = new CFileItemList;
  m_HomeShelfMusicAlbums = new CFileItemList;
  m_HomeShelfMusicSongs = new CFileItemList;
  m_HomeShelfMusicVideos = new CFileItemList;

  CAnnouncementManager::GetInstance().AddAnnouncer(this);
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
  // its stupid…
  // - if its mysql we have to trigger on start and on every return to home screen.
  // - if its service(plex/emby) we dont need it on start as the servers are still not discovered,
  //   once they are it will trigger the update but we do need it on every return to home.
  if ((StringUtils::EqualsNoCase(g_advancedSettings.m_databaseVideo.type, "mysql") ||
      StringUtils::EqualsNoCase(g_advancedSettings.m_databaseMusic.type, "mysql")))
  {
    // totals will be done after these jobs are finished
    m_updateHS = (Audio | Video);
    m_firstRun = false;
  }
  
  if (CServicesManager::GetInstance().HasServices())
    m_updateHS = (Audio | Video);
  
  if (!m_firstRun)
    AddHomeShelfJobs( m_updateHS );
  else
    m_firstRun = false;

  CGUIWindow::OnInitWindow();
}

void CGUIWindowHome::Announce(AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  // we are only interested in library changes
  if ((flag & (VideoLibrary | AudioLibrary)) == 0)
    return;

  if (data.isMember("transaction") && data["transaction"].asBoolean())
    return;

  if (strcmp(message, "OnScanStarted") == 0 ||
      strcmp(message, "OnCleanStarted") == 0)
    return;

  CLog::Log(LOGDEBUG, "CGUIWindowHome::Announce, type: %i, from %s, message %s",(int)flag, sender, message);

  bool onUpdate = strcmp(message, "OnUpdate") == 0;
  // always update Totals except on an OnUpdate with no playcount update
  int ra_flag = 0;
//  if (!onUpdate || data.isMember("playcount"))
//    ra_flag |= Totals;

  // always update the full list except on an OnUpdate
  if (!onUpdate)
  {
    if (flag & VideoLibrary)
      ra_flag |= Video;
    if (flag & AudioLibrary)
      ra_flag |= Audio;
  }

  CGUIMessage reload(GUI_MSG_NOTIFY_ALL, GetID(), 0, GUI_MSG_REFRESH_THUMBS, ra_flag);
  g_windowManager.SendThreadMessage(reload, GetID());
}

void CGUIWindowHome::AddHomeShelfJobs(int flag)
{
  CSingleLock lockMe(*this);
  if (!m_HomeShelfRunning)
  {
    flag |= m_cumulativeUpdateFlag; // add the flags from previous calls to AddHomeShelfJob

    m_cumulativeUpdateFlag = 0; // now taken care of in flag.
                                // reset this since we're going to execute a job

    // we're about to add one so set the indicator
    if (flag)
    {
      m_HomeShelfRunning = true; // this will happen in the if clause below
      CJobManager::GetInstance().AddJob(new CHomeShelfJob(flag), this);
    }
  }
  else
    // since we're going to skip a job, mark that one came in and ...
    m_cumulativeUpdateFlag |= flag; // this will be used later

  m_updateHS = 0;
}

void CGUIWindowHome::OnJobComplete(unsigned int jobID, bool success, CJob *job)
{
  int flag = 0;

  {
    CSingleLock lockMe(*this);

    // the job is finished.
    // did one come in in the meantime?
    flag = m_cumulativeUpdateFlag;
    m_HomeShelfRunning = false; /// we're done.
  }

  int jobFlag = ((CHomeShelfJob *)job)->GetFlag();

  if (flag)
    AddHomeShelfJobs(0 /* the flag will be set inside AddHomeShelfJobs via m_cumulativeUpdateFlag */ );
  else if(jobFlag != Totals)
    AddHomeShelfJobs(Totals);

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
    }
    
    int homeScreenItemSelector = CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOLIBRARY_HOMESHELFITEMS);
    
    if (homeScreenItemSelector == 1 || homeScreenItemSelector == 3)
    {
      CGUIMessage messageTVRA(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFTVSHOWSRA, 0, 0, m_HomeShelfTVRA);
      g_windowManager.SendThreadMessage(messageTVRA);

      
      CGUIMessage messageTVPR(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFTVSHOWSPR, 0, 0, m_HomeShelfTVPR);
      g_windowManager.SendThreadMessage(messageTVPR);
      
      
      CGUIMessage messageMovieRA(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMOVIESRA, 0, 0, m_HomeShelfMoviesRA);
      g_windowManager.SendThreadMessage(messageMovieRA);
      
      
      CGUIMessage messageMoviePR(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMOVIESPR, 0, 0, m_HomeShelfMoviesPR);
      g_windowManager.SendThreadMessage(messageMoviePR);
    }
    else
    {
      
      // if we are set to only do in Progress, push progress items into recently added shelf items
      // this is a hack for skins that only have one line
      CGUIMessage messageTVRA(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFTVSHOWSRA, 0, 0, m_HomeShelfTVPR);
      g_windowManager.SendThreadMessage(messageTVRA);
      
      CGUIMessage messageMovieRA(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMOVIESRA, 0, 0, m_HomeShelfMoviesPR);
      g_windowManager.SendThreadMessage(messageMovieRA);
    }
    
  }

  if (jobFlag & Audio)
  {
    CSingleLock lock(m_critsection);

    ((CHomeShelfJob *)job)->UpdateMusicAlbumItems(m_HomeShelfMusicAlbums);
    CGUIMessage messageAlbums(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMUSICALBUMS, 0, 0, m_HomeShelfMusicAlbums);
    g_windowManager.SendThreadMessage(messageAlbums);
  }

}

bool CGUIWindowHome::OnMessage(CGUIMessage& message)
{
  switch ( message.GetMessage() )
  {
  case GUI_MSG_NOTIFY_ALL:
  {
    if (message.GetParam1() == GUI_MSG_WINDOW_RESET || message.GetParam1() == GUI_MSG_REFRESH_THUMBS)
    {
      int updateRA = (message.GetSenderId() == GetID()) ? message.GetParam2() : (Video | Audio | Totals);

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
    bool selectAction = (message.GetParam1() == ACTION_SELECT_ITEM ||
                         message.GetParam1() == ACTION_MOUSE_LEFT_CLICK);

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
        OnClickHomeShelfItem(*itemPtr,clickSelectAction);
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
        OnClickHomeShelfItem(*itemPtr,clickSelectAction);
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
        OnClickHomeShelfItem(*itemPtr,clickSelectAction);
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
        OnClickHomeShelfItem(*itemPtr,clickSelectAction);
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
        OnClickHomeShelfItem(*itemPtr,clickSelectAction);
      }
      return true;
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

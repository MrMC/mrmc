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
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/LocalizeStrings.h"
#include "settings/Settings.h"
#include "playlists/PlayList.h"
#include "messaging/ApplicationMessenger.h"
#include "services/ServicesManager.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "music/MusicDatabase.h"

#define CONTROL_HOMESHELFMOVIES      8000
#define CONTROL_HOMESHELFTVSHOWS     8001
#define CONTROL_HOMESHELFMUSICALBUMS 8002
#define CONTROL_HOMESHELFMUSICVIDEOS 8003
#define CONTROL_HOMESHELFMUSICSONGS  8004


using namespace ANNOUNCEMENT;

CGUIWindowHome::CGUIWindowHome(void) : CGUIWindow(WINDOW_HOME, "Home.xml"), 
                                       m_HomeShelfRunning(false),
                                       m_cumulativeUpdateFlag(0),
                                       m_countBackCalled(0)
{
  m_updateHS = (Audio | Video | Totals);
  m_loadType = KEEP_IN_MEMORY;
  
  CAnnouncementManager::GetInstance().AddAnnouncer(this);
  m_HomeShelfTV = new CFileItemList;
  m_HomeShelfMovies = new CFileItemList;
  m_HomeShelfMusicAlbums = new CFileItemList;
  m_HomeShelfMusicSongs = new CFileItemList;
  m_HomeShelfMusicVideos = new CFileItemList;
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
/*
    if (!g_advancedSettings.m_disableminimize)
    {
      CLog::Log(LOGDEBUG, "CGUIWindowHome::OnBack - %d", m_countBackCalled);
      if (!m_countBackCalled)
      {
        m_countBackCalled++;
        CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, "Press again to Minimize", "", 1000, false);
        return false;
      }
      else
      {
        m_countBackCalled = 0;
        g_application.Minimize();
        return true;
      }
    }
*/
  }

  m_countBackCalled = 0;
  return CGUIWindow::OnAction(action);
}

void CGUIWindowHome::OnInitWindow()
{  
  // for shared databases (ie mysql) always force an update on return to home
  // this is a temporary solution until remote announcements can be delivered
  if ((StringUtils::EqualsNoCase(g_advancedSettings.m_databaseVideo.type, "mysql") ||
      StringUtils::EqualsNoCase(g_advancedSettings.m_databaseMusic.type, "mysql")) ||
      CServicesManager::GetInstance().HasServices())
  {
    // totals will be done after these jobs are finished
    m_updateHS = (Audio | Video);
    AddHomeShelfJobs( m_updateHS );
  }

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
  bool getAJob = false;

  // this block checks to see if another one is running
  // and keeps track of the flag
  {
    CSingleLock lockMe(*this);
    if (!m_HomeShelfRunning)
    {
      getAJob = true;

      flag |= m_cumulativeUpdateFlag; // add the flags from previous calls to AddHomeShelfJob

      m_cumulativeUpdateFlag = 0; // now taken care of in flag.
                                  // reset this since we're going to execute a job

      // we're about to add one so set the indicator
      if (flag)
        m_HomeShelfRunning = true; // this will happen in the if clause below
    }
    else
      // since we're going to skip a job, mark that one came in and ...
      m_cumulativeUpdateFlag |= flag; // this will be used later
  }

  if (flag && getAJob)
    CJobManager::GetInstance().AddJob(new CHomeShelfJob(flag), this);

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
    CFileItemList HomeShelfTV;
    HomeShelfTV.Copy(*((CHomeShelfJob *)job)->GetTvItems());
    m_HomeShelfTV->Assign(HomeShelfTV);

    CFileItemList HomeShelfMovies;
    HomeShelfMovies.Copy(*((CHomeShelfJob *)job)->GetMovieItems());
    m_HomeShelfMovies->Assign(HomeShelfMovies);

    CGUIMessage messageMovie(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMOVIES, 0, 0, m_HomeShelfMovies);
    OnMessage(messageMovie);
    CGUIMessage messageTV(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFTVSHOWS, 0, 0, m_HomeShelfTV);
    OnMessage(messageTV);
  }
  if (jobFlag & Audio)
  {
    CSingleLock lock(m_critsection);
    CFileItemList HomeShelfMusicAlbums;
    HomeShelfMusicAlbums.Copy(*((CHomeShelfJob *)job)->GetMusicAlbumItems());
    m_HomeShelfMusicAlbums->Assign(HomeShelfMusicAlbums);

    CGUIMessage messageAlbums(GUI_MSG_LABEL_BIND, GetID(), CONTROL_HOMESHELFMUSICALBUMS, 0, 0, m_HomeShelfMusicAlbums);
    OnMessage(messageAlbums);
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
      CFileItemPtr newItem = std::static_pointer_cast<CFileItem>(message.GetItem());
      if (IsActive())
      {
        if (newItem->GetVideoInfoTag()->m_type == MediaTypeMovie)
          m_HomeShelfMovies->UpdateItem(newItem.get());
        else
          m_HomeShelfTV->UpdateItem(newItem.get());
      }
    }

    break;
  }
  case GUI_MSG_CLICKED:
  {
    int iControl = message.GetSenderId();
    bool selectAction = (message.GetParam1() == ACTION_SELECT_ITEM ||
                         message.GetParam1() == ACTION_MOUSE_LEFT_CLICK);

    if (selectAction && iControl == CONTROL_HOMESHELFMOVIES)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOMESHELFMOVIES);
      OnMessage(msg);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_HomeShelfMovies->Size())
      {
        CFileItemPtr itemPtr = m_HomeShelfMovies->Get(item);
        PlayHomeShelfItem(*itemPtr);
      }
      return true;
    }
    else if (selectAction && iControl == CONTROL_HOMESHELFTVSHOWS)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOMESHELFTVSHOWS);
      OnMessage(msg);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_HomeShelfTV->Size())
      {
        CFileItemPtr itemPtr = m_HomeShelfTV->Get(item);
        PlayHomeShelfItem(*itemPtr);
      }
      return true;
    }
    else if (selectAction && iControl == CONTROL_HOMESHELFMUSICALBUMS)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_HOMESHELFMUSICALBUMS);
      OnMessage(msg);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_HomeShelfMusicAlbums->Size())
      {
        CFileItemPtr itemPtr = m_HomeShelfMusicAlbums->Get(item);
        PlayHomeShelfItem(*itemPtr);
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

bool CGUIWindowHome::PlayHomeShelfItem(CFileItem itemPtr)
{
  // play media
  if (itemPtr.IsAudio())
  {
    CFileItemList &items = *new CFileItemList;
    
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
    
    if (itemPtr.IsMediaServiceBased() && !CServicesManager::GetInstance().GetResolutions(itemPtr))
        return false;

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

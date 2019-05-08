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

#include "utils/log.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoTag.h"
#include "Application.h"
#include "FileItem.h"
#include "filesystem/Directory.h"
#include "HomeShelfJob.h"
#include "addons/AddonManager.h"
#include "guilib/GUIWindow.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "music/MusicDatabase.h"
#include "music/tags/MusicInfoTag.h"
#include "utils/StringUtils.h"
#include "settings/AdvancedSettings.h"
#include "music/MusicThumbLoader.h"
#include "video/VideoThumbLoader.h"
#include "settings/Settings.h"
#include "services/ServicesManager.h"
#include "interfaces/AnnouncementManager.h"
#include "GUIInfoManager.h"
#include "guiinfo/GUIInfoLabels.h"

#if defined(TARGET_DARWIN_TVOS)
  #include "platform/darwin/DarwinUtils.h"
  #include "platform/darwin/tvos/TVOSTopShelf.h"
#endif

#define NUM_ITEMS 10


CHomeButtonJob::CHomeButtonJob()
{

}

CHomeButtonJob::~CHomeButtonJob()
{

}

bool CHomeButtonJob::DoWork()
{
  int counter = 0;
  while (!(g_infoManager.GetBool(PVR_HAS_RADIO_CHANNELS) ||
         g_infoManager.GetBool(PVR_HAS_TV_CHANNELS)))
  {
    Sleep(500);
    counter++;
    if (counter > 10) // timeout after 5 seconds
      return true;
  }
  ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::PVR, "xbmc", "HomeScreenUpdate");
  return true;
}

CHomeShelfJob::CHomeShelfJob(int flag)
{
  m_flag = flag;
  m_compatibleSkin = false;
  std::string skinId = CSettings::GetInstance().GetString(CSettings::SETTING_LOOKANDFEEL_SKIN);
  ADDON::AddonPtr addon;
  if (ADDON::CAddonMgr::GetInstance().GetAddon(skinId, addon, ADDON::ADDON_SKIN))
  {
    if (skinId == "skin.opacity" || skinId == "skin.ariana" || skinId == "skin.ariana.touch")
      m_compatibleSkin = true;
  }
}

CHomeShelfJob::~CHomeShelfJob()
{
}

bool CHomeShelfJob::UpdateVideo()
{
  CSingleLock lock(m_critsection);

  CLog::Log(LOGDEBUG, "CHomeShelfJob::UpdateVideos() - Running HomeShelf screen update");

  CFileItemList homeShelfTVRA;
  CFileItemList homeShelfTVPR;
  CFileItemList homeShelfMoviesRA;
  CFileItemList homeShelfMoviesPR;
  CFileItemList homeShelfOnDeck;
  CFileItemList homeShelfContinueWatching;

  std::string serverType = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);

  bool homeScreenWatched = CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOLIBRARY_WATCHEDHOMESHELFITEMS);
  if (!m_compatibleSkin || serverType == "mrmc" || serverType.empty())
  {
    CVideoDatabase videodatabase;
    videodatabase.Open();

    if (videodatabase.HasContent())
    {
      CVideoThumbLoader loader;
      loader.OnLoaderStart();
      std::string path;
      path = g_advancedSettings.m_recentlyAddedMoviePath;
      if (!homeScreenWatched)
      {
        CVideoDbUrl url;
        url.FromString(path);
        url.AddOption("filter", "{\"type\":\"movies\", \"rules\":[{\"field\":\"playcount\", \"operator\":\"is\", \"value\":\"0\"}]}");
        path = url.ToString();
      }

      videodatabase.GetRecentlyAddedMoviesNav(path, homeShelfMoviesRA, NUM_ITEMS);

      for (int i = 0; i < homeShelfMoviesRA.Size(); i++)
      {
        CFileItemPtr item = homeShelfMoviesRA.Get(i);
        item->SetProperty("ItemType", g_localizeStrings.Get(681));
        if (!item->HasArt("thumb"))
        {
          loader.LoadItem(item.get());
        }
      }
      if (!m_compatibleSkin)
        CServicesManager::GetInstance().GetAllRecentlyAddedMovies(homeShelfMoviesRA, NUM_ITEMS, homeScreenWatched);

      homeShelfMoviesRA.SetContent("movies");
      ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfMoviesRA",homeShelfMoviesRA);

      path = g_advancedSettings.m_recentlyAddedEpisodePath;
      if (!homeScreenWatched)
      {
        CVideoDbUrl url;
        url.FromString(path);
        url.AddOption("filter", "{\"type\":\"episodes\", \"rules\":[{\"field\":\"playcount\", \"operator\":\"is\", \"value\":\"0\"}]}");
        path = url.ToString();
      }
      videodatabase.GetRecentlyAddedEpisodesNav(path, homeShelfTVRA, NUM_ITEMS);
      std::string seasonThumb;
      for (int i = 0; i < homeShelfTVRA.Size(); i++)
      {
        CFileItemPtr item = homeShelfTVRA.Get(i);
        std::string seasonEpisode = StringUtils::Format("S%02iE%02i", item->GetVideoInfoTag()->m_iSeason, item->GetVideoInfoTag()->m_iEpisode);
        item->SetProperty("SeasonEpisode", seasonEpisode);
        item->SetProperty("ItemType", g_localizeStrings.Get(681));
        if (!item->HasArt("thumb"))
        {
          loader.LoadItem(item.get());
        }
        if (!item->HasArt("tvshow.thumb"))
        {
          item->SetArt("tvshow.thumb", item->GetArt("season.poster"));
        }
      }
      if (!m_compatibleSkin)
        CServicesManager::GetInstance().GetAllRecentlyAddedShows(homeShelfTVRA, NUM_ITEMS, homeScreenWatched);

      homeShelfTVRA.SetContent("episodes");
      ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfTVRA",homeShelfTVRA);

      XFILE::CDirectory::GetDirectory("videodb://inprogressmovies/", homeShelfMoviesPR);
      XFILE::CDirectory::GetDirectory("library://video/inprogressepisodes.xml/", homeShelfTVPR);
      homeShelfMoviesPR.Sort(SortByLastPlayed, SortOrderDescending);
      homeShelfTVPR.Sort(SortByLastPlayed, SortOrderDescending);
      for (int i = 0; i < homeShelfMoviesPR.Size() && i < NUM_ITEMS; i++)
      {
        CFileItemPtr item = homeShelfMoviesPR.Get(i);
        item->SetProperty("ItemType", g_localizeStrings.Get(682));
        if (!item->HasArt("thumb"))
        {
          loader.LoadItem(item.get());
        }
      }
      if (!m_compatibleSkin)
        CServicesManager::GetInstance().GetAllInProgressMovies(homeShelfMoviesPR, NUM_ITEMS);

      homeShelfMoviesPR.SetContent("movies");
      ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfMoviesPR",homeShelfMoviesPR);

      for (int i = 0; i < homeShelfTVPR.Size() && i < NUM_ITEMS; i++)
      {
        CFileItemPtr item = homeShelfTVPR.Get(i);
        std::string seasonEpisode = StringUtils::Format("S%02iE%02i", item->GetVideoInfoTag()->m_iSeason, item->GetVideoInfoTag()->m_iEpisode);
        item->SetProperty("SeasonEpisode", seasonEpisode);
        item->SetProperty("ItemType", g_localizeStrings.Get(682));
        if (!item->HasArt("thumb"))
        {
          loader.LoadItem(item.get());
        }
        if (!item->HasArt("tvshow.thumb"))
        {
          item->SetArt("tvshow.thumb", item->GetArt("season.poster"));
        }
      }
      if (!m_compatibleSkin)
        CServicesManager::GetInstance().GetAllInProgressShows(homeShelfTVPR, NUM_ITEMS);
      
      homeShelfTVPR.SetContent("episodes");
      ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfTVPR",homeShelfTVPR);

      videodatabase.Close();
    }
  }
  else
  {
    // get recently added TVSHOWS and MOVIES for chosen server in Home Screen, get 20 items as its not as slow as it was before
    CServicesManager::GetInstance().GetContinueWatching(homeShelfContinueWatching, serverType, serverUUID);
    homeShelfContinueWatching.SetContent("movies");
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfContinueWatching",homeShelfContinueWatching);

    CServicesManager::GetInstance().GetRecentlyAddedShows(homeShelfTVRA, NUM_ITEMS*2, true, serverType, serverUUID);
    homeShelfTVRA.SetContent("episodes");
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfTVRA",homeShelfTVRA);

    CServicesManager::GetInstance().GetRecentlyAddedMovies(homeShelfMoviesRA, NUM_ITEMS*2, true, serverType, serverUUID);
    homeShelfMoviesRA.SetContent("movies");
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfMoviesRA",homeShelfMoviesRA);

    CServicesManager::GetInstance().GetInProgressShows(homeShelfTVPR, NUM_ITEMS*2, serverType, serverUUID);
    homeShelfTVPR.SetContent("episodes");
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfTVPR",homeShelfTVPR);

    CServicesManager::GetInstance().GetInProgressMovies(homeShelfMoviesPR, NUM_ITEMS*2, serverType, serverUUID);
    homeShelfMoviesPR.SetContent("movies");
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfMoviesPR",homeShelfMoviesPR);

  }

#if defined(TARGET_DARWIN_TVOS)
  // send recently added Movies and TvShows to TopShelf
 CTVOSTopShelf::GetInstance().SetTopShelfItems(homeShelfMoviesRA,homeShelfTVRA,homeShelfMoviesPR,homeShelfTVPR);
#endif

  return true;
}

bool CHomeShelfJob::UpdateMusic()
{
  CSingleLock lock(m_critsection);
  CFileItemList homeShelfMusicAlbums;
  CLog::Log(LOGDEBUG, "CHomeShelfJob::UpdateMusic() - Running HomeShelf screen update");

  std::string serverType = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);

  if (!m_compatibleSkin || serverType == "mrmc" || serverType.empty())
  {
    CMusicDatabase musicdatabase;
    musicdatabase.Open();
    if (musicdatabase.HasContent())
    {
      VECALBUMS albums;
      musicdatabase.GetRecentlyAddedAlbums(albums, NUM_ITEMS);
      for (size_t i = 0; i < albums.size(); ++i)
      {
        CAlbum &album = albums[i];
        std::string strDir = StringUtils::Format("musicdb://albums/%li/", album.idAlbum);
        CFileItemPtr pItem(new CFileItem(strDir, album));
        std::string strThumb = musicdatabase.GetArtForItem(album.idAlbum, MediaTypeAlbum, "thumb");
        std::string strFanart = musicdatabase.GetArtistArtForItem(album.idAlbum, MediaTypeAlbum, "fanart");
        pItem->SetProperty("thumb", strThumb);
        pItem->SetProperty("fanart", strFanart);
        pItem->SetProperty("artist", album.GetAlbumArtistString());
        pItem->SetProperty("ItemType", g_localizeStrings.Get(681));
        homeShelfMusicAlbums.Add(pItem);
      }
      musicdatabase.Close();
    }
    if (!serverType.empty() && serverType != "mrmc")
      // get recently added ALBUMS from any enabled service
      CServicesManager::GetInstance().GetAllRecentlyAddedAlbums(homeShelfMusicAlbums, NUM_ITEMS);
  }
  else
  {
    // get recently added ALBUMS for chosen server in Home Screen
    CServicesManager::GetInstance().GetRecentlyAddedAlbums(homeShelfMusicAlbums, NUM_ITEMS, serverType, serverUUID);
  }
  ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::HomeScreen, "mrmc", "UpdateRecentlyAddedShelfMusicAlbums",homeShelfMusicAlbums);
  return true;
}

bool CHomeShelfJob::DoWork()
{
  bool ret = true;

  if (g_application.m_bStop)
    return ret;

  if (m_flag & Audio)
    ret &= UpdateMusic();

  if (g_application.m_bStop)
    return ret;

  if (m_flag & Video)
    ret &= UpdateVideo();

  return ret;
}

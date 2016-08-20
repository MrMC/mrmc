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
#include "FileItem.h"
#include "filesystem/Directory.h"
#include "HomeShelfJob.h"
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

#if defined(TARGET_DARWIN_TVOS)
  #include "platform/darwin/DarwinUtils.h"
  #include "platform/darwin/tvos/TVOSTopShelf.h"
#endif

#define NUM_ITEMS 10

CHomeShelfJob::CHomeShelfJob(int flag)
{
  m_flag = flag;
  m_HomeShelfTV = new CFileItemList;
  m_HomeShelfMovies = new CFileItemList;
  m_HomeShelfMusicAlbums = new CFileItemList;
  m_HomeShelfMusicSongs = new CFileItemList;
  m_HomeShelfMusicVideos = new CFileItemList;
} 

bool CHomeShelfJob::UpdateVideo()
{
  CGUIWindow* home = g_windowManager.GetWindow(WINDOW_HOME);

  if ( home == NULL )
    return false;

  CLog::Log(LOGDEBUG, "CHomeShelfJob::UpdateVideos() - Running HomeShelf screen update");
  
  if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOLIBRARY_SHOWINPROGRESS))
  {
    CVideoThumbLoader loader;
    
    XFILE::CDirectory::GetDirectory("library://video/inprogressmovies.xml/", *m_HomeShelfMovies);
    XFILE::CDirectory::GetDirectory("library://video/inprogressshows.xml/", *m_HomeShelfTV);
    
    for (int i = 0; i < m_HomeShelfMovies->Size(); i++)
    {
      CFileItemPtr item          = m_HomeShelfMovies->Get(i);
      item->SetProperty("ItemType", g_localizeStrings.Get(20386));
      if (!item->HasArt("thumb"))
      {
        loader.LoadItem(item.get());
      }
    }
    
    for (int i = 0; i < m_HomeShelfTV->Size(); i++)
    {
      CFileItemPtr item = m_HomeShelfTV->Get(i);
      std::string seasonEpisode = StringUtils::Format("S%02iE%02i", item->GetVideoInfoTag()->m_iSeason, item->GetVideoInfoTag()->m_iEpisode);
      item->SetProperty("SeasonEpisode", seasonEpisode);
      item->SetProperty("ItemType", g_localizeStrings.Get(20387));
      if (!item->HasArt("thumb"))
      {
        loader.LoadItem(item.get());
      }
    }
    
    // get InProgress TVSHOWS and MOVIES from any enabled service
    CServicesManager::GetInstance().GetAllInProgressShows(*m_HomeShelfTV, NUM_ITEMS);
    CServicesManager::GetInstance().GetAllInProgressMovies(*m_HomeShelfMovies, NUM_ITEMS);
  }
  else
  {
    CVideoDatabase videodatabase;
    std::string path;
    CVideoThumbLoader loader;
    loader.OnLoaderStart();
    
    path = g_advancedSettings.m_recentlyAddedMoviePath;
    if (g_advancedSettings.m_iVideoLibraryRecentlyAddedUnseen)
    {
      CVideoDbUrl url;
      url.FromString(path);
      url.AddOption("filter", "{\"type\":\"movies\", \"rules\":[{\"field\":\"playcount\", \"operator\":\"is\", \"value\":\"0\"}]}");
      path = url.ToString();
    }

    videodatabase.Open();
    videodatabase.GetRecentlyAddedMoviesNav(path, *m_HomeShelfMovies, NUM_ITEMS);

    for (int i = 0; i < m_HomeShelfMovies->Size(); i++)
    {
      CFileItemPtr item          = m_HomeShelfMovies->Get(i);
      item->SetProperty("ItemType", g_localizeStrings.Get(20386));
      if (!item->HasArt("thumb"))
      {
        loader.LoadItem(item.get());
      }
    }

    path = g_advancedSettings.m_recentlyAddedEpisodePath;
    if (g_advancedSettings.m_iVideoLibraryRecentlyAddedUnseen)
    {
      CVideoDbUrl url;
      url.FromString(path);
      url.AddOption("filter", "{\"type\":\"episodes\", \"rules\":[{\"field\":\"playcount\", \"operator\":\"is\", \"value\":\"0\"}]}");
      path = url.ToString();
    }

    videodatabase.GetRecentlyAddedEpisodesNav(path, *m_HomeShelfTV, NUM_ITEMS);
    std::string seasonThumb;
    for (int i = 0; i < m_HomeShelfTV->Size(); i++)
    {
      CFileItemPtr item = m_HomeShelfTV->Get(i);
      std::string seasonEpisode = StringUtils::Format("S%02iE%02i", item->GetVideoInfoTag()->m_iSeason, item->GetVideoInfoTag()->m_iEpisode);
      item->SetProperty("SeasonEpisode", seasonEpisode);
      item->SetProperty("ItemType", g_localizeStrings.Get(20387));
      if (!item->HasArt("thumb"))
      {
        loader.LoadItem(item.get());
      }
    }

    // get recently added TVSHOWS and MOVIES from any enabled service
    CServicesManager::GetInstance().GetAllRecentlyAddedShows(*m_HomeShelfTV, NUM_ITEMS);
    CServicesManager::GetInstance().GetAllRecentlyAddedMovies(*m_HomeShelfMovies, NUM_ITEMS);
  }
  
#if defined(TARGET_DARWIN_TVOS)
  // send recently added Movies and TvShows to TopShelf
  CTVOSTopShelf::GetInstance().SetTopShelfItems(*m_HomeShelfMovies,*m_HomeShelfTV);
#endif

  return true;
}

bool CHomeShelfJob::UpdateMusic()
{
  CLog::Log(LOGDEBUG, "CHomeShelfJob::UpdateMusic() - Running HomeShelf screen update");
  
  CMusicDatabase musicdatabase;

  VECALBUMS albums;
  musicdatabase.Open();
  musicdatabase.GetRecentlyAddedAlbums(albums, NUM_ITEMS);
  
  for (int i=0; i<(int)albums.size(); ++i)
  {
    CAlbum& album=albums[i];
    std::string strDir = StringUtils::Format("musicdb://albums/%li/", album.idAlbum);
    CFileItemPtr pItem(new CFileItem(strDir, album));
    std::string strThumb = musicdatabase.GetArtForItem(album.idAlbum, MediaTypeAlbum, "thumb");
    std::string strFanart = musicdatabase.GetArtistArtForItem(album.idAlbum, MediaTypeAlbum, "fanart");
    pItem->SetProperty("thumb", strThumb);
    pItem->SetProperty("fanart", strFanart);
    pItem->SetProperty("artist", album.GetAlbumArtistString());
    pItem->SetProperty("ItemType", g_localizeStrings.Get(359));
    m_HomeShelfMusicAlbums->Add(pItem);
  }
  musicdatabase.Close();
  return true;
}

bool CHomeShelfJob::UpdateTotal()
{
  CGUIWindow* home = g_windowManager.GetWindow(WINDOW_HOME);
  
  if ( home == NULL )
    return false;
  
  CLog::Log(LOGDEBUG, "CHomeShelfJob::UpdateTotal() - Running HomeShelf home screen update");
  
  CVideoDatabase videodatabase;  
  CMusicDatabase musicdatabase;
  
  musicdatabase.Open();
  int MusSongTotals   = atoi(musicdatabase.GetSingleValue("songview"       , "count(1)").c_str());
  int MusAlbumTotals  = atoi(musicdatabase.GetSingleValue("songview"       , "count(distinct strAlbum)").c_str());
  int MusArtistTotals = atoi(musicdatabase.GetSingleValue("songview"       , "count(distinct strArtists)").c_str());
  musicdatabase.Close();
 
  videodatabase.Open();
  int tvShowCount     = atoi(videodatabase.GetSingleValue("tvshow_view"     , "count(1)").c_str());
  int movieTotals     = atoi(videodatabase.GetSingleValue("movie_view"      , "count(1)").c_str());
  int movieWatched    = atoi(videodatabase.GetSingleValue("movie_view"      , "count(playCount)").c_str());
  int MusVidTotals    = atoi(videodatabase.GetSingleValue("musicvideo_view" , "count(1)").c_str());
  int MusVidWatched   = atoi(videodatabase.GetSingleValue("musicvideo_view" , "count(playCount)").c_str());
  int EpWatched       = atoi(videodatabase.GetSingleValue("tvshow_view"     , "sum(watchedcount)").c_str());
  int EpCount         = atoi(videodatabase.GetSingleValue("tvshow_view"     , "sum(totalcount)").c_str());
  int TvShowsWatched  = atoi(videodatabase.GetSingleValue("tvshow_view"     , "sum(watchedcount = totalcount)").c_str());
  videodatabase.Close();
  
  home->SetProperty("TVShows.Count"         , tvShowCount);
  home->SetProperty("TVShows.Watched"       , TvShowsWatched);
  home->SetProperty("TVShows.UnWatched"     , tvShowCount - TvShowsWatched);
  home->SetProperty("Episodes.Count"        , EpCount);
  home->SetProperty("Episodes.Watched"      , EpWatched);
  home->SetProperty("Episodes.UnWatched"    , EpCount-EpWatched);  
  home->SetProperty("Movies.Count"          , movieTotals);
  home->SetProperty("Movies.Watched"        , movieWatched);
  home->SetProperty("Movies.UnWatched"      , movieTotals - movieWatched);
  home->SetProperty("MusicVideos.Count"     , MusVidTotals);
  home->SetProperty("MusicVideos.Watched"   , MusVidWatched);
  home->SetProperty("MusicVideos.UnWatched" , MusVidTotals - MusVidWatched);
  home->SetProperty("Music.SongsCount"      , MusSongTotals);
  home->SetProperty("Music.AlbumsCount"     , MusAlbumTotals);
  home->SetProperty("Music.ArtistsCount"    , MusArtistTotals);
  
  return true;
}


bool CHomeShelfJob::DoWork()
{
  bool ret = true;
  if (m_flag & Audio)
    ret &= UpdateMusic();
  
  if (m_flag & Video)
    ret &= UpdateVideo();
  
  if (m_flag & Totals)
    ret &= UpdateTotal();
    
  return ret; 
}

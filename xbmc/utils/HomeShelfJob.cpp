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
  m_HomeShelfTVRA = new CFileItemList;
  m_HomeShelfTVPR = new CFileItemList;
  m_HomeShelfMoviesRA = new CFileItemList;
  m_HomeShelfMoviesPR = new CFileItemList;
  m_HomeShelfMusicAlbums = new CFileItemList;
  m_HomeShelfMusicSongs = new CFileItemList;
  m_HomeShelfMusicVideos = new CFileItemList;
}

CHomeShelfJob::~CHomeShelfJob()
{
  SAFE_DELETE(m_HomeShelfTVRA);
  SAFE_DELETE(m_HomeShelfTVPR);
  SAFE_DELETE(m_HomeShelfMoviesRA);
  SAFE_DELETE(m_HomeShelfMoviesPR);
  SAFE_DELETE(m_HomeShelfMusicAlbums);
  SAFE_DELETE(m_HomeShelfMusicSongs);
  SAFE_DELETE(m_HomeShelfMusicVideos);
}

bool CHomeShelfJob::UpdateVideo()
{
  CGUIWindow* home = g_windowManager.GetWindow(WINDOW_HOME);

  if ( home == NULL )
    return false;

  CLog::Log(LOGDEBUG, "CHomeShelfJob::UpdateVideos() - Running HomeShelf screen update");

  CSingleLock lock(m_critsection);

  CVideoDatabase videodatabase;
  videodatabase.Open();
  CFileItemList homeShelfTVRA;
  CFileItemList homeShelfTVPR;
  CFileItemList homeShelfMoviesRA;
  CFileItemList homeShelfMoviesPR;
  
  m_HomeShelfTVRA->ClearItems();
  m_HomeShelfTVPR->ClearItems();
  m_HomeShelfMoviesRA->ClearItems();
  m_HomeShelfMoviesPR->ClearItems();

  int homeScreenItemSelector = CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOLIBRARY_HOMESHELFITEMS);
  
  if (homeScreenItemSelector > 1) // 2 and 3 are in progress and both
  {
    if (videodatabase.HasContent())
    {
      CVideoThumbLoader loader;

      XFILE::CDirectory::GetDirectory("library://video/inprogressmovies.xml/", homeShelfMoviesPR);
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
        m_HomeShelfMoviesPR->Add(item);
      }
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
        m_HomeShelfTVPR->Add(item);
      }
    }
    // get InProgress TVSHOWS and MOVIES from any enabled service
    CServicesManager::GetInstance().GetAllInProgressShows(*m_HomeShelfTVPR, NUM_ITEMS);
    CServicesManager::GetInstance().GetAllInProgressMovies(*m_HomeShelfMoviesPR, NUM_ITEMS);
  }
  
  if (homeScreenItemSelector == 1 || homeScreenItemSelector == 3) // 1 is recently added, 3 is both
  {
    if (videodatabase.HasContent())
    {
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

      videodatabase.GetRecentlyAddedMoviesNav(path, homeShelfMoviesRA, NUM_ITEMS);

      for (int i = 0; i < homeShelfMoviesRA.Size(); i++)
      {
        CFileItemPtr item = homeShelfMoviesRA.Get(i);
        item->SetProperty("ItemType", g_localizeStrings.Get(681));
        if (!item->HasArt("thumb"))
        {
          loader.LoadItem(item.get());
        }
        m_HomeShelfMoviesRA->Add(item);
      }

      path = g_advancedSettings.m_recentlyAddedEpisodePath;
      if (g_advancedSettings.m_iVideoLibraryRecentlyAddedUnseen)
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
        m_HomeShelfTVRA->Add(item);
      }
    }

    // get recently added TVSHOWS and MOVIES from any enabled service
    CServicesManager::GetInstance().GetAllRecentlyAddedShows(*m_HomeShelfTVRA, NUM_ITEMS);
    CServicesManager::GetInstance().GetAllRecentlyAddedMovies(*m_HomeShelfMoviesRA, NUM_ITEMS);
  }
  
  videodatabase.Close();
  m_HomeShelfTVRA->SetContent("episodes");
  m_HomeShelfTVPR->SetContent("episodes");
  m_HomeShelfMoviesRA->SetContent("movies");
  m_HomeShelfMoviesPR->SetContent("movies");
#if defined(TARGET_DARWIN_TVOS)
  // send recently added Movies and TvShows to TopShelf
  CTVOSTopShelf::GetInstance().SetTopShelfItems(*m_HomeShelfMoviesRA,*m_HomeShelfTVRA,*m_HomeShelfMoviesPR,*m_HomeShelfTVPR);
#endif

  return true;
}

bool CHomeShelfJob::UpdateMusic()
{
  CLog::Log(LOGDEBUG, "CHomeShelfJob::UpdateMusic() - Running HomeShelf screen update");

  CSingleLock lock(m_critsection);

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
      m_HomeShelfMusicAlbums->Add(pItem);
    }
    musicdatabase.Close();
  }

  // get recently added ALBUMS from any enabled service
  CServicesManager::GetInstance().GetAllRecentlyAddedAlbums(*m_HomeShelfMusicAlbums, NUM_ITEMS);
  
  return true;
}

bool CHomeShelfJob::UpdateTotal()
{
  CGUIWindow* home = g_windowManager.GetWindow(WINDOW_HOME);
  if (home == NULL)
    return false;

  CLog::Log(LOGDEBUG, "CHomeShelfJob::UpdateTotal() - Running HomeShelf home screen update");
  int MusSongTotals   = 0;
  int MusAlbumTotals  = 0;
  int MusArtistTotals = 0;
  int tvShowCount     = 0;
  int movieTotals     = 0;
  int movieWatched    = 0;
  int MusVidTotals    = 0;
  int MusVidWatched   = 0;
  int EpWatched       = 0;
  int EpCount         = 0;
  int TvShowsWatched  = 0;
  
  CMusicDatabase musicdatabase;
  musicdatabase.Open();
  if (musicdatabase.HasContent())
  {
    MusSongTotals   = atoi(musicdatabase.GetSingleValue("songview"       , "count(1)").c_str());
    MusAlbumTotals  = atoi(musicdatabase.GetSingleValue("songview"       , "count(distinct strAlbum)").c_str());
    MusArtistTotals = atoi(musicdatabase.GetSingleValue("songview"       , "count(distinct strArtists)").c_str());
  }
  musicdatabase.Close();

  CVideoDatabase videodatabase;
  videodatabase.Open();
  if (videodatabase.HasContent())
  {
    tvShowCount     = atoi(videodatabase.GetSingleValue("tvshow_view"     , "count(1)").c_str());
    movieTotals     = atoi(videodatabase.GetSingleValue("movie_view"      , "count(1)").c_str());
    movieWatched    = atoi(videodatabase.GetSingleValue("movie_view"      , "count(playCount)").c_str());
    MusVidTotals    = atoi(videodatabase.GetSingleValue("musicvideo_view" , "count(1)").c_str());
    MusVidWatched   = atoi(videodatabase.GetSingleValue("musicvideo_view" , "count(playCount)").c_str());
    EpWatched       = atoi(videodatabase.GetSingleValue("tvshow_view"     , "sum(watchedcount)").c_str());
    EpCount         = atoi(videodatabase.GetSingleValue("tvshow_view"     , "sum(totalcount)").c_str());
    TvShowsWatched  = atoi(videodatabase.GetSingleValue("tvshow_view"     , "sum(watchedcount = totalcount)").c_str());
  }
  videodatabase.Close();
  
  if(CServicesManager::GetInstance().HasServices())
  {
    // Pull up all plex totals and add to existing ones
    MediaServicesMediaCount mediaTotals;
    CServicesManager::GetInstance().GetMediaTotals(mediaTotals);
    
    MusSongTotals   = MusSongTotals + mediaTotals.iMusicSongs;
    MusAlbumTotals  = MusAlbumTotals + mediaTotals.iMusicAlbums;
    MusArtistTotals = MusArtistTotals + mediaTotals.iMusicArtist;
    tvShowCount     = tvShowCount + mediaTotals.iShowTotal;
    movieTotals     = movieTotals + mediaTotals.iMovieTotal;
    movieWatched    = movieWatched + (mediaTotals.iMovieTotal - mediaTotals.iMovieUnwatched);
    EpWatched       = EpWatched + (mediaTotals.iEpisodeTotal - mediaTotals.iEpisodeUnwatched);
    EpCount         = EpCount + mediaTotals.iEpisodeTotal;
    TvShowsWatched  = TvShowsWatched + (mediaTotals.iShowTotal - mediaTotals.iShowUnwatched);
  }
  
  home->SetProperty("Music.SongsCount"      , MusSongTotals);
  home->SetProperty("Music.AlbumsCount"     , MusAlbumTotals);
  home->SetProperty("Music.ArtistsCount"    , MusArtistTotals);
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
  
  CLog::Log(LOGDEBUG, "CHomeShelfJob::UpdateTotal() - Finished");
  return true;
}

void CHomeShelfJob::UpdateTvItemsRA(CFileItemList *list)
{
  CSingleLock lock(m_critsection);
  list->Assign(*m_HomeShelfTVRA);
}

void CHomeShelfJob::UpdateTvItemsPR(CFileItemList *list)
{
  CSingleLock lock(m_critsection);
  list->Assign(*m_HomeShelfTVPR);
}

void CHomeShelfJob::UpdateMovieItemsRA(CFileItemList *list)
{
  CSingleLock lock(m_critsection);
  list->Assign(*m_HomeShelfMoviesRA);
}

void CHomeShelfJob::UpdateMovieItemsPR(CFileItemList *list)
{
  CSingleLock lock(m_critsection);
  list->Assign(*m_HomeShelfMoviesPR);
}

void CHomeShelfJob::UpdateMusicSongItems(CFileItemList *list)
{
  CSingleLock lock(m_critsection);
  list->Assign(*m_HomeShelfMusicSongs);
}

void CHomeShelfJob::UpdateMusicAlbumItems(CFileItemList *list)
{
  CSingleLock lock(m_critsection);
  list->Assign(*m_HomeShelfMusicAlbums);
}

void CHomeShelfJob::UpdateMusicVideoItems(CFileItemList *list)
{
  CSingleLock lock(m_critsection);
  list->Assign(*m_HomeShelfMusicVideos);
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

  if (g_application.m_bStop)
    return ret;

  if (m_flag & Totals)
    ret &= UpdateTotal();

  return ret;
}

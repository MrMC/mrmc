#pragma once
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

#include <map>

#include "utils/JobManager.h"
#include "threads/SharedSection.h"
#include "filesystem/IDirectory.h"
#include "interfaces/IAnnouncer.h"

class CURL;
class CFileItem;
class CFileItemList;

enum class MediaServicesPlayerState
{
  paused = 1,
  playing = 2,
  stopped = 3,
};

typedef struct MediaServicesMediaCount
{
  int iMovieTotal = 0;
  int iMovieUnwatched = 0;
  int iEpisodeTotal = 0;
  int iEpisodeUnwatched = 0;
  int iShowTotal = 0;
  int iShowUnwatched = 0;
  int iMusicSongs = 0;
  int iMusicAlbums = 0;
  int iMusicArtist = 0;

  void reset()
  {
    iMovieTotal = 0;
    iMovieUnwatched = 0;
    iEpisodeTotal = 0;
    iEpisodeUnwatched = 0;
    iShowTotal = 0;
    iShowUnwatched = 0;
    iMusicSongs = 0;
    iMusicAlbums = 0;
    iMusicArtist = 0;
  }
} MediaServicesMediaCount;

class IMediaServicesHandler: public XFILE::IDirectory
{
public:
  virtual ~IMediaServicesHandler() { }

  virtual void SetItemWatched(CFileItem &item) { }
  virtual void SetItemUnWatched(CFileItem &item) { }
  virtual void UpdateItemState(CFileItem &item, double currentTime) { }
  virtual void GetAllRecentlyAddedMovies(CFileItemList &recentlyAdded, int itemLimit) { }
  virtual void GetAllRecentlyAddedShows(CFileItemList &recentlyAdded, int itemLimit){ }

  virtual bool GetDirectory(const CURL& url, CFileItemList &items);
  virtual XFILE::DIR_CACHE_TYPE GetCacheType(const CURL& url);
};

class CServicesManager
: public CJobQueue
, public ANNOUNCEMENT::IAnnouncer
{
public:
  static CServicesManager &GetInstance();

  bool HasServices();
  bool GetStartFolder(std::string &path);
  bool IsMediaServicesItem(const CFileItem &item);
  bool IsMediaServicesCloudItem(const CFileItem &item);
  bool UpdateMediaServicesLibraries(const CFileItem &item);
  bool ReloadProfiles();

  void SetItemWatched(CFileItem &item);
  void SetItemUnWatched(CFileItem &item);
  void UpdateItemState(CFileItem &item, double currentTime);
  void ShowMusicInfo(CFileItem item);
  void GetAllRecentlyAddedMovies(CFileItemList &recentlyAdded, int itemLimit);
  void GetAllRecentlyAddedShows(CFileItemList &recentlyAdded, int itemLimit);
  void GetAllRecentlyAddedAlbums(CFileItemList &recentlyAdded, int itemLimit);
  void GetAllInProgressShows(CFileItemList &inProgress, int itemLimit);
  void GetAllInProgressMovies(CFileItemList &inProgress, int itemLimit);
  void GetSubtitles(CFileItem &item);
  void GetMoreInfo(CFileItem &item);
  bool GetResolutions(CFileItem &item);
  bool GetURL(CFileItem &item);
  void SearchService(CFileItemList &items, std::string strSearchString);
  bool GetAlbumSongs(CFileItem item, CFileItemList &items);
  bool GetDirectory(const CURL& url, CFileItemList &items);
  XFILE::DIR_CACHE_TYPE GetCacheType(const CURL& url);
  bool GetMediaTotals(MediaServicesMediaCount &totals);

  void RegisterMediaServicesHandler(IMediaServicesHandler *mediaServicesHandler);
  void UnregisterSettingsHandler(IMediaServicesHandler *mediaServicesHandler);

  // IAnnouncer callbacks
  virtual void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data) override;

private:
  // private construction, and no assignements; use the provided singleton methods
  CServicesManager();
  CServicesManager(const CServicesManager&);
  virtual ~CServicesManager();


  typedef std::vector<IMediaServicesHandler*> MediaServicesHandlers;
  MediaServicesHandlers m_mediaServicesHandlers;
  CSharedSection m_mediaServicesCritical;
};

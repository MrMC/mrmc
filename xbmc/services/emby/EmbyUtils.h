#pragma once
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

#include <string>
#include "FileItem.h"
#include "services/ServicesManager.h"

//#define EMBY_DEBUG_VERBOSE
#define EMBY_DEBUG_TIMING

namespace XFILE
{
  class CCurlFile;
}
class CEmbyClient;
typedef std::shared_ptr<CEmbyClient> CEmbyClientPtr;


static const std::string EmbyApiKeyHeader = "X-MediaBrowser-Token";
static const std::string EmbyAuthorizationHeader = "X-Emby-Authorization";

static const std::string EmbyTypeVideo = "Video";
static const std::string EmbyTypeAudio = "Audio";
static const std::string EmbyTypeMovie = "Movie";
static const std::string EmbyTypeSeries = "Series";
static const std::string EmbyTypeSeason = "Season";
static const std::string EmbyTypeSeasons = "Seasons";
static const std::string EmbyTypeEpisode = "Episode";
static const std::string EmbyTypeMusicArtist = "MusicArtist";
static const std::string EmbyTypeMusicAlbum = "MusicAlbum";
static const std::string EmbyTypeBoxSet = "BoxSet";

class CEmbyUtils
{
public:
  static bool HasClients();
  static bool GetIdentity(CURL url, int timeout);
  static void PrepareApiCall(const std::string& userId, const std::string& accessToken, XFILE::CCurlFile &curl);
  static void SetEmbyItemProperties(CFileItem &item, const char *content);
  static void SetEmbyItemProperties(CFileItem &item, const char *content, const CEmbyClientPtr &client);
  static uint64_t TicksToSeconds(uint64_t ticks);
  static uint64_t SecondsToTicks(uint64_t seconds);

  #pragma mark - Emby Server Utils
  static void SetWatched(CFileItem &item);
  static void SetUnWatched(CFileItem &item);
  static void ReportProgress(CFileItem &item, double currentSeconds);
  static void SetPlayState(MediaServicesPlayerState state);
  static bool GetItemSubtiles(CFileItem &item);
  static bool GetMoreItemInfo(CFileItem &item);
  static bool GetMoreResolutions(CFileItem &item);
  static bool GetURL(CFileItem &item);
  static bool SearchEmby(CFileItemList &items, std::string strSearchString);

  #pragma mark - Emby Recently Added and InProgress
  static bool GetEmbyRecentlyAddedEpisodes(CFileItemList &items, const std::string url, int limit=25);
  static bool GetEmbyInProgressShows(CFileItemList &items, const std::string url, int limit=25);
  static bool GetEmbyRecentlyAddedMovies(CFileItemList &items, const std::string url, int limit=25);
  static bool GetEmbyInProgressMovies(CFileItemList &items, const std::string url, int limit=25);
  static bool GetAllEmbyInProgress(CFileItemList &items, bool tvShow);
  static bool GetAllEmbyRecentlyAddedMoviesAndShows(CFileItemList &items, bool tvShow=false);
  static bool GetEmbyRecentlyAddedAlbums(CFileItemList &items,int limit);

  #pragma mark - Emby TV
  static bool GetEmbySeasons(CFileItemList &items, const std::string url);
  static bool GetEmbyEpisodes(CFileItemList &items, const std::string url);

  #pragma mark - Emby Music
  static bool GetEmbyAlbum(CFileItemList &items, std::string url, int limit = 100);
  static bool GetEmbyArtistAlbum(CFileItemList &items, std::string url);
  static bool GetEmbySongs(CFileItemList &items, std::string url);
  static bool GetEmbyAlbumSongs(CFileItemList &items, std::string url);
  static bool ShowMusicInfo(CFileItem item);
  static bool GetEmbyAlbumSongs(CFileItem item, CFileItemList &items);
  static bool GetEmbyMediaTotals(MediaServicesMediaCount &totals);

  static CFileItemPtr ToFileItemPtr(CEmbyClient *client, const CVariant &object);

  #pragma mark - Emby parsers
  static bool ParseEmbyVideos(CFileItemList &items, const CURL url, const CVariant &object, std::string type);
  static bool ParseEmbySeries(CFileItemList &items, const CURL &url, const CVariant &variant);
  static bool ParseEmbySeasons(CFileItemList &items, const CURL &url, const CVariant &series, const CVariant &variant);
  static bool ParseEmbyAudio(CFileItemList &items, const CURL &url, const CVariant &variant);
  static bool ParseEmbyAlbum(CFileItemList &items, const CURL &url, const CVariant &variant);
  static bool ParseEmbyArtists(CFileItemList &items, const CURL &url, const CVariant &variant);
  static bool ParseEmbyMoviesFilter(CFileItemList &items, const CURL url, const CVariant &object, const std::string &filter);
  static bool ParseEmbyTVShowsFilter(CFileItemList &items, const CURL url, const CVariant &object, const std::string &filter);
  static CVariant GetEmbyCVariant(std::string url, std::string filter = "");

private:
  #pragma mark - Emby private
  static CFileItemPtr ToVideoFileItemPtr(CURL url, const CVariant &variant, std::string type);
  static void GetVideoDetails(CFileItem &item, const CVariant &variant);
  static void GetMusicDetails(CFileItem &item, const CVariant &variant);
  static void GetMediaDetals(CFileItem &item, const CVariant &variant, std::string id = "0");
  static void RemoveSubtitleProperties(CFileItem &item);
  
};

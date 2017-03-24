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

#define EMBY_DEBUG_VERBOSE

namespace XFILE
{
  class CCurlFile;
}
class CEmbyClient;
typedef std::shared_ptr<CEmbyClient> CEmbyClientPtr;


static const std::string EmbyApiKeyHeader = "X-MediaBrowser-Token";
static const std::string EmbyAuthorizationHeader = "X-Emby-Authorization";

class CEmbyUtils
{
  friend class CEmbyClient;
public:
  static bool HasClients();
  static bool GetIdentity(CURL url, int timeout);
  static void PrepareApiCall(const std::string& userId, const std::string& accessToken, XFILE::CCurlFile &curl);
  static void SetEmbyItemProperties(CFileItem &item);
  static void SetEmbyItemProperties(CFileItem &item, const CEmbyClientPtr &client);
  static void SetEmbyItemsProperties(CFileItemList &items);
  static void SetEmbyItemsProperties(CFileItemList &items, const CEmbyClientPtr &client);

  static void SetWatched(CFileItem &item);
  static void SetUnWatched(CFileItem &item);
  static void ReportProgress(CFileItem &item, double currentSeconds);
  static void SetPlayState(MediaServicesPlayerState state);
  static bool GetEmbyRecentlyAddedEpisodes(CFileItemList &items, const std::string url, int limit=25);
  static bool GetEmbyInProgressShows(CFileItemList &items, const std::string url, int limit=25);
  static bool GetEmbyRecentlyAddedMovies(CFileItemList &items, const std::string url, int limit=25);
  static bool GetEmbyInProgressMovies(CFileItemList &items, const std::string url, int limit=25);
  static bool GetAllEmbyInProgress(CFileItemList &items, bool tvShow);
  static bool GetAllEmbyRecentlyAddedMoviesAndShows(CFileItemList &items, bool tvShow=false);

  static CFileItemPtr ToFileItemPtr(const CEmbyClient *client, const CVariant &object);

  // Emby Movie/TV
  static bool GetEmbyMovies(CFileItemList &items, std::string url, std::string filter = "");
  static bool GetEmbyTvshows(CFileItemList &items, std::string url);
  static bool GetEmbySeasons(CFileItemList &items, const std::string url);
  static bool GetEmbyEpisodes(CFileItemList &items, const std::string url);
  static bool GetEmbyMovieFilter(CFileItemList &items, std::string url, std::string parentPath, std::string filter);
  static bool GetEmbyTVFilter(CFileItemList &items, std::string url, std::string parentPath, std::string filter);
  static bool GetItemSubtiles(CFileItem &item);
  static bool GetMoreItemInfo(CFileItem &item);
  static bool GetMoreResolutions(CFileItem &item);
  static bool GetURL(CFileItem &item);
  static bool SearchEmby(CFileItemList &items, std::string strSearchString);
  
  // Emby Music
  static bool GetEmbyArtistsOrAlbum(CFileItemList &items, std::string url, bool album);
  static bool GetEmbySongs(CFileItemList &items, std::string url);
  static bool ShowMusicInfo(CFileItem item);
  static bool GetEmbyRecentlyAddedAlbums(CFileItemList &items,int limit);
  static bool GetEmbyAlbumSongs(CFileItem item, CFileItemList &items);
  static bool GetEmbyMediaTotals(MediaServicesMediaCount &totals);

private:
  static void ReportToServer(std::string url, std::string filename);
  static bool GetVideoItems(CFileItemList &items,CURL url, const CVariant &object, std::string type, bool formatLabel, int season = -1);
  static void GetVideoDetails(CFileItem &fileitem, const CVariant &cvariant);
  static void GetMusicDetails(CFileItem &fileitem, const CVariant &cvariant);
  static void GetMediaDetals(CFileItem &fileitem, const CVariant &cvariant, std::string id = "0");
  static CVariant GetEmbyCVariant(std::string url, std::string filter = "");
  static void RemoveSubtitleProperties(CFileItem &item);
};

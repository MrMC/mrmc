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

#include <string>
#include "FileItem.h"
#include "utils/XBMCTinyXML.h"
#include "utils/XMLUtils.h"
#include "services/ServicesManager.h"

//#define PLEX_DEBUG_VERBOSE

namespace XFILE
{
  class CCurlFile;
}
class CPlexClient;
typedef std::shared_ptr<CPlexClient> CPlexClientPtr;


class CPlexUtils
{
public:
  static bool HasClients();
  static bool GetIdentity(CURL url, int timeout);
  static void GetDefaultHeaders(XFILE::CCurlFile &curl);
  static void SetPlexItemProperties(CFileItem &item);
  static void SetPlexItemProperties(CFileItem &item, const CPlexClientPtr &client);

#pragma mark - Plex Server Utils
  static void SetWatched(CFileItem &item);
  static void SetUnWatched(CFileItem &item);
  static void ReportProgress(CFileItem &item, double currentSeconds);
  static void SetPlayState(MediaServicesPlayerState state);

#pragma mark - Plex Recently Added and InProgress
  static bool GetPlexRecentlyAddedEpisodes(CFileItemList &items, const std::string url, int limit=25);
  static bool GetPlexInProgressShows(CFileItemList &items, const std::string url, int limit=25);
  static bool GetPlexRecentlyAddedMovies(CFileItemList &items, const std::string url, int limit=25);
  static bool GetPlexInProgressMovies(CFileItemList &items, const std::string url, int limit=25);
  static bool GetAllPlexInProgress(CFileItemList &items, bool tvShow);
  static bool GetAllPlexRecentlyAddedMoviesAndShows(CFileItemList &items, bool tvShow=false);

  static bool GetPlexMovies(CFileItemList &items, std::string url, std::string filter = "");

#pragma mark - Plex TV
  static bool GetPlexTvshows(CFileItemList &items, std::string url);
  static bool GetPlexSeasons(CFileItemList &items, const std::string url);
  static bool GetPlexEpisodes(CFileItemList &items, const std::string url);

  static bool GetPlexFilter(CFileItemList &items, std::string url, std::string parentPath, std::string filter);
  static bool GetItemSubtiles(CFileItem &item);
  static bool GetMoreItemInfo(CFileItem &item);
  static bool GetMoreResolutions(CFileItem &item);
  static bool GetURL(CFileItem &item);
  static void StopTranscode(CFileItem &item);
  static void PingTranscoder(CFileItem &item);
  static bool SearchPlex(CFileItemList &items, std::string strSearchString);
  
#pragma mark - Plex Music
  static bool GetPlexArtistsOrAlbum(CFileItemList &items, std::string url, bool album);
  static bool GetPlexSongs(CFileItemList &items, std::string url);
  static bool ShowMusicInfo(CFileItem item);
  static bool GetPlexRecentlyAddedAlbums(CFileItemList &items,int limit);
  static bool GetPlexAlbumSongs(CFileItem item, CFileItemList &items);
  static bool GetPlexMediaTotals(MediaServicesMediaCount &totals);

  #pragma mark - Plex parsers
  static bool ParsePlexVideos(CFileItemList &items,CURL url, TiXmlElement* rootXmlNode, std::string type, bool formatLabel, int season = -1);
  static bool ParsePlexSeries(CFileItemList  &items, const CURL &url, const TiXmlElement* node);
  static bool ParsePlexSeasons(CFileItemList &items, const CURL &url, const TiXmlElement* root, const TiXmlElement* node);
  static bool ParseEmbySongs(CFileItemList &items, const CURL &url, const TiXmlElement* node);
  static bool ParseEmbyArtistsAlbum(CFileItemList &items, const CURL &url, const TiXmlElement* node, bool album);
  static bool ParsePlexMoviesFilter(CFileItemList  &items, const CURL url, const TiXmlElement* node, const std::string &filter);
  static bool ParsePlexTVShowsFilter(CFileItemList &items, const CURL url, const TiXmlElement* node, const std::string &filter);

private:
  static void ReportToServer(std::string url, std::string filename);
  static void GetVideoDetails(CFileItem &item, const TiXmlElement* videoNode);
  static void GetMusicDetails(CFileItem &item, const TiXmlElement* videoNode);
  static void GetMediaDetals(CFileItem &item, CURL url, const TiXmlElement* videoNode, std::string id = "0");
  static TiXmlDocument GetPlexXML(std::string url, std::string filter = "");
  static int ParsePlexMediaXML(TiXmlDocument xml);
  static void RemoveSubtitleProperties(CFileItem &item);
};

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
//#define PLEX_DEBUG_TIMING

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
  static void GetClientHosts(std::vector<std::string>& hosts);
  static bool GetIdentity(CURL url, int timeout);
  static void GetDefaultHeaders(XFILE::CCurlFile &curl);
  static void SetPlexItemProperties(CFileItem &item);
  static void SetPlexItemProperties(CFileItem &item, const CPlexClientPtr &client);

  // Plex Server Utils
  static void SetWatched(CFileItem &item);
  static void SetUnWatched(CFileItem &item);
  static void ReportProgress(CFileItem &item, double currentSeconds);
  static void SetPlayState(MediaServicesPlayerState state);
  static bool GetPlexMediaTotals(MediaServicesMediaCount &totals);
  static bool DeletePlexMedia(CFileItem &item);

  // Plex Recently Added and InProgress
  static bool GetPlexRecentlyAddedEpisodes(CFileItemList &items, const std::string url, int limit=25);
  static bool GetPlexInProgressShows(CFileItemList &items, const std::string url, int limit=25);
  static bool GetPlexRecentlyAddedMovies(CFileItemList &items, const std::string url, int limit=25);
  static bool GetPlexInProgressMovies(CFileItemList &items, const std::string url, int limit=25);
  static bool GetAllPlexInProgress(CFileItemList &items, bool tvShow);
  static bool GetAllPlexRecentlyAddedMoviesAndShows(CFileItemList &items, bool tvShow=false);

  // Plex Movies
  static bool GetPlexMovies(CFileItemList &items, std::string url, std::string filter = "");

  // Plex TV
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
  
  // Plex Music
  static bool ShowMusicInfo(CFileItem item);
  static bool GetPlexSongs(CFileItemList &items, std::string url);
  static bool GetPlexAlbumSongs(CFileItem item, CFileItemList &items);
  static bool GetPlexArtistsOrAlbum(CFileItemList &items, std::string url, bool album);
  static bool GetPlexRecentlyAddedAlbums(CFileItemList &items,int limit);

  // Plex parsers
  static bool ParsePlexVideos(CFileItemList &items, CURL url, const CVariant &video, std::string type, bool formatLabel, int season = -1);
  static bool ParsePlexVideos(CFileItemList &items, CURL url, TiXmlElement* rootXmlNode, std::string type, bool formatLabel, int season = -1);
  static bool ParsePlexSeries(CFileItemList &items, const CURL &url, const CVariant &directory);
  static bool ParsePlexSeries(CFileItemList  &items, const CURL &url, const TiXmlElement* node);
  static bool ParsePlexSeasons(CFileItemList &items, const CURL &url, const CVariant &mediacontainer, const CVariant &directory);
  static bool ParsePlexSeasons(CFileItemList &items, const CURL &url, const TiXmlElement* root, const TiXmlElement* node);
  static bool ParsePlexSongs(CFileItemList &items, const CURL &url, const CVariant &track);
  static bool ParsePlexSongs(CFileItemList &items, const CURL &url, const TiXmlElement* node);
  static bool ParsePlexArtistsAlbum(CFileItemList &items, const CURL &url, const CVariant &directory, bool album);
  static bool ParsePlexArtistsAlbum(CFileItemList &items, const CURL &url, const TiXmlElement* node, bool album);
  static bool ParsePlexMoviesFilter(CFileItemList  &items, const CURL url, const TiXmlElement* node, const std::string &filter);
  static bool ParsePlexTVShowsFilter(CFileItemList &items, const CURL url, const TiXmlElement* node, const std::string &filter);

private:
  static void ReportToServer(std::string url, std::string filename);
  static void GetVideoDetails(CFileItem &item, const CVariant &variant);
  static void GetVideoDetails(CFileItem &item, const TiXmlElement* videoNode);
  static void GetMusicDetails(CFileItem &item, const CVariant &video);
  static void GetMusicDetails(CFileItem &item, const TiXmlElement* videoNode);
  static void GetMediaDetals(CFileItem &item, CURL url, const CVariant &media, std::string id = "0");
  static void GetMediaDetals(CFileItem &item, CURL url, const TiXmlElement* mediaNode, std::string id = "0");
  static CVariant GetPlexCVariant(std::string url, std::string filter = "");
  static TiXmlDocument GetPlexXML(std::string url, std::string filter = "");
  static int ParsePlexCVariant(const CVariant &item);
  static int ParsePlexMediaXML(TiXmlDocument xml);
  static void RemoveSubtitleProperties(CFileItem &item);
};

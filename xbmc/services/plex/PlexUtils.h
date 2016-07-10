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

//#define PLEX_DEBUG_VERBOSE

namespace XFILE
{
  class CCurlFile;
}
class CPlexClient;
typedef std::shared_ptr<CPlexClient> CPlexClientPtr;


enum class PlexUtilsPlayerState
{
  paused = 1,
  playing = 2,
  stopped = 3,
};

class CPlexUtils
{
public:
  static bool HasClients();
  static bool GetIdentity(CURL url, int timeout);
  static void GetDefaultHeaders(XFILE::CCurlFile &curl);
  static void SetPlexItemProperties(CFileItem &item, const CPlexClientPtr &client);

  static void SetWatched(CFileItem &item);
  static void SetUnWatched(CFileItem &item);
  static void ReportProgress(CFileItem &item, double currentSeconds);
  static void SetPlayState(PlexUtilsPlayerState state);
  static bool GetPlexRecentlyAddedEpisodes(CFileItemList &items, const std::string url, int limit=25);
  static bool GetPlexRecentlyAddedMovies(CFileItemList &items, const std::string url, int limit=25);
  static bool GetAllPlexRecentlyAddedMoviesAndShows(CFileItemList &items, bool tvShow=false);

  static bool GetPlexMovies(CFileItemList &items, std::string url, std::string filter = "");
  static bool GetPlexTvshows(CFileItemList &items, std::string url);
  static bool GetPlexSeasons(CFileItemList &items, const std::string url);
  static bool GetPlexEpisodes(CFileItemList &items, const std::string url);
  static bool GetPlexFilter(CFileItemList &items, std::string url, std::string parentPath, std::string filter);
  static bool GetItemSubtiles(CFileItem &item);
  static bool GetMoreItemInfo(CFileItem &item);
  static bool GetMoreResolutions(CFileItem &item);

private:
  static void ReportToServer(std::string url, std::string filename);
  static bool GetVideoItems(CFileItemList &items,CURL url, TiXmlElement* rootXmlNode, std::string type, int season = -1);
  static void GetVideoDetails(CFileItem &item, const TiXmlElement* videoNode);
  static void GetMediaDetals(CFileItem &item, CURL url, const TiXmlElement* videoNode, std::string id = "0");
  static TiXmlDocument GetPlexXML(std::string url, std::string filter = "");
  static void RemoveSubtitleProperties(CFileItem &item);
};

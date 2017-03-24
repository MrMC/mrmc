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

#include "EmbyUtils.h"
#include "EmbyServices.h"
#include "Application.h"
#include "ContextMenuManager.h"
#include "Util.h"
#include "URL.h"
#include "filesystem/StackDirectory.h"
#include "network/Network.h"
#include "utils/Base64.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/SystemInfo.h"
#include "utils/JSONVariantParser.h"
#include "utils/URIUtils.h"
#include "filesystem/File.h"
#include "filesystem/CurlFile.h"
#include "filesystem/ZipFile.h"
#include "settings/Settings.h"

#include "video/VideoInfoTag.h"
#include "video/windows/GUIWindowVideoBase.h"

#include "music/tags/MusicInfoTag.h"
#include "music/dialogs/GUIDialogSongInfo.h"
#include "music/dialogs/GUIDialogMusicInfo.h"
#include "guilib/GUIWindowManager.h"

// one tick is 0.1 microseconds
static const uint64_t TicksToSecondsFactor = 10000000;
static uint64_t TicksToSeconds(uint64_t ticks)
{
  return ticks / TicksToSecondsFactor;
}
static uint64_t SecondsToTicks(uint64_t seconds)
{
  return seconds * TicksToSecondsFactor;
}

static int  g_progressSec = 0;
static CFileItem m_curItem;
static MediaServicesPlayerState g_playbackState = MediaServicesPlayerState::stopped;

bool CEmbyUtils::HasClients()
{
  return CEmbyServices::GetInstance().HasClients();
}

bool CEmbyUtils::GetIdentity(CURL url, int timeout)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(timeout);
  curlfile.SetRequestHeader("Cache-Control", "no-cache");
  curlfile.SetRequestHeader("Content-Type", "application/json");

  CURL curl(url);
  curl.SetFileName("emby/system/info/public");
  // do not need user/pass for server info
  curl.SetUserName("");
  curl.SetPassword("");
  curl.SetOptions("");

  std::string path = curl.Get();
  std::string response;
  return curlfile.Get(path, response);
}

void CEmbyUtils::PrepareApiCall(const std::string& userId, const std::string& accessToken, XFILE::CCurlFile &curl)
{
  curl.SetRequestHeader("Accept", "application/json");

  if (!accessToken.empty())
    curl.SetRequestHeader(EmbyApiKeyHeader, accessToken);

  curl.SetRequestHeader(EmbyAuthorizationHeader,
    StringUtils::Format("MediaBrowser Client=\"%s\", Device=\"%s\", DeviceId=\"%s\", Version=\"%s\", UserId=\"%s\"",
      CSysInfo::GetAppName().c_str(), CSysInfo::GetDeviceName().c_str(),
      CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_UUID).c_str(),
      CSysInfo::GetVersionShort().c_str(), userId.c_str()));
}

void CEmbyUtils::SetEmbyItemProperties(CFileItem &item)
{
  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(item.GetPath());
  SetEmbyItemProperties(item, client);
}

void CEmbyUtils::SetEmbyItemProperties(CFileItem &item, const CEmbyClientPtr &client)
{
  item.SetProperty("EmbyItem", true);
  item.SetProperty("MediaServicesItem", true);
  if (!client)
    return;
  if (client->IsCloud())
    item.SetProperty("MediaServicesCloudItem", true);
  item.SetProperty("MediaServicesClientID", client->GetUuid());
}

void CEmbyUtils::SetEmbyItemsProperties(CFileItemList &items)
{
  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(items.GetPath());
  SetEmbyItemsProperties(items, client);
}

void CEmbyUtils::SetEmbyItemsProperties(CFileItemList &items, const CEmbyClientPtr &client)
{
  items.SetProperty("EmbyItem", true);
  items.SetProperty("MediaServicesItem", true);
  if (!client)
    return;
  if (client->IsCloud())
    items.SetProperty("MediaServicesCloudItem", true);
  items.SetProperty("MediaServicesClientID", client->GetUuid());
}

void CEmbyUtils::SetWatched(CFileItem &item)
{
  // POST to /Users/{UserId}/PlayedItems/{Id}
  std::string url = item.GetPath();
  if (URIUtils::IsStack(url))
    url = XFILE::CStackDirectory::GetFirstStackedFile(url);
  else
    url = URIUtils::GetParentPath(url);
  if (StringUtils::StartsWithNoCase(url, "emby://"))
    url = Base64::Decode(URIUtils::GetFileName(item.GetPath()));

  // need userId which only client knows
  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(url);
  if (!client || !client->GetPresence())
    return;

  // use the current date and time if lastPlayed is invalid
  if (!item.GetVideoInfoTag()->m_lastPlayed.IsValid())
    item.GetVideoInfoTag()->m_lastPlayed = CDateTime::GetUTCDateTime();

  // get the URL to updated the item's played state for this user ID
  CURL url2(url);
  url2.SetFileName("emby/Users/" + client->GetUserID() + "/PlayedItems/" + item.GetVideoInfoTag()->m_strServiceId);
  url2.SetOptions("");
  // and add the DatePlayed URL parameter
  url2.SetOption("DatePlayed",
    StringUtils::Format("%04i%02i%02i%02i%02i%02i",
      item.GetVideoInfoTag()->m_lastPlayed.GetYear(),
      item.GetVideoInfoTag()->m_lastPlayed.GetMonth(),
      item.GetVideoInfoTag()->m_lastPlayed.GetDay(),
      item.GetVideoInfoTag()->m_lastPlayed.GetHour(),
      item.GetVideoInfoTag()->m_lastPlayed.GetMinute(),
      item.GetVideoInfoTag()->m_lastPlayed.GetSecond()));

  std::string data;
  std::string response;
  // execute the POST request
  XFILE::CCurlFile curl;
  if (curl.Post(url2.Get(), data, response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    if (!response.empty())
      CLog::Log(LOGDEBUG, "CEmbyUtils::SetWatched %s", response.c_str());
#endif
  }
}

void CEmbyUtils::SetUnWatched(CFileItem &item)
{
  // DELETE to /Users/{UserId}/PlayedItems/{Id}
  std::string url = item.GetPath();
  if (URIUtils::IsStack(url))
    url = XFILE::CStackDirectory::GetFirstStackedFile(url);
  else
    url = URIUtils::GetParentPath(url);
  if (StringUtils::StartsWithNoCase(url, "emby://"))
    url = Base64::Decode(URIUtils::GetFileName(item.GetPath()));

  // need userId which only client knows
  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(url);
  if (!client || !client->GetPresence())
    return;

  // get the URL to updated the item's played state for this user ID
  CURL url2(url);
  url2.SetFileName("emby/Users/" + client->GetUserID() + "/PlayedItems/" + item.GetVideoInfoTag()->m_strServiceId);
  url2.SetOptions("");

  std::string data;
  std::string response;
  // execute the DELETE request
  XFILE::CCurlFile curl;
  if (curl.Delete(url2.Get(), data, response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    if (!response.empty())
      CLog::Log(LOGDEBUG, "CEmbyUtils::SetUnWatched %s", response.c_str());
#endif
  }
}

void CEmbyUtils::ReportProgress(CFileItem &item, double currentSeconds)
{
  // if we are Emby music, do not report
  if (item.IsAudio())
    return;

  // we get called from Application.cpp every 500ms
  if ((g_playbackState == MediaServicesPlayerState::stopped || g_progressSec <= 0 || g_progressSec > 30))
  {
    std::string status;
    if (g_playbackState == MediaServicesPlayerState::playing )
      status = "playing";
    else if (g_playbackState == MediaServicesPlayerState::paused )
      status = "paused";
    else if (g_playbackState == MediaServicesPlayerState::stopped)
      status = "stopped";

    if (!status.empty())
    {
      std::string url = item.GetPath();
      if (URIUtils::IsStack(url))
        url = XFILE::CStackDirectory::GetFirstStackedFile(url);
      else
      {
        CURL url1(item.GetPath());
        CURL url2(URIUtils::GetParentPath(url));
        CURL url3(url2.GetWithoutFilename());
        url3.SetProtocolOptions(url1.GetProtocolOptions());
        url = url3.Get();
      }
      if (StringUtils::StartsWithNoCase(url, "emby://"))
        url = Base64::Decode(URIUtils::GetFileName(item.GetPath()));

      /*
      # Postdata structure to send to Emby server
      url = "{server}/emby/Sessions/Playing"
      postdata = {
        'QueueableMediaTypes': "Video",
        'CanSeek': True,
        'ItemId': itemId,
        'MediaSourceId': itemId,
        'PlayMethod': playMethod,
        'VolumeLevel': volume,
        'PositionTicks': int(seekTime * 10000000),
        'IsMuted': muted
      }
      */

      CURL url4(item.GetPath());
      if (status == "playing")
      {
        if (g_progressSec < 0)
          // playback started
          url4.SetFileName("emby/Sessions/Playing");
        else
          url4.SetFileName("emby/Sessions/Playing/Progress");
      }
      else if (status == "stopped")
        url4.SetFileName("emby/Sessions/Playing/Stopped");

      std::string id = item.GetVideoInfoTag()->m_strServiceId;
      url4.SetOptions("");
      url4.SetOption("QueueableMediaTypes", "Video");
      url4.SetOption("CanSeek", "True");
      url4.SetOption("ItemId", id);
      url4.SetOption("MediaSourceId", id);
      url4.SetOption("PlayMethod", "DirectPlay");
      url4.SetOption("PositionTicks", StringUtils::Format("%llu", SecondsToTicks(currentSeconds)));
      url4.SetOption("IsMuted", "False");
      url4.SetOption("IsPaused", status == "paused" ? "True" : "False");

      std::string data;
      std::string response;
      // execute the POST request
      XFILE::CCurlFile curl;
      if (curl.Post(url4.Get(), data, response))
      {
#if defined(EMBY_DEBUG_VERBOSE)
        if (!response.empty())
          CLog::Log(LOGDEBUG, "CEmbyUtils::ReportProgress %s", response.c_str());
#endif
      }
      g_progressSec = 0;
    }
  }
  g_progressSec++;
}

void CEmbyUtils::SetPlayState(MediaServicesPlayerState state)
{
  g_progressSec = -1;
  g_playbackState = state;
}

bool CEmbyUtils::GetEmbyRecentlyAddedEpisodes(CFileItemList &items, const std::string url, int limit)
{
  static const std::string PropertyItemPath = "Path";
  static const std::string PropertyItemDateCreated = "DateCreated";
  static const std::string PropertyItemGenres = "Genres";
  static const std::string PropertyItemMediaStreams = "MediaStreams";
  static const std::string PropertyItemOverview = "Overview";
  static const std::string PropertyItemShortOverview = "ShortOverview";
  static const std::string PropertyItemPeople = "People";
  static const std::string PropertyItemSortName = "SortName";
  static const std::string PropertyItemOriginalTitle = "OriginalTitle";
  static const std::string PropertyItemProviderIds = "ProviderIds";
  static const std::string PropertyItemStudios = "Studios";
  static const std::string PropertyItemTaglines = "Taglines";
  static const std::string PropertyItemProductionLocations = "ProductionLocations";
  static const std::string PropertyItemTags = "Tags";
  static const std::string PropertyItemVoteCount = "VoteCount";
  
  static const std::vector<std::string> Fields = {
    PropertyItemDateCreated,
    PropertyItemGenres,
    PropertyItemMediaStreams,
    PropertyItemOverview,
    //    PropertyItemShortOverview,
    PropertyItemPath,
    //    PropertyItemPeople,
    //    PropertyItemProviderIds,
    //    PropertyItemSortName,
    //    PropertyItemOriginalTitle,
    //    PropertyItemStudios,
    //    PropertyItemTaglines,
    //    PropertyItemProductionLocations,
    //    PropertyItemTags,
    //    PropertyItemVoteCount,
  };
  
  CURL url2(url);
  
  url2.SetFileName(url2.GetFileName() + "/Latest");
  
  url2.SetOption("IncludeItemTypes", "Episode");
  url2.SetOption("Limit", StringUtils::Format("%i",limit));
  url2.SetOption("GroupItems", "False");
  url2.SetOption("LocationTypes", "FileSystem,Remote,Offline");
  url2.SetOption("Fields", StringUtils::Join(Fields, ","));
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");
  CVariant result = GetEmbyCVariant(url2.Get());
  
  std::map<std::string, CVariant> variantMap;
  variantMap["Items"] = result;
  result = CVariant(variantMap);

  bool rtn = GetVideoItems(items, url2, result, MediaTypeEpisode, false);
  return rtn;
}

bool CEmbyUtils::GetEmbyInProgressShows(CFileItemList &items, const std::string url, int limit)
{
  // SortBy=DatePlayed&SortOrder=Descending&Filters=IsResumable&Limit=5
  
  static const std::string PropertyItemPath = "Path";
  static const std::string PropertyItemDateCreated = "DateCreated";
  static const std::string PropertyItemGenres = "Genres";
  static const std::string PropertyItemMediaStreams = "MediaStreams";
  static const std::string PropertyItemOverview = "Overview";
  static const std::string PropertyItemShortOverview = "ShortOverview";
  static const std::string PropertyItemPeople = "People";
  static const std::string PropertyItemSortName = "SortName";
  static const std::string PropertyItemOriginalTitle = "OriginalTitle";
  static const std::string PropertyItemProviderIds = "ProviderIds";
  static const std::string PropertyItemStudios = "Studios";
  static const std::string PropertyItemTaglines = "Taglines";
  static const std::string PropertyItemProductionLocations = "ProductionLocations";
  static const std::string PropertyItemTags = "Tags";
  static const std::string PropertyItemVoteCount = "VoteCount";
  
  static const std::vector<std::string> Fields = {
    PropertyItemDateCreated,
    PropertyItemGenres,
    PropertyItemMediaStreams,
    PropertyItemOverview,
    //    PropertyItemShortOverview,
    PropertyItemPath,
    //    PropertyItemPeople,
    //    PropertyItemProviderIds,
    //    PropertyItemSortName,
    //    PropertyItemOriginalTitle,
    //    PropertyItemStudios,
    //    PropertyItemTaglines,
    //    PropertyItemProductionLocations,
    //    PropertyItemTags,
    //    PropertyItemVoteCount,
  };
  
  CURL url2(url);
  
  url2.SetOption("IncludeItemTypes", "Episode");
  url2.SetOption("SortBy", "DatePlayed");
  url2.SetOption("SortOrder", "Descending");
  url2.SetOption("Filters", "IsResumable");
  url2.SetOption("Limit", StringUtils::Format("%i",limit));
  url2.SetOption("Recursive", "true");
  url2.SetOption("LocationTypes", "FileSystem,Remote,Offline");
  url2.SetOption("Fields", StringUtils::Join(Fields, ","));
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");
  CVariant result = GetEmbyCVariant(url2.Get());
  
  bool rtn = GetVideoItems(items, url2, result, MediaTypeEpisode, false);
  return rtn;
  return false;
}

bool CEmbyUtils::GetEmbyRecentlyAddedMovies(CFileItemList &items, const std::string url, int limit)
{
  static const std::string PropertyItemPath = "Path";
  static const std::string PropertyItemDateCreated = "DateCreated";
  static const std::string PropertyItemGenres = "Genres";
  static const std::string PropertyItemMediaStreams = "MediaStreams";
  static const std::string PropertyItemOverview = "Overview";
  static const std::string PropertyItemShortOverview = "ShortOverview";
  static const std::string PropertyItemPeople = "People";
  static const std::string PropertyItemSortName = "SortName";
  static const std::string PropertyItemOriginalTitle = "OriginalTitle";
  static const std::string PropertyItemProviderIds = "ProviderIds";
  static const std::string PropertyItemStudios = "Studios";
  static const std::string PropertyItemTaglines = "Taglines";
  static const std::string PropertyItemProductionLocations = "ProductionLocations";
  static const std::string PropertyItemTags = "Tags";
  static const std::string PropertyItemVoteCount = "VoteCount";
  
  static const std::vector<std::string> Fields = {
    PropertyItemDateCreated,
    PropertyItemGenres,
    PropertyItemMediaStreams,
    PropertyItemOverview,
    //    PropertyItemShortOverview,
    PropertyItemPath,
    //    PropertyItemPeople,
    //    PropertyItemProviderIds,
    //    PropertyItemSortName,
    //    PropertyItemOriginalTitle,
    //    PropertyItemStudios,
    //    PropertyItemTaglines,
    //    PropertyItemProductionLocations,
    //    PropertyItemTags,
    //    PropertyItemVoteCount,
  };
  
  CURL url2(url);
  
  url2.SetFileName(url2.GetFileName() + "/Latest");
  
  url2.SetOption("IncludeItemTypes", "Movie");
  url2.SetOption("Limit", StringUtils::Format("%i",limit));
  url2.SetOption("GroupItems", "False");
  url2.SetOption("LocationTypes", "FileSystem,Remote,Offline");
  url2.SetOption("Fields", StringUtils::Join(Fields, ","));
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");
  CVariant result = GetEmbyCVariant(url2.Get());
  
  std::map<std::string, CVariant> variantMap;
  variantMap["Items"] = result;
  result = CVariant(variantMap);
  
  bool rtn = GetVideoItems(items, url2, result, MediaTypeMovie, false);
  return rtn;
}

bool CEmbyUtils::GetEmbyInProgressMovies(CFileItemList &items, const std::string url, int limit)
{
  
  // SortBy=DatePlayed&SortOrder=Descending&Filters=IsResumable&Limit=5
  
  static const std::string PropertyItemPath = "Path";
  static const std::string PropertyItemDateCreated = "DateCreated";
  static const std::string PropertyItemGenres = "Genres";
  static const std::string PropertyItemMediaStreams = "MediaStreams";
  static const std::string PropertyItemOverview = "Overview";
  static const std::string PropertyItemShortOverview = "ShortOverview";
  static const std::string PropertyItemPeople = "People";
  static const std::string PropertyItemSortName = "SortName";
  static const std::string PropertyItemOriginalTitle = "OriginalTitle";
  static const std::string PropertyItemProviderIds = "ProviderIds";
  static const std::string PropertyItemStudios = "Studios";
  static const std::string PropertyItemTaglines = "Taglines";
  static const std::string PropertyItemProductionLocations = "ProductionLocations";
  static const std::string PropertyItemTags = "Tags";
  static const std::string PropertyItemVoteCount = "VoteCount";
  
  static const std::vector<std::string> Fields = {
    PropertyItemDateCreated,
    PropertyItemGenres,
    PropertyItemMediaStreams,
    PropertyItemOverview,
    //    PropertyItemShortOverview,
    PropertyItemPath,
    //    PropertyItemPeople,
    //    PropertyItemProviderIds,
    //    PropertyItemSortName,
    //    PropertyItemOriginalTitle,
    //    PropertyItemStudios,
    //    PropertyItemTaglines,
    //    PropertyItemProductionLocations,
    //    PropertyItemTags,
    //    PropertyItemVoteCount,
  };
  
  CURL url2(url);
  
  url2.SetOption("IncludeItemTypes", "Movie");
  url2.SetOption("SortBy", "DatePlayed");
  url2.SetOption("SortOrder", "Descending");
  url2.SetOption("Filters", "IsResumable");
  url2.SetOption("Limit", StringUtils::Format("%i",limit));
  url2.SetOption("GroupItems", "False");
  url2.SetOption("LocationTypes", "FileSystem,Remote,Offline");
  url2.SetOption("Fields", StringUtils::Join(Fields, ","));
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");
  CVariant result = GetEmbyCVariant(url2.Get());
  
  bool rtn = GetVideoItems(items, url2, result, MediaTypeMovie, false);
  return rtn;
  return false;
}

bool CEmbyUtils::GetAllEmbyInProgress(CFileItemList &items, bool tvShow)
{
  bool rtn = false;
  
  if (CEmbyServices::GetInstance().HasClients())
  {
    CFileItemList embyItems;
    bool limitToLocal = CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_EMBYLIMITHOMETOLOCAL);
    //look through all emby clients and pull in progress for each library section
    std::vector<CEmbyClientPtr> clients;
    CEmbyServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      if (limitToLocal && !client->IsOwned())
        continue;
      EmbyViewContentVector contents;
      if (tvShow)
        contents = client->GetTvShowContent();
      else
        contents = client->GetMoviesContent();
      for (const auto &content : contents)
      {
        std::string userId = client->GetUserID();
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetOption("ParentId", content.id);
        curl.SetFileName("Users/" + userId + "/Items");
        
        if (tvShow)
          rtn = GetEmbyInProgressShows(embyItems, curl.Get(), 10);
        else
          rtn = GetEmbyInProgressMovies(embyItems, curl.Get(), 10);
        
        items.Append(embyItems);
        embyItems.ClearItems();
      }
    }
  }
  return rtn;
}

bool CEmbyUtils::GetAllEmbyRecentlyAddedMoviesAndShows(CFileItemList &items, bool tvShow)
{
  bool rtn = false;
  
  if (CEmbyServices::GetInstance().HasClients())
  {
    CFileItemList embyItems;
    bool limitToLocal = CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_EMBYLIMITHOMETOLOCAL);
    //look through all emby clients and pull recently added for each library section
    std::vector<CEmbyClientPtr> clients;
    CEmbyServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      if (limitToLocal && !client->IsOwned())
        continue;
      EmbyViewContentVector contents;
      if (tvShow)
        contents = client->GetTvShowContent();
      else
        contents = client->GetMoviesContent();
      for (const auto &content : contents)
      {
        std::string userId = client->GetUserID();
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetOption("ParentId", content.id);
        curl.SetFileName("Users/" + userId + "/Items");
        
        if (tvShow)
          rtn = GetEmbyRecentlyAddedEpisodes(embyItems, curl.Get(), 10);
        else
          rtn = GetEmbyRecentlyAddedMovies(embyItems, curl.Get(), 10);
        
        items.Append(embyItems);
        embyItems.ClearItems();
      }
    }
  }
  return rtn;
}

CFileItemPtr ParseVideo(const CEmbyClient *client, const CVariant &object)
{
  return nullptr;
}

CFileItemPtr ParseMusic(const CEmbyClient *client, const CVariant &object)
{
  return nullptr;
}

CFileItemPtr CEmbyUtils::ToFileItemPtr(const CEmbyClient *client, const CVariant &object)
{
  if (object.isNull() || !object.isObject() || !object.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ToFileItemPtr cvariant is empty");
    return nullptr;
  }

  const auto& items = object["Items"];
  //int totalRecordCount = object["TotalRecordCount"].asInteger();
  for (auto itemsIt = items.begin_array(); itemsIt != items.end_array(); ++itemsIt)
  {
    const auto item = *itemsIt;
    if (!item.isMember("Id"))
      continue;

    std::string mediaType = item["MediaType"].asString();
    if (mediaType == "Video")
      return ParseVideo(client, item);
    else if (mediaType == "Music")
      return ParseMusic(client, item);
  }

  return nullptr;
}


  // Emby Movie/TV
bool CEmbyUtils::GetEmbyMovies(CFileItemList &items, std::string url, std::string filter)
{
  static const std::string PropertyItemPath = "Path";
  static const std::string PropertyItemDateCreated = "DateCreated";
  static const std::string PropertyItemGenres = "Genres";
  static const std::string PropertyItemMediaStreams = "MediaStreams";
  static const std::string PropertyItemOverview = "Overview";
  static const std::string PropertyItemShortOverview = "ShortOverview";
  static const std::string PropertyItemPeople = "People";
  static const std::string PropertyItemSortName = "SortName";
  static const std::string PropertyItemOriginalTitle = "OriginalTitle";
  static const std::string PropertyItemProviderIds = "ProviderIds";
  static const std::string PropertyItemStudios = "Studios";
  static const std::string PropertyItemTaglines = "Taglines";
  static const std::string PropertyItemProductionLocations = "ProductionLocations";
  static const std::string PropertyItemTags = "Tags";
  static const std::string PropertyItemVoteCount = "VoteCount";

  static const std::vector<std::string> Fields = {
    PropertyItemDateCreated,
    PropertyItemGenres,
    PropertyItemMediaStreams,
    PropertyItemOverview,
    PropertyItemShortOverview,
    PropertyItemPath,
//    PropertyItemPeople,
//    PropertyItemProviderIds,
//    PropertyItemSortName,
//    PropertyItemOriginalTitle,
//    PropertyItemStudios,
    PropertyItemTaglines,
//    PropertyItemProductionLocations,
//    PropertyItemTags,
//    PropertyItemVoteCount,
  };

  CURL url2(url);

  const CVariant resultObject = GetEmbyCVariant(url2.Get());

  std::vector<std::string> iDS;
  const auto& objectItems = resultObject["Items"];
  for (auto objectItemIt = objectItems.begin_array(); objectItemIt != objectItems.end_array(); ++objectItemIt)
  {
    const auto item = *objectItemIt;
    iDS.push_back(item["Id"].asString());
  }

  std::string testIDs = StringUtils::Join(iDS, ",");
  url2.SetOption("Ids", testIDs);
  url2.SetOption("Fields", StringUtils::Join(Fields, ","));
  url2.SetOption("ExcludeLocationTypes", "Virtual,Offline");

  const CVariant result = GetEmbyCVariant(url2.Get());

  bool rtn = GetVideoItems(items, url2, result, MediaTypeMovie, false);
  return rtn;
}

bool CEmbyUtils::GetEmbyTvshows(CFileItemList &items, std::string url)
{
  static const std::string PropertyItemPath = "Path";
  static const std::string PropertyItemDateCreated = "DateCreated";
  static const std::string PropertyItemGenres = "Genres";
  static const std::string PropertyItemMediaStreams = "MediaStreams";
  static const std::string PropertyItemOverview = "Overview";
  static const std::string PropertyItemShortOverview = "ShortOverview";
  static const std::string PropertyItemPeople = "People";
  static const std::string PropertyItemSortName = "SortName";
  static const std::string PropertyItemOriginalTitle = "OriginalTitle";
  static const std::string PropertyItemProviderIds = "ProviderIds";
  static const std::string PropertyItemStudios = "Studios";
  static const std::string PropertyItemTaglines = "Taglines";
  static const std::string PropertyItemProductionLocations = "ProductionLocations";
  static const std::string PropertyItemTags = "Tags";
  static const std::string PropertyItemVoteCount = "VoteCount";
  static const std::string PropertyItemRecursiveItemCount = "RecursiveItemCount";

  static const std::vector<std::string> Fields = {
    PropertyItemDateCreated,
    PropertyItemGenres,
    PropertyItemMediaStreams,
    PropertyItemOverview,
    PropertyItemShortOverview,
    PropertyItemPath,
//    PropertyItemPeople,
//    PropertyItemProviderIds,
//    PropertyItemSortName,
//    PropertyItemOriginalTitle,
//    PropertyItemStudios,
    PropertyItemTaglines,
//    PropertyItemProductionLocations,
//    PropertyItemTags,
//    PropertyItemVoteCount,
    PropertyItemRecursiveItemCount
  };

  bool rtn = false;

  CURL url2(url);
  url2.SetOption("IncludeItemTypes", "Series");
  url2.SetOption("LocationTypes", "FileSystem,Remote,Offline");
  url2.SetOption("Fields", StringUtils::Join(Fields, ","));
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");


 /*
  
   params = {
   
   'ParentId': parentid,
   'ArtistIds': artist_id,
   'IncludeItemTypes': itemtype,
   'LocationTypes': "FileSystem,Remote,Offline",
   'CollapseBoxSetItems': False,
   'IsVirtualUnaired': False,
   'IsMissing': False,
   'Recursive': True,
   'Limit': 1
   }
   
  */
  
  const CVariant object = GetEmbyCVariant(url2.Get());

  if (!object.isNull() || object.isObject() || object.isMember("Items"))
  {
    const auto& objectItems = object["Items"];
    for (auto objectItemIt = objectItems.begin_array(); objectItemIt != objectItems.end_array(); ++objectItemIt)
    {
      const auto item = *objectItemIt;
      rtn = true;

      std::string fanart;
      std::string value;
      // clear url options
      CURL url2(url);
      url2.SetOption("ParentId", item["Id"].asString());
 //     url2.SetOptions("");

      CFileItemPtr newItem(new CFileItem());
      // set m_bIsFolder to true to indicate we are tvshow list
      newItem->m_bIsFolder = true;

      std::string title = item["Name"].asString();
      newItem->SetLabel(title);

      CDateTime premiereDate;
      premiereDate.SetFromW3CDateTime(item["PremiereDate"].asString());
      newItem->m_dateTime = premiereDate;

     // url2.SetFileName("Users/" + item["Id"].asString() + "/Items");
      newItem->SetPath("emby://tvshows/shows/" + Base64::Encode(url2.Get()));

      url2.SetFileName("Items/" + item["Id"].asString() + "/Images/Primary");
      newItem->SetArt("thumb", url2.Get());
      newItem->SetIconImage(url2.Get());
      url2.SetFileName("Items/" + item["Id"].asString() + "/Images/Banner");
      newItem->SetArt("banner", url2.Get());
      url2.SetFileName("Items/" + item["Id"].asString() + "/Images/Backdrop");
      newItem->SetArt("fanart", url2.Get());

      newItem->GetVideoInfoTag()->m_playCount = static_cast<int>(item["UserData"]["PlayCount"].asInteger());
      newItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, item["UserData"]["Played"].asBoolean());

      newItem->GetVideoInfoTag()->m_strTitle = title;
      newItem->GetVideoInfoTag()->m_strStatus = item["Status"].asString();

      newItem->GetVideoInfoTag()->m_type = MediaTypeMovie;
      newItem->GetVideoInfoTag()->m_strFileNameAndPath = newItem->GetPath();
      newItem->GetVideoInfoTag()->m_strServiceId = item["Id"].asString();
      newItem->GetVideoInfoTag()->SetSortTitle(item["SortName"].asString());
      newItem->GetVideoInfoTag()->SetOriginalTitle(item["OriginalTitle"].asString());
      newItem->SetProperty("EmbySeriesID", item["SeriesId"].asString());
      //newItem->SetProperty("EmbyShowKey", XMLUtils::GetAttribute(rootXmlNode, "grandparentRatingKey"));
      newItem->GetVideoInfoTag()->SetPlot(item["Overview"].asString());
      newItem->GetVideoInfoTag()->SetPlotOutline(item["ShortOverview"].asString());
      newItem->GetVideoInfoTag()->m_firstAired = premiereDate;
      newItem->GetVideoInfoTag()->SetPremiered(premiereDate);
      newItem->GetVideoInfoTag()->m_dateAdded.SetFromW3CDateTime(item["DateCreated"].asString());
      newItem->GetVideoInfoTag()->SetYear(static_cast<int>(item["ProductionYear"].asInteger()));
      newItem->GetVideoInfoTag()->SetRating(item["CommunityRating"].asFloat(), static_cast<int>(item["VoteCount"].asInteger()), "", true);
      newItem->GetVideoInfoTag()->m_strMPAARating = item["OfficialRating"].asString();

      int totalEpisodes = item["RecursiveItemCount"].asInteger() - item["ChildCount"].asInteger();
      int unWatchedEpisodes = static_cast<int>(item["UserData"]["UnplayedItemCount"].asInteger());
      int watchedEpisodes = totalEpisodes - unWatchedEpisodes;
      int iSeasons        = static_cast<int>(item["ChildCount"].asInteger());
      
      newItem->GetVideoInfoTag()->m_iSeason = iSeasons;
      newItem->GetVideoInfoTag()->m_iEpisode = totalEpisodes;
      newItem->GetVideoInfoTag()->m_playCount = (int)watchedEpisodes >= newItem->GetVideoInfoTag()->m_iEpisode;

      newItem->SetProperty("totalseasons", iSeasons);
      newItem->SetProperty("totalepisodes", newItem->GetVideoInfoTag()->m_iEpisode);
      newItem->SetProperty("numepisodes",   newItem->GetVideoInfoTag()->m_iEpisode);
      newItem->SetProperty("watchedepisodes", watchedEpisodes);
      newItem->SetProperty("unwatchedepisodes", unWatchedEpisodes);

      GetVideoDetails(*newItem, item);
      SetEmbyItemProperties(*newItem);
      items.Add(newItem);
    }
    // this is needed to display movies/episodes properly ... dont ask
    // good thing it didnt take 2 days to figure it out
    items.SetProperty("library.filter", "true");
    SetEmbyItemProperties(items);
  }
  return rtn;
}

bool CEmbyUtils::GetEmbySeasons(CFileItemList &items, const std::string url)
{
  // "Shows/\(query.seriesId)/Seasons"


  bool rtn = false;
  
  CURL url2(url);
  url2.SetOption("IncludeItemTypes", "Seasons");
  url2.SetOption("LocationTypes", "FileSystem,Remote,Offline,Virtual");
  url2.SetOption("Fields", "Etag,RecursiveItemCount");
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");

  const CVariant object = GetEmbyCVariant(url2.Get());
  std::string seriesName;
  if (!object.isNull() || object.isObject() || object.isMember("Items"))
  {
    CURL url3(url);
    std::string seriesID = url3.GetOption("ParentId");
    url3.SetOptions("");
    url3.SetOption("Ids", seriesID);
    url3.SetOption("Fields", "Overview,Genres");
    url3.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");
    const CVariant seriesObject = CEmbyUtils::GetEmbyCVariant(url3.Get());
    const auto& seriesItem = seriesObject["Items"][0];
    const auto& objectItems = object["Items"];
    for (auto objectItemIt = objectItems.begin_array(); objectItemIt != objectItems.end_array(); ++objectItemIt)
    {
      
      const auto item = *objectItemIt;
      rtn = true;
      
      std::string fanart;
      std::string value;
      // clear url options
      CURL url2(url);
      url2.SetOptions("");
      url2.SetOption("ParentId", item["Id"].asString());
      
      CFileItemPtr newItem(new CFileItem());
      // set m_bIsFolder to true to indicate we are tvshow list
      newItem->m_bIsFolder = true;
      
      //CURL url1(url);
      //url1.SetFileName("/Users/" + item["Id"].asString() + "/Items");
      newItem->SetLabel(item["Name"].asString());
      newItem->SetPath("emby://tvshows/seasons/" + Base64::Encode(url2.Get()));
      newItem->GetVideoInfoTag()->m_strServiceId = item["Id"].asString();
      newItem->GetVideoInfoTag()->m_strServiceFile = item["Path"].asString();
      
      url2.SetFileName("Items/" + item["ImageTags"]["Primary"].asString() + "/Images/Primary");
      newItem->SetArt("thumb", url2.Get());
      newItem->SetIconImage(url2.Get());
      url2.SetFileName("Items/" + item["SeriesId"].asString() + "/Images/Banner");
      newItem->SetArt("banner", url2.Get());
      url2.SetFileName("Items/" + item["SeriesId"].asString() + "/Images/Backdrop");
      newItem->SetArt("fanart", url2.Get());
      
      newItem->GetVideoInfoTag()->m_type = MediaTypeTvShow;
      newItem->GetVideoInfoTag()->m_strTitle = item["Name"].asString();
      // we get these from rootXmlNode, where all show info is
      seriesName = item["SeriesName"].asString();
      newItem->GetVideoInfoTag()->m_strShowTitle = item["SeriesName"].asString();
      newItem->GetVideoInfoTag()->SetPlotOutline(seriesItem["Overview"].asString());
      newItem->GetVideoInfoTag()->SetPlot(seriesItem["Overview"].asString());
      newItem->GetVideoInfoTag()->SetYear(seriesItem["ProductionYear"].asInteger());
      std::vector<std::string> genres;
      const auto& streams = seriesItem["Genres"];
      for (auto streamIt = streams.begin_array(); streamIt != streams.end_array(); ++streamIt)
      {
        const auto stream = *streamIt;
        genres.push_back(stream.asString());
      }
      newItem->GetVideoInfoTag()->SetGenre(genres);
      newItem->SetProperty("EmbySeriesID", item["SeriesId"].asString());

      int totalEpisodes = item["RecursiveItemCount"].asInteger();
      int unWatchedEpisodes = item["UserData"]["UnplayedItemCount"].asInteger();
      int watchedEpisodes = totalEpisodes - unWatchedEpisodes;
      int iSeason = item["IndexNumber"].asInteger();
      newItem->GetVideoInfoTag()->m_iSeason = iSeason;
      newItem->GetVideoInfoTag()->m_iEpisode = totalEpisodes;
      newItem->GetVideoInfoTag()->m_playCount = item["UserData"]["PlayCount"].asInteger();
      
      newItem->SetProperty("totalepisodes", totalEpisodes);
      newItem->SetProperty("numepisodes", totalEpisodes);
      newItem->SetProperty("watchedepisodes", watchedEpisodes);
      newItem->SetProperty("unwatchedepisodes", unWatchedEpisodes);
      
      newItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, item["UserData"]["Played"].asBoolean());
      
      items.Add(newItem);
    }
  }
  items.SetLabel(seriesName);
  items.SetProperty("library.filter", "true");
  return rtn;
}

bool CEmbyUtils::GetEmbyEpisodes(CFileItemList &items, const std::string url)
{
  static const std::string PropertyItemPath = "Path";
  static const std::string PropertyItemDateCreated = "DateCreated";
  static const std::string PropertyItemGenres = "Genres";
  static const std::string PropertyItemMediaStreams = "MediaStreams";
  static const std::string PropertyItemOverview = "Overview";
  static const std::string PropertyItemShortOverview = "ShortOverview";
  static const std::string PropertyItemPeople = "People";
  static const std::string PropertyItemSortName = "SortName";
  static const std::string PropertyItemOriginalTitle = "OriginalTitle";
  static const std::string PropertyItemProviderIds = "ProviderIds";
  static const std::string PropertyItemStudios = "Studios";
  static const std::string PropertyItemTaglines = "Taglines";
  static const std::string PropertyItemProductionLocations = "ProductionLocations";
  static const std::string PropertyItemTags = "Tags";
  static const std::string PropertyItemVoteCount = "VoteCount";
  
  static const std::vector<std::string> Fields = {
    PropertyItemDateCreated,
    PropertyItemGenres,
    PropertyItemMediaStreams,
    PropertyItemOverview,
    //    PropertyItemShortOverview,
    PropertyItemPath,
    //    PropertyItemPeople,
    //    PropertyItemProviderIds,
    //    PropertyItemSortName,
    //    PropertyItemOriginalTitle,
    //    PropertyItemStudios,
    //    PropertyItemTaglines,
    //    PropertyItemProductionLocations,
    //    PropertyItemTags,
    //    PropertyItemVoteCount,
  };
  
  CURL url2(url);

  url2.SetOption("IncludeItemTypes", "Episode");
  url2.SetOption("LocationTypes", "FileSystem,Remote,Offline");
  url2.SetOption("Fields", StringUtils::Join(Fields, ","));
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");
  const CVariant result = GetEmbyCVariant(url2.Get());
  
  bool rtn = GetVideoItems(items, url2, result, MediaTypeEpisode, false);
  return rtn;
}

bool CEmbyUtils::GetEmbyTVFilter(CFileItemList &items, std::string url, std::string parentPath, std::string filter)
{
  /*
   std::string filter = "year";
   if (path == "genres")
   filter = "genre";
   else if (path == "actors")
   filter = "actor";
   else if (path == "directors")
   filter = "director";
   else if (path == "sets")
   filter = "collection";
   else if (path == "countries")
   filter = "country";
   else if (path == "studios")
   filter = "studio";
   
   http://192.168.1.200:8096/emby/Genres?SortBy=SortName&SortOrder=Ascending&IncludeItemTypes=Movie&Recursive=true&EnableTotalRecordCount=false&ParentId=f137a2dd21bbc1b99aa5c0f6bf02a805&userId=cf28f6d51dd54c63a27fed6600c5b6cb
   */
  
  std::string userID = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYUSERID);
  
  CURL url2(url);

  url2.SetFileName("emby/"+ filter);
  url2.SetOption("IncludeItemTypes", "Series");

  
  url2.SetOption("LocationTypes", "FileSystem,Remote,Offline");
  url2.SetOption("Fields", "Etag");
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");
  const CVariant result = GetEmbyCVariant(url2.Get());
  
  
  bool rtn = false;
  
  
  if (result.isNull() || !result.isObject() || !result.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::GetEmbyMovieFilter invalid response from %s", url2.GetRedacted().c_str());
    return false;
  }
  
  const auto& objectItems = result["Items"];
  for (auto objectItemIt = objectItems.begin_array(); objectItemIt != objectItems.end_array(); ++objectItemIt)
  {
    rtn = true;
    const auto item = *objectItemIt;
    CFileItemPtr newItem(new CFileItem());
    std::string title = item["Name"].asString();
    std::string key = item["Id"].asString();
    newItem->m_bIsFolder = true;
    newItem->m_bIsShareOrDrive = false;
    
    if (filter == "Genres")
      url2.SetOption("GenreIds", key);
    else if (filter == "Years")
      url2.SetOption("Years", title);
    
    
    url2.SetFileName("Users/" + userID +"/Items");
    newItem->SetPath(parentPath + Base64::Encode(url2.Get()));
    newItem->SetLabel(title);
    newItem->SetProperty("SkipLocalArt", true);
    items.Add(newItem);
  }
  return rtn;
}

bool CEmbyUtils::GetEmbyMovieFilter(CFileItemList &items, std::string url, std::string parentPath, std::string filter)
{
  /*
  std::string filter = "year";
  if (path == "genres")
    filter = "genre";
  else if (path == "actors")
    filter = "actor";
  else if (path == "directors")
    filter = "director";
  else if (path == "sets")
    filter = "collection";
  else if (path == "countries")
    filter = "country";
  else if (path == "studios")
    filter = "studio";
   
   http://192.168.1.200:8096/emby/Genres?SortBy=SortName&SortOrder=Ascending&IncludeItemTypes=Movie&Recursive=true&EnableTotalRecordCount=false&ParentId=f137a2dd21bbc1b99aa5c0f6bf02a805&userId=cf28f6d51dd54c63a27fed6600c5b6cb
  */
  
  std::string userID = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYUSERID);
  
  CURL url2(url);
  if (filter != "Collections")
  {
    url2.SetFileName("emby/"+ filter);
    url2.SetOption("IncludeItemTypes", "Movie");
  }
  else
  {
    url2.SetOption("IncludeItemTypes", "BoxSet");
    url2.SetOption("Recursive", "true");
    url2.SetOption("ParentId", "");
  }
  
  
  url2.SetOption("LocationTypes", "FileSystem,Remote,Offline");
  url2.SetOption("Fields", "Etag");
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");
  const CVariant result = GetEmbyCVariant(url2.Get());
  
  
  bool rtn = false;
  
  
  if (result.isNull() || !result.isObject() || !result.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::GetEmbyMovieFilter invalid response from %s", url2.GetRedacted().c_str());
    return false;
  }

  const auto& objectItems = result["Items"];
  for (auto objectItemIt = objectItems.begin_array(); objectItemIt != objectItems.end_array(); ++objectItemIt)
  {
    rtn = true;
    const auto item = *objectItemIt;
    CFileItemPtr newItem(new CFileItem());
    std::string title = item["Name"].asString();
    std::string key = item["Id"].asString();
    newItem->m_bIsFolder = true;
    newItem->m_bIsShareOrDrive = false;
    
    if (filter == "Genres")
      url2.SetOption("GenreIds", key);
    else if (filter == "Years")
      url2.SetOption("Years", title);
    else if (filter == "Collections")
      url2.SetOption("ParentId", key);

    
    url2.SetFileName("Users/" + userID +"/Items");
    newItem->SetPath(parentPath + Base64::Encode(url2.Get()));
    newItem->SetLabel(title);
    newItem->SetProperty("SkipLocalArt", true);
    items.Add(newItem);
  }
  return rtn;
}

bool CEmbyUtils::GetItemSubtiles(CFileItem &item)
{
  return false;
}

bool CEmbyUtils::GetMoreItemInfo(CFileItem &item)
{
  static const std::string PropertyItemPath = "Path";
  static const std::string PropertyItemDateCreated = "DateCreated";
  static const std::string PropertyItemGenres = "Genres";
  static const std::string PropertyItemMediaStreams = "MediaStreams";
  static const std::string PropertyItemOverview = "Overview";
  static const std::string PropertyItemShortOverview = "ShortOverview";
  static const std::string PropertyItemPeople = "People";
  static const std::string PropertyItemSortName = "SortName";
  static const std::string PropertyItemOriginalTitle = "OriginalTitle";
  static const std::string PropertyItemProviderIds = "ProviderIds";
  static const std::string PropertyItemStudios = "Studios";
  static const std::string PropertyItemTaglines = "Taglines";
  static const std::string PropertyItemProductionLocations = "ProductionLocations";
  static const std::string PropertyItemTags = "Tags";
  static const std::string PropertyItemVoteCount = "VoteCount";
  
  static const std::vector<std::string> Fields = {
    //PropertyItemDateCreated,
    PropertyItemGenres,
    //PropertyItemMediaStreams,
    //PropertyItemOverview,
    //    PropertyItemShortOverview,
    //PropertyItemPath,
    PropertyItemPeople,
    //    PropertyItemProviderIds,
    //    PropertyItemSortName,
    //    PropertyItemOriginalTitle,
    //    PropertyItemStudios,
    //    PropertyItemTaglines,
    //    PropertyItemProductionLocations,
    //    PropertyItemTags,
    //    PropertyItemVoteCount,
  };
  
  std::string url = URIUtils::GetParentPath(item.GetPath());
  if (StringUtils::StartsWithNoCase(url, "emby://"))
    url = Base64::Decode(URIUtils::GetFileName(item.GetPath()));
  
  CURL url2(url);
  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(url2.Get());
  if (!client || !client->GetPresence())
    return false;
  
  std::string itemId;
  if (item.HasProperty("EmbySeriesID") && !item.GetProperty("EmbySeriesID").asString().empty())
    itemId = item.GetProperty("EmbySeriesID").asString();
  else
    itemId = item.GetVideoInfoTag()->m_strServiceId;
  
  url2.SetFileName("emby/Users/" + client->GetUserID() + "/Items");
  url2.SetOptions("");
  url2.SetOption("Fields", StringUtils::Join(Fields, ","));
  url2.SetOption("IDs", itemId);
  url2.SetProtocolOptions(url2.GetProtocolOptions() + "&format=json");
  const CVariant result = GetEmbyCVariant(url2.Get());
  
  GetVideoDetails(item,result["Items"][0]);
  return true;
}

bool CEmbyUtils::GetMoreResolutions(CFileItem &item)
{
  return true;
}

bool CEmbyUtils::GetURL(CFileItem &item)
{
  return true;
}

bool CEmbyUtils::SearchEmby(CFileItemList &items, std::string strSearchString)
{
  return false;
}


  // Emby Music
bool CEmbyUtils::GetEmbyArtistsOrAlbum(CFileItemList &items, std::string url, bool album)
{
  return false;
}

bool CEmbyUtils::GetEmbySongs(CFileItemList &items, std::string url)
{
  return false;
}

bool CEmbyUtils::ShowMusicInfo(CFileItem item)
{
  return false;
}

bool CEmbyUtils::GetEmbyRecentlyAddedAlbums(CFileItemList &items,int limit)
{
  return false;
}

bool CEmbyUtils::GetEmbyAlbumSongs(CFileItem item, CFileItemList &items)
{
  return false;
}

bool CEmbyUtils::GetEmbyMediaTotals(MediaServicesMediaCount &totals)
{
  return false;
}


void CEmbyUtils::ReportToServer(std::string url, std::string filename)
{
}

bool CEmbyUtils::GetVideoItems(CFileItemList &items, CURL url, const CVariant &object, std::string type, bool formatLabel, int season)
{
  bool rtn = false;
  if (object.isNull() || !object.isObject() || !object.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::GetVideoItems invalid response from %s", url.GetRedacted().c_str());
    return false;
  }

  const auto& objectItems = object["Items"];
  for (auto objectItemIt = objectItems.begin_array(); objectItemIt != objectItems.end_array(); ++objectItemIt)
  {
    const auto item = *objectItemIt;
    rtn = true;
    CFileItemPtr newItem(new CFileItem());

    std::string fanart;
    std::string value;
    // clear url options
    CURL url2(url);
    url2.SetOptions("");
    // if we have season means we are listing episodes, we need to get the fanart from rootXmlNode.
    // movies has it in videoNode
    if (item.isMember("ParentIndexNumber"))
    {
      url2.SetFileName("Items/" + item["Id"].asString() + "/Images/Primary");
      newItem->SetArt("thumb", url2.Get());
      newItem->SetIconImage(url2.Get());
      url2.SetFileName("Items/" + item["Id"].asString() + "/Images/Backdrop");
      fanart = url2.Get();
      newItem->GetVideoInfoTag()->m_strShowTitle = item["SeriesName"].asString();
      newItem->GetVideoInfoTag()->m_iSeason = item["ParentIndexNumber"].asInteger();
      newItem->GetVideoInfoTag()->m_iEpisode = item["IndexNumber"].asInteger();
      items.SetLabel(item["SeasonName"].asString());
      newItem->SetProperty("EmbySeriesID", item["SeriesId"].asString());
    }
 /*
    else if (((TiXmlElement*) videoNode)->Attribute("grandparentTitle")) // only recently added episodes have this
    {
      fanart = XMLUtils::GetAttribute(videoNode, "art");
      videoInfo->m_strShowTitle = XMLUtils::GetAttribute(videoNode, "grandparentTitle");
      videoInfo->m_iSeason = atoi(XMLUtils::GetAttribute(videoNode, "parentIndex").c_str());
      videoInfo->m_iEpisode = atoi(XMLUtils::GetAttribute(videoNode, "index").c_str());

      value = XMLUtils::GetAttribute(videoNode, "thumb");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url.SetFileName(value);
      newItem->SetArt("thumb", url.Get());

      value = XMLUtils::GetAttribute(videoNode, "parentThumb");
      if (value.empty())
        value = XMLUtils::GetAttribute(videoNode, "grandparentThumb");
      if (!value.empty() && (value[0] == '/'))
        StringUtils::TrimLeft(value, "/");
      url.SetFileName(value);
      newItem->SetArt("tvshow.poster", url.Get());
      newItem->SetArt("tvshow.thumb", url.Get());
      newItem->SetIconImage(url.Get());
      std::string seasonEpisode = StringUtils::Format("S%02iE%02i", plexItem->GetVideoInfoTag()->m_iSeason, plexItem->GetVideoInfoTag()->m_iEpisode);
      newItem->SetProperty("SeasonEpisode", seasonEpisode);
    }
*/
    else
    {
      url2.SetFileName("Items/" + item["Id"].asString() + "/Images/Primary");
      newItem->SetArt("thumb", url2.Get());
      newItem->SetIconImage(url2.Get());
      url2.SetFileName("Items/" + item["Id"].asString() + "/Images/Backdrop");
      fanart = url2.Get();
    }

    std::string title = item["Name"].asString();
    newItem->SetLabel(title);
    newItem->m_dateTime.SetFromW3CDateTime(item["PremiereDate"].asString());

    
    newItem->SetArt("fanart", fanart);

    newItem->GetVideoInfoTag()->m_strTitle = title;
    newItem->GetVideoInfoTag()->SetSortTitle(item["SortName"].asString());
    newItem->GetVideoInfoTag()->SetOriginalTitle(item["OriginalTitle"].asString());

    url2.SetFileName("Videos/" + item["Id"].asString() +"/stream?static=true");
    newItem->SetPath(url2.Get());
    newItem->GetVideoInfoTag()->m_strFileNameAndPath = url2.Get();
    newItem->GetVideoInfoTag()->m_strServiceId = item["Id"].asString();
    newItem->GetVideoInfoTag()->m_strServiceFile = item["Path"].asString();

    //newItem->SetProperty("EmbyShowKey", XMLUtils::GetAttribute(rootXmlNode, "grandparentRatingKey"));
    newItem->GetVideoInfoTag()->m_type = type;
    newItem->GetVideoInfoTag()->SetPlot(item["Overview"].asString());
    newItem->GetVideoInfoTag()->SetPlotOutline(item["ShortOverview"].asString());

    CDateTime premiereDate;
    premiereDate.SetFromW3CDateTime(item["PremiereDate"].asString());
    newItem->GetVideoInfoTag()->m_firstAired = premiereDate;
    newItem->GetVideoInfoTag()->SetPremiered(premiereDate);
    newItem->GetVideoInfoTag()->m_dateAdded.SetFromW3CDateTime(item["DateCreated"].asString());

    newItem->GetVideoInfoTag()->SetYear(static_cast<int>(item["ProductionYear"].asInteger()));
    newItem->GetVideoInfoTag()->SetRating(item["CommunityRating"].asFloat(), static_cast<int>(item["VoteCount"].asInteger()), "", true);
    newItem->GetVideoInfoTag()->m_strMPAARating = item["OfficialRating"].asString();

    GetVideoDetails(*newItem, item);

    newItem->GetVideoInfoTag()->m_duration = static_cast<int>(TicksToSeconds(item["RunTimeTicks"].asInteger()));
    newItem->GetVideoInfoTag()->m_resumePoint.totalTimeInSeconds = newItem->GetVideoInfoTag()->m_duration;
    newItem->GetVideoInfoTag()->m_playCount = static_cast<int>(item["UserData"]["PlayCount"].asInteger());
    newItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, item["UserData"]["Played"].asBoolean());
    newItem->GetVideoInfoTag()->m_lastPlayed.SetFromW3CDateTime(item["UserData"]["LastPlayedDate"].asString());
    newItem->GetVideoInfoTag()->m_resumePoint.timeInSeconds = static_cast<int>(TicksToSeconds(item["UserData"]["PlaybackPositionTicks"].asUnsignedInteger()));

    GetMediaDetals(*newItem, item);

    if (formatLabel)
    {
      CLabelFormatter formatter("%H. %T", "");
      formatter.FormatLabel(newItem.get());
      newItem->SetLabelPreformated(true);
    }
    SetEmbyItemProperties(*newItem);
    items.Add(newItem);
  }
  // this is needed to display movies/episodes properly ... dont ask
  // good thing it didnt take 2 days to figure it out
  items.SetProperty("library.filter", "true");
  SetEmbyItemProperties(items);

  return rtn;
}

void CEmbyUtils::GetVideoDetails(CFileItem &fileitem, const CVariant &cvariant)
{
  if (cvariant.isMember("Genres"))
  {
    // get all genres
    std::vector<std::string> genres;
    const auto& streams = cvariant["Genres"];
    for (auto streamIt = streams.begin_array(); streamIt != streams.end_array(); ++streamIt)
    {
      const auto stream = *streamIt;
      genres.push_back(stream.asString());
    }
    fileitem.GetVideoInfoTag()->SetGenre(genres);
  }
  
  if (cvariant.isMember("People"))
  {
    std::vector< SActorInfo > roles;
    std::vector<std::string> directors;
    const auto& peeps = cvariant["People"];
    for (auto peepsIt = peeps.begin_array(); peepsIt != peeps.end_array(); ++peepsIt)
    {
      const auto peep = *peepsIt;
      if (peep["Type"].asString() == "Director")
        directors.push_back(peep["Name"].asString());
      else if (peep["Type"].asString() == "Actor")
      {
        SActorInfo role;
        role.strName = peep["Name"].asString();
        role.strRole = peep["Role"].asString();
        // Items/acae838242b43ad786c2cae52ff412d2/Images/Primary
        std::string urlStr = URIUtils::GetParentPath(fileitem.GetPath());
        if (StringUtils::StartsWithNoCase(urlStr, "emby://"))
          urlStr = Base64::Decode(URIUtils::GetFileName(fileitem.GetPath()));
        CURL url(urlStr);
        url.SetFileName("Items/" + peep["Id"].asString() + "/Images/Primary");
        role.thumb = url.Get();
        roles.push_back(role);
      }
    }
    
    fileitem.GetVideoInfoTag()->m_cast = roles;
    fileitem.GetVideoInfoTag()->SetDirector(directors);
  }
}

void CEmbyUtils::GetMusicDetails(CFileItem &fileitem, const CVariant &cvariant)
{
}

void CEmbyUtils::GetMediaDetals(CFileItem &fileitem, const CVariant &cvariant, std::string id)
{
  if (cvariant.isMember("MediaStreams") && cvariant["MediaStreams"].isArray())
  {
    CStreamDetails streamDetail;
    const auto& streams = cvariant["MediaStreams"];
    for (auto streamIt = streams.begin_array(); streamIt != streams.end_array(); ++streamIt)
    {
      const auto stream = *streamIt;
      const auto streamType = stream["Type"].asString();
      if (streamType == "Video")
      {
        CStreamDetailVideo* videoStream = new CStreamDetailVideo();
        videoStream->m_strCodec = stream["Codec"].asString();
        videoStream->m_strLanguage = stream["Language"].asString();
        videoStream->m_iWidth = static_cast<int>(stream["Width"].asInteger());
        videoStream->m_iHeight = static_cast<int>(stream["Height"].asInteger());
        videoStream->m_iDuration = fileitem.GetVideoInfoTag()->m_duration;

        streamDetail.AddStream(videoStream);
      }
      else if (streamType == "Audio")
      {
        CStreamDetailAudio* audioStream = new CStreamDetailAudio();
        audioStream->m_strCodec = stream["Codec"].asString();
        audioStream->m_strLanguage = stream["Language"].asString();
        audioStream->m_iChannels = static_cast<int>(stream["Channels"].asInteger());

        streamDetail.AddStream(audioStream);
      }
      else if (streamType == "Subtitle")
      {
        CStreamDetailSubtitle* subtitleStream = new CStreamDetailSubtitle();
        subtitleStream->m_strLanguage = stream["Language"].asString();

        streamDetail.AddStream(subtitleStream);
      }
    }
    fileitem.GetVideoInfoTag()->m_streamDetails = streamDetail;
  }
}

CVariant CEmbyUtils::GetEmbyCVariant(std::string url, std::string filter)
{
  XFILE::CCurlFile emby;
  emby.SetRequestHeader("Cache-Control", "no-cache");
  emby.SetRequestHeader("Content-Type", "application/json");
  emby.SetRequestHeader("Accept-Encoding", "gzip");

  CURL curl(url);
  // this is key to get back gzip encoded content
  curl.SetProtocolOption("seekable", "0");
  std::string response;
  if (emby.Get(curl.Get(), response))
  {
    if (emby.GetContentEncoding() == "gzip")
    {
      std::string buffer;
      if (XFILE::CZipFile::DecompressGzip(response, buffer))
        response = std::move(buffer);
      else
        return CVariant(CVariant::VariantTypeNull);
    }
#if defined(EMBY_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CEmbyUtils::GetEmbyCVariant %s", curl.Get().c_str());
    CLog::Log(LOGDEBUG, "CEmbyUtils::GetEmbyCVariant %s", response.c_str());
#endif
    auto resultObject = CJSONVariantParser::Parse(response);
    // recently added does not return proper object, we make one up later
    if (resultObject.isObject() || resultObject.isArray())
      return resultObject;
  }
  return CVariant(CVariant::VariantTypeNull);
}

void CEmbyUtils::RemoveSubtitleProperties(CFileItem &item)
{
}


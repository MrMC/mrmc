
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
#include "EmbyViewCache.h"
#include "Application.h"
#include "ContextMenuManager.h"
#include "Util.h"
#include "URL.h"
#include "filesystem/DirectoryCache.h"
#include "filesystem/StackDirectory.h"
#include "network/Network.h"
#include "utils/Base64URL.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/SystemInfo.h"
#include "utils/JSONVariantParser.h"
#include "utils/URIUtils.h"
#include "filesystem/File.h"
#include "filesystem/CurlFile.h"
#include "filesystem/ZipFile.h"
#include "settings/Settings.h"
#include "settings/MediaSettings.h"
#include "utils/JobManager.h"

#include "video/VideoInfoTag.h"
#include "video/windows/GUIWindowVideoBase.h"

#include "music/tags/MusicInfoTag.h"
#include "music/dialogs/GUIDialogSongInfo.h"
#include "music/dialogs/GUIDialogMusicInfo.h"
#include "guilib/GUIWindowManager.h"

static const std::string StandardFields = {
  "DateCreated,Genres,MediaStreams,Overview,Path,ImageTags,BackdropImageTags"
};

static const std::string MoviesFields = {
  "DateCreated,Genres,MediaStreams,Overview,ShortOverview,Path,ImageTags,BackdropImageTags,RecursiveItemCount,ProviderIds"
};

static const std::string TVShowsFields = {
  "DateCreated,Genres,MediaStreams,Overview,ShortOverview,Path,ImageTags,BackdropImageTags,RecursiveItemCount"
};


static int g_progressSec = 0;
static CFileItem m_curItem;
// one tick is 0.1 microseconds
static const uint64_t TicksToSecondsFactor = 10000000;

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
  curlfile.SetSilent(true);

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

void CEmbyUtils::SetEmbyItemProperties(CFileItem &item, const char *content)
{
  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(item.GetPath());
  SetEmbyItemProperties(item, content, client);
}

void CEmbyUtils::SetEmbyItemProperties(CFileItem &item, const char *content, const CEmbyClientPtr &client)
{
  item.SetProperty("EmbyItem", true);
  item.SetProperty("MediaServicesItem", true);
  if (!client)
    return;
  if (client->IsCloud())
    item.SetProperty("MediaServicesCloudItem", true);
  item.SetProperty("MediaServicesContent", content);
  item.SetProperty("MediaServicesClientID", client->GetUuid());
}

uint64_t CEmbyUtils::TicksToSeconds(uint64_t ticks)
{
  return ticks / TicksToSecondsFactor;
}
uint64_t CEmbyUtils::SecondsToTicks(uint64_t seconds)
{
  return seconds * TicksToSecondsFactor;
}

#pragma mark - Emby Server Utils
void CEmbyUtils::SetWatched(CFileItem &item)
{
  // use the current date and time if lastPlayed is invalid
  if (!item.GetVideoInfoTag()->m_lastPlayed.IsValid())
    item.GetVideoInfoTag()->m_lastPlayed = CDateTime::GetUTCDateTime();

  std::string url = item.GetPath();
  if (URIUtils::IsStack(url))
    url = XFILE::CStackDirectory::GetFirstStackedFile(url);
  else
    url = URIUtils::GetParentPath(url);
  if (StringUtils::StartsWithNoCase(url, "emby://"))
    url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));

  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(url);
  if (!client || !client->GetPresence())
    return;

  client->SetWatched(item);
}

void CEmbyUtils::SetUnWatched(CFileItem &item)
{
  std::string url = item.GetPath();
  if (URIUtils::IsStack(url))
    url = XFILE::CStackDirectory::GetFirstStackedFile(url);
  else
    url = URIUtils::GetParentPath(url);
  if (StringUtils::StartsWithNoCase(url, "emby://"))
    url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));

  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(url);
  if (!client || !client->GetPresence())
    return;

  client->SetUnWatched(item);
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
        CURL curl1(item.GetPath());
        CURL curl2(URIUtils::GetParentPath(url));
        CURL curl3(curl2.GetWithoutFilename());
        curl3.SetProtocolOptions(curl1.GetProtocolOptions());
        url = curl3.Get();
      }
      if (StringUtils::StartsWithNoCase(url, "emby://"))
        url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));

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

      CURL curl(item.GetPath());
      if (status == "playing")
      {
        if (g_progressSec < 0)
          // playback started
          curl.SetFileName("emby/Sessions/Playing");
        else
          curl.SetFileName("emby/Sessions/Playing/Progress");
      }
      else if (status == "stopped")
        curl.SetFileName("emby/Sessions/Playing/Stopped");

      std::string id = item.GetMediaServiceId();
      curl.SetOptions("");
      curl.SetOption("QueueableMediaTypes", "Video");
      curl.SetOption("CanSeek", "True");
      curl.SetOption("ItemId", id);
      curl.SetOption("MediaSourceId", id);
      curl.SetOption("PlayMethod", "DirectPlay");
      curl.SetOption("PositionTicks", StringUtils::Format("%llu", SecondsToTicks(currentSeconds)));
      curl.SetOption("IsMuted", "False");
      curl.SetOption("IsPaused", status == "paused" ? "True" : "False");

      std::string data;
      std::string response;
      // execute the POST request
      XFILE::CCurlFile curlfile;
      if (curlfile.Post(curl.Get(), data, response))
      {
#if defined(EMBY_DEBUG_VERBOSE)
        if (!response.empty())
          CLog::Log(LOGDEBUG, "CEmbyUtils::ReportProgress %s", response.c_str());
#endif
      }
      // mrmc assumes that less than 3 min, we want to start at beginning
      // but emby see stop with zero PositionTicks as a completed play
      // and we will get the item marked watched when we get an emby update msg.
      // so we need to followup and fixup emby server side.
      if (g_playbackState == MediaServicesPlayerState::stopped && currentSeconds <= 0.0)
        CEmbyUtils::SetUnWatched(item);
    }
  }
  g_progressSec++;
}

void CEmbyUtils::SetPlayState(MediaServicesPlayerState state)
{
  g_progressSec = -1;
  g_playbackState = state;
}

bool CEmbyUtils::GetItemSubtiles(CFileItem &item)
{
  return false;
}

bool CEmbyUtils::GetMoreItemInfo(CFileItem &item)
{
  std::string url = URIUtils::GetParentPath(item.GetPath());
  if (StringUtils::StartsWithNoCase(url, "emby://"))
    url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));
  
  CURL url2(url);
  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(url2.Get());
  if (!client || !client->GetPresence())
    return false;
  
  std::string itemId;
  if (item.HasProperty("EmbySeriesID") && !item.GetProperty("EmbySeriesID").asString().empty())
    itemId = item.GetProperty("EmbySeriesID").asString();
  else
    itemId = item.GetMediaServiceId();
  
  url2.SetFileName("emby/Users/" + client->GetUserID() + "/Items");
  url2.SetOptions("");
  url2.SetOption("Fields", "Genres,People");
  url2.SetOption("IDs", itemId);
  const CVariant variant = GetEmbyCVariant(url2.Get());
  
  GetVideoDetails(item, variant["Items"][0]);
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
  bool rtn = false;
  
  if (CEmbyServices::GetInstance().HasClients())
  {
    CFileItemList embyItems;
    std::string personID;
    //look through all emby clients and search
    std::vector<CEmbyClientPtr> clients;
    CEmbyServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      CURL curl(client->GetUrl());
      curl.SetProtocol(client->GetProtocol());
      curl.SetOption("userId", client->GetUserID());
      curl.SetOption("searchTerm", strSearchString);
      curl.SetFileName("emby/Search/Hints");
      CVariant variant = GetEmbyCVariant(curl.Get());
      
      personID = variant["SearchHints"][0]["ItemId"].asString();

      if (personID.empty())
        return false;
      
      // get all tvshows with selected actor
      variant.clear();
      curl.SetOptions("");
      curl.SetFileName("Users/" + client->GetUserID() + "/Items");
      curl.SetOption("IncludeItemTypes", "Series");
      curl.SetOption("Fields", TVShowsFields);
      curl.SetOption("Recursive","true");
      curl.SetOption("PersonIds", personID);
      variant = GetEmbyCVariant(curl.Get());
      
      ParseEmbySeries(embyItems, curl, variant);
      CGUIWindowVideoBase::AppendAndClearSearchItems(embyItems, "[" + g_localizeStrings.Get(20343) + "] ", items);
      
      // get all movies with selected actor
      variant.clear();
      curl.SetOption("IncludeItemTypes", "Movie");
      variant = GetEmbyCVariant(curl.Get());
      ParseEmbyVideos(embyItems, curl, variant, MediaTypeMovie);
      CGUIWindowVideoBase::AppendAndClearSearchItems(embyItems, "[" + g_localizeStrings.Get(20338) + "] ", items);
    }
    rtn = items.Size() > 0;
  }
  return rtn;
}

#pragma mark - Emby Recently Added and InProgress
bool CEmbyUtils::GetEmbyRecentlyAddedEpisodes(CFileItemList &items, const std::string url, int limit)
{
  CURL url2(url);

  url2.SetFileName(url2.GetFileName() + "/Latest");
  
  url2.SetOption("IncludeItemTypes", EmbyTypeEpisode);
  url2.SetOption("Limit", StringUtils::Format("%i",limit));
  url2.SetOption("GroupItems", "False");
  url2.SetOption("Fields", StandardFields);
  CVariant variant = GetEmbyCVariant(url2.Get());

  std::map<std::string, CVariant> variantMap;
  variantMap["Items"] = variant;
  variant = CVariant(variantMap);

  bool rtn = ParseEmbyVideos(items, url2, variant, MediaTypeEpisode);
  return rtn;
}

bool CEmbyUtils::GetEmbyInProgressShows(CFileItemList &items, const std::string url, int limit)
{
  CURL url2(url);

  url2.SetOption("IncludeItemTypes", EmbyTypeEpisode);
  url2.SetOption("SortBy", "DatePlayed");
  url2.SetOption("SortOrder", "Descending");
  url2.SetOption("Filters", "IsResumable");
  url2.SetOption("Limit", StringUtils::Format("%i",limit));
  url2.SetOption("Recursive", "true");
  url2.SetOption("Fields", StandardFields);
  CVariant result = GetEmbyCVariant(url2.Get());

  bool rtn = ParseEmbyVideos(items, url2, result, MediaTypeEpisode);
  return rtn;
}

bool CEmbyUtils::GetEmbyRecentlyAddedMovies(CFileItemList &items, const std::string url, int limit)
{
  CURL url2(url);
  url2.SetFileName(url2.GetFileName() + "/Latest");

  url2.SetOption("IncludeItemTypes", EmbyTypeMovie);
  url2.SetOption("Limit", StringUtils::Format("%i",limit));
  url2.SetOption("GroupItems", "False");
  url2.SetOption("Fields", MoviesFields);
  CVariant variant = GetEmbyCVariant(url2.Get());

  std::map<std::string, CVariant> variantMap;
  variantMap["Items"] = variant;
  variant = CVariant(variantMap);

  bool rtn = ParseEmbyVideos(items, url2, variant, MediaTypeMovie);
  return rtn;
}

bool CEmbyUtils::GetEmbyInProgressMovies(CFileItemList &items, const std::string url, int limit)
{
  CURL url2(url);

  url2.SetOption("IncludeItemTypes", EmbyTypeMovie);
  url2.SetOption("SortBy", "DatePlayed");
  url2.SetOption("SortOrder", "Descending");
  url2.SetOption("Filters", "IsResumable");
  url2.SetOption("Limit", StringUtils::Format("%i",limit));
  url2.SetOption("GroupItems", "False");
  url2.SetOption("Fields", MoviesFields);
  CVariant result = GetEmbyCVariant(url2.Get());

  bool rtn = ParseEmbyVideos(items, url2, result, MediaTypeMovie);
  return rtn;
}

bool CEmbyUtils::GetAllEmbyInProgress(CFileItemList &items, bool tvShow)
{
  bool rtn = false;

  if (CEmbyServices::GetInstance().HasClients())
  {
    CFileItemList embyItems;
    int limitTo = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_EMBYLIMITHOMETO);
    if (limitTo < 2)
      return false;
    //look through all emby clients and pull in progress for each library section
    std::vector<CEmbyClientPtr> clients;
    CEmbyServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      if (limitTo == 2 && !client->IsOwned())
        continue;

      std::vector<EmbyViewInfo> viewinfos;
      if (tvShow)
        viewinfos = client->GetViewInfoForTVShowContent();
      else
        viewinfos = client->GetViewInfoForMovieContent();
      for (const auto &viewinfo : viewinfos)
      {
        std::string userId = client->GetUserID();
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetOption("ParentId", viewinfo.id);
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
    int limitTo = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_EMBYLIMITHOMETO);
    if (limitTo < 2)
      return false;
    //look through all emby clients and pull recently added for each library section
    std::vector<CEmbyClientPtr> clients;
    CEmbyServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      if (limitTo == 2 && !client->IsOwned())
        continue;

      std::vector<EmbyViewInfo> contents;
      if (tvShow)
        contents = client->GetViewInfoForTVShowContent();
      else
        contents = client->GetViewInfoForMovieContent();
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

bool CEmbyUtils::GetEmbyRecentlyAddedAlbums(CFileItemList &items,int limit)
{
  
  bool rtn = false;
  
  if (CEmbyServices::GetInstance().HasClients())
  {
    CFileItemList embyItems;
    int limitTo = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_EMBYLIMITHOMETO);
    if (limitTo < 2)
      return false;
    //look through all emby clients and pull recently added for each library section
    std::vector<CEmbyClientPtr> clients;
    CEmbyServices::GetInstance().GetClients(clients);
    for (const auto &client : clients)
    {
      if (limitTo == 2 && !client->IsOwned())
        continue;
      
      std::vector<EmbyViewInfo> viewinfos;
      viewinfos = client->GetViewInfoForMusicContent();
      for (const auto &viewinfo : viewinfos)
      {
        std::string userId = client->GetUserID();
        CURL curl(client->GetUrl());
        curl.SetProtocol(client->GetProtocol());
        curl.SetOption("ParentId", viewinfo.id);
        curl.SetFileName("emby/Users/" + userId + "/Items/Latest");
        
        rtn = GetEmbyAlbum(embyItems, curl.Get(), 10);
        
        items.Append(embyItems);
        embyItems.ClearItems();
      }
    }
  }
  return rtn;
}

#pragma mark - Emby TV
bool CEmbyUtils::GetEmbySeasons(CFileItemList &items, const std::string url)
{
  bool rtn = false;
  
  CURL url2(url);
  url2.SetOption("IncludeItemTypes", EmbyTypeSeasons);
  url2.SetOption("Fields", "Etag,DateCreated,ImageTags,RecursiveItemCount");
  std::string parentId = url2.GetOption("ParentId");
  url2.SetOptions("");
  url2.SetOption("ParentId", parentId);
  
  const CVariant variant = GetEmbyCVariant(url2.Get());
  
  if (!variant.isNull() || variant.isObject() || variant.isMember("Items"))
  {
    CURL url3(url);
    std::string seriesID = url3.GetOption("ParentId");
    url3.SetOptions("");
    url3.SetOption("Ids", seriesID);
    url3.SetOption("Fields", "Overview,Genres");
    const CVariant seriesObject = CEmbyUtils::GetEmbyCVariant(url3.Get());
    
    rtn = ParseEmbySeasons(items, url2, seriesObject, variant);
  }
  return rtn;
}

bool CEmbyUtils::GetEmbyEpisodes(CFileItemList &items, const std::string url)
{
  CURL url2(url);
  
  url2.SetOption("IncludeItemTypes", EmbyTypeEpisode);
  url2.SetOption("Fields", StandardFields);
  const CVariant variant = GetEmbyCVariant(url2.Get());
  
  bool rtn = ParseEmbyVideos(items, url2, variant, MediaTypeEpisode);
  return rtn;
}

#pragma mark - Emby Music
CFileItemPtr ParseMusic(const CEmbyClient *client, const CVariant &variant)
{
  return nullptr;
}

bool CEmbyUtils::GetEmbyAlbum(CFileItemList &items, std::string url, int limit)
{
  CURL curl(url);
  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(curl.Get());
  if (!client || !client->GetPresence())
    return false;
  
  curl.SetOption("IncludeItemTypes", EmbyTypeAudio);
  curl.SetOption("Limit", StringUtils::Format("%i",limit));
  curl.SetOption("Fields", "BasicSyncInfo");
  CVariant variant = GetEmbyCVariant(curl.Get());
  
  if(!variant.isMember("Items"))
  {
    std::map<std::string, CVariant> variantMap;
    variantMap["Items"] = variant;
    variant = CVariant(variantMap);
  }

  curl.SetFileName("emby/Users/" + client->GetUserID() + "/Items");
  bool rtn = ParseEmbyAlbum(items, curl, variant);
  return rtn;

}

bool CEmbyUtils::GetEmbyArtistAlbum(CFileItemList &items, std::string url)
{
  CURL curl(url);
  CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(curl.Get());
  if (!client || !client->GetPresence())
    return false;
  
  curl.SetOptions("");
  curl.SetOption("Recursive", "true");
  curl.SetOption("Fields", "Etag,Genres");
  curl.SetOption("IncludeItemTypes", EmbyTypeMusicAlbum);
  curl.SetOption("ArtistIds", curl.GetProtocolOption("ArtistIds"));
  curl.SetFileName("emby/Users/" + client->GetUserID() + "/Items");
  const CVariant variant = GetEmbyCVariant(curl.Get());

  bool rtn = ParseEmbyAlbum(items, curl, variant);
  return rtn;
}

bool CEmbyUtils::GetEmbySongs(CFileItemList &items, std::string url)
{
  return false;
}

bool CEmbyUtils::GetEmbyAlbumSongs(CFileItemList &items, std::string url)
{
  CURL curl(url);
  curl.SetOption("Fields", "Etag,DateCreated,MediaStreams,ItemCounts,Genres");
  const CVariant variant = GetEmbyCVariant(curl.Get());

  bool rtn = ParseEmbyAudio(items, curl, variant);
  return rtn;
}

bool CEmbyUtils::ShowMusicInfo(CFileItem item)
{
  std::string type = item.GetMusicInfoTag()->m_type;
  if (type == MediaTypeSong)
  {
    CGUIDialogSongInfo *dialog = (CGUIDialogSongInfo *)g_windowManager.GetWindow(WINDOW_DIALOG_SONG_INFO);
    if (dialog)
    {
      dialog->SetSong(&item);
      dialog->Open();
    }
  }
  else if (type == MediaTypeAlbum)
  {
    CGUIDialogMusicInfo *pDlgAlbumInfo = (CGUIDialogMusicInfo*)g_windowManager.GetWindow(WINDOW_DIALOG_MUSIC_INFO);
    if (pDlgAlbumInfo)
    {
      pDlgAlbumInfo->SetAlbum(item);
      pDlgAlbumInfo->Open();
    }
  }
  else if (type == MediaTypeArtist)
  {
    CGUIDialogMusicInfo *pDlgArtistInfo = (CGUIDialogMusicInfo*)g_windowManager.GetWindow(WINDOW_DIALOG_MUSIC_INFO);
    if (pDlgArtistInfo)
    {
      pDlgArtistInfo->SetArtist(item);
      pDlgArtistInfo->Open();
    }
  }
  return true;
}

bool CEmbyUtils::GetEmbyAlbumSongs(CFileItem item, CFileItemList &items)
{
  std::string url = URIUtils::GetParentPath(item.GetPath());
  if (StringUtils::StartsWithNoCase(url, "emby://"))
    url = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));
  
  return GetEmbyAlbumSongs(items, url);
}

bool CEmbyUtils::GetEmbyMediaTotals(MediaServicesMediaCount &totals)
{
  return false;
}

#pragma mark - Emby parsers
CFileItemPtr CEmbyUtils::ToFileItemPtr(CEmbyClient *client, const CVariant &variant)
{
  if (variant.isNull() || !variant.isObject() || !variant.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ToFileItemPtr cvariant is empty");
    return nullptr;
  }

  const auto& variantItems = variant["Items"];
  for (auto variantitemsIt = variantItems.begin_array(); variantitemsIt != variantItems.end_array(); ++variantitemsIt)
  {
    const auto variantItem = *variantitemsIt;
    if (!variantItem.isMember("Id"))
      continue;

    CFileItemList items;
    std::string type = variantItem["Type"].asString();
    std::string mediaType = variantItem["MediaType"].asString();
    CURL url2(client->GetUrl());
    url2.SetProtocol(client->GetProtocol());
    url2.SetPort(client->GetPort());
    url2.SetFileName("emby/Users/" + client->GetUserID() + "/Items");

    if (type == EmbyTypeMovie)
    {
#if defined(EMBY_DEBUG_VERBOSE)
      CLog::Log(LOGDEBUG, "CEmbyUtils::ToFileItemPtr EmbyTypeMovie: %s",
        variantItem["Name"].asString().c_str());
#endif
      ParseEmbyVideos(items, url2, variant, MediaTypeMovie);
    }
    else if (type == EmbyTypeSeries)
    {
#if defined(EMBY_DEBUG_VERBOSE)
      CLog::Log(LOGDEBUG, "CEmbyUtils::ToFileItemPtr EmbyTypeSeries: %s",
        variantItem["Name"].asString().c_str());
#endif
      ParseEmbySeries(items, url2, variant);
    }
    else if (type == EmbyTypeSeason)
    {
#if defined(EMBY_DEBUG_VERBOSE)
      CLog::Log(LOGDEBUG, "CEmbyUtils::ToFileItemPtr EmbyTypeSeason: %s",
        variantItem["Name"].asString().c_str());
#endif
      CURL url3(url2);
      std::string seriesID = variantItem["ParentId"].asString();
      url3.SetOptions("");
      url3.SetOption("Ids", seriesID);
      url3.SetOption("Fields", "Overview,Genres");
      const CVariant seriesObject = CEmbyUtils::GetEmbyCVariant(url3.Get());
      ParseEmbySeasons(items, url2, seriesObject, variant);
    }
    else if (type == EmbyTypeEpisode)
    {
#if defined(EMBY_DEBUG_VERBOSE)
      CLog::Log(LOGDEBUG, "CEmbyUtils::ToFileItemPtr EmbyTypeEpisode: %s",
        variantItem["Name"].asString().c_str());
#endif
      ParseEmbyVideos(items, url2, variant, MediaTypeEpisode);
    }
    else if (type == EmbyTypeAudio)
    {
#if defined(EMBY_DEBUG_VERBOSE)
      CLog::Log(LOGDEBUG, "CEmbyUtils::ToFileItemPtr EmbyTypeAudio: %s",
        variantItem["Name"].asString().c_str());
#endif
      ParseEmbyAudio(items, url2, variant);
    }
    else if (type == EmbyTypeMusicAlbum)
    {
#if defined(EMBY_DEBUG_VERBOSE)
      CLog::Log(LOGDEBUG, "CEmbyUtils::ToFileItemPtr EmbyTypeMusicAlbum: %s",
        variantItem["Name"].asString().c_str());
#endif
      ParseEmbyAlbum(items, url2, variant);
    }
    else if (type == EmbyTypeMusicArtist)
    {
#if defined(EMBY_DEBUG_VERBOSE)
      CLog::Log(LOGDEBUG, "CEmbyUtils::ToFileItemPtr EmbyTypeMusicArtist: %s",
        variantItem["Name"].asString().c_str());
#endif
      ParseEmbyArtists(items, url2, variant);
    }
    else if (type == EmbyTypeFolder)
    {
      // ignore these, useless info
    }
    else
    {
      CLog::Log(LOGDEBUG, "CEmbyUtils::ToFileItemPtr unknown type: %s with name %s",
        type.c_str(), variantItem["Name"].asString().c_str());
    }

    return items[0];
  }

  return nullptr;
}

bool CEmbyUtils::ParseEmbyVideos(CFileItemList &items, CURL url, const CVariant &variant, std::string type)
{
  if (variant.isNull() || !variant.isObject() || !variant.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ParseEmbyVideos invalid response from %s", url.GetRedacted().c_str());
    return false;
  }

#if defined(EMBY_DEBUG_TIMING)
  unsigned int currentTime = XbmcThreads::SystemClockMillis();
#endif
  bool rtn = false;
  const auto& variantItems = variant["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    if (*variantItemIt == CVariant::VariantTypeNull)
      continue;

    const auto objectItem = *variantItemIt;
    rtn = true;

    // ignore raw blueray rips, these are designed to be
    // direct played (ie via mounted filesystem)
    // and we do not do that yet.
    if (objectItem["VideoType"].asString() == "BluRay")
      continue;
    if (objectItem["VideoType"].asString() == "Dvd")
      continue;

    CFileItemPtr item = ToVideoFileItemPtr(url, objectItem, type);
    items.Add(item);
  }
  // this is needed to display movies/episodes properly ... dont ask
  // good thing it didnt take 2 days to figure it out
  items.SetLabel(variantItems[0]["SeasonName"].asString());
  items.SetProperty("library.filter", "true");
  if (type == MediaTypeTvShow)
    SetEmbyItemProperties(items, "episodes");
  else
    SetEmbyItemProperties(items, "movies");

#if defined(EMBY_DEBUG_TIMING)
  int delta = XbmcThreads::SystemClockMillis() - currentTime;
  if (delta > 1)
  {
    CLog::Log(LOGDEBUG, "CEmbyUtils::GetVideoItems %d(msec) for %d items",
      XbmcThreads::SystemClockMillis() - currentTime, variantItems.size());
  }
#endif
  return rtn;
}

bool CEmbyUtils::ParseEmbySeries(CFileItemList &items, const CURL &url, const CVariant &variant)
{
  if (variant.isNull() || !variant.isObject() || !variant.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ParseEmbySeries invalid response from %s", url.GetRedacted().c_str());
    return false;
  }

  bool rtn = false;
  std::string imagePath;

  if (!variant.isNull() || variant.isObject() || variant.isMember("Items"))
  {
    const auto& variantItems = variant["Items"];
    for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
    {
      if (*variantItemIt == CVariant::VariantTypeNull)
        continue;

      const auto item = *variantItemIt;
      rtn = true;

      // local vars for common fields
      std::string itemId = item["Id"].asString();
      std::string seriesId = item["SeriesId"].asString();
      // clear url options
      CURL curl(url);
      curl.SetOption("ParentId", itemId);

      CFileItemPtr newItem(new CFileItem());
      // set m_bIsFolder to true to indicate we are series list
      newItem->m_bIsFolder = true;

      std::string title = item["Name"].asString();
      newItem->SetLabel(title);

      CDateTime premiereDate;
      premiereDate.SetFromW3CDateTime(item["PremiereDate"].asString());
      newItem->m_dateTime = premiereDate;

      newItem->SetPath("emby://tvshows/shows/" + Base64URL::Encode(curl.Get()));
      newItem->SetMediaServiceId(itemId);
      newItem->SetMediaServiceFile(item["Path"].asString());

      curl.SetFileName("Items/" + itemId + "/Images/Primary");
      imagePath = curl.Get();
      newItem->SetArt("thumb", imagePath);
      newItem->SetIconImage(imagePath);

      curl.SetFileName("Items/" + itemId + "/Images/Banner");
      imagePath = curl.Get();
      newItem->SetArt("banner", imagePath);

      curl.SetFileName("Items/" + itemId + "/Images/Backdrop");
      imagePath = curl.Get();
      newItem->SetArt("fanart", imagePath);

      newItem->GetVideoInfoTag()->m_playCount = static_cast<int>(item["UserData"]["PlayCount"].asInteger());
      newItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, newItem->GetVideoInfoTag()->m_playCount > 0);

      newItem->GetVideoInfoTag()->m_strTitle = title;
      newItem->GetVideoInfoTag()->m_strStatus = item["Status"].asString();

      newItem->GetVideoInfoTag()->m_type = MediaTypeTvShow;
      newItem->GetVideoInfoTag()->m_strFileNameAndPath = newItem->GetPath();
      newItem->GetVideoInfoTag()->SetSortTitle(title);
      newItem->GetVideoInfoTag()->SetOriginalTitle(title);
      //newItem->GetVideoInfoTag()->SetSortTitle(item["SortName"].asString());
      //newItem->GetVideoInfoTag()->SetOriginalTitle(item["OriginalTitle"].asString());
      newItem->SetProperty("EmbySeriesID", seriesId);
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
      SetEmbyItemProperties(*newItem, "tvshows");
      items.Add(newItem);
    }
    // this is needed to display movies/episodes properly ... dont ask
    // good thing it didnt take 2 days to figure it out
    items.SetProperty("library.filter", "true");
    items.SetCacheToDisc(CFileItemList::CACHE_NEVER);
    SetEmbyItemProperties(items, "tvshows");
  }
  return rtn;
}

bool CEmbyUtils::ParseEmbySeasons(CFileItemList &items, const CURL &url, const CVariant &series, const CVariant &variant)
{
  if (variant.isNull() || !variant.isObject() || series.isNull() || !series.isObject())
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ParseEmbySeasons invalid response from %s", url.GetRedacted().c_str());
    return false;
  }

  bool rtn = false;
  std::string imagePath;
  std::string seriesName;
  std::string seriesId;
  const auto& seriesItem = series["Items"][0];

  const auto& variantItems = variant["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    if (*variantItemIt == CVariant::VariantTypeNull)
      continue;

    const auto item = *variantItemIt;
    rtn = true;

    // local vars for common fields
    std::string itemId = item["Id"].asString();
    seriesId = item["SeriesId"].asString();
    // clear url options
    CURL curl(url);
    curl.SetOptions("");
    curl.SetOption("ParentId", itemId);

    CFileItemPtr newItem(new CFileItem());
    // set m_bIsFolder to true to indicate we are seasons list
    newItem->m_bIsFolder = true;

    newItem->SetLabel(item["Name"].asString());
    newItem->SetPath("emby://tvshows/seasons/" + Base64URL::Encode(curl.Get()));
    newItem->SetMediaServiceId(itemId);
    newItem->SetMediaServiceFile(item["Path"].asString());

    curl.SetFileName("Items/" + itemId + "/Images/Primary");
    imagePath = curl.Get();
    newItem->SetArt("thumb", imagePath);
    newItem->SetIconImage(imagePath);

    curl.SetFileName("Items/" + seriesId + "/Images/Banner");
    imagePath = curl.Get();
    newItem->SetArt("banner", imagePath);
    curl.SetFileName("Items/" + seriesId + "/Images/Backdrop");
    newItem->SetArt("fanart", imagePath);

    newItem->GetVideoInfoTag()->m_type = MediaTypeSeason;
    newItem->GetVideoInfoTag()->m_strTitle = item["Name"].asString();
    // we get these from rootXmlNode, where all show info is
    seriesName = item["SeriesName"].asString();
    newItem->GetVideoInfoTag()->m_strShowTitle = seriesName;
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
    newItem->SetProperty("EmbySeriesID", seriesId);

    int totalEpisodes = item["RecursiveItemCount"].asInteger();
    int unWatchedEpisodes = item["UserData"]["UnplayedItemCount"].asInteger();
    int watchedEpisodes = totalEpisodes - unWatchedEpisodes;
    int iSeason = item["IndexNumber"].asInteger();
    newItem->GetVideoInfoTag()->m_iSeason = iSeason;
    newItem->GetVideoInfoTag()->m_iEpisode = totalEpisodes;
    newItem->GetVideoInfoTag()->m_playCount = (totalEpisodes == watchedEpisodes) ? 1 : 0;

    newItem->SetProperty("totalepisodes", totalEpisodes);
    newItem->SetProperty("numepisodes", totalEpisodes);
    newItem->SetProperty("watchedepisodes", watchedEpisodes);
    newItem->SetProperty("unwatchedepisodes", unWatchedEpisodes);

    newItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, (newItem->GetVideoInfoTag()->m_playCount > 0) && (newItem->GetVideoInfoTag()->m_iEpisode > 0));

    SetEmbyItemProperties(*newItem, "seasons");
    items.Add(newItem);
  }
  
  SetEmbyItemProperties(items, "seasons");
  items.SetProperty("showplot", seriesItem["Overview"].asString());
  
  if (!items.IsEmpty())
  {
    int iFlatten = CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOLIBRARY_FLATTENTVSHOWS);
    int itemsSize = items.GetObjectCount();
    int firstIndex = items.Size() - itemsSize;
    // check if the last item is the "All seasons" item which should be ignored for flattening
    if (!items[items.Size() - 1]->HasVideoInfoTag() || items[items.Size() - 1]->GetVideoInfoTag()->m_iSeason < 0)
      itemsSize -= 1;
    
    bool bFlatten = (itemsSize == 1 && iFlatten == 1) || iFlatten == 2 ||                              // flatten if one one season or if always flatten is enabled
    (itemsSize == 2 && iFlatten == 1 &&                                                // flatten if one season + specials
     (items[firstIndex]->GetVideoInfoTag()->m_iSeason == 0 || items[firstIndex + 1]->GetVideoInfoTag()->m_iSeason == 0));
    
    if (iFlatten > 0 && !bFlatten && (WatchedMode)CMediaSettings::GetInstance().GetWatchedMode("tvshows") == WatchedModeUnwatched)
    {
      int count = 0;
      for(int i = 0; i < items.Size(); i++)
      {
        const CFileItemPtr item = items.Get(i);
        if (item->GetProperty("unwatchedepisodes").asInteger() != 0 && item->GetVideoInfoTag()->m_iSeason > 0)
          count++;
      }
      bFlatten = (count < 2); // flatten if there is only 1 unwatched season (not counting specials)
    }
    
    if (bFlatten)
    { // flatten if one season or flatten always
      CFileItemList tempItems;
      
      // clear url options
      CURL curl(url);
      curl.SetOptions("");
      curl.SetOption("ParentId", seriesId);
      curl.SetOption("Recursive", "true");
      
      XFILE::CDirectory::GetDirectory("emby://tvshows/seasons/" + Base64URL::Encode(curl.Get()),tempItems);
      
      items.Clear();
      items.Assign(tempItems);
    }
  }
  items.SetLabel(seriesName);
  items.SetProperty("library.filter", "true");
  items.SetCacheToDisc(CFileItemList::CACHE_NEVER);

  return rtn;
}

bool CEmbyUtils::ParseEmbyAudio(CFileItemList &items, const CURL &url, const CVariant &variant)
{
  if (variant.isNull() || !variant.isObject())
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ParseEmbyAudio invalid response from %s", url.GetRedacted().c_str());
    return false;
  }

  // clear base url options
  CURL curl(url);
  curl.SetOptions("");
  std::string imagePath;

  bool rtn = false;
  const auto& variantItems = variant["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    if (*variantItemIt == CVariant::VariantTypeNull)
      continue;

    const auto item = *variantItemIt;
    rtn = true;

    // local vars for common fields
    std::string itemId = item["Id"].asString();
    std::string albumId = item["AlbumId"].asString();

    CFileItemPtr embyItem(new CFileItem());
    embyItem->SetLabel(item["Name"].asString());
    curl.SetFileName("Audio/" + itemId +"/stream?static=true");
    embyItem->SetPath(curl.Get());
    embyItem->SetMediaServiceId(itemId);
    embyItem->SetProperty("EmbySongKey", itemId);
    embyItem->GetMusicInfoTag()->m_type = MediaTypeSong;
    embyItem->GetMusicInfoTag()->SetTitle(item["Name"].asString());
    embyItem->GetMusicInfoTag()->SetAlbum(item["Album"].asString());
    embyItem->GetMusicInfoTag()->SetYear(item["ProductionYear"].asInteger());
    embyItem->GetMusicInfoTag()->SetTrackNumber(item["IndexNumber"].asInteger());
    embyItem->GetMusicInfoTag()->SetDuration(TicksToSeconds(variant["RunTimeTicks"].asInteger()));

    curl.SetFileName("Items/" + albumId + "/Images/Primary");
    imagePath = curl.Get();
    embyItem->SetArt("thumb", imagePath);
    embyItem->SetProperty("thumb", imagePath);

    curl.SetFileName("Items/" + albumId + "/Images/Backdrop");
    imagePath = curl.Get();
    embyItem->SetArt("fanart", imagePath);
    embyItem->SetProperty("fanart", imagePath);

    GetMusicDetails(*embyItem, item);

    embyItem->GetMusicInfoTag()->m_dateAdded.SetFromW3CDateTime(item["DateCreated"].asString());
    embyItem->GetMusicInfoTag()->m_lastPlayed.SetFromW3CDateTime(item["LastPlayedDate"].asString());
    embyItem->GetMusicInfoTag()->SetLoaded(true);
    SetEmbyItemProperties(*embyItem, MediaTypeSong);
    items.Add(embyItem);
  }
  items.SetProperty("library.filter", "true");
  items.GetMusicInfoTag()->m_type = MediaTypeSong;
  items.SetCacheToDisc(CFileItemList::CACHE_NEVER);
  SetEmbyItemProperties(items, MediaTypeSong);

  return rtn;
}

bool CEmbyUtils::ParseEmbyAlbum(CFileItemList &items, const CURL &url, const CVariant &variant)
{
  if (variant.isNull() || !variant.isObject() || !variant.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ParseEmbyAlbum invalid response from %s", url.GetRedacted().c_str());
    return false;
  }

  // clear base url options
  CURL curl(url);
  curl.SetOptions("");
  std::string imagePath;

  bool rtn = false;
  const auto& variantItems = variant["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    if (*variantItemIt == CVariant::VariantTypeNull)
      continue;

    const auto item = *variantItemIt;
    rtn = true;

    // local vars for common fields
    std::string itemId = item["Id"].asString();

    CFileItemPtr embyItem(new CFileItem());
    // set m_bIsFolder to true to indicate we are artist list

    embyItem->m_bIsFolder = true;
    embyItem->SetLabel(item["Name"].asString());
    curl.SetOption("ParentId", itemId);
    embyItem->SetPath("emby://music/albumsongs/" + Base64URL::Encode(curl.Get()));
    embyItem->SetMediaServiceId(itemId);

    embyItem->GetMusicInfoTag()->m_type = MediaTypeAlbum;
    embyItem->GetMusicInfoTag()->SetTitle(item["Name"].asString());

    embyItem->GetMusicInfoTag()->SetArtistDesc(item["ArtistItems"]["Name"].asString());
    embyItem->SetProperty("artist", item["ArtistItems"]["Name"].asString());
    embyItem->SetProperty("EmbyAlbumKey", item["Id"].asString());

    embyItem->GetMusicInfoTag()->SetAlbum(item["Name"].asString());
    embyItem->GetMusicInfoTag()->SetYear(item["ProductionYear"].asInteger());
    
    CURL curl2(url);
    curl2.SetOptions("");
    curl2.RemoveProtocolOption("ArtistIds");
    curl2.SetFileName("Items/" + itemId + "/Images/Primary");
    imagePath = curl2.Get();
    embyItem->SetArt("thumb", imagePath);
    embyItem->SetProperty("thumb", imagePath);

    curl2.SetFileName("Items/" + itemId + "/Images/Backdrop");
    imagePath = curl2.Get();
    embyItem->SetArt("fanart", imagePath);
    embyItem->SetProperty("fanart", imagePath);

    embyItem->GetMusicInfoTag()->m_dateAdded.SetFromW3CDateTime(item["DateCreated"].asString());
    embyItem->GetMusicInfoTag()->m_lastPlayed.SetFromW3CDateTime(item["LastPlayedDate"].asString());

    GetMusicDetails(*embyItem, item);
    SetEmbyItemProperties(*embyItem, MediaTypeAlbum);
    items.Add(embyItem);
  }
  items.SetProperty("library.filter", "true");
  items.GetMusicInfoTag()->m_type = MediaTypeAlbum;
  items.SetCacheToDisc(CFileItemList::CACHE_NEVER);
  SetEmbyItemProperties(items, MediaTypeAlbum);

  return rtn;
}

bool CEmbyUtils::ParseEmbyArtists(CFileItemList &items, const CURL &url, const CVariant &variant)
{
  if (variant.isNull() || !variant.isObject())
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ParseEmbyArtists invalid response from %s", url.GetRedacted().c_str());
    return false;
  }

  // clear base url options
  CURL curl(url);
  curl.SetOptions("");
  std::string imagePath;

  bool rtn = false;
  const auto& variantItems = variant["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    if (*variantItemIt == CVariant::VariantTypeNull)
      continue;

    const auto item = *variantItemIt;
    rtn = true;

    // local vars for common fields
    std::string itemId = item["Id"].asString();

    CFileItemPtr embyItem(new CFileItem());
    // set m_bIsFolder to true to indicate we are artist list

    embyItem->m_bIsFolder = true;
    embyItem->SetLabel(item["Name"].asString());
    curl.SetProtocolOption("ArtistIds", itemId);
    curl.SetFileName("Items");
    embyItem->SetPath("emby://music/artistalbums/" + Base64URL::Encode(curl.Get()));
    embyItem->SetMediaServiceId(itemId);

    embyItem->GetMusicInfoTag()->m_type = MediaTypeArtist;
    embyItem->GetMusicInfoTag()->SetTitle(item["Name"].asString());

    embyItem->GetMusicInfoTag()->SetYear(item["ProductionYear"].asInteger());

    CURL curl2(url);
    curl2.SetOptions("");
    curl2.RemoveProtocolOption("ArtistIds");
    curl2.SetFileName("Items/" + item["Id"].asString() + "/Images/Primary");
    imagePath = curl2.Get();
    embyItem->SetArt("thumb", imagePath);
    embyItem->SetProperty("thumb", imagePath);

    curl2.SetFileName("Items/" + itemId + "/Images/Backdrop");
    imagePath = curl2.Get();
    embyItem->SetArt("fanart", imagePath);
    embyItem->SetProperty("fanart", imagePath);

    embyItem->GetMusicInfoTag()->m_dateAdded.SetFromW3CDateTime(item["DateCreated"].asString());
    embyItem->GetMusicInfoTag()->m_lastPlayed.SetFromW3CDateTime(item["LastPlayedDate"].asString());

    GetMusicDetails(*embyItem, item);

    SetEmbyItemProperties(*embyItem, MediaTypeArtist);
    items.Add(embyItem);
  }
  items.SetProperty("library.filter", "true");
  items.GetMusicInfoTag()->m_type = MediaTypeArtist;
  items.SetCacheToDisc(CFileItemList::CACHE_NEVER);
  SetEmbyItemProperties(items, MediaTypeArtist);

  return rtn;
}

bool CEmbyUtils::ParseEmbyMoviesFilter(CFileItemList &items, CURL url, const CVariant &variant, const std::string &filter)
{
  if (variant.isNull() || !variant.isObject() || !variant.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ParseEmbyMoviesFilter invalid response from %s", url.GetRedacted().c_str());
    return false;
  }

  bool rtn = false;
  CURL curl(url);
  const auto& variantItems = variant["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    if (*variantItemIt == CVariant::VariantTypeNull)
      continue;

    const auto item = *variantItemIt;
    rtn = true;

    // local vars for common fields
    std::string itemId = item["Id"].asString();
    std::string itemName = item["Name"].asString();

    CFileItemPtr newItem(new CFileItem());
    newItem->m_bIsFolder = true;
    newItem->m_bIsShareOrDrive = false;

    CURL curl1(url);
    curl1.SetOption("Fields", "DateCreated,Genres,MediaStreams,Overview,Path,ProviderIds");
    if (filter == "Genres")
      curl1.SetOption("Genres", itemName);
    else if (filter == "Years")
      curl1.SetOption("Years", itemName);
    else if (filter == "Collections")
      curl1.SetOption("ParentId", itemId);

    newItem->SetPath("emby://movies/filter/" + Base64URL::Encode(curl1.Get()));
    newItem->SetLabel(itemName);
    newItem->SetProperty("SkipLocalArt", true);
    items.Add(newItem);
  }
  items.SetCacheToDisc(CFileItemList::CACHE_NEVER);

  return rtn;
}

bool CEmbyUtils::ParseEmbyTVShowsFilter(CFileItemList &items, const CURL url, const CVariant &variant, const std::string &filter)
{
  if (variant.isNull() || !variant.isObject() || !variant.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyUtils::ParseEmbyTVShowsFilter invalid response from %s", url.GetRedacted().c_str());
    return false;
  }

  bool rtn = false;
  CURL curl1(url);
  const auto& variantItems = variant["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    if (*variantItemIt == CVariant::VariantTypeNull)
      continue;

    const auto item = *variantItemIt;
    rtn = true;

    // local vars for common fields
    std::string itemId = item["Id"].asString();
    std::string itemName = item["Name"].asString();

    CFileItemPtr newItem(new CFileItem());
    newItem->m_bIsFolder = true;
    newItem->m_bIsShareOrDrive = false;

    if (filter == "Genres")
      curl1.SetOption("Genres", itemName);
    else if (filter == "Years")
      curl1.SetOption("Years", itemName);
    else if (filter == "Collections")
      curl1.SetOption("ParentId", itemId);

    newItem->SetPath("emby://tvshows/filter/" + Base64URL::Encode(curl1.Get()));
    newItem->SetLabel(itemName);
    newItem->SetProperty("SkipLocalArt", true);
    items.Add(newItem);
  }
  items.SetCacheToDisc(CFileItemList::CACHE_NEVER);

  return rtn;
}

CVariant CEmbyUtils::GetEmbyCVariant(std::string url, std::string filter)
{
#if defined(EMBY_DEBUG_TIMING)
  unsigned int currentTime = XbmcThreads::SystemClockMillis();
#endif
  
  XFILE::CCurlFile emby;
  emby.SetRequestHeader("Cache-Control", "no-cache");
  emby.SetRequestHeader("Content-Type", "application/json");
  emby.SetRequestHeader("Accept-Encoding", "gzip");
  
  CURL curl(url);
  // this is key to get back gzip encoded content
  curl.SetProtocolOption("seekable", "0");
  // we always want json back
  curl.SetProtocolOptions(curl.GetProtocolOptions() + "&format=json");
  std::string response;
  if (emby.Get(curl.Get(), response))
  {
#if defined(EMBY_DEBUG_TIMING)
    CLog::Log(LOGDEBUG, "CEmbyUtils::GetEmbyCVariant %d(msec) for %lu bytes",
              XbmcThreads::SystemClockMillis() - currentTime, response.size());
#endif
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
#if defined(EMBY_DEBUG_TIMING)
    currentTime = XbmcThreads::SystemClockMillis();
#endif
    CVariant resultObject;
    if (CJSONVariantParser::Parse(response, resultObject))
    {
#if defined(EMBY_DEBUG_TIMING)
      CLog::Log(LOGDEBUG, "CEmbyUtils::GetEmbyCVariant parsed in %d(msec)",
                XbmcThreads::SystemClockMillis() - currentTime);
#endif
      // recently added does not return proper object, we make one up later
      if (resultObject.isObject() || resultObject.isArray())
        return resultObject;
    }
  }
  return CVariant(CVariant::VariantTypeNull);
}

#pragma mark - Emby private
CFileItemPtr CEmbyUtils::ToVideoFileItemPtr(CURL url, const CVariant &variant, std::string type)
{
  // clear base url options
  CURL url2(url);
  url2.SetOptions("");

  CFileItemPtr item(new CFileItem());

  // cache common accessors.
  std::string itemId = variant["Id"].asString();
  std::string seriesId = variant["SeriesId"].asString();

  std::string value;
  std::string fanart;
  // if we have "ParentIndexNumber" means we are listing episodes
  if (variant.isMember("ParentIndexNumber"))
  {
    url2.SetFileName("Items/" + itemId + "/Images/Primary");
    item->SetArt("thumb", url2.Get());
    item->SetIconImage(url2.Get());
    url2.SetFileName("Items/" + itemId + "/Images/Backdrop");
    fanart = url2.Get();

    item->GetVideoInfoTag()->m_strShowTitle = variant["SeriesName"].asString();
    item->GetVideoInfoTag()->m_iSeason = variant["ParentIndexNumber"].asInteger();
    item->GetVideoInfoTag()->m_iEpisode = variant["IndexNumber"].asInteger();
    item->SetLabel(variant["SeasonName"].asString());
    item->SetProperty("EmbySeriesID", seriesId);
    std::string seasonEpisode = StringUtils::Format("S%02iE%02i", item->GetVideoInfoTag()->m_iSeason, item->GetVideoInfoTag()->m_iEpisode);
    item->SetProperty("SeasonEpisode", seasonEpisode);
    url2.SetFileName("Items/" + variant["SeasonId"].asString() + "/Images/Primary");
    item->SetArt("tvshow.thumb", url2.Get());
  }
  else
  {
    url2.SetFileName("Items/" + itemId + "/Images/Primary");
    item->SetArt("thumb", url2.Get());
    item->SetIconImage(url2.Get());
    url2.SetFileName("Items/" + itemId + "/Images/Backdrop");
    fanart = url2.Get();
    
    CVariant paramsProvID = variant["ProviderIds"];
    if (paramsProvID.isObject())
    {
      for (CVariant::iterator_map it = paramsProvID.begin_map(); it != paramsProvID.end_map(); it++)
      {
        std::string strFirst = it->first;
        StringUtils::ToLower(strFirst);
        item->GetVideoInfoTag()->SetUniqueID(it->second.asString(),strFirst);
      }
    }
  }

  std::string title = variant["Name"].asString();
  item->SetLabel(title);
  item->m_dateTime.SetFromW3CDateTime(variant["PremiereDate"].asString());

  item->SetArt("fanart", fanart);

  item->GetVideoInfoTag()->m_strTitle = title;
  item->GetVideoInfoTag()->SetSortTitle(title);
  item->GetVideoInfoTag()->SetOriginalTitle(title);
  //item->GetVideoInfoTag()->SetSortTitle(variant["SortName"].asString());
  //item->GetVideoInfoTag()->SetOriginalTitle(variant["OriginalTitle"].asString());

  url2.SetFileName("Videos/" + itemId +"/stream?static=true");
  item->SetPath(url2.Get());
  item->SetMediaServiceId(itemId);
  item->SetMediaServiceFile(variant["Path"].asString());
  item->GetVideoInfoTag()->m_strFileNameAndPath = url2.Get();

  item->GetVideoInfoTag()->m_type = type;
  item->GetVideoInfoTag()->SetPlot(variant["Overview"].asString());
  item->GetVideoInfoTag()->SetPlotOutline(variant["ShortOverview"].asString());

  CDateTime premiereDate;
  premiereDate.SetFromW3CDateTime(variant["PremiereDate"].asString());
  item->GetVideoInfoTag()->m_firstAired = premiereDate;
  item->GetVideoInfoTag()->SetPremiered(premiereDate);
  item->GetVideoInfoTag()->m_dateAdded.SetFromW3CDateTime(variant["DateCreated"].asString());

  item->GetVideoInfoTag()->SetYear(static_cast<int>(variant["ProductionYear"].asInteger()));
  item->GetVideoInfoTag()->SetRating(variant["CommunityRating"].asFloat(), static_cast<int>(variant["VoteCount"].asInteger()), "", true);
  item->GetVideoInfoTag()->m_strMPAARating = variant["OfficialRating"].asString();

  GetVideoDetails(*item, variant);

  item->GetVideoInfoTag()->m_duration = static_cast<int>(TicksToSeconds(variant["RunTimeTicks"].asInteger()));
  item->GetVideoInfoTag()->m_resumePoint.totalTimeInSeconds = item->GetVideoInfoTag()->m_duration;
  item->GetVideoInfoTag()->m_resumePoint.timeInSeconds = static_cast<int>(TicksToSeconds(variant["UserData"]["PlaybackPositionTicks"].asUnsignedInteger()));
  
  // ["UserData"]["PlayCount"] means that it was watched, if so we set m_playCount to 1 and set overlay.
  // If we have ["UserData"]["PlaybackPositionTicks"]/m_resumePoint.timeInSeconds that means we are partially watched and should not set m_playCount to 1
  
  if (variant["UserData"]["PlayCount"].asInteger() && item->GetVideoInfoTag()->m_resumePoint.timeInSeconds <= 0)
  {
    item->GetVideoInfoTag()->m_playCount = static_cast<int>(variant["UserData"]["PlayCount"].asInteger());
  }
  
  item->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, item->GetVideoInfoTag()->m_playCount > 0);


  GetMediaDetals(*item, variant, itemId);

  if (type == MediaTypeTvShow)
    SetEmbyItemProperties(*item, "episodes");
  else
    SetEmbyItemProperties(*item, "movies");
  return item;
}

void CEmbyUtils::GetVideoDetails(CFileItem &item, const CVariant &variant)
{
  if (variant.isMember("Genres"))
  {
    // get all genres
    std::vector<std::string> genres;
    const auto& streams = variant["Genres"];
    for (auto streamIt = streams.begin_array(); streamIt != streams.end_array(); ++streamIt)
    {
      const auto stream = *streamIt;
      genres.push_back(stream.asString());
    }
    item.GetVideoInfoTag()->SetGenre(genres);
  }

  if (variant.isMember("People"))
  {
    std::vector< SActorInfo > roles;
    std::vector<std::string> directors;
    const auto& peeps = variant["People"];
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
        std::string urlStr = URIUtils::GetParentPath(item.GetPath());
        if (StringUtils::StartsWithNoCase(urlStr, "emby://"))
          urlStr = Base64URL::Decode(URIUtils::GetFileName(item.GetPath()));
        CURL url(urlStr);
        url.SetFileName("Items/" + peep["Id"].asString() + "/Images/Primary");
        role.thumb = url.Get();
        roles.push_back(role);
      }
    }

    item.GetVideoInfoTag()->m_cast = roles;
    item.GetVideoInfoTag()->SetDirector(directors);
  }
}

void CEmbyUtils::GetMusicDetails(CFileItem &item, const CVariant &variant)
{
  if (variant.isMember("Genres"))
  {
    // get all genres
    std::vector<std::string> genres;
    const auto& streams = variant["Genres"];
    for (auto streamIt = streams.begin_array(); streamIt != streams.end_array(); ++streamIt)
    {
      const auto stream = *streamIt;
      genres.push_back(stream.asString());
    }
    item.GetMusicInfoTag()->SetGenre(genres);
  }
  if (variant.isMember("ArtistItems"))
  {
    // get all artists
    std::vector<std::string> artists;
    const auto& artistsItems = variant["ArtistItems"];
    for (auto artistIt = artistsItems.begin_array(); artistIt != artistsItems.end_array(); ++artistIt)
    {
      const auto artist = *artistIt;
      artists.push_back(artist["Name"].asString());
    }
    item.GetMusicInfoTag()->SetArtist(StringUtils::Join(artists, ","));
    item.GetMusicInfoTag()->SetAlbumArtist(artists);
    item.SetProperty("artist", StringUtils::Join(artists, ","));
    item.GetMusicInfoTag()->SetArtistDesc(StringUtils::Join(artists, ","));
  }
}

void CEmbyUtils::GetMediaDetals(CFileItem &item, const CVariant &variant, std::string id)
{
  if (variant.isMember("MediaStreams") && variant["MediaStreams"].isArray())
  {
    CStreamDetails streamDetail;
    const auto& streams = variant["MediaStreams"];
    int iSubPart = 1;
    for (auto streamIt = streams.begin_array(); streamIt != streams.end_array(); ++streamIt)
    {
      const auto stream = *streamIt;
      const auto streamType = stream["Type"].asString();
      if (streamType == "Video")
      {
        CStreamDetailVideo* videoStream = new CStreamDetailVideo();
        videoStream->m_strCodec = stream["Codec"].asString();
        videoStream->m_fAspect = (float)stream["Width"].asInteger()/stream["Height"].asInteger();
        videoStream->m_strLanguage = stream["Language"].asString();
        videoStream->m_iWidth = static_cast<int>(stream["Width"].asInteger());
        videoStream->m_iHeight = static_cast<int>(stream["Height"].asInteger());
        videoStream->m_iDuration = item.GetVideoInfoTag()->m_duration;

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
        
        if (stream["IsExternal"].asBoolean() && stream["IsTextSubtitleStream"].asBoolean())
        {
          CURL url(item.GetPath());
          url.SetFileName("Videos/" + id + "/" + id + "/Subtitles/" + stream["Index"].asString() + "/Stream.srt");
          std::string propertyKey = StringUtils::Format("subtitle:%i", iSubPart);
          std::string propertyLangKey = StringUtils::Format("subtitle:%i_language", iSubPart);
          item.SetProperty(propertyKey, url.Get());
          item.SetProperty(propertyLangKey, stream["Language"].asString());
          iSubPart ++;
        }
      }
    }
    item.GetVideoInfoTag()->m_streamDetails = streamDetail;
  }
}

void CEmbyUtils::RemoveSubtitleProperties(CFileItem &item)
{
}

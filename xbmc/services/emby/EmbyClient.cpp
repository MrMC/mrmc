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

#include <atomic>
#include <memory>
#include <algorithm>

#include "EmbyClient.h"
#include "EmbyClientSync.h"
#include "EmbyViewCache.h"
#include "EmbyServices.h"
#include "EmbyUtils.h"

#include "Application.h"
#include "URL.h"
#include "GUIUserMessages.h"
#include "TextureCache.h"

#include "dialogs/GUIDialogBusy.h"
#include "filesystem/CurlFile.h"
#include "filesystem/StackDirectory.h"
#include "guilib/GUIWindowManager.h"
#include "network/Network.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Base64URL.h"
#include "utils/JobManager.h"
#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"
#include "video/VideoInfoTag.h"
#include "music/tags/MusicInfoTag.h"

#include <string>

static const std::string MoviesFields = {
  "DateCreated,Genres,MediaStreams,Overview,Path"
};

static const std::string TVShowsFields = {
  "DateCreated,Genres,MediaStreams,Overview,ShortOverview,Path,RecursiveItemCount"
};

class CEmbyUtilsJob: public CJob
{
public:
  CEmbyUtilsJob(const std::string &path, std::vector<std::string> itemIDs)
  :m_path(path),
  m_itemIDs(itemIDs)
  {
  }
  virtual ~CEmbyUtilsJob()
  {
  }
  virtual bool DoWork()
  {
    CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(m_path);
    if (client && m_itemIDs.size() > 0)
      client->UpdateViewItems(m_itemIDs);
    return true;
  }
private:
  const std::string &m_path;
  std::vector<std::string> m_itemIDs;
};

class CThreadedFetchViewItems : public CThread
{
public:
  CThreadedFetchViewItems(CEmbyClient *client, CEvent &event, CEmbyViewCachePtr &view, const CURL &url, const std::string &type)
  : CThread("CThreadedFetchViewItems")
  , m_url(url)
  , m_client(client)
  , m_type(type)
  , m_view(view)
  , m_event(event)
  {
    Create();
  }
  virtual ~CThreadedFetchViewItems()
  {
    StopThread();
  }
  void Cancel()
  {
    m_bStop = true;
  }
protected:
  virtual void Process()
  {
    m_client->FetchViewItems(m_view, m_url, m_type);
    m_event.Set();
  }
  const CURL m_url;
  CEmbyClient *m_client;
  const std::string m_type;
  CEmbyViewCachePtr m_view;
  CEvent &m_event;
};

CEmbyClient::CEmbyClient()
{
  m_local = true;
  m_owned = true;
  m_presence = true;
  m_protocol = "http";
  m_needUpdate = false;
  m_clientSync = nullptr;
  m_viewMoviesFilter = nullptr;
  m_viewTVShowsFilter = nullptr;
}

CEmbyClient::~CEmbyClient()
{
  SAFE_DELETE(m_clientSync);
}

bool CEmbyClient::Init(const EmbyServerInfo &serverInfo)
{
  m_local = true;
  m_serverInfo = serverInfo;
  m_owned = serverInfo.UserType == "Linked";

  // protocol (http/https) and port will be in ServerUrl
  CURL curl(m_serverInfo.ServerURL);
  curl.SetProtocolOptions("&X-MediaBrowser-Token=" + serverInfo.AccessToken);
  m_url = curl.Get();
  m_protocol = curl.GetProtocol();

  if (m_clientSync)
    SAFE_DELETE(m_clientSync);

  m_clientSync = new CEmbyClientSync(m_serverInfo.ServerName, m_serverInfo.ServerURL,
    CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_UUID).c_str(), serverInfo.AccessToken);
  m_clientSync->Start();

  return true;
}

void CEmbyClient::SetWatched(CFileItem &item)
{
  std::string itemId = item.GetMediaServiceId();
  std::string content = item.GetProperty("MediaServicesContent").asString();
  CDateTime lastPlayed;
  if (item.IsVideo())
    lastPlayed = item.GetVideoInfoTag()->m_lastPlayed;
  else if (item.IsAudio())
    lastPlayed = item.GetMusicInfoTag()->m_lastPlayed;
  else
    lastPlayed = CDateTime::GetUTCDateTime();

  if (content == "movies")
  {
    CSingleLock lock(m_viewMoviesLock);
    for (auto &view : m_viewMovies)
    {
      bool hit = view->SetWatched(itemId,
        item.GetVideoInfoTag()->m_playCount, item.GetVideoInfoTag()->m_resumePoint.timeInSeconds);
      if (hit)
        break;
    }
  }
  else if (content == "tvshows")
  {
    CSingleLock lock(m_viewTVShowsLock);
    for (auto &view : m_viewTVShows)
    {
      bool hit = view->SetWatched(itemId,
        item.GetVideoInfoTag()->m_playCount, item.GetVideoInfoTag()->m_resumePoint.timeInSeconds);
      if (hit)
        break;
    }
  }

  // POST to /Users/{UserId}/PlayedItems/{Id}
  CURL curl(m_url);
  curl.SetFileName("emby/Users/" + GetUserID() + "/PlayedItems/" + itemId);
  curl.SetOptions("");
  // and add the DatePlayed URL parameter
  curl.SetOption("DatePlayed",
    StringUtils::Format("%04i%02i%02i%02i%02i%02i",
      lastPlayed.GetYear(),
      lastPlayed.GetMonth(),
      lastPlayed.GetDay(),
      lastPlayed.GetHour(),
      lastPlayed.GetMinute(),
      lastPlayed.GetSecond()));

  std::string data;
  std::string response;
  // execute the POST request
  XFILE::CCurlFile curlfile;
  if (curlfile.Post(curl.Get(), data, response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    if (!response.empty())
      CLog::Log(LOGDEBUG, "CEmbyClient::SetWatched %s", response.c_str());
#endif
  }
}

void CEmbyClient::SetUnWatched(CFileItem &item)
{
  std::string itemId = item.GetMediaServiceId();
  std::string content = item.GetProperty("MediaServicesContent").asString();
  if (content == "movies")
  {
    CSingleLock lock(m_viewMoviesLock);
    for (auto &view : m_viewMovies)
    {
      bool hit = view->SetUnWatched(itemId);
      if (hit)
        break;
    }
  }
  else if (content == "tvshows")
  {
    CSingleLock lock(m_viewTVShowsLock);
    for (auto &view : m_viewTVShows)
    {
      bool hit = view->SetUnWatched(itemId);
      if (hit)
        break;
    }
  }
  else if (content == "song")
  {
  }

  // DELETE to /Users/{UserId}/PlayedItems/{Id}
  CURL curl(m_url);
  curl.SetFileName("emby/Users/" + GetUserID() + "/PlayedItems/" + itemId);
  curl.SetOptions("");

  std::string data;
  std::string response;
  // execute the DELETE request
  XFILE::CCurlFile curlfile;
  if (curlfile.Delete(curl.Get(), data, response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    if (!response.empty())
      CLog::Log(LOGDEBUG, "CEmbyUtils::SetUnWatched %s", response.c_str());
#endif
  }
}

void CEmbyClient::UpdateLibrary(const std::string &content)
{
  bool viewHit = false;
  CVariant nullvariant(CVariant::VariantTypeNull);
  if (content == "movies")
  {
    CSingleLock lock(m_viewMoviesLock);
    for (auto &view : m_viewMovies)
    {
      viewHit = true;
      view->SetItems(nullvariant);
    }
  }
  else if (content == "tvshows")
  {
    CSingleLock lock(m_viewTVShowsLock);
    for (auto &view : m_viewTVShows)
    {
      viewHit = true;
      view->SetItems(nullvariant);
    }
  }
  if (viewHit)
  {
    CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
    g_windowManager.SendThreadMessage(msg);
  }
}

bool CEmbyClient::GetMovies(CFileItemList &items, std::string url, bool fromfilter)
{
  bool rtn = false;
  CURL curl(url);
  if (fromfilter)
  {
    CSingleLock lock(m_viewMoviesFilterLock);
    FetchViewItems(m_viewMoviesFilter, curl, EmbyTypeMovie);
    if (m_viewMoviesFilter->ItemsValid())
      rtn = CEmbyUtils::ParseEmbyVideos(items, curl, m_viewMoviesFilter->GetItems(), MediaTypeMovie);
  }
  else
  {
    std::string parentId;
    if (curl.HasOption("ParentId"))
      curl.GetOption("ParentId", parentId);

    CSingleLock lock(m_viewMoviesLock);
    for (auto &view : m_viewMovies)
    {
      if (!parentId.empty() && parentId != view->GetId())
        continue;

      if (!view->ItemsValid())
        FetchViewItems(view, curl, EmbyTypeMovie);
      if (view->ItemsValid())
        rtn = CEmbyUtils::ParseEmbyVideos(items, curl, view->GetItems(), MediaTypeMovie);
      if (rtn)
        break;
    }
  }
  return rtn;
}

bool CEmbyClient::GetMoviesFilter(CFileItemList &items, std::string url, std::string filter)
{
  CSingleLock lock(m_viewMoviesFilterLock);
  EmbyViewContent filterView;
  filterView.id = m_viewMovies[0]->GetId();
  filterView.name = filter;
  m_viewMoviesFilter = CEmbyViewCachePtr(new CEmbyViewCache);
  m_viewMoviesFilter->Init(filterView);

  CURL curl(url);
  FetchFilterItems(m_viewMoviesFilter, curl, EmbyTypeMovie, filter);
  bool rtn = false;
  if (m_viewMoviesFilter->ItemsValid())
    rtn = CEmbyUtils::ParseEmbyMoviesFilter(items, curl, m_viewMoviesFilter->GetItems(), filter);
  return rtn;
}

bool CEmbyClient::GetTVShows(CFileItemList &items, std::string url, bool fromfilter)
{
  bool rtn = false;
  CURL curl(url);
  if (fromfilter)
  {
    CSingleLock lock(m_viewTVShowsFilterLock);
    FetchViewItems(m_viewTVShowsFilter, curl, EmbyTypeSeries);
    if (m_viewTVShowsFilter->ItemsValid())
      rtn = CEmbyUtils::ParseEmbySeries(items, curl, m_viewTVShowsFilter->GetItems());
  }
  else
  {
    std::string parentId;
    if (curl.HasOption("ParentId"))
      curl.GetOption("ParentId", parentId);

    CSingleLock lock(m_viewTVShowsLock);
    for (auto &view : m_viewTVShows)
    {
      if (!parentId.empty() && parentId != view->GetId())
        continue;

      if (!view->ItemsValid())
        FetchViewItems(view, curl, EmbyTypeSeries);
      if (view->ItemsValid())
        rtn = CEmbyUtils::ParseEmbySeries(items, curl, view->GetItems());
      if (rtn)
        break;
    }
  }
  return rtn;
}

bool CEmbyClient::GetTVShowsFilter(CFileItemList &items, std::string url, std::string filter)
{
  CSingleLock lock(m_viewTVShowsFilterLock);
  EmbyViewContent filterView;
  filterView.name = filter;
  m_viewTVShowsFilter = CEmbyViewCachePtr(new CEmbyViewCache);
  m_viewTVShowsFilter->Init(filterView);

  CURL curl(url);
  FetchFilterItems(m_viewTVShowsFilter, curl, EmbyTypeSeries, filter);
  bool rtn = false;
  if (m_viewTVShowsFilter->ItemsValid())
    rtn = CEmbyUtils::ParseEmbyTVShowsFilter(items, curl, m_viewTVShowsFilter->GetItems(), filter);
  return rtn;
}

bool CEmbyClient::GetMusicArtists(CFileItemList &items, std::string url)
{
  bool rtn = false;
  CURL curl(url);
  std::string parentId;
  if (curl.HasOption("ParentId"))
    curl.GetOption("ParentId", parentId);

  CSingleLock lock(m_viewMusicLock);
  for (auto &view : m_viewMusic)
  {
    if (!parentId.empty() && parentId != view->GetId())
      continue;

    if (!view->ItemsValid())
      FetchViewItems(view, curl, EmbyTypeMusicArtist);
    if (view->ItemsValid())
      rtn = CEmbyUtils::ParseEmbyArtists(items, curl, view->GetItems());
    if (rtn)
      break;
  }
  return rtn;
}

void CEmbyClient::AddNewViewItems(const std::vector<std::string> &ids)
{
#if defined(EMBY_DEBUG_VERBOSE)
  CLog::Log(LOGDEBUG, "CEmbyClient::AddNewViewItem");
#endif
  const CVariant variant = FetchItemByIds(ids);
  if (variant.isNull() || !variant.isObject() || !variant.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyClient::AddNewViewItems invalid response");
    return;
  }

  int itemsAdded = 0;
  const auto& variantItems = variant["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    const std::string viewId = FetchViewIdByItemId((*variantItemIt)["Id"].asString());
    itemsAdded += AppendItemToCache(viewId, *variantItemIt) ? 1:0;
  }
  if (itemsAdded)
  {
    // GUI_MSG_UPDATE will Refresh and that will pull a new list of items for display
    // and keep the same selection point.
    CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
    g_windowManager.SendThreadMessage(msg);
  }
}

void CEmbyClient::UpdateViewItems(const std::vector<std::string> &ids)
{
#if defined(EMBY_DEBUG_VERBOSE)
  CLog::Log(LOGDEBUG, "CEmbyClient::UpdateViewItems");
#endif
  const CVariant variant = FetchItemByIds(ids);
  if (variant.isNull() || !variant.isObject() || !variant.isMember("Items"))
  {
    CLog::Log(LOGERROR, "CEmbyClient::UpdateViewItems invalid response");
    return;
  }

  bool updateArtwork = false;
  const auto& variantItems = variant["Items"];
  for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
  {
    UpdateItemInCache(*variantItemIt);

    std::map<std::string, CVariant> variantMap;
    variantMap["Items"].push_back(*variantItemIt);

    CFileItemPtr item = CEmbyUtils::ToFileItemPtr(this, variantMap);
    if (item != nullptr)
    {
      // hack, if season state has changed, maybe show state has changed as well. seems like emby bug
      if (item->GetVideoInfoTag()->m_type == MediaTypeSeason &&  item->HasProperty("EmbySeriesID"))
      {
        std::vector<std::string> seriesIds;
        seriesIds.push_back(item->GetProperty("EmbySeriesID").asString());
        UpdateViewItems(seriesIds);
      }

      // artwork might have changed, just pop the image cache so next
      // time a user nav's to the item, they will be updated.
      std::string thumb = item->GetArt("thumb");
      if (!thumb.empty() && CTextureCache::GetInstance().HasCachedImage(thumb))
      {
        updateArtwork = true;
        CTextureCache::GetInstance().ClearCachedImage(thumb);
      }

      std::string banner = item->GetArt("banner");
      if (!banner.empty() && CTextureCache::GetInstance().HasCachedImage(banner))
      {
        updateArtwork = true;
        CTextureCache::GetInstance().ClearCachedImage(banner);
      }

      std::string fanart = item->GetArt("fanart");
      if (!fanart.empty() && CTextureCache::GetInstance().HasCachedImage(fanart))
      {
        updateArtwork = true;
        CTextureCache::GetInstance().ClearCachedImage(fanart);
      }

      // -------------
      CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_ITEM, 0, item);
      g_windowManager.SendThreadMessage(msg);
    }
  }
  if (updateArtwork)
  {
    CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_REFRESH_THUMBS);
    g_windowManager.SendThreadMessage(msg);
  }
}

void CEmbyClient::RemoveViewItems(const std::vector<std::string> &ids)
{
#if defined(EMBY_DEBUG_VERBOSE)
  CLog::Log(LOGDEBUG, "CEmbyClient::RemoveViewItems");
#endif
  int itemsRemoved = 0;
  for (auto &id : ids)
    itemsRemoved += RemoveItemFromCache(id) ? 1:0;

  if (itemsRemoved)
  {
    // GUI_MSG_UPDATE will Refresh and that will pull a new list of items for display
    // and keep the same selection point.
    CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
    g_windowManager.SendThreadMessage(msg);
  }
}

std::string CEmbyClient::GetUrl()
{
  return m_url;
}

std::string CEmbyClient::GetHost()
{
  CURL url(m_url);
  return url.GetHostName();
}

int CEmbyClient::GetPort()
{
  CURL url(m_url);
  return url.GetPort();
}

std::string CEmbyClient::GetUserID()
{
  return m_serverInfo.UserId;
}

const std::vector<EmbyViewInfo> CEmbyClient::GetViewInfoForMovieContent() const
{
  std::vector<EmbyViewInfo> infos;
  CSingleLock lock(m_viewMoviesLock);
  for (auto &view : m_viewMovies)
    infos.push_back(view->GetInfo());
  return infos;
}

const std::vector<EmbyViewInfo> CEmbyClient::GetViewInfoForMusicContent() const
{
  std::vector<EmbyViewInfo> infos;
  CSingleLock lock(m_viewMusicLock);
  for (auto &view : m_viewMusic)
    infos.push_back(view->GetInfo());
  return infos;
}

const std::vector<EmbyViewInfo> CEmbyClient::GetViewInfoForPhotoContent() const
{
  CSingleLock lock(m_viewPhotosLock);
  std::vector<EmbyViewInfo> infos;
  for (auto &view : m_viewPhotos)
    infos.push_back(view->GetInfo());
  return infos;
}

const std::vector<EmbyViewInfo> CEmbyClient::GetViewInfoForTVShowContent() const
{
  CSingleLock lock(m_viewTVShowsLock);
  std::vector<EmbyViewInfo> infos;
  for (auto &view : m_viewTVShows)
    infos.push_back(view->GetInfo());
  return infos;
}

const std::string CEmbyClient::FormatContentTitle(const std::string contentTitle) const
{
  std::string owned = IsOwned() ? "O":"S";
  std::string title = StringUtils::Format("Emby(%s) - %s - %s %s",
              owned.c_str(), GetServerName().c_str(), contentTitle.c_str(), GetPresence()? "":"(off-line)");
  return title;
}

bool CEmbyClient::IsSameClientHostName(const CURL& url)
{
  CURL real_url(url);
  if (real_url.GetProtocol() == "emby")
    real_url = CURL(Base64URL::Decode(URIUtils::GetFileName(real_url)));

  if (URIUtils::IsStack(real_url.Get()))
    real_url = CURL(XFILE::CStackDirectory::GetFirstStackedFile(real_url.Get()));
  
  return GetHost() == real_url.GetHostName();
}

bool CEmbyClient::FetchViews()
{
#if defined(EMBY_DEBUG_VERBOSE)
  CLog::Log(LOGDEBUG, "CEmbyClient::FetchViews");
#endif
  bool rtn = false;
  XFILE::CCurlFile emby;
  emby.SetRequestHeader("Cache-Control", "no-cache");
  emby.SetRequestHeader("Content-Type", "application/json");
  CEmbyUtils::PrepareApiCall(m_serverInfo.UserId, m_serverInfo.AccessToken, emby);

  CURL curl(m_url);
  // /Users/{UserId}/Views
  curl.SetFileName(curl.GetFileName() + "Users/" + m_serverInfo.UserId + "/Views");
  std::string path = curl.Get();
  std::string response;
  if (emby.Get(path, response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CEmbyClient::FetchViews %s", response.c_str());
#endif

    CVariant resultObject;
    if (!CJSONVariantParser::Parse(response, resultObject) ||
        !resultObject.isObject() || !resultObject.isMember("Items"))
    {
      CLog::Log(LOGERROR, "CEmbyClient::FetchViews: invalid response for library views from %s", CURL::GetRedacted(path).c_str());
      return false;
    }

    static const std::string PropertyViewId = "Id";
    static const std::string PropertyViewName = "Name";
    static const std::string PropertyViewETag = "Etag";
    static const std::string PropertyViewServerID = "ServerId";
    static const std::string PropertyViewCollectionType = "CollectionType";

    std::vector<EmbyViewContent> views;
    const std::vector<std::string> mediaTypes = {
    "movies",
  // musicvideos,
  // homevideos,
    "tvshows",
  // livetv,
  // channels,
    "music"
    };

    const auto& viewsObject = resultObject["Items"];
    for (auto viewIt = viewsObject.begin_array(); viewIt != viewsObject.end_array(); ++viewIt)
    {
      const auto view = *viewIt;
      if (!view.isObject() || !view.isMember(PropertyViewId) ||
          !view.isMember(PropertyViewName) || !view.isMember(PropertyViewCollectionType))
        continue;

      std::string type = view[PropertyViewCollectionType].asString();
      if (type.empty())
        continue;

      if (!std::any_of(mediaTypes.cbegin(), mediaTypes.cend(),
          [type](const MediaType& mediaType)
          {
            return MediaTypes::IsMediaType(type, mediaType);
          }))
        continue;

      EmbyViewContent libraryView;
      libraryView.id = view[PropertyViewId].asString();
      libraryView.name = view[PropertyViewName].asString();
      libraryView.etag = view[PropertyViewETag].asString();
      libraryView.prefix = "Users/" + m_serverInfo.UserId + "/Items?ParentId=" + view[PropertyViewId].asString();
      libraryView.serverId = view[PropertyViewServerID].asString();
      libraryView.iconId = view["ImageTags"]["Primary"].asString();
      libraryView.mediaType = type;
      if (libraryView.id.empty() || libraryView.name.empty())
        continue;

      views.push_back(libraryView);
    }

    for (const auto &content : views)
    {
      if (content.mediaType == "movies")
      {
        CSingleLock lock(m_viewMoviesLock);
        CEmbyViewCachePtr viewCache = CEmbyViewCachePtr(new CEmbyViewCache);
        viewCache->Init(content);
        m_viewMovies.push_back(viewCache);
      }
      else if (content.mediaType == "tvshows")
      {
        CSingleLock lock(m_viewTVShowsLock);
        CEmbyViewCachePtr viewCache = CEmbyViewCachePtr(new CEmbyViewCache);
        viewCache->Init(content);
        m_viewTVShows.push_back(viewCache);
      }
      else if (content.mediaType == "music")
      {
        CSingleLock lock(m_viewMusicLock);
        CEmbyViewCachePtr viewCache = CEmbyViewCachePtr(new CEmbyViewCache);
        viewCache->Init(content);
        m_viewMusic.push_back(viewCache);
      }
      else if (content.mediaType == "photo")
      {
        CSingleLock lock(m_viewPhotosLock);
        CEmbyViewCachePtr viewCache = CEmbyViewCachePtr(new CEmbyViewCache);
        viewCache->Init(content);
        m_viewPhotos.push_back(viewCache);
      }
      else
      {
        CLog::Log(LOGDEBUG, "CEmbyClient::ParseView %s found unhandled content type %s",
          m_serverInfo.ServerName.c_str(), content.name.c_str());
      }
    }

    if (!views.empty())
    {
      CLog::Log(LOGDEBUG, "CEmbyClient::ParseView %s found %d movies view",
        m_serverInfo.ServerName.c_str(), (int)m_viewMovies.size());
      CLog::Log(LOGDEBUG, "CEmbyClient::ParseView %s found %d tvshows view",
        m_serverInfo.ServerName.c_str(), (int)m_viewTVShows.size());
      CLog::Log(LOGDEBUG, "CEmbyClient::ParseView %s found %d Music view",
        m_serverInfo.ServerName.c_str(), (int)m_viewMusic.size());
      CLog::Log(LOGDEBUG, "CEmbyClient::ParseView %s found %d photos view",
        m_serverInfo.ServerName.c_str(), (int)m_viewPhotos.size());
      rtn = true;
    }
  }

  return rtn;
}

bool CEmbyClient::FetchViewItems(CEmbyViewCachePtr &view, const CURL &url, const std::string &type)
{
#if defined(EMBY_DEBUG_VERBOSE)
  CLog::Log(LOGDEBUG, "CEmbyClient::FetchViewItems");
#endif
  bool rtn = false;
  CURL curl(url);
  if (type == EmbyTypeMovie)
  {
    curl.SetFileName("Users/" + GetUserID() + "/Items");
    curl.SetOption("IncludeItemTypes", type);
    curl.SetOption("Recursive", "true");
    curl.SetOption("Fields", MoviesFields);
    // must be last, wtf?
    if (view && !view->GetId().empty())
      curl.SetOption("ParentId", view->GetId());
  }
  else if (type == EmbyTypeSeries)
  {
    // also known as TVShows for non-eu'ers
    curl.SetFileName("Users/" + GetUserID() + "/Items");
    curl.SetOption("IncludeItemTypes", type);
    curl.SetOption("Fields", TVShowsFields);
    // must be last, wtf?
    if (view && !view->GetId().empty())
      curl.SetOption("ParentId", view->GetId());
  }
  else if (type == EmbyTypeMusicArtist)
  {
    //TODO: why is this different than for movies/series ?
    // maybe should be using "MusicArtist" ?
    curl.SetFileName("/emby/Artists");
    curl.SetOption("Fields", "Etag,Genres");
    curl.SetProtocolOption("userId", GetUserID());
  }
  else
  {
    CLog::Log(LOGDEBUG, "CEmbyClient::FetchViewItems unknown type: %s", type.c_str());
    return false;
  }
  std::string path = curl.Get();
  CVariant variant = CEmbyUtils::GetEmbyCVariant(path);
  if (variant.isNull())
  {
    CLog::Log(LOGERROR, "CEmbyClient::FetchViewItems: invalid response for views items from %s", CURL::GetRedacted(path).c_str());
    return false;
  }

  view->SetItems(variant);
  return rtn;
}

bool CEmbyClient::DoThreadedFetchViewItems(CEmbyViewCachePtr &view, const CURL &url, const std::string &type)
{
  CEvent threadDone;
  threadDone.Reset();
  CThreadedFetchViewItems threadedFetchViewItems(this, threadDone, view, url, type);

  CGUIDialogBusy *busyDialog = (CGUIDialogBusy*)g_windowManager.GetWindow(WINDOW_DIALOG_BUSY);
  if (busyDialog)
  {
    busyDialog->Open();
    while(!threadDone.WaitMSec(10))
    {
      g_windowManager.ProcessRenderLoop(false);
    }
    busyDialog->Close();
  }
  return true;
}

bool CEmbyClient::FetchFilterItems(CEmbyViewCachePtr &view, const CURL &url, const std::string &type, const std::string &filter)
{
#if defined(EMBY_DEBUG_VERBOSE)
  CLog::Log(LOGDEBUG, "CEmbyClient::FetchFilterItems");
#endif
  bool rtn = false;

  CURL curl(url);
  if (type == EmbyTypeMovie)
  {
    if (filter != "Collections")
    {
      curl.SetFileName("emby/"+ filter);
      curl.SetOption("IncludeItemTypes", EmbyTypeMovie);
    }
    else
    {
      curl.SetFileName("Users/" + GetUserID() + "/Items");
      curl.SetOption("IncludeItemTypes", EmbyTypeBoxSet);
      curl.SetOption("Recursive", "true");
      curl.SetOption("ParentId", "");
    }
    curl.SetOption("Fields", "Etag,DateCreated,ImageTags");
  }
  else if (type == EmbyTypeSeries)
  {
    curl.SetFileName("emby/" + filter);
    curl.SetOption("IncludeItemTypes", EmbyTypeSeries);
    curl.SetOption("Fields", "Etag,DateCreated,ImageTags");
  }
  else
  {
    CLog::Log(LOGDEBUG, "CEmbyClient::FetchFilterItems unknown type: %s", type.c_str());
    return false;
  }
  std::string path = curl.Get();
  CVariant variant = CEmbyUtils::GetEmbyCVariant(path);
  if (variant.isNull())
  {
    CLog::Log(LOGERROR, "CEmbyClient::FetchFilterItems: invalid response for views items from %s", CURL::GetRedacted(path).c_str());
    return false;
  }

  view->SetItems(variant);

  return rtn;
}

void CEmbyClient::SetPresence(bool presence)
{
  if (m_presence != presence)
    m_presence = presence;
}

const CVariant CEmbyClient::FetchItemById(const std::string &Id)
{
  std::vector<std::string> Ids;
  Ids.push_back(Id);
  return FetchItemByIds(Ids);
}

const CVariant CEmbyClient::FetchItemByIds(const std::vector<std::string> &Ids)
{
  if (Ids.size() < 1)
    return CVariant(CVariant::VariantTypeNull);

  static const std::string Fields = {
    "DateCreated,Genres,MediaStreams,Overview,ShortOverview,Path,ImageTags,Taglines,RecursiveItemCount,ProviderIds"
  };

  CURL curl(m_url);
  curl.SetFileName("emby/Users/" + GetUserID() + "/Items/");
  curl.SetOptions("");
  curl.SetOption("Ids", StringUtils::Join(Ids, ","));
  curl.SetOption("Fields", Fields);
  const CVariant variant = CEmbyUtils::GetEmbyCVariant(curl.Get());
  return variant;
}

const std::string CEmbyClient::FetchViewIdByItemId(const std::string &Id)
{
  CURL curl(m_url);
  curl.SetFileName("emby/Items/" + Id + "/Ancestors");
  curl.SetOptions("");
  curl.SetOption("UserId", GetUserID());
  const CVariant views = CEmbyUtils::GetEmbyCVariant(curl.Get());

  std::string ViewId;
  for (auto variantItemIt = views.begin_array(); variantItemIt != views.end_array(); ++variantItemIt)
  {
    if ((*variantItemIt)["Type"].asString() == "CollectionFolder")
    {
      ViewId = (*variantItemIt)["Id"].asString();
      break;
    }
  }
  return ViewId;
}

bool CEmbyClient::UpdateItemInCache(const CVariant &variant)
{
  std::string type = variant["Type"].asString();
  if (type == EmbyTypeMovie)
  {
    CSingleLock lock(m_viewMoviesLock);
    for (auto &view : m_viewMovies)
    {
      if (view->ItemsValid() && view->UpdateItem(variant))
        return true;
    }
  }
  else if (type == EmbyTypeSeries)
  {
    CSingleLock lock(m_viewTVShowsLock);
    for (auto &view : m_viewTVShows)
    {
      if (view->ItemsValid() && view->UpdateItem(variant))
        return true;
    }
  }
  else if (type == EmbyTypeMusicArtist)
  {
    CSingleLock lock(m_viewMusicLock);
    for (auto &view : m_viewMusic)
    {
      if (view->ItemsValid() && view->UpdateItem(variant))
        return true;
    }
  }
  return false;
}

bool CEmbyClient::AppendItemToCache(const std::string &viewId, const CVariant &variant)
{
  if (viewId.empty())
  {
    CLog::Log(LOGDEBUG, "CEmbyClient::AppendItemToCache viewId is null: %s",
      variant["Type"].asString().c_str());
    return false;
  }

  std::string type = variant["Type"].asString();
  if (type == EmbyTypeMovie)
  {
    CSingleLock lock(m_viewMoviesLock);
    for (auto &view : m_viewMovies)
    {
      if (viewId == view->GetId() && view->ItemsValid())
        return view->AppendItem(variant);
    }
  }
  else if (type == EmbyTypeSeries)
  {
    CSingleLock lock(m_viewTVShowsLock);
    for (auto &view : m_viewTVShows)
    {
      if (viewId == view->GetId() && view->ItemsValid())
        return view->AppendItem(variant);
    }
  }
  else if (type == EmbyTypeMusicArtist)
  {
    CSingleLock lock(m_viewMusicLock);
    for (auto &view : m_viewMusic)
    {
      if (viewId == view->GetId() && view->ItemsValid())
        return view->AppendItem(variant);
    }
  }
  return false;
}

bool CEmbyClient::RemoveItemFromCache(const std::string &itemId)
{
  if (itemId.empty())
  {
    CLog::Log(LOGDEBUG, "CEmbyClient::RemoveItemFromCache itemId is null");
    return false;
  }
  // unfortunately, we only have the item Id and will
  // have to search over all cached views.
  {
    CSingleLock lock(m_viewMoviesLock);
    for (auto &view : m_viewMovies)
    {
      if (view->RemoveItem(itemId))
        return true;
    }
  }
  {
    CSingleLock lock(m_viewTVShowsLock);
    for (auto &view : m_viewTVShows)
    {
      if (view->RemoveItem(itemId))
        return true;
    }
  }
  {
    CSingleLock lock(m_viewMusicLock);
    for (auto &view : m_viewMusic)
    {
      if (view->RemoveItem(itemId))
        return true;
    }
  }
  return false;
}

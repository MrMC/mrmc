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
#include "EmbyUtils.h"

#include "Application.h"
#include "URL.h"
#include "filesystem/CurlFile.h"
#include "filesystem/StackDirectory.h"
#include "network/Network.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Base64.h"
#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"
#include "video/VideoInfoTag.h"
#include "music/tags/MusicInfoTag.h"

#include <string>

CEmbyClient::CEmbyClient()
{
  m_local = true;
  m_owned = true;
  m_presence = true;
  m_protocol = "http";
  m_needUpdate = false;
  m_clientSync = nullptr;
  m_viewItems = new CFileItemList();
}

CEmbyClient::~CEmbyClient()
{
  SAFE_DELETE(m_clientSync);
  SAFE_DELETE(m_viewItems);
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

void CEmbyClient::ClearViewItems()
{
  CSingleLock lock(m_viewItemsLock);
  m_viewItems->ClearItems();
}

static bool IsSameEmbyID(const CFileItemPtr &a, const CFileItemPtr &b)
{
  const std::string testIdA = a->GetMediaServiceId();
  const std::string testIdB = b->GetMediaServiceId();
  if (!testIdA.empty() && !testIdB.empty())
    return (testIdA == testIdB);
  return false;
}

static bool IsSameEmbyID(const CFileItemPtr &a, const std::string &serviceId)
{
  const std::string testId = a->GetMediaServiceId();
  if (!testId.empty())
    return (testId == serviceId);
  return false;
}

void CEmbyClient::AddViewItem(const CFileItemPtr &item)
{
  CSingleLock lock(m_viewItemsLock);
  for (int i = 0; i < m_viewItems->Size(); ++i)
  {
    if (IsSameEmbyID(m_viewItems->Get(i), item))
      return;
  }
  m_viewItems->Add(item);
}

void CEmbyClient::AddViewItems(const CFileItemList &items)
{
  CSingleLock lock(m_viewItemsLock);
  if (m_viewItems->IsEmpty())
  {
    m_viewItems->Append(items);
    return;
  }

  for (int j = 0; j < items.Size(); ++j)
  {
    bool found = false;
    for (int i = 0; i < m_viewItems->Size(); ++i)
    {
      if (IsSameEmbyID(m_viewItems->Get(i), items[j]))
      {
        found = true;
        break;
      }
    }
    if (!found)
      m_viewItems->Add(items[j]);
  }
}

void CEmbyClient::AddNewViewItem(const std::string &serviceId)
{
  // TODO: fetch id details
  // TODO: create new CFileItemPtr based on id details
  // TODO: add item and add item to relavent windows
  CLog::Log(LOGERROR, "CEmbyClient::AddNewViewItem: failed to add item with id \"%s\"", serviceId.c_str());
  const CVariant object = FetchItemById(serviceId);
  CFileItemPtr item = CEmbyUtils::ToFileItemPtr(this, object);
  if (item != nullptr)
  {
  }
}

void CEmbyClient::UpdateViewItem(const std::string &serviceId)
{
  CSingleLock lock(m_viewItemsLock);
  for (int i = 0; i < m_viewItems->Size(); ++i)
  {
    const CFileItemPtr &item = m_viewItems->Get(i);
    if (IsSameEmbyID(item, serviceId))
    {
      CLog::Log(LOGDEBUG, "CEmbyClient::UpdateViewItem: \"%s\"", item->GetLabel().c_str());
      // TODO: update the item
      // TODO: update any window containing the item
      const CVariant object = FetchItemById(serviceId);
      CFileItemPtr item = CEmbyUtils::ToFileItemPtr(this, object);
      if (item != nullptr)
      {
      }
      return;
    }
  }
  CLog::Log(LOGERROR, "CEmbyClient::UpdateViewItem: failed to find/update item with id \"%s\"", serviceId.c_str());
}

void CEmbyClient::RemoveViewItem(const std::string &serviceId)
{
  CSingleLock lock(m_viewItemsLock);
  for (int i = 0; i < m_viewItems->Size(); ++i)
  {
    const CFileItemPtr &item = m_viewItems->Get(i);
    if (IsSameEmbyID(item, serviceId))
    {
      CLog::Log(LOGDEBUG, "CEmbyClient::RemoveViewItem: \"%s\"", item->GetLabel().c_str());
      m_viewItems->Remove(i);
      // TODO: remove the item from any window
      return;
    }
  }
  CLog::Log(LOGERROR, "CEmbyClient::RemoveViewItem: failed to find/remove item with id \"%s\"", serviceId.c_str());
}

CFileItemPtr CEmbyClient::FindViewItem(const std::string &serviceId)
{
  CSingleLock lock(m_viewItemsLock);
  for (int i = 0; i < m_viewItems->Size(); ++i)
  {
    CFileItemPtr item = m_viewItems->Get(i);
    if (IsSameEmbyID(item, serviceId))
    {
      CLog::Log(LOGDEBUG, "CEmbyClient::FindViewItemByServiceId: \"%s\"", item->GetLabel().c_str());
      return item;
    }
  }
  CLog::Log(LOGERROR, "CEmbyClient::FindViewItemByServiceId: failed to get find item with id \"%s\"", serviceId.c_str());
  return nullptr;
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

const EmbyViewContentVector CEmbyClient::GetTvShowContent() const
{
  CSingleLock lock(m_viewTVshowContentsLock);
  return m_viewTVshowContents;
}

const EmbyViewContentVector CEmbyClient::GetMoviesContent() const
{
  CSingleLock lock(m_viewMoviesContentsLock);
  return m_viewMoviesContents;
}

const EmbyViewContentVector CEmbyClient::GetArtistContent() const
{
  CSingleLock lock(m_viewArtistContentsLock);
  return m_viewArtistContents;
}

const EmbyViewContentVector CEmbyClient::GetPhotoContent() const
{
  CSingleLock lock(m_viewPhotosContentsLock);
  return m_viewPhotosContents;
}

const std::string CEmbyClient::FormatContentTitle(const std::string contentTitle) const
{
  std::string owned = IsOwned() ? "O":"S";
  std::string title = StringUtils::Format("Emby(%s) - %s - %s %s",
              owned.c_str(), GetServerName().c_str(), contentTitle.c_str(), GetPresence()? "":"(off-line)");
  return title;
}

std::string CEmbyClient::FindViewName(const std::string &path)
{
  CURL real_url(path);
  if (real_url.GetProtocol() == "emby")
    real_url = CURL(Base64::Decode(URIUtils::GetFileName(real_url)));

  if (!real_url.GetFileName().empty())
  {
    {
      CSingleLock lock(m_viewMoviesContentsLock);
      for (const auto &contents : m_viewMoviesContents)
      {
        if (real_url.GetFileName().find(contents.viewprefix) != std::string::npos)
          return contents.name;
      }
    }
    {
      CSingleLock lock(m_viewTVshowContentsLock);
      for (const auto &contents : m_viewTVshowContents)
      {
        if (real_url.GetFileName().find(contents.viewprefix) != std::string::npos)
          return contents.name;
      }
    }
  }

  return "";
}

bool CEmbyClient::IsSameClientHostName(const CURL& url)
{
  CURL real_url(url);
  if (real_url.GetProtocol() == "emby")
    real_url = CURL(Base64::Decode(URIUtils::GetFileName(real_url)));

  if (URIUtils::IsStack(real_url.Get()))
    real_url = CURL(XFILE::CStackDirectory::GetFirstStackedFile(real_url.Get()));
  
  return GetHost() == real_url.GetHostName();
}

bool CEmbyClient::ParseViews()
{
  bool rtn = false;
  XFILE::CCurlFile emby;
  emby.SetTimeout(10);
  emby.SetRequestHeader("Cache-Control", "no-cache");
  emby.SetRequestHeader("Content-Type", "application/json");
  CEmbyUtils::PrepareApiCall(m_serverInfo.UserId, m_serverInfo.AccessToken, emby);

  CURL curl(m_url);
  // /Users/{UserId}/Views
  curl.SetFileName(curl.GetFileName() + "Users/" + m_serverInfo.UserId + "/Views");
  std::string viewsUrl = curl.Get();
  std::string response;
  if (emby.Get(viewsUrl, response))
  {
#if defined(EMBY_DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, "CEmbyClient::ParseViews %s", response.c_str());
#endif

    auto resultObject = CJSONVariantParser::Parse(response);
    if (!resultObject.isObject() || !resultObject.isMember("Items"))
    {
      CLog::Log(LOGERROR, "CEmbyClient::ParseViews: invalid response for library views from %s", CURL::GetRedacted(viewsUrl).c_str());
      return false;
    }

    static const std::string PropertyViewId = "Id";
    static const std::string PropertyViewName = "Name";
    static const std::string PropertyViewETag = "Etag";
    static const std::string PropertyViewServerID = "ServerId";
    static const std::string PropertyViewCollectionType = "CollectionType";

    std::vector<EmbyViewContent> views;
    std::vector<const std::string> mediaTypes = {
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

      EmbyViewContent libraryView = {
        view[PropertyViewId].asString(),
        view[PropertyViewName].asString(),
        view[PropertyViewETag].asString(),
        view[PropertyViewServerID].asString(),
        type,
        "Users/" + m_serverInfo.UserId + "/Items?ParentId=" + view[PropertyViewId].asString()
      };
      if (libraryView.id.empty() || libraryView.name.empty())
        continue;

      views.push_back(libraryView);
    }

    for (const auto &content : views)
    {
      if (content.mediaType == "movies")
      {
        CSingleLock lock(m_viewMoviesContentsLock);
        m_viewMoviesContents.push_back(content);
      }
      else if (content.mediaType == "tvshows")
      {
        CSingleLock lock(m_viewTVshowContentsLock);
        m_viewTVshowContents.push_back(content);
      }
      else if (content.mediaType == "artist")
      {
        CSingleLock lock(m_viewArtistContentsLock);
        m_viewArtistContents.push_back(content);
      }
      else if (content.mediaType == "photo")
      {
        CSingleLock lock(m_viewPhotosContentsLock);
        m_viewPhotosContents.push_back(content);
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
        m_serverInfo.ServerName.c_str(), (int)m_viewMoviesContents.size());
      CLog::Log(LOGDEBUG, "CEmbyClient::ParseView %s found %d tvshows view",
        m_serverInfo.ServerName.c_str(), (int)m_viewTVshowContents.size());
      CLog::Log(LOGDEBUG, "CEmbyClient::ParseView %s found %d artist view",
        m_serverInfo.ServerName.c_str(), (int)m_viewArtistContents.size());
      CLog::Log(LOGDEBUG, "CEmbyClient::ParseView %s found %d photos view",
        m_serverInfo.ServerName.c_str(), (int)m_viewPhotosContents.size());
      rtn = true;
    }
  }

  return rtn;
}

void CEmbyClient::SetPresence(bool presence)
{
  if (m_presence != presence)
    m_presence = presence;
}

const CVariant CEmbyClient::FetchItemById(const std::string &Id)
{
  if (Id.empty())
    return CVariant(CVariant::VariantTypeNull);

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

  CURL curl(m_url);
  curl.SetFileName("emby/Users/" + GetUserID() + "/Items/");
  curl.SetOptions("");
  curl.SetOption("Ids", Id);
  const CVariant object = CEmbyUtils::GetEmbyCVariant(curl.Get());

  return object;
}

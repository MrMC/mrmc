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

/*
Direct Play - The client plays the file by accessing the file system
  directly using the Path property. The server is bypassed with this
  mechanism. Whenever possible this is the most desirable form of playback.

Direct Stream - The client streams the file from the server as-is, in
  its original format, without any encoding or remuxing applied.
  Aside from Direct Play, this is the next most desirable playback method.

Transcode - The client streams the file from the server with encoding
  applied in order to convert it to a format that it can understand.
*/


#include <string>

#include "URL.h"
#include "threads/CriticalSection.h"
#include "threads/SingleLock.h"

class CFileItem;
class CFileItemList;
class CEmbyViewCache;
class CEmbyClientSync;
typedef struct EmbyViewInfo EmbyViewInfo;
typedef struct EmbyViewContent EmbyViewContent;
typedef std::shared_ptr<CFileItem> CFileItemPtr;
typedef std::shared_ptr<CEmbyViewCache> CEmbyViewCachePtr;

typedef struct EmbyServerInfo
{
  std::string UserId;
  std::string AccessToken;

  std::string UserType;
  std::string ServerId;
  std::string AccessKey;
  std::string ServerURL;
  std::string ServerName;
  std::string WanAddress;
  std::string LocalAddress;
} EmbyServerInfo;
typedef std::vector<EmbyServerInfo> EmbyServerInfoVector;


class CEmbyClient
{
  friend class CEmbyServices;
  friend class CThreadedFetchViewItems;

public:
  CEmbyClient();
 ~CEmbyClient();

  bool Init(const EmbyServerInfo &serverInfo);

  const bool NeedUpdate() const             { return m_needUpdate; }
  const std::string &GetServerName() const  { return m_serverInfo.ServerName; }
  const std::string &GetUuid() const        { return m_serverInfo.UserId; }
  // bool GetPresence() const                  { return m_presence; }
  bool GetPresence() const                  { return true; }
  const std::string &GetProtocol() const    { return m_protocol; }
  const bool &IsLocal() const               { return m_local; }
  const bool IsCloud() const                { return (m_platform == "Cloud"); }
  const bool IsOwned() const                { return m_owned; }

  void  SetWatched(CFileItem &item);
  void  SetUnWatched(CFileItem &item);

  void  UpdateLibrary(const std::string &content);

  // main view entry points (from CEmbyDirectory)
  bool  GetMovies(CFileItemList &items, std::string url, bool fromfilter);
  bool  GetMoviesFilter(CFileItemList &items, std::string url, std::string filter);
  bool  GetTVShows(CFileItemList &items, std::string url, bool fromfilter);
  bool  GetTVShowsFilter(CFileItemList &items, std::string url, std::string filter);
  bool  GetMusicArtists(CFileItemList &items, std::string url);

  void  AddNewViewItems(const std::vector<std::string> &ids);
  void  UpdateViewItems(const std::vector<std::string> &ids);
  void  RemoveViewItems(const std::vector<std::string> &ids);

  const std::vector<EmbyViewInfo> GetViewInfoForMovieContent() const;
  const std::vector<EmbyViewInfo> GetViewInfoForMusicContent() const;
  const std::vector<EmbyViewInfo> GetViewInfoForPhotoContent() const;
  const std::vector<EmbyViewInfo> GetViewInfoForTVShowContent() const;
  const std::string FormatContentTitle(const std::string contentTitle) const;

  std::string GetUrl();
  std::string GetHost();
  int         GetPort();
  std::string GetUserID();

protected:
  bool        IsSameClientHostName(const CURL& url);
  bool        FetchViews();
  bool        FetchViewItems(CEmbyViewCachePtr &view, const CURL& url, const std::string &type);
  bool        DoThreadedFetchViewItems(CEmbyViewCachePtr &view, const CURL& url, const std::string &type);
  bool        FetchFilterItems(CEmbyViewCachePtr &view, const CURL &url, const std::string &type, const std::string &filter);
  void        SetPresence(bool presence);
  const CVariant FetchItemById(const std::string &Id);
  const CVariant FetchItemByIds(const std::vector<std::string> &Ids);

private:
  bool        AppendItemToCache(const CVariant &variant);
  bool        UpdateItemInCache(const CVariant &variant);
  bool        RemoveItemFromCache(const CVariant &variant);

  bool m_local;
  std::string m_url;
  bool m_owned;
  std::string m_protocol;
  std::string m_platform;
  EmbyServerInfo m_serverInfo;
  std::atomic<bool> m_presence;
  std::atomic<bool> m_needUpdate;
  CEmbyClientSync  *m_clientSync;

  CEmbyViewCachePtr m_viewMoviesFilter;
  CEmbyViewCachePtr m_viewTVShowsFilter;
  CCriticalSection m_viewMoviesFilterLock;
  CCriticalSection m_viewTVShowsFilterLock;

  CCriticalSection m_viewMusicLock;
  CCriticalSection m_viewMoviesLock;
  CCriticalSection m_viewPhotosLock;
  CCriticalSection m_viewTVShowsLock;
  std::vector<CEmbyViewCachePtr> m_viewMusic;
  std::vector<CEmbyViewCachePtr> m_viewMovies;
  std::vector<CEmbyViewCachePtr> m_viewPhotos;
  std::vector<CEmbyViewCachePtr> m_viewTVShows;

};

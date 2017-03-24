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

struct EmbyViewContent
{
  std::string id;
  std::string name;
  std::string etag;
  std::string serverid;
  std::string mediaType;
  std::string viewprefix;
};

class CFileItem;
class CFileItemList;
class CEmbyClientSync;
typedef std::shared_ptr<CFileItem> CFileItemPtr;
typedef std::vector<EmbyViewContent> EmbyViewContentVector;

class CEmbyClient
{
  friend class CEmbyServices;

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

  void  ClearViewItems();
  void  AddViewItem(const CFileItemPtr &item);
  void  AddViewItems(const CFileItemList &items);
  void  AddNewViewItem(const std::string &serviceId);
  void  UpdateViewItem(const std::string &serviceId);
  void  RemoveViewItem(const std::string &serviceId);
  CFileItemPtr FindViewItem(const std::string &serviceId);

  const EmbyViewContentVector GetTvShowContent() const;
  const EmbyViewContentVector GetMoviesContent() const;
  const EmbyViewContentVector GetArtistContent() const;
  const EmbyViewContentVector GetPhotoContent() const;
  const std::string FormatContentTitle(const std::string contentTitle) const;
  std::string FindViewName(const std::string &path);

  std::string GetUrl();
  std::string GetHost();
  int         GetPort();
  std::string GetUserID();

protected:
  bool        IsSameClientHostName(const CURL& url);
  bool        ParseViews();
  void        SetPresence(bool presence);
  const CVariant FetchItemById(const std::string &Id);

private:
  bool m_local;
  std::string m_url;
  bool m_owned;
  std::string m_protocol;
  std::string m_platform;
  EmbyServerInfo m_serverInfo;
  std::atomic<bool> m_presence;
  std::atomic<bool> m_needUpdate;
  CEmbyClientSync *m_clientSync;

  CFileItemList *m_viewItems;
  CCriticalSection m_viewItemsLock;

  CCriticalSection  m_viewMoviesContentsLock;
  CCriticalSection  m_viewTVshowContentsLock;
  CCriticalSection  m_viewArtistContentsLock;
  CCriticalSection  m_viewPhotosContentsLock;
  std::vector<EmbyViewContent> m_viewMoviesContents;
  std::vector<EmbyViewContent> m_viewTVshowContents;
  std::vector<EmbyViewContent> m_viewArtistContents;
  std::vector<EmbyViewContent> m_viewPhotosContents;
};

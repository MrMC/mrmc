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

#include "EmbyDirectory.h"

#include "Application.h"
#include "FileItem.h"
#include "URL.h"
#include "network/Network.h"
#include "network/Socket.h"
#include "filesystem/Directory.h"
#include "guilib/LocalizeStrings.h"
#include "services/emby/EmbyUtils.h"
#include "services/emby/EmbyViewCache.h"
#include "services/emby/EmbyServices.h"
#include "utils/log.h"
#include "utils/Base64.h"
#include "utils/StringUtils.h"
#include "utils/JSONVariantParser.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML.h"

using namespace XFILE;

CEmbyDirectory::CEmbyDirectory()
{ }

CEmbyDirectory::~CEmbyDirectory()
{ }

bool CEmbyDirectory::GetDirectory(const CURL& url, CFileItemList &items)
{
  CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory");
  {
    assert(url.IsProtocol("emby"));
    std::string path = url.Get();
    if (path == "emby://")
    {
      // we are broswing network for clients.
      return FindByBroadcast(items);
    }
    if (url.GetFileName() == "wan" ||
        url.GetFileName() == "local")
    {
      // user selected some emby server found by
      // broadcast. now we need to stop the dir
      // recursion so return a dummy items.
      CFileItemPtr item(new CFileItem("", false));
      CURL curl1(url);
      curl1.SetFileName("dummy");
      item->SetPath(curl1.Get());
      item->SetLabel("dummy");
      item->SetLabelPreformated(true);
      //just set the default folder icon
      item->FillInDefaultIcon();
      item->m_bIsShareOrDrive = true;
      items.Add(item);
      return true;
    }
  }

  std::string strUrl = url.Get();
  std::string section = URIUtils::GetFileName(strUrl);
  items.SetPath(strUrl);
  std::string basePath = strUrl;
  URIUtils::RemoveSlashAtEnd(basePath);
  basePath = URIUtils::GetFileName(basePath);

  CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory strURL = %s", strUrl.c_str());

  if (StringUtils::StartsWithNoCase(strUrl, "emby://movies/"))
  {
    if (section.empty())
    {
      //look through all emby clients and pull content data for "movie" type
      std::vector<CEmbyClientPtr> clients;
      CEmbyServices::GetInstance().GetClients(clients);
      for (const auto &client : clients)
      {
        const std::vector<EmbyViewInfo> contents = client->GetViewInfoForMovieContent();
        if (contents.size() > 1 || ((items.Size() > 0 || CServicesManager::GetInstance().HasPlexServices() || clients.size() > 1) && contents.size() == 1))
        {
          for (const auto &content : contents)
          {
            std::string title = client->FormatContentTitle(content.name);
            CFileItemPtr pItem(new CFileItem(title));
            pItem->m_bIsFolder = true;
            pItem->m_bIsShareOrDrive = true;
            CEmbyUtils::SetEmbyItemProperties(*pItem, "movies", client);
            // have to do it this way because raw url has authToken as protocol option
            CURL curl(client->GetUrl());
            curl.SetProtocol(client->GetProtocol());
            curl.SetFileName(content.prefix);
            pItem->SetPath("emby://movies/" + basePath + "/" + Base64::Encode(curl.Get()));
            pItem->SetLabel(title);
            curl.SetFileName("Items/" + content.id + "/Images/Primary");
            pItem->SetArt("thumb", curl.Get());
            pItem->SetIconImage(curl.Get());
            items.Add(pItem);
            CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory client(%s), title(%s)", client->GetServerName().c_str(), title.c_str());
          }
        }
        else if (contents.size() == 1)
        {
          CURL curl(client->GetUrl());
          curl.SetProtocol(client->GetProtocol());
          curl.SetFileName(contents[0].prefix);
          //client->GetMovies(items, curl.Get()); ????
          CDirectory::GetDirectory("emby://movies/" + basePath + "/" + Base64::Encode(curl.Get()), items);
          items.SetContent("movies");
          CEmbyUtils::SetEmbyItemProperties(items, "movies", client);
          for (int item = 0; item < items.Size(); ++item)
            CEmbyUtils::SetEmbyItemProperties(*items[item], "movies", client);
        }
        std::string label = basePath;
        if (URIUtils::GetFileName(basePath) == "recentlyaddedmovies")
          label = g_localizeStrings.Get(20386);
        else if (URIUtils::GetFileName(basePath) == "inprogressmovies")
          label = g_localizeStrings.Get(627);
        else
          StringUtils::ToCapitalize(label);
        items.SetLabel(label);
      }
    }
    else
    {
      CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(strUrl);
      if (!client || !client->GetPresence())
      {
        CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory no client or client not present %s", CURL::GetRedacted(strUrl).c_str());
        return false;
      }

      items.ClearItems();
      std::string path = URIUtils::GetParentPath(strUrl);
      URIUtils::RemoveSlashAtEnd(path);
      path = URIUtils::GetFileName(path);

      std::string filter;
      if (path == "genres")
        filter = "Genres";
      else if (path == "years")
        filter = "Years";
      else if (path == "sets")
        filter = "Collections";
   //   else if (path == "countries")
   //     filter = "country";
      else if (path == "studios")
        filter = "Studios";

      if (path == "" || path == "titles" || path == "filter")
      {
        client->GetMovies(items, Base64::Decode(section), path == "filter");
        items.SetLabel(g_localizeStrings.Get(369));
        items.SetContent("movies");
      }
      else if (path == "recentlyaddedmovies")
      {
        CEmbyUtils::GetEmbyRecentlyAddedMovies(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(20386));
        items.SetContent("movies");
      }
      else if (path == "inprogressmovies")
      {
        CEmbyUtils::GetEmbyInProgressMovies(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(627));
        items.SetContent("movies");
      }
      else if(!filter.empty())
      {
        client->GetMoviesFilter(items, Base64::Decode(section), filter);
        StringUtils::ToCapitalize(path);
        items.SetLabel(path);
        items.SetContent("movies");
      }
      CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory' client(%s), found %d movies", client->GetServerName().c_str(), items.Size());
    }
    return true;
  }
  else if (StringUtils::StartsWithNoCase(strUrl, "emby://tvshows/"))
  {
    if (section.empty())
    {
      //look through all emby servers and pull content data for "show" type
      std::vector<CEmbyClientPtr> clients;
      CEmbyServices::GetInstance().GetClients(clients);
      for (const auto &client : clients)
      {
        const std::vector<EmbyViewInfo> contents = client->GetViewInfoForTVShowContent();
        if (contents.size() > 1 || ((items.Size() > 0 || CServicesManager::GetInstance().HasPlexServices() || clients.size() > 1) && contents.size() == 1))
        {
          for (const auto &content : contents)
          {
            std::string title = client->FormatContentTitle(content.name);
            CFileItemPtr pItem(new CFileItem(title));
            pItem->m_bIsFolder = true;
            pItem->m_bIsShareOrDrive = true;
            CEmbyUtils::SetEmbyItemProperties(*pItem, "tvshows", client);
            // have to do it this way because raw url has authToken as protocol option
            CURL curl(client->GetUrl());
            curl.SetProtocol(client->GetProtocol());
            curl.SetFileName(content.prefix);
            pItem->SetPath("emby://tvshows/" + basePath + "/" + Base64::Encode(curl.Get()));
            pItem->SetLabel(title);
            curl.SetFileName("Items/" + content.id + "/Images/Primary");
            pItem->SetArt("thumb", curl.Get());
            pItem->SetIconImage(curl.Get());
            items.Add(pItem);
            CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory client(%s), title(%s)", client->GetServerName().c_str(), title.c_str());
          }
        }
        else if (contents.size() == 1)
        {
          CURL curl(client->GetUrl());
          curl.SetProtocol(client->GetProtocol());
          curl.SetFileName(contents[0].prefix);
          //client->GetTVShows(items, curl.Get()); ????
          CDirectory::GetDirectory("emby://tvshows/" + basePath + "/" + Base64::Encode(curl.Get()), items);
          CEmbyUtils::SetEmbyItemProperties(items, "tvshows", client);
          for (int item = 0; item < items.Size(); ++item)
            CEmbyUtils::SetEmbyItemProperties(*items[item], "tvshows", client);
        }
        std::string label = basePath;
        if (URIUtils::GetFileName(basePath) == "recentlyaddedepisodes")
          label = g_localizeStrings.Get(20387);
        else if (URIUtils::GetFileName(basePath) == "inprogressshows")
          label = g_localizeStrings.Get(626);
        else
          StringUtils::ToCapitalize(label);
        items.SetLabel(label);
      }
    }
    else
    {
      CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(strUrl);
      if (!client || !client->GetPresence())
      {
        CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory no client or client not present %s", CURL::GetRedacted(strUrl).c_str());
        return false;
      }

      items.ClearItems();
      std::string path = URIUtils::GetParentPath(strUrl);
      URIUtils::RemoveSlashAtEnd(path);
      path = URIUtils::GetFileName(path);
      
      std::string filter;
      if (path == "genres")
        filter = "Genres";
      else if (path == "years")
        filter = "Years";
     // else if (path == "sets")
     //   filter = "Collections";
      //   else if (path == "countries")
      //     filter = "country";
      else if (path == "studios")
        filter = "Studios";

      if (path == "" || path == "titles" || path == "filter")
      {
        client->GetTVShows(items, Base64::Decode(section), path == "filter");
        items.SetLabel(g_localizeStrings.Get(369));
        items.SetContent("tvshows");
      }
      else if (path == "shows")
      {
        CEmbyUtils::GetEmbySeasons(items,Base64::Decode(section));
        items.SetContent("seasons");
      }
      else if (path == "seasons")
      {
        CEmbyUtils::GetEmbyEpisodes(items,Base64::Decode(section));
        items.SetContent("episodes");
      }
      else if (path == "recentlyaddedepisodes")
      {
        CEmbyUtils::GetEmbyRecentlyAddedEpisodes(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(20387));
        items.SetContent("episodes");
      }
      else if (path == "inprogressshows")
      {
        CEmbyUtils::GetEmbyInProgressShows(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(626));
        items.SetContent("episodes");
      }
      else if(!filter.empty())
      {
        client->GetTVShowsFilter(items, Base64::Decode(section), filter);
        StringUtils::ToCapitalize(path);
        items.SetLabel(path);
        items.SetContent("tvshows");
      }
      CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory' client(%s), found %d shows", client->GetServerName().c_str(), items.Size());
    }
    return true;
  }
  else if (StringUtils::StartsWithNoCase(strUrl, "emby://music/"))
  {
    if (section.empty())
    {
      //look through all emby servers and pull content data for "show" type
      std::vector<CEmbyClientPtr> clients;
      CEmbyServices::GetInstance().GetClients(clients);
      for (const auto &client : clients)
      {
        const std::vector<EmbyViewInfo> contents = client->GetViewInfoForMusicContent();
        if (contents.size() > 1 || ((items.Size() > 0 || CServicesManager::GetInstance().HasPlexServices() || clients.size() > 1) && contents.size() == 1))
        {
          for (const auto &content : contents)
          {
            std::string title = client->FormatContentTitle(content.name);
            CFileItemPtr pItem(new CFileItem(title));
            pItem->m_bIsFolder = true;
            pItem->m_bIsShareOrDrive = true;
            CEmbyUtils::SetEmbyItemProperties(*pItem, "music", client);
            // have to do it this way because raw url has authToken as protocol option
            CURL curl(client->GetUrl());
            curl.SetProtocol(client->GetProtocol());
            curl.SetFileName(content.prefix);
            pItem->SetPath("emby://music/" + basePath + "/" + Base64::Encode(curl.Get()));
            pItem->SetLabel(title);
            curl.SetFileName("Items/" + content.id + "/Images/Primary");
            pItem->SetArt("thumb", curl.Get());
            pItem->SetIconImage(curl.Get());
            items.Add(pItem);
            CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory client(%s), title(%s)", client->GetServerName().c_str(), title.c_str());
          }
        }
        else if (contents.size() == 1)
        {
          CURL curl(client->GetUrl());
          curl.SetProtocol(client->GetProtocol());
          curl.SetFileName(contents[0].prefix);
          client->GetMusicArtists(items, curl.Get());
          items.SetContent("artists");
          items.SetPath("emby://music/albums/");
          CEmbyUtils::SetEmbyItemProperties(items, "music", client);
          for (int item = 0; item < items.Size(); ++item)
            CEmbyUtils::SetEmbyItemProperties(*items[item], "music", client);
          CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory '/all' client(%s), shows(%d)", client->GetServerName().c_str(), items.Size());
        }
      }
    }
    else
    {
      CEmbyClientPtr client = CEmbyServices::GetInstance().FindClient(strUrl);
      if (!client || !client->GetPresence())
      {
        CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory no client or client not present %s", CURL::GetRedacted(strUrl).c_str());
        return false;
      }

      items.ClearItems();
      std::string path = URIUtils::GetParentPath(strUrl);
      URIUtils::RemoveSlashAtEnd(path);
      path = URIUtils::GetFileName(path);
      
      std::string filter = "all";
      if (path == "albums")
        filter = "albums";
      
      if (path == "" || path == "root" || path == "artists")
      {
        client->GetMusicArtists(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(36917));
        items.SetContent("artist");
      }
      if (path == "albums")
      {
        CEmbyUtils::GetEmbyAlbum(items,Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(36919));
        items.SetContent("albums");
      }
      if (path == "artistalbums")
      {
        CEmbyUtils::GetEmbyArtistAlbum(items,Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(36919));
        items.SetContent("albums");
      }
      if (path == "songs")
      {
        CEmbyUtils::GetEmbySongs(items,Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(36921));
        items.SetContent("songs");
      }
      if (path == "albumsongs")
      {
        CEmbyUtils::GetEmbyAlbumSongs(items,Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(36921));
        items.SetContent("songs");
      }
    }
    return true;
  }
  else
  {
    CLog::Log(LOGDEBUG, "CEmbyDirectory::GetDirectory got nothing from %s", CURL::GetRedacted(strUrl).c_str());
  }

  return false;
}

DIR_CACHE_TYPE CEmbyDirectory::GetCacheType(const CURL& url) const
{
  return DIR_CACHE_NEVER;
}

bool CEmbyDirectory::FindByBroadcast(CFileItemList &items)
{
  bool rtn = false;
  static const int NS_EMBY_BROADCAST_PORT(7359);
  static const std::string NS_EMBY_BROADCAST_ADDRESS("255.255.255.255");
  static const std::string NS_EMBY_BROADCAST_SEARCH_MSG("who is EmbyServer?");

  SOCKETS::CSocketListener *m_broadcastListener = nullptr;

  if (!m_broadcastListener)
  {
    SOCKETS::CUDPSocket *socket = SOCKETS::CSocketFactory::CreateUDPSocket();
    if (socket)
    {
      CNetworkInterface *iface = g_application.getNetwork().GetFirstConnectedInterface();
      if (iface && iface->IsConnected())
      {
        SOCKETS::CAddress my_addr;
        my_addr.SetAddress(iface->GetCurrentIPAddress().c_str());
        if (!socket->Bind(my_addr, NS_EMBY_BROADCAST_PORT, 0))
        {
          CLog::Log(LOGERROR, "CEmbyServices:CheckEmbyServers Could not listen on port %d", NS_EMBY_BROADCAST_PORT);
          SAFE_DELETE(m_broadcastListener);
          return rtn;
        }

        if (socket)
        {
          socket->SetBroadCast(true);
          // create and add our socket to the 'select' listener
          m_broadcastListener = new SOCKETS::CSocketListener();
          m_broadcastListener->AddSocket(socket);
        }
      }
      else
      {
        SAFE_DELETE(socket);
      }
    }
    else
    {
      CLog::Log(LOGERROR, "CEmbyServices:CheckEmbyServers Could not create socket for GDM");
      return rtn;
    }
  }

  SOCKETS::CUDPSocket *socket = (SOCKETS::CUDPSocket*)m_broadcastListener->GetFirstSocket();
  if (socket)
  {
    SOCKETS::CAddress discoverAddress;
    discoverAddress.SetAddress(NS_EMBY_BROADCAST_ADDRESS.c_str(), NS_EMBY_BROADCAST_PORT);
    std::string discoverMessage = NS_EMBY_BROADCAST_SEARCH_MSG;
    int packetSize = socket->SendTo(discoverAddress, discoverMessage.length(), discoverMessage.c_str());
    if (packetSize < 0)
      CLog::Log(LOGERROR, "CEmbyServices::CheckEmbyServers:CheckforGDMServers discover send failed");
  }

  static const int DiscoveryTimeoutMs = 10000;
  // listen for broadcast reply until we timeout
  if (socket && m_broadcastListener->Listen(DiscoveryTimeoutMs))
  {
    char buffer[8192] = {0};
    SOCKETS::CAddress sender;
    int packetSize = socket->Read(sender, 8192, buffer);
    if (packetSize > -1)
    {
      if (packetSize > 0)
      {
        CVariant data;
        std::string jsonpacket = buffer;
        if (!CJSONVariantParser::Parse(jsonpacket, data))
          return rtn;
        static const std::string ServerPropertyAddress = "Address";
        if (data.isObject() && data.isMember(ServerPropertyAddress))
        {
          EmbyServerInfo embyServerInfo = CEmbyServices::GetInstance().GetEmbyLocalServerInfo(data[ServerPropertyAddress].asString());
          if (!embyServerInfo.ServerId.empty())
          {
            CLog::Log(LOGNOTICE, "CEmbyServices::CheckEmbyServers Server found %s", embyServerInfo.ServerName.c_str());
            CFileItemPtr local(new CFileItem("", true));
            CURL curl1(embyServerInfo.LocalAddress);
            curl1.SetProtocol("emby");
            // set a magic key
            curl1.SetFileName("local");
            local->SetPath(curl1.Get());
            local->SetLabel(embyServerInfo.ServerName + " (local)");
            local->SetLabelPreformated(true);
            //just set the default folder icon
            local->FillInDefaultIcon();
            local->m_bIsShareOrDrive = true;

            items.Add(local);

            CFileItemPtr remote(new CFileItem("", true));
            CURL curl2(embyServerInfo.WanAddress);
            curl2.SetProtocol("emby");
            // set a magic key
            curl1.SetFileName("wan");
            remote->SetPath(curl2.Get());
            remote->SetLabel(embyServerInfo.ServerName + " (wan)");
            remote->SetLabelPreformated(true);
            //just set the default folder icon
            remote->FillInDefaultIcon();
            remote->m_bIsShareOrDrive = true;
            items.Add(remote);
            rtn = true;
          }
        }
      }
    }
  }

  if (m_broadcastListener)
  {
    // before deleting listener, fetch and delete any sockets it uses.
    SOCKETS::CUDPSocket *socket = (SOCKETS::CUDPSocket*)m_broadcastListener->GetFirstSocket();
    // we should not have to do the close,
    // delete 'should' do it.
    socket->Close();
    SAFE_DELETE(socket);
    SAFE_DELETE(m_broadcastListener);
  }

  return rtn;
}

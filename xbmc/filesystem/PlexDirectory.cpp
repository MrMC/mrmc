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

#include "PlexDirectory.h"
#include "FileItem.h"
#include "URL.h"
#include "filesystem/Directory.h"
#include "services/plex/PlexUtils.h"
#include "services/plex/PlexServices.h"
#include "utils/Base64.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML.h"
#include "utils/log.h"

#include "video/VideoDatabase.h"
#include "music/MusicDatabase.h"

using namespace XFILE;

CPlexDirectory::CPlexDirectory()
{ }

CPlexDirectory::~CPlexDirectory()
{ }

bool CPlexDirectory::GetDirectory(const CURL& url, CFileItemList &items)
{
  CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory");

  items.ClearItems();
  std::string strUrl = url.Get();
  std::string section = URIUtils::GetFileName(strUrl);
  items.SetPath(strUrl);
  std::string basePath = strUrl;
  URIUtils::RemoveSlashAtEnd(basePath);
  basePath = URIUtils::GetFileName(basePath);

  CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory strURL = %s", strUrl.c_str());

  if (StringUtils::StartsWithNoCase(strUrl, "plex://movies/"))
  {
    if (section.empty())
    {
      CVideoDatabase database;
      database.Open();
      bool hasMovies = database.HasContent(VIDEODB_CONTENT_MOVIES);
      database.Close();

      if (hasMovies)
      {
        //add local Movies
        std::string title = StringUtils::Format("MrMC - %s", g_localizeStrings.Get(342).c_str());
        CFileItemPtr pItem(new CFileItem(title));
        pItem->m_bIsFolder = true;
        pItem->m_bIsShareOrDrive = false;
        if (URIUtils::GetFileName(basePath) == "recentlyaddedmovies")
          pItem->SetPath("videodb://recentlyaddedmovies/");
        else if (URIUtils::GetFileName(basePath) == "inprogressmovies")
          pItem->SetPath("library://video/inprogressmovies.xml/");
        else
          pItem->SetPath("videodb://movies/" + basePath + "/");
        pItem->SetLabel(title);
        items.Add(pItem);
      }

      //look through all plex clients and pull content data for "movie" type
      std::vector<CPlexClientPtr> clients;
      CPlexServices::GetInstance().GetClients(clients);
      for (const auto &client : clients)
      {
        client->ClearSectionItems();
        std::vector<PlexSectionsContent> contents = client->GetMovieContent();
        if (contents.size() > 1 || ((hasMovies || clients.size() > 1) && contents.size() == 1))
        {
          for (const auto &content : contents)
          {
            std::string title = client->FormatContentTitle(content.title);
            CFileItemPtr pItem(new CFileItem(title));
            pItem->m_bIsFolder = true;
            pItem->m_bIsShareOrDrive = true;
            CPlexUtils::SetPlexItemProperties(*pItem, client);
            // have to do it this way because raw url has authToken as protocol option
            CURL curl(client->GetUrl());
            curl.SetProtocol(client->GetProtocol());
            std::string filename = StringUtils::Format("%s/%s", content.section.c_str(), (basePath == "titles"? "all":""));
            curl.SetFileName(filename);
            pItem->SetPath("plex://movies/" + basePath + "/" + Base64::Encode(curl.Get()));
            pItem->SetLabel(title);
            std::string value = content.thumb;
            if (!value.empty() && (value[0] == '/'))
              StringUtils::TrimLeft(value, "/");
            curl.SetFileName(value);
            pItem->SetIconImage(curl.Get());
            items.Add(pItem);
            client->AddSectionItem(pItem);
            CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory client(%s), title(%s)", client->GetServerName().c_str(), title.c_str());
          }
        }
        else if (contents.size() == 1)
        {
          CURL curl(client->GetUrl());
          curl.SetProtocol(client->GetProtocol());
          std::string filename = StringUtils::Format("%s/%s", contents[0].section.c_str(), (basePath == "titles"? "all":""));
          curl.SetFileName(filename);
          CDirectory::GetDirectory("plex://movies/" + basePath + "/" + Base64::Encode(curl.Get()), items);
          items.SetContent("movies");
          CPlexUtils::SetPlexItemProperties(items, client);
          for (int item = 0; item < items.Size(); ++item)
            CPlexUtils::SetPlexItemProperties(*items[item], client);
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
      CPlexClientPtr client = CPlexServices::GetInstance().FindClient(strUrl);
      if (!client || !client->GetPresence())
      {
        CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory no client or client not present %s", CURL::GetRedacted(strUrl).c_str());
        return false;
      }

      std::string path = URIUtils::GetParentPath(strUrl);
      URIUtils::RemoveSlashAtEnd(path);
      path = URIUtils::GetFileName(path);

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

      if (path == "titles" || path == "filter")
      {
        CPlexUtils::GetPlexMovies(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(369));
        items.SetContent("movies");
      }
      else if (path == "recentlyaddedmovies")
      {
        CPlexUtils::GetPlexRecentlyAddedMovies(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(20386));
        items.SetContent("movies");
      }
      else if (path == "inprogressmovies")
      {
        CPlexUtils::GetPlexInProgressMovies(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(627));
        items.SetContent("movies");
      }
      else
      {
        CPlexUtils::GetPlexFilter(items, Base64::Decode(section), "plex://movies/filter/", filter);
        StringUtils::ToCapitalize(path);
        items.SetLabel(path);
        items.SetContent("movies");
      }
      CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory' client(%s), found %d movies", client->GetServerName().c_str(), items.Size());
    }
    return true;
  }
  else if (StringUtils::StartsWithNoCase(strUrl, "plex://tvshows/"))
  {
    if (section.empty())
    {
      CVideoDatabase database;
      database.Open();
      bool hasTvShows = database.HasContent(VIDEODB_CONTENT_TVSHOWS);
      database.Close();

      if (hasTvShows)
      {
        //add local Shows
        std::string title = StringUtils::Format("MrMC - %s", g_localizeStrings.Get(20343).c_str());
        CFileItemPtr pItem(new CFileItem(title));
        pItem->m_bIsFolder = true;
        pItem->m_bIsShareOrDrive = false;
        if (URIUtils::GetFileName(basePath) == "recentlyaddedepisodes")
          pItem->SetPath("videodb://recentlyaddedepisodes/");
        else if (URIUtils::GetFileName(basePath) == "inprogressshows")
          pItem->SetPath("library://video/inprogressshows.xml/");
        else
          pItem->SetPath("videodb://tvshows/" + basePath + "/");
        pItem->SetLabel(title);
        items.Add(pItem);
      }

      //look through all plex servers and pull content data for "show" type
      std::vector<CPlexClientPtr> clients;
      CPlexServices::GetInstance().GetClients(clients);
      for (const auto &client : clients)
      {
        client->ClearSectionItems();
        std::vector<PlexSectionsContent> contents = client->GetTvContent();
        if (contents.size() > 1 || ((hasTvShows || clients.size() > 1) && contents.size() == 1))
        {
          for (const auto &content : contents)
          {
            std::string title = client->FormatContentTitle(content.title);
            CFileItemPtr pItem(new CFileItem(title));
            pItem->m_bIsFolder = true;
            pItem->m_bIsShareOrDrive = true;
            CPlexUtils::SetPlexItemProperties(*pItem, client);
            // have to do it this way because raw url has authToken as protocol option
            CURL curl(client->GetUrl());
            curl.SetProtocol(client->GetProtocol());
            std::string filename = StringUtils::Format("%s/%s", content.section.c_str(), (basePath == "titles"? "all":""));
            curl.SetFileName(filename);
            pItem->SetPath("plex://tvshows/" + basePath + "/" + Base64::Encode(curl.Get()));
            pItem->SetLabel(title);
            std::string value = content.thumb;
            if (!value.empty() && (value[0] == '/'))
              StringUtils::TrimLeft(value, "/");
            curl.SetFileName(value);
            pItem->SetIconImage(curl.Get());
            items.Add(pItem);
            client->AddSectionItem(pItem);
            CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory client(%s), title(%s)", client->GetServerName().c_str(), title.c_str());
          }
        }
        else if (contents.size() == 1)
        {
          CURL curl(client->GetUrl());
          curl.SetProtocol(client->GetProtocol());
          std::string filename = StringUtils::Format("%s/%s", contents[0].section.c_str(), (basePath == "titles"? "all":""));
          curl.SetFileName(filename);
          CDirectory::GetDirectory("plex://tvshows/" + basePath + "/" + Base64::Encode(curl.Get()), items);
          CPlexUtils::SetPlexItemProperties(items, client);
          for (int item = 0; item < items.Size(); ++item)
            CPlexUtils::SetPlexItemProperties(*items[item], client);
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
      CPlexClientPtr client = CPlexServices::GetInstance().FindClient(strUrl);
      if (!client || !client->GetPresence())
      {
        CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory no client or client not present %s", CURL::GetRedacted(strUrl).c_str());
        return false;
      }

      std::string path = URIUtils::GetParentPath(strUrl);
      URIUtils::RemoveSlashAtEnd(path);
      path = URIUtils::GetFileName(path);

      std::string filter = "year";
      if (path == "genres")
        filter = "genre";
      else if (path == "actors")
        filter = "actor";
      else if (path == "studios")
        filter = "studio";

      if (path == "titles" || path == "filter")
      {
        CPlexUtils::GetPlexTvshows(items,Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(369));
        items.SetContent("tvshows");
      }
      else if (path == "shows")
      {
        CPlexUtils::GetPlexSeasons(items,Base64::Decode(section));
        items.SetContent("tvshows");
      }
      else if (path == "seasons")
      {
        CPlexUtils::GetPlexEpisodes(items,Base64::Decode(section));
        items.SetContent("episodes");
      }
      else if (path == "recentlyaddedepisodes")
      {
        CPlexUtils::GetPlexRecentlyAddedEpisodes(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(20387));
        items.SetContent("episodes");
      }
      else if (path == "inprogressshows")
      {
        CPlexUtils::GetPlexInProgressShows(items, Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(626));
        items.SetContent("episodes");
      }
      else
      {
        CPlexUtils::GetPlexFilter(items, Base64::Decode(section), "plex://tvshows/filter/", filter);
        StringUtils::ToCapitalize(path);
        items.SetLabel(path);
        items.SetContent("tvshows");
      }
      CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory' client(%s), found %d shows", client->GetServerName().c_str(), items.Size());
    }
    return true;
  }
  else if (StringUtils::StartsWithNoCase(strUrl, "plex://music/"))
  {
    if (section.empty())
    {
      CMusicDatabase database;
      database.Open();
      bool hasMusic = database.HasContent();
      database.Close();
      
      if (hasMusic)
      {
        //add local Music
        std::string title = StringUtils::Format("MrMC - %s", g_localizeStrings.Get(249).c_str());
        CFileItemPtr pItem(new CFileItem(title));
        pItem->m_bIsFolder = true;
        pItem->m_bIsShareOrDrive = false;
        pItem->SetPath("");
        pItem->SetLabel(title);
        items.Add(pItem);
      }
      
      //look through all plex servers and pull content data for "show" type
      std::vector<CPlexClientPtr> clients;
      CPlexServices::GetInstance().GetClients(clients);
      for (const auto &client : clients)
      {
        client->ClearSectionItems();
        std::vector<PlexSectionsContent> contents = client->GetArtistContent();
        if (contents.size() > 1 || ((hasMusic || clients.size() > 1) && contents.size() == 1))
        {
          for (const auto &content : contents)
          {
            std::string title = client->FormatContentTitle(content.title);
            CFileItemPtr pItem(new CFileItem(title));
            pItem->m_bIsFolder = true;
            pItem->m_bIsShareOrDrive = true;
            CPlexUtils::SetPlexItemProperties(*pItem, client);
            // have to do it this way because raw url has authToken as protocol option
            CURL curl(client->GetUrl());
            curl.SetProtocol(client->GetProtocol());
            std::string filename = StringUtils::Format("%s/%s", content.section.c_str(), (basePath == "root" || basePath == "artists"? "all":basePath.c_str()));
            curl.SetFileName(filename);
            pItem->SetPath("plex://music/" + basePath + "/" + Base64::Encode(curl.Get()));
            pItem->SetLabel(title);
            std::string value = content.thumb;
            if (!value.empty() && (value[0] == '/'))
              StringUtils::TrimLeft(value, "/");
            curl.SetFileName(value);
            pItem->SetIconImage(curl.Get());
            items.Add(pItem);
            client->AddSectionItem(pItem);
            CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory client(%s), title(%s)", client->GetServerName().c_str(), title.c_str());
          }
        }
        else if (contents.size() == 1)
        {
          CURL curl(client->GetUrl());
          curl.SetProtocol(client->GetProtocol());
          curl.SetFileName(contents[0].section + "/all");
          CPlexUtils::GetPlexArtistsOrAlbum(items, curl.Get(), false);
          items.SetContent("artists");
          items.SetPath("plex://music/albums/");
          CPlexUtils::SetPlexItemProperties(items, client);
          for (int item = 0; item < items.Size(); ++item)
            CPlexUtils::SetPlexItemProperties(*items[item], client);
          CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory '/all' client(%s), shows(%d)", client->GetServerName().c_str(), items.Size());
        }
      }
    }
    else
    {
      CPlexClientPtr client = CPlexServices::GetInstance().FindClient(strUrl);
      if (!client || !client->GetPresence())
      {
        CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory no client or client not present %s", CURL::GetRedacted(strUrl).c_str());
        return false;
      }
      
      std::string path = URIUtils::GetParentPath(strUrl);
      URIUtils::RemoveSlashAtEnd(path);
      path = URIUtils::GetFileName(path);
      
      std::string filter = "all";
      if (path == "albums")
        filter = "albums";
      
      if (path == "root" || path == "artists")
      {
        CPlexUtils::GetPlexArtistsOrAlbum(items,Base64::Decode(section), false);
        items.SetLabel(g_localizeStrings.Get(36917));
        items.SetContent("artist");
      }
      if (path == "albums")
      {
        CPlexUtils::GetPlexArtistsOrAlbum(items,Base64::Decode(section), true);
        items.SetLabel(g_localizeStrings.Get(36919));
        items.SetContent("albums");
      }
      if (path == "songs")
      {
        CPlexUtils::GetPlexSongs(items,Base64::Decode(section));
        items.SetLabel(g_localizeStrings.Get(36921));
        items.SetContent("songs");
      }
    }
    return true;
  }
  else
  {
    CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory got nothing from %s", CURL::GetRedacted(strUrl).c_str());
  }

  return false;
}

DIR_CACHE_TYPE CPlexDirectory::GetCacheType(const CURL& url) const
{
  return DIR_CACHE_NEVER;
}

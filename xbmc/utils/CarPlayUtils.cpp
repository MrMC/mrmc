/*
 *      Copyright (C) 2019 Team MrMC
 *      http://mrmc.tv
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

#include "system.h"
#include "Application.h"
#include "DllPaths.h"
#include "URL.h"
#include "GUIUserMessages.h"
#include "filesystem/File.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "utils/StringUtils.h"
#include <ifaddrs.h>
#include <arpa/inet.h>
#include "filesystem/SpecialProtocol.h"
#include "utils/Base64URL.h"
#include "CompileInfo.h"


//#include "utils/StringHasher.h"
//#import <Foundation/Foundation.h>
//#import <AVFoundation/AVFoundation.h>
//#import <UIKit/UIKit.h>
//#import <mach/mach_host.h>
//#import <sys/sysctl.h>


#include "CarPlayUtils.h"
#include "services/plex/PlexUtils.h"
#include "services/emby/EmbyUtils.h"
#include "services/plex/PlexServices.h"
#include "services/emby/EmbyServices.h"
#include "services/emby/EmbyViewCache.h"
#include "services/ServicesManager.h"

CFileItemList *CarPlayUtils::GetRecentlyAddedAlbums()
{
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
  std::string serverType = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
  CFileItemList *itemListData = new CFileItemList;
  if (serverType == "plex" || serverType == "emby")
  {
    CServicesManager::GetInstance().GetRecentlyAddedAlbums(*itemListData, 25, serverType, serverUUID);
  }
  else if (serverType == "mrmc")
  {

  }
  return itemListData;
}

bool CarPlayUtils::GetAlbumSongs(CFileItemPtr itemPtr, CFileItemList &items)
{
  return CServicesManager::GetInstance().GetAlbumSongs(*itemPtr, items);
}

CFileItemList *CarPlayUtils::GetMostPlayedSongs()
{
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
  std::string serverType = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
  CFileItemList *itemListData = new CFileItemList;
  if (serverType == "plex" || serverType == "emby")
  {
    CServicesManager::GetInstance().GetMostPlayedSongs(*itemListData, 25, serverType, serverUUID);
  }
  else if (serverType == "mrmc")
  {

  }
  return itemListData;
}

CFileItemList *CarPlayUtils::GetPlaylists()
{
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
  std::string serverType = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
  CFileItemList *itemListData = new CFileItemList;
  if (serverType == "plex")
  {
    std::string uuid = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
    CPlexClientPtr plexClient = CPlexServices::GetInstance().GetClient(uuid);
    if (plexClient)
    {
      std::vector<PlexSectionsContent> playlists = plexClient->GetPlaylistContent();
      for (const auto &playlist : playlists)
      {
        if (playlist.contentType == "audio")
        {
          CFileItemPtr item(new CFileItem());
          item->m_bIsFolder = true;
          item->m_bIsShareOrDrive = false;
          item->SetLabel(playlist.title);
          item->SetLabel2(playlist.duration);
          CURL curl(plexClient->GetUrl());
          curl.SetProtocol(plexClient->GetProtocol());
          curl.SetFileName(playlist.section);
          std::string strAction = "mediasources://plexmusicplaylistitems/" + Base64URL::Encode(curl.Get());
          item->SetPath(strAction);
          itemListData->Add(item);
        }
      }
    }
  }
  else if (serverType == "emby")
  {

  }
  else if (serverType == "mrmc")
  {

  }
  return itemListData;
}

CFileItemList *CarPlayUtils::GetPlaylistItems(std::string url)
{
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
  std::string serverType = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
  CFileItemList *itemListData = new CFileItemList;
  if (serverType == "plex")
  {
    std::string section = URIUtils::GetFileName(url);
    CPlexUtils::GetPlexMusicPlaylistItems(*itemListData, Base64URL::Decode(section));
  }
  else if (serverType == "emby")
  {

  }
  else if (serverType == "mrmc")
  {

  }
  return itemListData;
}

CFileItemList *CarPlayUtils::GetArtists()
{
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
  std::string serverType = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
  CFileItemList *itemListData = new CFileItemList;
  if (serverType == "plex" || serverType == "emby")
  {
    CServicesManager::GetInstance().GetAllAlbums(*itemListData, serverType, serverUUID);
  }
  else if (serverType == "mrmc")
  {

  }
  return itemListData;
}

CFileItemList *CarPlayUtils::GetArtistAlbum(std::string url)
{
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
  std::string serverType = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_TYPE);
  CFileItemList *itemListData = new CFileItemList;
  if (serverType == "plex")
  {
    std::string section = URIUtils::GetFileName(url);
    CPlexUtils::GetPlexArtistsOrAlbum(*itemListData, Base64URL::Decode(section),true);
  }
  else if (serverType == "emby")
  {

  }
  else if (serverType == "mrmc")
  {

  }
  return itemListData;
}



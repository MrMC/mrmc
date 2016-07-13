/*
 *      Copyright (C) 2015 Team MrMC
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

#import "system.h"

#import "TVOSTopShelf.h"

#include "Application.h"
#include "messaging/ApplicationMessenger.h"
#include "FileItem.h"
#include "DatabaseManager.h"
#include "guilib/GUIWindowManager.h"
#include "video/VideoThumbLoader.h"
#include "video/VideoInfoTag.h"
#include "video/dialogs/GUIDialogVideoInfo.h"
#include "video/windows/GUIWindowVideoNav.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "filesystem/File.h"

#include "utils/StringUtils.h"
#include "utils/URIUtils.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <mach/mach_host.h>
#import <sys/sysctl.h>

std::string CTVOSTopShelf::m_url;
bool        CTVOSTopShelf::m_handleUrl;

CTVOSTopShelf::CTVOSTopShelf()
{
  m_RecentlyAddedTV = new CFileItemList;
  m_RecentlyAddedMovies = new CFileItemList;
}

CTVOSTopShelf::~CTVOSTopShelf()
{
}

CTVOSTopShelf &CTVOSTopShelf::GetInstance()
{
  static CTVOSTopShelf sTopShelf;
  return sTopShelf;
}

void CTVOSTopShelf::SetTopShelfItems(CFileItemList& movies, CFileItemList& tv)
{
  // save these for later
  CFileItemList recentlyAddedTV;
  recentlyAddedTV.Copy(tv);
  m_RecentlyAddedTV->Assign(recentlyAddedTV);

  CFileItemList recentlyAddedMovies;
  recentlyAddedMovies.Copy(movies);
  m_RecentlyAddedMovies->Assign(recentlyAddedMovies);

  CVideoThumbLoader loader;
  NSMutableArray * movieArray = [[NSMutableArray alloc] init];
  NSMutableArray * tvArray = [[NSMutableArray alloc] init];
  NSUserDefaults *shared = [[NSUserDefaults alloc] initWithSuiteName:@"group.tv.mrmc.shared"];
  
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSURL* storeUrl = [fileManager containerURLForSecurityApplicationGroupIdentifier:@"group.tv.mrmc.shared"];
  storeUrl = [storeUrl URLByAppendingPathComponent:@"Library" isDirectory:TRUE];
  storeUrl = [storeUrl URLByAppendingPathComponent:@"Caches" isDirectory:TRUE];
  storeUrl = [storeUrl URLByAppendingPathComponent:@"RA" isDirectory:TRUE];
  
  // store all old thumbs in array
  NSMutableArray *filePaths = (NSMutableArray *)[fileManager contentsOfDirectoryAtPath:storeUrl.path error:nil];
  std::string raPath = [storeUrl.path UTF8String];
  
  
  if (movies.Size() > 0)
  {
    for (int i = 0; i < movies.Size() && i < 5; ++i)
    {
      CFileItemPtr item          = movies.Get(i);
      NSMutableDictionary * movieDict = [[NSMutableDictionary alloc] init];
      if (!item->HasArt("thumb"))
        loader.LoadItem(item.get());
      
      // srcPath == full path to the thumb
      std::string srcPath = item->GetArt("thumb");
      // make the destfilename different for distinguish files with the same name
      std::string fileName = std::to_string(item->GetVideoInfoTag()->m_iDbId) + URIUtils::GetFileName(srcPath);
      std::string destPath = URIUtils::AddFileToFolder(raPath, fileName);
      if (!XFILE::CFile::Exists(destPath))
        XFILE::CFile::Copy(srcPath,destPath);
      else
        // remove from array so it doesnt get deleted at the end
        if ([filePaths containsObject:[NSString stringWithUTF8String:fileName.c_str()]])
          [filePaths removeObject:[NSString stringWithUTF8String:fileName.c_str()]];
        
      
      [movieDict setValue:[NSString stringWithUTF8String:item->GetLabel().c_str()] forKey:@"title"];
      [movieDict setValue:[NSString stringWithUTF8String:fileName.c_str()] forKey:@"thumb"];
      std::string fullPath = StringUtils::Format("movie/%i", i);
      [movieDict setValue:[NSString stringWithUTF8String:fullPath.c_str()] forKey:@"url"];
      
      [movieArray addObject:movieDict];
    }
    [shared setObject:movieArray forKey:@"movies"];
    NSString *tvTitle = [NSString stringWithUTF8String:g_localizeStrings.Get(20386).c_str()];
    [shared setObject:tvTitle forKey:@"moviesTitle"];
  }
  else
  {
    // cleanup if there is no RA
    [shared removeObjectForKey:@"movies"];
    [shared removeObjectForKey:@"moviesTitle"];
  }
  
  if (tv.Size() > 0)
  {
    for (int i = 0; i < tv.Size() && i < 5; ++i)
    {
      std::string fileName;
      std::string seasonThumb;
      CFileItemPtr item = tv.Get(i);
      NSMutableDictionary * tvDict = [[NSMutableDictionary alloc] init];
      std::string title = StringUtils::Format("%s s%02de%02d",
                                              item->GetVideoInfoTag()->m_strShowTitle.c_str(),
                                              item->GetVideoInfoTag()->m_iSeason,
                                              item->GetVideoInfoTag()->m_iEpisode);
      
      if (item->IsMediaServiceBased())
      {
        seasonThumb = item->GetArt("tvshow.poster");
        fileName = URIUtils::GetFileName(seasonThumb);
      }
      else
      {
        if (!item->HasArt("thumb"))
          loader.LoadItem(item.get());
        if (item->GetVideoInfoTag()->m_iIdSeason > 0)
        {
          CVideoDatabase videodatabase;
          videodatabase.Open();
          seasonThumb = videodatabase.GetArtForItem(item->GetVideoInfoTag()->m_iIdSeason, MediaTypeSeason, "poster");

          videodatabase.Close();
        }
        fileName = std::to_string(item->GetVideoInfoTag()->m_iDbId) + URIUtils::GetFileName(seasonThumb);
      }
      std::string destPath = URIUtils::AddFileToFolder(raPath, fileName);
      if (!XFILE::CFile::Exists(destPath))
        XFILE::CFile::Copy(seasonThumb ,destPath);
      else
        // remove from array so it doesnt get deleted at the end
        if ([filePaths containsObject:[NSString stringWithUTF8String:fileName.c_str()]])
          [filePaths removeObject:[NSString stringWithUTF8String:fileName.c_str()]];
      
      [tvDict setValue:[NSString stringWithUTF8String:title.c_str()] forKey:@"title"];
      [tvDict setValue:[NSString stringWithUTF8String:fileName.c_str()] forKey:@"thumb"];
      std::string fullPath = StringUtils::Format("tv/%i", i);
      [tvDict setValue:[NSString stringWithUTF8String:fullPath.c_str()] forKey:@"url"];
      [tvArray addObject:tvDict];
    }
    [shared setObject:tvArray forKey:@"tv"];
    NSString *tvTitle = [NSString stringWithUTF8String:g_localizeStrings.Get(20387).c_str()];
    [shared setObject:tvTitle forKey:@"tvTitle"];
  }
  else
  {
    // cleanup if there is no RA
    [shared removeObjectForKey:@"tv"];
    [shared removeObjectForKey:@"tvTitle"];
  }
  
  // remove unused thumbs from cache folder
  for (NSString *strFiles in filePaths)
    [fileManager removeItemAtURL:[storeUrl URLByAppendingPathComponent:strFiles isDirectory:FALSE] error:nil];
  
  [shared synchronize];
}

void CTVOSTopShelf::RunTopShelf()
{
  if (m_handleUrl)
  {
    m_handleUrl = false;
    CFileItemPtr itemPtr;
    std::vector<std::string> split = StringUtils::Split(m_url, "/");
    int item = std::atoi(split[4].c_str());

    if (split[3] == "movie")
      itemPtr = m_RecentlyAddedMovies->Get(item);
    else
      itemPtr = m_RecentlyAddedTV->Get(item);

    if (split[2] == "display")
    {
      if (itemPtr)
        KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_SHOW_VIDEO_INFO, -1, -1,  static_cast<void*>(new CFileItem(*itemPtr)));
    }
    else //play
    {
      if (itemPtr)
      {
        int playlist = itemPtr->IsAudio() ? PLAYLIST_MUSIC : PLAYLIST_VIDEO;
        g_playlistPlayer.ClearPlaylist(playlist);
        g_playlistPlayer.SetCurrentPlaylist(playlist);

        // play media
        g_application.PlayMedia(*itemPtr, playlist);
      }
    }
  }
}

void CTVOSTopShelf::HandleTopShelfUrl(const std::string& url, const bool run)
{
  m_url = url;
  m_handleUrl = run;
}
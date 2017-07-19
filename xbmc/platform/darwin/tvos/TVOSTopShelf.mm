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

#if defined(APP_PACKAGE_LITE)
  #define GROUPID "group.tv.mrmc.lite.shared"
#else
  #define GROUPID "group.tv.mrmc.shared"
#endif

std::string CTVOSTopShelf::m_url;
bool        CTVOSTopShelf::m_handleUrl;

CTVOSTopShelf::CTVOSTopShelf()
{
  m_HomeShelfTVRA = new CFileItemList;
  m_HomeShelfTVPR = new CFileItemList;
  m_HomeShelfMoviesRA = new CFileItemList;
  m_HomeShelfMoviesPR = new CFileItemList;
}

CTVOSTopShelf::~CTVOSTopShelf()
{
}

CTVOSTopShelf &CTVOSTopShelf::GetInstance()
{
  static CTVOSTopShelf sTopShelf;
  return sTopShelf;
}

void CTVOSTopShelf::SetTopShelfItems(CFileItemList& moviesRA, CFileItemList& tvRA, CFileItemList& moviesPR, CFileItemList& tvPR)
{
  {
    CSingleLock lock (m_cs);
    // save these for later
    CFileItemList homeShelfTVRA;
    homeShelfTVRA.Copy(tvRA);
    m_HomeShelfTVRA->Assign(homeShelfTVRA);

    CFileItemList homeShelfTVPR;
    homeShelfTVPR.Copy(tvPR);
    m_HomeShelfTVPR->Assign(homeShelfTVPR);
    
    CFileItemList homeShelfMoviesRA;
    homeShelfMoviesRA.Copy(moviesRA);
    m_HomeShelfMoviesRA->Assign(homeShelfMoviesRA);
    
    CFileItemList homeShelfMoviesPR;
    homeShelfMoviesPR.Copy(moviesPR);
    m_HomeShelfMoviesPR->Assign(homeShelfMoviesPR);
  }

  CVideoThumbLoader loader;
  NSMutableArray * movieArrayRA = [[NSMutableArray alloc] init];
  NSMutableArray * tvArrayRA = [[NSMutableArray alloc] init];
  NSMutableArray * movieArrayPR = [[NSMutableArray alloc] init];
  NSMutableArray * tvArrayPR = [[NSMutableArray alloc] init];
  NSString* groupid = [NSString stringWithUTF8String:GROUPID];
  NSUserDefaults *shared = [[NSUserDefaults alloc] initWithSuiteName:groupid];
  
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSURL* storeUrl = [fileManager containerURLForSecurityApplicationGroupIdentifier:groupid];
  if (!storeUrl)
    return;

  storeUrl = [storeUrl URLByAppendingPathComponent:@"Library" isDirectory:TRUE];
  storeUrl = [storeUrl URLByAppendingPathComponent:@"Caches" isDirectory:TRUE];
  storeUrl = [storeUrl URLByAppendingPathComponent:@"RA" isDirectory:TRUE];
  
  // store all old thumbs in array
  NSMutableArray *filePaths = (NSMutableArray *)[fileManager contentsOfDirectoryAtPath:storeUrl.path error:nil];
  std::string raPath = [storeUrl.path UTF8String];
  
  NSString *movieTitleRA;
  NSString *tvTitleRA;
  NSString *movieTitlePR;
  NSString *tvTitlePR;
  
  // in progress items, if they exist
  if (m_HomeShelfMoviesRA->Size() > 0)
  {
    for (int i = 0; i < m_HomeShelfMoviesRA->Size() && i < 5; ++i)
    {
      CFileItemPtr item          = m_HomeShelfMoviesRA->Get(i);
      movieTitleRA               = [NSString stringWithUTF8String:item->GetProperty("ItemType").asString().c_str()];
      NSMutableDictionary * movieDictRA = [[NSMutableDictionary alloc] init];
      if (!item->HasArt("thumb"))
        loader.LoadItem(item.get());
      
      // srcPath == full path to the thumb
      std::string srcPath = item->GetArt("thumb");
      // make the destfilename different for distinguish files with the same name
      std::string fileName;
      if(item->IsMediaServiceBased())
        fileName = item->GetMediaServiceId() + URIUtils::GetFileName(srcPath);
      else
        fileName = std::to_string(item->GetVideoInfoTag()->m_iDbId) + URIUtils::GetFileName(srcPath);
      std::string destPath = URIUtils::AddFileToFolder(raPath, fileName);
      if (!XFILE::CFile::Exists(destPath))
        XFILE::CFile::Copy(srcPath,destPath);
      else
        // remove from array so it doesnt get deleted at the end
        if ([filePaths containsObject:[NSString stringWithUTF8String:fileName.c_str()]])
          [filePaths removeObject:[NSString stringWithUTF8String:fileName.c_str()]];
        
      
      [movieDictRA setValue:[NSString stringWithUTF8String:item->GetLabel().c_str()] forKey:@"title"];
      [movieDictRA setValue:[NSString stringWithUTF8String:fileName.c_str()] forKey:@"thumb"];
      std::string fullPath = StringUtils::Format("movieRA/%i", i);
      [movieDictRA setValue:[NSString stringWithUTF8String:fullPath.c_str()] forKey:@"url"];
      
      [movieArrayRA addObject:movieDictRA];
    }
    [shared setObject:movieArrayRA forKey:@"moviesRA"];
    [shared setObject:movieTitleRA forKey:@"moviesTitleRA"];
  }
  else
  {
    // cleanup if there is no RA
    [shared removeObjectForKey:@"moviesRA"];
    [shared removeObjectForKey:@"moviesTitleRA"];
  }
  
  if (m_HomeShelfTVRA->Size() > 0)
  {
    for (int i = 0; i < m_HomeShelfTVRA->Size() && i < 5; ++i)
    {
      std::string fileName;
      std::string seasonThumb;
      CFileItemPtr item = m_HomeShelfTVRA->Get(i);
      tvTitleRA = [NSString stringWithUTF8String:item->GetProperty("ItemType").asString().c_str()];
      NSMutableDictionary * tvDictRA = [[NSMutableDictionary alloc] init];
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
      
      [tvDictRA setValue:[NSString stringWithUTF8String:title.c_str()] forKey:@"title"];
      [tvDictRA setValue:[NSString stringWithUTF8String:fileName.c_str()] forKey:@"thumb"];
      std::string fullPath = StringUtils::Format("tvRA/%i", i);
      [tvDictRA setValue:[NSString stringWithUTF8String:fullPath.c_str()] forKey:@"url"];
      [tvArrayRA addObject:tvDictRA];
    }
    [shared setObject:tvArrayRA forKey:@"tvRA"];
    [shared setObject:tvTitleRA forKey:@"tvTitleRA"];
  }
  else
  {
    // cleanup if there is no RA
    [shared removeObjectForKey:@"tvRA"];
    [shared removeObjectForKey:@"tvTitleRA"];
  }
  
  // in progress items, if they exist
  if (m_HomeShelfMoviesPR->Size() > 0)
  {
    for (int i = 0; i < m_HomeShelfMoviesPR->Size() && i < 5; ++i)
    {
      CFileItemPtr item          = m_HomeShelfMoviesPR->Get(i);
      movieTitlePR               = [NSString stringWithUTF8String:item->GetProperty("ItemType").asString().c_str()];
      NSMutableDictionary * movieDictPR = [[NSMutableDictionary alloc] init];
      if (!item->HasArt("thumb"))
        loader.LoadItem(item.get());
      
      // srcPath == full path to the thumb
      std::string srcPath = item->GetArt("thumb");
      // make the destfilename different for distinguish files with the same name
      std::string fileName;
      if(item->IsMediaServiceBased())
        fileName = item->GetMediaServiceId() + URIUtils::GetFileName(srcPath);
      else
        fileName = std::to_string(item->GetVideoInfoTag()->m_iDbId) + URIUtils::GetFileName(srcPath);
      std::string destPath = URIUtils::AddFileToFolder(raPath, fileName);
      if (!XFILE::CFile::Exists(destPath))
        XFILE::CFile::Copy(srcPath,destPath);
      else
        // remove from array so it doesnt get deleted at the end
        if ([filePaths containsObject:[NSString stringWithUTF8String:fileName.c_str()]])
          [filePaths removeObject:[NSString stringWithUTF8String:fileName.c_str()]];
      
      
      [movieDictPR setValue:[NSString stringWithUTF8String:item->GetLabel().c_str()] forKey:@"title"];
      [movieDictPR setValue:[NSString stringWithUTF8String:fileName.c_str()] forKey:@"thumb"];
      std::string fullPath = StringUtils::Format("moviePR/%i", i);
      [movieDictPR setValue:[NSString stringWithUTF8String:fullPath.c_str()] forKey:@"url"];
      
      [movieArrayPR addObject:movieDictPR];
    }
    [shared setObject:movieArrayPR forKey:@"moviesPR"];
    [shared setObject:movieTitlePR forKey:@"moviesTitlePR"];
  }
  else
  {
    // cleanup if there is no PR
    [shared removeObjectForKey:@"moviesPR"];
    [shared removeObjectForKey:@"moviesTitlePR"];
  }
  
  if (m_HomeShelfTVPR->Size() > 0)
  {
    for (int i = 0; i < m_HomeShelfTVPR->Size() && i < 5; ++i)
    {
      std::string fileName;
      std::string seasonThumb;
      CFileItemPtr item = m_HomeShelfTVPR->Get(i);
      tvTitlePR = [NSString stringWithUTF8String:item->GetProperty("ItemType").asString().c_str()];
      NSMutableDictionary * tvDictPR = [[NSMutableDictionary alloc] init];
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
      
      [tvDictPR setValue:[NSString stringWithUTF8String:title.c_str()] forKey:@"title"];
      [tvDictPR setValue:[NSString stringWithUTF8String:fileName.c_str()] forKey:@"thumb"];
      std::string fullPath = StringUtils::Format("tvPR/%i", i);
      [tvDictPR setValue:[NSString stringWithUTF8String:fullPath.c_str()] forKey:@"url"];
      [tvArrayPR addObject:tvDictPR];
    }
    [shared setObject:tvArrayPR forKey:@"tvPR"];
    [shared setObject:tvTitlePR forKey:@"tvTitlePR"];
  }
  else
  {
    // cleanup if there is no RA
    [shared removeObjectForKey:@"tvPR"];
    [shared removeObjectForKey:@"tvTitlePR"];
  }
  
  // remove unused thumbs from cache folder
  for (NSString *strFiles in filePaths)
    [fileManager removeItemAtURL:[storeUrl URLByAppendingPathComponent:strFiles isDirectory:FALSE] error:nil];

  [shared synchronize];
}

bool CTVOSTopShelf::RunTopShelf()
{
  bool rtn = false;
  if (m_handleUrl)
  {
    rtn = true;
    m_handleUrl = false;
    CFileItemPtr itemPtr;
    std::vector<std::string> split = StringUtils::Split(m_url, "/");
    int item = std::atoi(split[4].c_str());

    if (split[3] == "movieRA")
      itemPtr = m_HomeShelfMoviesRA->Get(item);
    else if (split[3] == "moviePR")
      itemPtr = m_HomeShelfMoviesPR->Get(item);
    else if (split[3] == "tvRA")
      itemPtr = m_HomeShelfTVRA->Get(item);
    else
      itemPtr = m_HomeShelfTVPR->Get(item);

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
  return rtn;
}

void CTVOSTopShelf::HandleTopShelfUrl(const std::string& url, const bool run)
{
  m_url = url;
  m_handleUrl = run;
}

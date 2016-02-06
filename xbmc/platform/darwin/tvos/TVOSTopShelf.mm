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
#include "video/VideoDatabase.h"
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
      
      std::string fileName = URIUtils::GetFileName(item->GetArt("thumb").c_str());
      std::string destPath = URIUtils::AddFileToFolder(raPath,fileName);
      if (!XFILE::CFile::Exists(destPath))
        XFILE::CFile::Copy(item->GetArt("thumb").c_str(),destPath);
      else
        // remove from array so it doesnt get deleted at the end
        if ([filePaths containsObject:[NSString stringWithUTF8String:fileName.c_str()]])
          [filePaths removeObject:[NSString stringWithUTF8String:fileName.c_str()]];
        
      
      [movieDict setValue:[NSString stringWithUTF8String:item->GetLabel().c_str()] forKey:@"title"];
      [movieDict setValue:[NSString stringWithUTF8String:fileName.c_str()] forKey:@"thumb"];
      [movieDict setValue:[NSString stringWithFormat:@"%d",item->GetVideoInfoTag()->m_iDbId] forKey:@"url"];
      
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
      CFileItemPtr item = tv.Get(i);
      NSMutableDictionary * tvDict = [[NSMutableDictionary alloc] init];
      if (!item->HasArt("thumb"))
        loader.LoadItem(item.get());
      
      std::string title = StringUtils::Format("%s s%02de%02d",
                                              item->GetVideoInfoTag()->m_strShowTitle.c_str(),
                                              item->GetVideoInfoTag()->m_iSeason,
                                              item->GetVideoInfoTag()->m_iEpisode);
      
      std::string seasonThumb;
      
      if (item->GetVideoInfoTag()->m_iIdSeason > 0)
      {
        CVideoDatabase videodatabase;
        videodatabase.Open();
        seasonThumb = videodatabase.GetArtForItem(item->GetVideoInfoTag()->m_iIdSeason, MediaTypeSeason, "poster");
        
        videodatabase.Close();
      }
      
      std::string fileName = URIUtils::ReplaceExtension(title,URIUtils::GetExtension(seasonThumb.c_str()));
      std::string destPath = URIUtils::AddFileToFolder(raPath,fileName);
      if (!XFILE::CFile::Exists(destPath))
        XFILE::CFile::Copy(seasonThumb.c_str(),destPath);
      else
        // remove from array so it doesnt get deleted at the end
        if ([filePaths containsObject:[NSString stringWithUTF8String:fileName.c_str()]])
          [filePaths removeObject:[NSString stringWithUTF8String:fileName.c_str()]];
      
      [tvDict setValue:[NSString stringWithUTF8String:title.c_str()] forKey:@"title"];
      [tvDict setValue:[NSString stringWithUTF8String:fileName.c_str()] forKey:@"thumb"];
      [tvDict setValue:[NSString stringWithUTF8String:item->GetVideoInfoTag()->m_strUniqueId.c_str()] forKey:@"url"];
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
    
    std::vector<std::string> split = StringUtils::Split(m_url, "/");
    
    std::string url;
    CVideoDatabase videodatabase;
    if (videodatabase.Open())
    {
      if (split[3] == "tv")
      {
        url = videodatabase.GetEpisodeUrlFromUnique(split[4].c_str());
      }
      else
      {
        url = videodatabase.GetMovieUrlFromMovieId(split[4].c_str());
      }
      videodatabase.Close();
      
      if (split[2] == "display")
      {
        KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_SHOW_VIDEO_INFO, -1, -1, static_cast<void*>(new CFileItem(url.c_str(), false)));
      }
      else //play
      {
        // its a bit ugly, but only way to get resume window to show
        std::string cmd = StringUtils::Format("PlayMedia(%s)", StringUtils::Paramify(url.c_str()).c_str());
        KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);
      }
    }
  }
}

void CTVOSTopShelf::HandleTopShelfUrl(const std::string& url, const bool run)
{
  m_url = url;
  m_handleUrl = run;
}
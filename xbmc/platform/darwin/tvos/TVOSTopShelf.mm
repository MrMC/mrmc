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
#include "guilib/GUIWindowManager.h"
#include "video/VideoDatabase.h"
#include "video/VideoThumbLoader.h"
#include "video/VideoInfoTag.h"
#include "video/dialogs/GUIDialogVideoInfo.h"
#include "video/windows/GUIWindowVideoNav.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "filesystem/File.h"

#include "utils/StringUtils.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <mach/mach_host.h>
#import <sys/sysctl.h>

std::string CTVOSTopShelf::m_url;
bool        CTVOSTopShelf::m_handleUrl;

CTVOSTopShelf::CTVOSTopShelf()
{
  m_movieItems = new CFileItemList;
  m_tvItems= new CFileItemList;
}

CTVOSTopShelf::~CTVOSTopShelf()
{
}

CTVOSTopShelf &CTVOSTopShelf::GetInstance()
{
  static CTVOSTopShelf sTopShelf;
  return sTopShelf;
}

void CTVOSTopShelf::SetTopShelfItems()
{
  CVideoThumbLoader loader;
  NSMutableArray * movieArray = [[NSMutableArray alloc] init];
  NSMutableArray * tvArray = [[NSMutableArray alloc] init];
  NSUserDefaults *shared = [[NSUserDefaults alloc] initWithSuiteName:@"group.tv.mrmc.shared"];
  CFileItemList* movieItems = m_movieItems;
  
  if (movieItems && movieItems->Size() > 0)
  {
    for (int i = 0; i < movieItems->Size() && i < 5; ++i)
    {
      CFileItemPtr item          = movieItems->Get(i);
      NSMutableDictionary * movieDict = [[NSMutableDictionary alloc] init];
      if (!item->HasArt("thumb"))
        loader.LoadItem(item.get());
      
      [movieDict setValue:[NSString stringWithUTF8String:item->GetLabel().c_str()] forKey:@"title"];
      [movieDict setValue:[NSString stringWithUTF8String:item->GetArt("thumb").c_str()] forKey:@"thumb"];
      
      [movieArray addObject:movieDict];
    }
    [shared setObject:movieArray forKey:@"movies"];
    NSString *tvTitle = [NSString stringWithUTF8String:g_localizeStrings.Get(20386).c_str()];
    [shared setObject:tvTitle forKey:@"moviesTitle"];
  }
  
  CFileItemList* tvItems = m_tvItems;
  if (tvItems && tvItems->Size() > 0)
  {
    for (int i = 0; i < tvItems->Size() && i < 5; ++i)
    {
      CFileItemPtr item = tvItems->Get(i);
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
      
      [tvDict setValue:[NSString stringWithUTF8String:title.c_str()] forKey:@"title"];
      [tvDict setValue:[NSString stringWithUTF8String:seasonThumb.c_str()] forKey:@"thumb"];
      
      [tvArray addObject:tvDict];
    }
    [shared setObject:tvArray forKey:@"tv"];
    NSString *tvTitle = [NSString stringWithUTF8String:g_localizeStrings.Get(20387).c_str()];
    [shared setObject:tvTitle forKey:@"tvTitle"];
  }
  [shared synchronize];
}

void CTVOSTopShelf::SetMovieList(CFileItemList& movies )
{
  m_movieItems->ClearItems();
  m_movieItems->Append(movies);
}

void CTVOSTopShelf::SetTvList(CFileItemList& tv )
{
  m_tvItems->ClearItems();
  m_tvItems->Append(tv);
}

void CTVOSTopShelf::RunTopShelf()
{
  if (m_handleUrl)
  {
    m_handleUrl = false;
    std::vector<std::string> split = StringUtils::Split(m_url, "/");
    int item = std::atoi(split[4].c_str());    
    CFileItemPtr ptrItem;
    
    if (split[3] == "tv")
      ptrItem = m_tvItems->Get(item);
    else //movie
      ptrItem = m_movieItems->Get(item);
    
    if (split[2] == "display")
    {
      KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_SHOW_VIDEO_INFO, -1, -1, static_cast<void*>(new CFileItem(*ptrItem)));
    }
    else //play
    {
      // its a bit ugly, but only way to get resume window to show
      std::string cmd = StringUtils::Format("PlayMedia(%s)", StringUtils::Paramify(ptrItem->GetVideoInfoTag()->m_strFileNameAndPath).c_str());
      KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);
    }
  }
}

void CTVOSTopShelf::HandleTopShelfUrl(const std::string& url, const bool run)
{
  m_url = url;
  m_handleUrl = run;
}
/*
 *      Copyright (C) 2010-2016 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#import <UIKit/UIKit.h>

#import "system.h"

#import "Application.h"
#import "FileItem.h"
#import "music/tags/MusicInfoTag.h"
#import "video/VideoInfoTag.h"
#import "music/MusicDatabase.h"
#import "TextureCache.h"
#import "SpecialProtocol.h"
#import "PlayList.h"

#import "threads/Atomics.h"
#import "platform/darwin/ios-common/AnnounceReceiver.h"
#if defined(TARGET_DARWIN_TVOS)
#import "platform/darwin/tvos/MainController.h"
#else
#import "platform/darwin/ios/XBMCController.h"
#endif
#import "utils/StringUtils.h"
#import "utils/Variant.h"


id objectFromVariant(const CVariant &data);

NSArray *arrayFromVariantArray(const CVariant &data)
{
  if (!data.isArray())
    return nil;
  NSMutableArray *array = [[NSMutableArray alloc] initWithCapacity:data.size()];
  for (CVariant::const_iterator_array itr = data.begin_array(); itr != data.end_array(); ++itr)
    [array addObject:objectFromVariant(*itr)];

  return array;
}

NSDictionary *dictionaryFromVariantMap(const CVariant &data)
{
  if (!data.isObject())
    return nil;
  NSMutableDictionary *dict = [[NSMutableDictionary alloc] initWithCapacity:data.size()];
  for (CVariant::const_iterator_map itr = data.begin_map(); itr != data.end_map(); ++itr)
    [dict setValue:objectFromVariant(itr->second) forKey:[NSString stringWithUTF8String:itr->first.c_str()]];

  return dict;
}

id objectFromVariant(const CVariant &data)
{
  if (data.isNull())
    return nil;
  if (data.isString())
    return [NSString stringWithUTF8String:data.asString().c_str()];
  if (data.isWideString())
    return [NSString stringWithCString:(const char *)data.asWideString().c_str() encoding:NSUnicodeStringEncoding];
  if (data.isInteger())
    return [NSNumber numberWithLongLong:data.asInteger()];
  if (data.isUnsignedInteger())
    return [NSNumber numberWithUnsignedLongLong:data.asUnsignedInteger()];
  if (data.isBoolean())
    return [NSNumber numberWithInt:data.asBoolean()?1:0];
  if (data.isDouble())
    return [NSNumber numberWithDouble:data.asDouble()];
  if (data.isArray())
    return arrayFromVariantArray(data);
  if (data.isObject())
    return dictionaryFromVariantMap(data);

  return nil;
}

void AnnounceBridge(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  std::string item_type = "";
  CVariant nonConstData = data;
  const std::string msg(message);
  CFileItem curItem(g_application.CurrentFileItem());
  // handle data which only has a database id and not the metadata inside
  
  NSDictionary *dict = dictionaryFromVariantMap(nonConstData);
  //LOG(@"data: %@", dict.description);
  if (msg == "OnPlay")
  {
    NSDictionary *item = [dict valueForKey:@"item"];
    NSDictionary *player = [dict valueForKey:@"player"];
    [item setValue:[player valueForKey:@"speed"] forKey:@"speed"];
    std::string thumb = g_application.CurrentFileItem().GetArt("thumb");
    if (!thumb.empty())
    {
      bool needsRecaching;
      std::string cachedThumb(CTextureCache::GetInstance().CheckCachedImage(thumb, false, needsRecaching));
      //LOG("thumb: %s, %s", thumb.c_str(), cachedThumb.c_str());
      if (!cachedThumb.empty())
      {
        std::string thumbRealPath = CSpecialProtocol::TranslatePath(cachedThumb);
        UIImage *image = [UIImage imageWithContentsOfFile:[NSString stringWithUTF8String:thumbRealPath.c_str()]];
        [item setValue:image forKey:@"thumb"];
      }
    }
    double duration = g_application.GetTotalTime();
    if (duration > 0)
      [item setValue:[NSNumber numberWithDouble:duration] forKey:@"duration"];
    double elapsed = g_application.GetTime();
    if (elapsed >= 0)
      [item setValue:[NSNumber numberWithDouble:elapsed] forKey:@"elapsed"];
    int current = g_playlistPlayer.GetCurrentSong();
    if (current >= 0)
    {
      [item setValue:[NSNumber numberWithInt:current] forKey:@"current"];
      [item setValue:[NSNumber numberWithInt:g_playlistPlayer.GetPlaylist(g_playlistPlayer.GetCurrentPlaylist()).size()] forKey:@"total"];
    }
    if (curItem.HasMusicInfoTag())
    {
      const std::vector<std::string> &genre = g_application.CurrentFileItem().GetMusicInfoTag()->GetGenre();
      if (!genre.empty())
      {
        NSMutableArray *genreArray = [[NSMutableArray alloc] initWithCapacity:genre.size()];
        for(std::vector<std::string>::const_iterator it = genre.begin(); it != genre.end(); ++it)
        {
          [genreArray addObject:[NSString stringWithUTF8String:it->c_str()]];
        }
        [item setValue:genreArray forKey:@"genre"];
      }
      std::string strTrack = StringUtils::Format("%i", curItem.GetMusicInfoTag()->GetTrackNumber());
      std::string strArtist = StringUtils::Join(curItem.GetMusicInfoTag()->GetArtist(), ",");
      [item setValue:[NSString stringWithUTF8String:curItem.GetMusicInfoTag()->GetTitle().c_str()] forKey:@"title"];
      [item setValue:[NSString stringWithUTF8String:strTrack.c_str()] forKey:@"track"];
      [item setValue:[NSString stringWithUTF8String:curItem.GetMusicInfoTag()->GetAlbum().c_str()] forKey:@"album"];
      [item setValue:[NSString stringWithUTF8String:strArtist.c_str()] forKey:@"artist"];
    }
    else if(curItem.HasVideoInfoTag())
    {
      if(curItem.GetVideoInfoTag()->m_type == MediaTypeMovie)
      {
        [item setValue:[NSString stringWithUTF8String:curItem.GetVideoInfoTag()->m_strTitle.c_str()] forKey:@"title"];
        [item setValue:[NSString stringWithUTF8String:StringUtils::Format("(%i)", curItem.GetVideoInfoTag()->GetYear()).c_str()] forKey:@"artist"];
      }
      else if(curItem.GetVideoInfoTag()->m_type == MediaTypeEpisode)
      {
        std::string seasonEpisode = StringUtils::Format("S%02iE%02i", curItem.GetVideoInfoTag()->m_iSeason, curItem.GetVideoInfoTag()->m_iEpisode);
        [item setValue:[NSString stringWithUTF8String:seasonEpisode.c_str()] forKey:@"album"];
        [item setValue:[NSString stringWithUTF8String:curItem.GetVideoInfoTag()->m_strShowTitle.c_str()] forKey:@"title"];
        [item setValue:[NSString stringWithUTF8String:curItem.GetVideoInfoTag()->m_strTitle.c_str()] forKey:@"artist"];
      }
    }
    //LOG(@"item: %@", item.description);
    [g_xbmcController performSelectorOnMainThread:@selector(onPlayDelayed:) withObject:item  waitUntilDone:NO];
  }
  else if (msg == "OnSpeedChanged" || msg == "OnPause")
  {
    NSDictionary *item = [dict valueForKey:@"item"];
    NSDictionary *player = [dict valueForKey:@"player"];
    [item setValue:[player valueForKey:@"speed"] forKey:@"speed"];
    [item setValue:[NSNumber numberWithDouble:g_application.GetTime()] forKey:@"elapsed"];
    //LOG(@"item: %@", item.description);
    [g_xbmcController performSelectorOnMainThread:@selector(onSpeedChanged:) withObject:item  waitUntilDone:NO];
    if (msg == "OnPause")
      [g_xbmcController performSelectorOnMainThread:@selector(onPausePlaying:) withObject:[dict valueForKey:@"item"]  waitUntilDone:NO];
  }
  else if (msg == "OnStop")
  {
    [g_xbmcController performSelectorOnMainThread:@selector(onStopPlaying:) withObject:[dict valueForKey:@"item"]  waitUntilDone:NO];
  }
  else if (msg == "OnSeek")
  {
    [g_xbmcController performSelectorOnMainThread:@selector(onSeekPlaying) withObject:nil  waitUntilDone:NO];
  }
}


static std::atomic<long> sg_singleton_lock_variable {0};
CAnnounceReceiver* CAnnounceReceiver::m_instance = nullptr;
CAnnounceReceiver& CAnnounceReceiver::GetInstance()
{
  CAtomicSpinLock lock(sg_singleton_lock_variable);
  if (!m_instance)
    m_instance = new CAnnounceReceiver();

  return *m_instance;
}

void CAnnounceReceiver::Initialize()
{
  ANNOUNCEMENT::CAnnouncementManager::GetInstance().AddAnnouncer(m_instance);
}

void CAnnounceReceiver::DeInitialize()
{
  ANNOUNCEMENT::CAnnouncementManager::GetInstance().RemoveAnnouncer(m_instance);
}

void CAnnounceReceiver::Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  AnnounceBridge(flag, sender, message, data);
}

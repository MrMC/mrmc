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
 
#import <UIKit/UIKit.h>

#import "platform/darwin/NSLogDebugHelpers.h"
#import "platform/darwin/ios/CarPlayDelegate.h"
#import "platform/darwin/DarwinUtils.h"
#import "threads/Atomics.h"
#import "utils/CarPlayUtils.h"
#import "utils/Variant.h"
#import "settings/Settings.h"
#import "services/ServicesManager.h"
#import "playlists/PlayList.h"
#import "PlayListPlayer.h"
#import "guilib/GUIWindowManager.h"
#import "filesystem/Directory.h"
#import "URL.h"

@implementation CarPlayDelegate

CFileItemList *CCarPlayAnnounceReceiver::m_RAAlbums;
CFileItemList *CCarPlayAnnounceReceiver::m_MPSongs;
CFileItemList *CCarPlayAnnounceReceiver::m_Playlists;
CFileItemList *CCarPlayAnnounceReceiver::m_SelectedPlaylist;
CFileItemList *CCarPlayAnnounceReceiver::m_Artists;
CFileItemList *CCarPlayAnnounceReceiver::m_SelectedArtistAlbums;
CFileItemList *CCarPlayAnnounceReceiver::m_SelectedArtistAlbumsSongs;

#pragma mark - CarPlay Helpers

- (MPMediaItemArtwork *) MakeMediaItemArtwork:(NSString *) imageStr
{
  MPMediaItemArtwork *mArt;
  UIImage *logo = [UIImage imageNamed:imageStr];
  if (logo)
  {
    mArt = [[MPMediaItemArtwork alloc] initWithBoundsSize:logo.size requestHandler:^UIImage * _Nonnull(CGSize size) { return logo;}];
    if (mArt)
    {
      return mArt;
    }
  }
  return mArt;
}

- (MPMediaItemArtwork *) MakeMediaItemArtworkCURL:(CURL *) imageURL
{
  MPMediaItemArtwork *mArt;
  std::string strThumb = imageURL->GetWithoutOptions() + imageURL->GetOptions() + "&" + imageURL->GetProtocolOptions();
  NSString *thumb = [NSString stringWithCString:strThumb.c_str() encoding:[NSString defaultCStringEncoding]];
  UIImage *image = [UIImage imageWithData:[NSData dataWithContentsOfURL:[NSURL URLWithString:thumb]]];
  if (image)
  {
    mArt = [[MPMediaItemArtwork alloc] initWithBoundsSize:image.size requestHandler:^UIImage * _Nonnull(CGSize size) { return image;}];
    if (mArt)
    {
      return mArt;
    }
  }
  return mArt;
}

#pragma mark - CarPlay Delegate
- (MPContentItem *)contentItemAtIndexPath:(NSIndexPath *)indexPath
{
  MPContentItem *contentItem;
  auto IndexLength = [indexPath length];
  if (IndexLength == 1)
  {
    auto tabIndex = [indexPath indexAtPosition:0];
    if (tabIndex == 0)
    {
      contentItem = [[MPContentItem alloc] initWithIdentifier:@"recentlyadded"];
      contentItem.title = [NSString stringWithFormat:@"Recently Added"];
      contentItem.playable = NO;
      contentItem.container = YES;
      contentItem.artwork = [self MakeMediaItemArtwork:@"recentlyadded"];
    }
    else if (tabIndex == 1)
    {
      contentItem = [[MPContentItem alloc] initWithIdentifier:@"mostplayed"];
      contentItem.title = [NSString stringWithFormat:@"Most Played"];
      contentItem.playable = NO;
      contentItem.container = YES;
      contentItem.artwork = [self MakeMediaItemArtwork:@"mostplayed"];
    }
    else if (tabIndex == 2)
    {
      contentItem = [[MPContentItem alloc] initWithIdentifier:@"playlist"];
      contentItem.title = [NSString stringWithFormat:@"Playlists"];
      contentItem.playable = NO;
      contentItem.container = YES;
      contentItem.artwork = [self MakeMediaItemArtwork:@"playlist"];
    }
    else if (tabIndex == 3)
    {
      contentItem = [[MPContentItem alloc] initWithIdentifier:@"artist"];
      contentItem.title = [NSString stringWithFormat:@"Artists"];
      contentItem.playable = NO;
      contentItem.container = YES;
      contentItem.artwork = [self MakeMediaItemArtwork:@"artist"];
    }
  }
  else if (IndexLength == 2)
  {
    auto tabIndex = [indexPath indexAtPosition:0];
    auto itemIndex = [indexPath indexAtPosition:1];
    if (tabIndex == 0)
    {
//      CVariant album = albums[itemIndex];
      CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_RAAlbums->Get(itemIndex);
      contentItem = [[MPContentItem alloc] initWithIdentifier:[NSString stringWithFormat:@"recentlyaddedItem %lu", itemIndex]];
      contentItem.title = [NSString stringWithCString:itemPtr->GetLabel().c_str()
                                             encoding:[NSString defaultCStringEncoding]];
      contentItem.playable = YES;
      contentItem.container = NO;
      CURL curl(itemPtr->GetArt("thumb"));
      contentItem.artwork = [self MakeMediaItemArtworkCURL:&curl];
    }
    else if (tabIndex == 1) // List All Most Played
    {
      if (CCarPlayAnnounceReceiver::m_MPSongs->Size() > 0)
      {
        CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_MPSongs->Get(itemIndex);
        contentItem = [[MPContentItem alloc] initWithIdentifier:[NSString stringWithFormat:@"MostPlayedItem %lu", itemIndex]];
        contentItem.title = [NSString stringWithCString:itemPtr->GetLabel().c_str()
                                               encoding:[NSString defaultCStringEncoding]];
        contentItem.playable = YES;
        contentItem.container = NO;
        CURL curl(itemPtr->GetArt("thumb"));
        contentItem.artwork = [self MakeMediaItemArtworkCURL:&curl];
      }
    }
    else if (tabIndex == 2) // List All Playlists
    {
      if (CCarPlayAnnounceReceiver::m_Playlists->Size() > 0)
      {
        CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_Playlists->Get(itemIndex);
        contentItem = [[MPContentItem alloc] initWithIdentifier:[NSString stringWithFormat:@"AllPlaylistsItem %lu", itemIndex]];
        contentItem.title = [NSString stringWithCString:itemPtr->GetLabel().c_str()
                                               encoding:[NSString defaultCStringEncoding]];
        contentItem.playable = NO;
        contentItem.container = YES;
        CURL curl(itemPtr->GetArt("thumb"));
        contentItem.artwork = [self MakeMediaItemArtworkCURL:&curl];
      }
    }
    else if (tabIndex == 3) // List All Artists
    {
      if (CCarPlayAnnounceReceiver::m_Artists->Size() > 0)
      {
        CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_Artists->Get(itemIndex);
        contentItem = [[MPContentItem alloc] initWithIdentifier:[NSString stringWithFormat:@"ArtistItem %lu", itemIndex]];
        contentItem.title = [NSString stringWithCString:itemPtr->GetLabel().c_str()
                                               encoding:[NSString defaultCStringEncoding]];
        contentItem.playable = NO;
        contentItem.container = YES;
        CURL curl(itemPtr->GetArt("thumb"));
        contentItem.artwork = [self MakeMediaItemArtworkCURL:&curl];
      }
    }
  }
  else if (IndexLength == 3)
  {
    auto tabIndex = [indexPath indexAtPosition:0];
//    auto itemIndex = [indexPath indexAtPosition:1];
    auto itemIndex2 = [indexPath indexAtPosition:2];
    if (tabIndex == 2)
    {
      // Playlist listing
      if (CCarPlayAnnounceReceiver::m_SelectedPlaylist->Size() > 0)
      {
        CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_SelectedPlaylist->Get(itemIndex2);
        contentItem = [[MPContentItem alloc] initWithIdentifier:[NSString stringWithFormat:@"PlaylistItem %lu", itemIndex2]];
        contentItem.title = [NSString stringWithCString:itemPtr->GetLabel().c_str()
                                               encoding:[NSString defaultCStringEncoding]];
        contentItem.playable = YES;
        contentItem.container = NO;
        CURL curl(itemPtr->GetArt("thumb"));
        contentItem.artwork = [self MakeMediaItemArtworkCURL:&curl];
      }
    }
    else if (tabIndex == 3)
    {
      // List Artist albums
      if (CCarPlayAnnounceReceiver::m_SelectedArtistAlbums->Size() > 0)
      {
        CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_SelectedArtistAlbums->Get(itemIndex2);
        contentItem = [[MPContentItem alloc] initWithIdentifier:[NSString stringWithFormat:@"ArtistAlbumsItem %lu", itemIndex2]];
        contentItem.title = [NSString stringWithCString:itemPtr->GetLabel().c_str()
                                               encoding:[NSString defaultCStringEncoding]];
        contentItem.playable = NO;
        contentItem.container = YES;
        CURL curl(itemPtr->GetArt("thumb"));
        contentItem.artwork = [self MakeMediaItemArtworkCURL:&curl];
//      contentItem.streamingContent = YES;
      }
    }
  }
  else if (IndexLength == 4)
  {
    auto tabIndex = [indexPath indexAtPosition:0];
    //    auto itemIndex = [indexPath indexAtPosition:1];
    auto itemIndex3 = [indexPath indexAtPosition:3];
    if (tabIndex == 3)
    {
      // Artist/Album/Songs listing
      if (CCarPlayAnnounceReceiver::m_SelectedArtistAlbumsSongs->Size() > 0)
      {
        CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_SelectedArtistAlbumsSongs->Get(itemIndex3);
        contentItem = [[MPContentItem alloc] initWithIdentifier:[NSString stringWithFormat:@"PlaylistItem %lu", itemIndex3]];
        contentItem.title = [NSString stringWithCString:itemPtr->GetLabel().c_str()
                                               encoding:[NSString defaultCStringEncoding]];
        contentItem.playable = YES;
        contentItem.container = NO;
        CURL curl(itemPtr->GetArt("thumb"));
        contentItem.artwork = [self MakeMediaItemArtworkCURL:&curl];
      }
    }
  }
  return contentItem;
}

- (NSInteger)numberOfChildItemsAtIndexPath:(NSIndexPath *)indexPath
{
  auto indexPathLength = indexPath.length;
  auto tabIndex = [indexPath indexAtPosition:0];
  if (indexPathLength == 0) // Number of tabs
  {
    return 4;
  }
  else if (indexPathLength == 1)
  {
    if (tabIndex == 0)
    {
      CCarPlayAnnounceReceiver::m_RAAlbums = CarPlayUtils::GetRecentlyAddedAlbums();
      return CCarPlayAnnounceReceiver::m_RAAlbums->Size(); // Length recently added
    }
    else if (tabIndex == 1)
    {
      CCarPlayAnnounceReceiver::m_MPSongs  = CarPlayUtils::GetMostPlayedSongs();
      return CCarPlayAnnounceReceiver::m_MPSongs->Size(); // Length most played
    }
    else if (tabIndex == 2)
    {
      CCarPlayAnnounceReceiver::m_Playlists= CarPlayUtils::GetPlaylists();
      return CCarPlayAnnounceReceiver::m_Playlists->Size(); // Length playlists
    }
    else if (tabIndex == 3)
    {
      CCarPlayAnnounceReceiver::m_Artists = CarPlayUtils::GetArtists();
      return CCarPlayAnnounceReceiver::m_Artists->Size(); // Length Artists
    }
  }
  else if (indexPath.length == 2)
  {
    auto tabIndex1 = [indexPath indexAtPosition:1];
    if (tabIndex == 2)
    {
      if (CCarPlayAnnounceReceiver::m_Playlists->Size() > 0)
      {
        CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_Playlists->Get(tabIndex1);
        CCarPlayAnnounceReceiver::m_SelectedPlaylist->ClearItems();
        CCarPlayAnnounceReceiver::m_SelectedPlaylist = CarPlayUtils::GetPlaylistItems(itemPtr->GetPath());
        return CCarPlayAnnounceReceiver::m_SelectedPlaylist->Size(); // Length of selected Playlist
      }
    }
    if (tabIndex == 3)
    {
      if (CCarPlayAnnounceReceiver::m_Artists->Size() > 0)
      {
        CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_Artists->Get(tabIndex1);
        CCarPlayAnnounceReceiver::m_SelectedArtistAlbums->ClearItems();
        CCarPlayAnnounceReceiver::m_SelectedArtistAlbums = CarPlayUtils::GetArtistAlbum(itemPtr->GetPath());
        return CCarPlayAnnounceReceiver::m_SelectedArtistAlbums->Size(); // Length of selected Artist Albums
      }
    }
  }
  else if (indexPath.length == 3)
  {
    auto tabIndex2 = [indexPath indexAtPosition:2];
    if (tabIndex == 3)
    {
      if (CCarPlayAnnounceReceiver::m_SelectedArtistAlbums->Size() > 0)
      {
        CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_SelectedArtistAlbums->Get(tabIndex2);
        CCarPlayAnnounceReceiver::m_SelectedArtistAlbumsSongs->ClearItems();
        CarPlayUtils::GetAlbumSongs(itemPtr, *CCarPlayAnnounceReceiver::m_SelectedArtistAlbumsSongs);
  //      m_SelectedArtistAlbumsSongs = CarPlayUtils::GetAlbumSongs(CFileItemPtr itemPtr, CFileItemList &items)
        return CCarPlayAnnounceReceiver::m_SelectedArtistAlbumsSongs->Size(); // Length Selected Artist songs in Album
      }
    }
  }
  return 0;
}

- (void)playableContentManager:(MPPlayableContentManager *)contentManager
initiatePlaybackOfContentItemAtIndexPath:(NSIndexPath *)indexPath
             completionHandler:(void (^)(NSError *))completionHandler {
  auto indexPathLength = indexPath.length;
  auto tabIndex = [indexPath indexAtPosition:0];
  CFileItemList items;
  int playOffset = 0;
  if (indexPathLength == 2)
  {
    auto tabIndex1 = [indexPath indexAtPosition:1];
    if (tabIndex == 0)
    {
      // Play RA Albums
      CFileItemPtr itemPtr = CCarPlayAnnounceReceiver::m_RAAlbums->Get(tabIndex1);
      CarPlayUtils::GetAlbumSongs(itemPtr, items);
    }
    else if (tabIndex == 1)
    {
      // Play Most Play (songs)
      // we still send all 25 songs to playback
      // but start playback from the selected one
      playOffset = tabIndex1;
      items.Assign(*CCarPlayAnnounceReceiver::m_MPSongs);
    }
  }
  else if (indexPath.length == 3)
  {
    auto tabIndex2 = [indexPath indexAtPosition:2];
    if (tabIndex == 2)
    {
      // Play Playlist items
      playOffset = tabIndex2;
      items.Assign(*CCarPlayAnnounceReceiver::m_SelectedPlaylist);
    }
  }
  else if (indexPath.length == 4)
  {
    auto tabIndex3 = [indexPath indexAtPosition:3];
    if (tabIndex == 3)
    {
      // Play Playlist items
      playOffset = tabIndex3;
      items.Assign(*CCarPlayAnnounceReceiver::m_SelectedArtistAlbumsSongs);
    }
  }
  g_playlistPlayer.Reset();
  g_playlistPlayer.SetCurrentPlaylist(PLAYLIST_MUSIC);
  PLAYLIST::CPlayList& playlist = g_playlistPlayer.GetPlaylist(PLAYLIST_MUSIC);
  playlist.Clear();
  playlist.Add(items);
  g_playlistPlayer.Play(playOffset);
  // activate visualisation screen
  g_windowManager.ActivateWindow(WINDOW_VISUALISATION);
  completionHandler(nil);

#if TARGET_IPHONE_SIMULATOR
  // Workaround to make the Now Playing working on the simulator:
  dispatch_async(dispatch_get_main_queue(), ^{
    [[UIApplication sharedApplication] endReceivingRemoteControlEvents];
    [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];
  });
#endif
}

@end

#pragma mark - CarPlay Announcer
static std::atomic<long> sg_singleton_lock_variable {0};
CCarPlayAnnounceReceiver* CCarPlayAnnounceReceiver::m_instance = nullptr;
CCarPlayAnnounceReceiver& CCarPlayAnnounceReceiver::GetInstance()
{
  CAtomicSpinLock lock(sg_singleton_lock_variable);
  if (!m_instance)
    m_instance = new CCarPlayAnnounceReceiver();

  return *m_instance;
}

void CCarPlayAnnounceReceiver::Initialize()
{
  m_RAAlbums = new CFileItemList;
  m_MPSongs = new CFileItemList;
  m_Playlists = new CFileItemList;
  m_SelectedPlaylist = new CFileItemList;
  m_Artists = new CFileItemList;
  m_SelectedArtistAlbums = new CFileItemList;
  m_SelectedArtistAlbumsSongs = new CFileItemList;
  ANNOUNCEMENT::CAnnouncementManager::GetInstance().AddAnnouncer(m_instance);
  [[MPPlayableContentManager sharedContentManager] endUpdates];
  [[MPPlayableContentManager sharedContentManager] reloadData];
}

void CCarPlayAnnounceReceiver::DeInitialize()
{
  ANNOUNCEMENT::CAnnouncementManager::GetInstance().RemoveAnnouncer(m_instance);
}

void CCarPlayAnnounceReceiver::Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
//  AnnounceBridge(flag, sender, message, data);
  std::string serverUUID = CSettings::GetInstance().GetString(CSettings::SETTING_GENERAL_SERVER_UUID);
  if (!data.isMember("uuid") || (serverUUID == data["uuid"].asString()))
  {
    [[MPPlayableContentManager sharedContentManager] endUpdates];
    [[MPPlayableContentManager sharedContentManager] reloadData];
  }
}

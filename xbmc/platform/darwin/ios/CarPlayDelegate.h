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
#import <MediaPlayer/MediaPlayer.h>
//#import <AVFoundation/AVFoundation.h>
#import "interfaces/AnnouncementManager.h"

class CVariant;
@interface CarPlayDelegate : NSObject <MPPlayableContentDataSource, MPPlayableContentDelegate> {
}
- (MPMediaItemArtwork *) MakeMediaItemArtwork:(NSString *) imageStr;
- (MPMediaItemArtwork *) MakeMediaItemArtworkCURL:(CURL *) imageURL;
@end

class CCarPlayAnnounceReceiver : public ANNOUNCEMENT::IAnnouncer
{
public:
  static  CCarPlayAnnounceReceiver& GetInstance();

  void    Initialize();
  void    DeInitialize();

  virtual void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data);
  static CFileItemList *m_RAAlbums;
  static CFileItemList *m_MPSongs;
  static CFileItemList *m_Playlists;
  static CFileItemList *m_SelectedPlaylist;
  static CFileItemList *m_Artists;
  static CFileItemList *m_SelectedArtistAlbums;
  static CFileItemList *m_SelectedArtistAlbumsSongs;
private:
  CCarPlayAnnounceReceiver() {};
  static CCarPlayAnnounceReceiver *m_instance;
};

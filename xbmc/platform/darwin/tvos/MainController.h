/*
 *      Copyright (C) 2010-2013 Team XBMC
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
 *
 *  Refactored. Copyright (C) 2015 Team MrMC
 *  https://github.com/MrMC
 *
 */

#import <UIKit/UIKit.h>
#import "windowing/XBMC_events.h"

@class MainEAGLView;

typedef enum
{
  IOS_PLAYBACK_STOPPED,
  IOS_PLAYBACK_PAUSED,
  IOS_PLAYBACK_PLAYING
} IOSPlaybackState;

@interface MainController : UIViewController <UIGestureRecognizerDelegate>
{
@private
  UIWindow                   *m_window;
  MainEAGLView               *m_glView;
  int                         m_screensaverTimeout;
  // Touch handling
  CGSize                      m_screensize;
  CGPoint                     m_lastGesturePoint;
  CGFloat                     m_screenScale;
  bool                        m_touchBeginSignaled;
  int                         m_screenIdx;

  bool                        m_isPlayingBeforeInactive;
  UIBackgroundTaskIdentifier  m_bgTask;
  NSTimer                    *m_networkAutoSuspendTimer;
  IOSPlaybackState            m_playbackState;
  NSDictionary               *m_nowPlayingInfo;

  BOOL                        m_pause;
  BOOL                        m_appAlive;
  BOOL                        m_animating;
  BOOL                        m_readyToRun;
  NSConditionLock            *m_animationThreadLock;
  NSThread                   *m_animationThread;
}
// why are these properties ?
@property (nonatomic, retain) NSDictionary *m_nowPlayingInfo;
@property (nonatomic, retain) NSTimer      *m_networkAutoSuspendTimer;
@property (nonatomic, strong) NSTimer      *m_holdTimer;
@property int                 m_holdCounter;
@property CGPoint             m_lastGesturePoint;
@property CGFloat             m_screenScale;
@property bool                m_touchBeginSignaled;
@property int                 m_screenIdx;
@property CGSize              m_screensize;

- (void) pauseAnimation;
- (void) resumeAnimation;
- (void) startAnimation;
- (void) stopAnimation;

- (void) enterBackground;
- (void) enterForeground;
- (void) becomeInactive;
- (void) setIOSNowPlayingInfo:(NSDictionary *)info;
- (void) sendKeyDownUp:(XBMCKey)key;
- (void) observeDefaultCenterStuff: (NSNotification *)notification;
- (void) setFramebuffer;
- (bool) presentFramebuffer;
- (CGSize) getScreenSize;
- (CGFloat) getScreenScale:(UIScreen *)screen;
- (void) activateKeyboard:(UIView *)view;
- (void) deactivateKeyboard:(UIView *)view;

- (void) disableNetworkAutoSuspend;
- (void) enableNetworkAutoSuspend:(id)obj;
- (void) disableSystemSleep;
- (void) enableSystemSleep;
- (void) disableScreenSaver;
- (void) enableScreenSaver;
- (bool) changeScreen: (unsigned int)screenIdx withMode:(UIScreenMode *)mode;
- (void) activateScreen: (UIScreen *)screen;
  // message from which our instance is obtained
- (id)   initWithFrame:(CGRect)frame withScreen:(UIScreen *)screen;
- (void) insertVideoView:(UIView*)view;
- (void) removeVideoView:(UIView*)view;
@end

extern MainController *g_xbmcController;

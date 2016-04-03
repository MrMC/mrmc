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
 */

#import <UIKit/UIKit.h>

@class IOSEAGLView;

#import "input/XBMC_keysym.h"
#import "windowing/XBMC_events.h"

@class IOSEAGLView;

typedef enum
{
  IOS_PLAYBACK_STOPPED,
  IOS_PLAYBACK_PAUSED,
  IOS_PLAYBACK_PLAYING
} IOSPlaybackState;

@interface XBMCController : UIViewController <UIGestureRecognizerDelegate, UIKeyInput>
{
  UIWindow                   *m_window;
  IOSEAGLView                *m_glView;
  int                         m_screensaverTimeout;
	  // Touch handling
  CGSize                      m_screensize;
  CGPoint                     m_lastGesturePoint;
  CGFloat                     m_screenScale;
  bool                        m_touchBeginSignaled;
  int                         m_screenIdx;

  UIInterfaceOrientation      m_orientation;
  
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
@property (nonatomic, retain) NSDictionary *m_nowPlayingInfo;
@property (nonatomic, retain) NSTimer      *m_networkAutoSuspendTimer;
@property CGPoint             m_lastGesturePoint;
@property CGFloat             m_screenScale;
@property bool                m_touchBeginSignaled;
@property int                 m_screenIdx;
@property CGSize              m_screensize;

// message from which our instance is obtained
- (void) pauseAnimation;
- (void) resumeAnimation;
- (void) startAnimation;
- (void) stopAnimation;

- (void) enterBackground;
- (void) enterForeground;
- (void) becomeInactive;
- (void) audioRouteChanged;

- (void) setIOSNowPlayingInfo:(NSDictionary *)info;
- (void) sendKey: (XBMCKey) key;
- (void) observeDefaultCenterStuff: (NSNotification *) notification;
- (void) setFramebuffer;
- (bool) presentFramebuffer;
- (CGSize) getScreenSize;
- (CGFloat) getScreenScale:(UIScreen *)screen;
- (void) createGestureRecognizers;
- (void) activateKeyboard:(UIView *)view;
- (void) deactivateKeyboard:(UIView *)view;

- (void) disableNetworkAutoSuspend;
- (void) enableNetworkAutoSuspend:(id)obj;
- (void) disableSystemSleep;
- (void) enableSystemSleep;
- (void) disableScreenSaver;
- (void) enableScreenSaver;
- (bool) resetSystemIdleTimer;

- (NSArray<UIScreenMode *> *) availableScreenModes:(UIScreen*) screen;
- (UIScreenMode*) preferredScreenMode:(UIScreen*) screen;
- (bool) changeScreen: (unsigned int)screenIdx withMode:(UIScreenMode *)mode;
- (void) activateScreen: (UIScreen *)screen withOrientation:(UIInterfaceOrientation)newOrientation;
  // message from which our instance is obtained
- (id)   initWithFrame:(CGRect)frame withScreen:(UIScreen *)screen;

- (void) insertVideoView:(UIView*)view;
- (void) removeVideoView:(UIView*)view;
@end

extern XBMCController *g_xbmcController;

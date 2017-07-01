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
 *  Refactored. Copyright (C) 2015-2017 Team MrMC
 *  https://github.com/MrMC
 *
 */

#import <UIKit/UIKit.h>

typedef enum
{
  MC_NONE = 0,
  MC_ACTIVE,
  MC_INACTIVE,
  MC_INACTIVE_ISPAUSED,   // player was in pause state
  MC_INACTIVE_WASPAUSED,  // player was playing and we paused it
  MC_BACKGROUND,
  MC_BACKGROUND_RESTORE,
} MC_STATES;

typedef NS_ENUM(NSUInteger, UIPanGestureRecognizerDirection)
{
  UIPanGestureRecognizerDirectionUndefined,
  UIPanGestureRecognizerDirectionUp,
  UIPanGestureRecognizerDirectionDown,
  UIPanGestureRecognizerDirectionLeft,
  UIPanGestureRecognizerDirectionRight
};

@class MainEAGLView;

@interface MainController : UIViewController <UIGestureRecognizerDelegate>
{
@private
  UIWindow                   *m_window;
  MainEAGLView               *m_glView;
  // Touch handling
  CGSize                      m_screensize;
  CGFloat                     m_screenScale;
  int                         m_screenIdx;


  UIBackgroundTaskIdentifier  m_bgTask;
  NSDictionary               *m_nowPlayingInfo;
  double                      m_wasPlayingTime;

  BOOL                        m_pause;
  BOOL                        m_appAlive;
  BOOL                        m_animating;
  MC_STATES                   m_controllerState;
  BOOL                        m_disableIdleTimer;
  NSConditionLock            *m_animationThreadLock;
  NSThread                   *m_animationThread;
  BOOL                        m_mimicAppleSiri;
  BOOL                        m_remoteIdleState;
  CGFloat                     m_remoteIdleTimeout;
  BOOL                        m_enableRemoteIdle;
  BOOL                        m_allowTap;
}
// why are these properties ?
@property (nonatomic, strong) NSTimer *m_holdTimer;
@property (nonatomic, retain) NSDictionary *m_nowPlayingInfo;
@property int                 m_holdCounter;
@property CGFloat             m_screenScale;
@property int                 m_screenIdx;
@property CGSize              m_screensize;
@property BOOL                m_remoteIdleState;
@property CGFloat             m_remoteIdleTimeout;
@property BOOL                m_enableRemoteIdle;
@property BOOL                m_allowTap;

- (void)onPlayDelayed:(NSDictionary *)item;
- (void)onSpeedChanged:(NSDictionary *)item;
- (void)onPausePlaying:(NSDictionary *)item;
- (void)onStopPlaying:(NSDictionary *)item;
- (void)onSeekPlaying;

- (void) pauseAnimation;
- (void) resumeAnimation;
- (void) startAnimation;
- (void) stopAnimation;

- (void) enterForeground;
- (void) becomeActive;
- (void) becomeInactive;
- (void) enterBackground;

- (void) audioRouteChanged;
- (EAGLContext*) getEAGLContextObj;

- (void) setFramebuffer;
- (bool) presentFramebuffer;
- (CGSize) getScreenSize;
- (void) activateKeyboard:(UIView *)view;
- (void) deactivateKeyboard:(UIView *)view;

- (void) enableBackGroundTask;
- (void) disableBackGroundTask;

- (void) disableSystemSleep;
- (void) enableSystemSleep;
- (void) disableScreenSaver;
- (void) enableScreenSaver;
- (bool) resetSystemIdleTimer;
- (void) setRemoteIdleTimeout:(int)timeout;
- (void) enableRemoteIdle:(BOOL)enable;
- (void) enableRemotePanSwipe:(BOOL)enable;

- (NSArray<UIScreenMode *> *) availableScreenModes:(UIScreen*) screen;
- (UIScreenMode*) preferredScreenMode:(UIScreen*) screen;
- (bool) changeScreen: (unsigned int)screenIdx withMode:(UIScreenMode *)mode;
  // message from which our instance is obtained
- (id)   initWithFrame:(CGRect)frame withScreen:(UIScreen *)screen;
- (void) insertVideoView:(UIView*)view;
- (void) removeVideoView:(UIView*)view;
@end

extern MainController *g_xbmcController;

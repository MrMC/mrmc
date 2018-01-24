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
#import "guilib/Geometry.h"

#define SHOW_FOCUS_FRAMES 1

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

typedef enum
{
  TOUCH_UP = 0,
  TOUCH_DOWN,
  TOUCH_LEFT,
  TOUCH_RIGHT,
  TOUCH_CENTER
} TOUCH_POSITION;

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
  BOOL                        m_enableRemoteExpertMode;
  BOOL                        m_stopPlaybackOnMenu;
  BOOL                        m_nativeKeyboardActive;
  BOOL                        m_focusIdleState;
  TOUCH_POSITION              m_touchPosition;
}

// why are these properties ?
@property (nonatomic, strong) NSTimer *m_selectHoldTimer;
@property (nonatomic, strong) NSTimer *m_irArrowHoldTimer;
@property (nonatomic, retain) NSDictionary *m_nowPlayingInfo;
@property int                 m_selectHoldCounter;
@property int                 m_irArrowHoldCounter;
@property CGFloat             m_screenScale;
@property int                 m_screenIdx;
@property CGSize              m_screensize;
@property BOOL                m_focusIdleState;
@property CGFloat             m_focusIdleTimeout;
@property BOOL                m_enableRemoteExpertMode;
@property BOOL                m_stopPlaybackOnMenu;
@property TOUCH_POSITION      m_touchPosition;

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

- (void) updateFocusLayerMainThread;

- (void) setFramebuffer;
- (bool) presentFramebuffer;
- (CGSize) getScreenSize;
- (void) activateKeyboard:(UIView *)view;
- (void) deactivateKeyboard:(UIView *)view;
- (void) nativeKeyboardActive:(bool)active;

- (void) enableBackGroundTask;
- (void) disableBackGroundTask;

- (void) disableSystemSleep;
- (void) enableSystemSleep;
- (void) disableScreenSaver;
- (void) enableScreenSaver;
- (bool) resetSystemIdleTimer;
- (void) enableRemoteExpertMode:(BOOL)enable;
- (void) stopPlaybackOnMenu:(BOOL)enable;


- (NSArray<UIScreenMode *> *) availableScreenModes:(UIScreen*) screen;
- (UIScreenMode*) preferredScreenMode:(UIScreen*) screen;
- (bool) changeScreen: (unsigned int)screenIdx withMode:(UIScreenMode *)mode;
  // message from which our instance is obtained
- (id)   initWithFrame:(CGRect)frame withScreen:(UIScreen *)screen;
- (void) insertVideoView:(UIView*)view;
- (void) removeVideoView:(UIView*)view;
- (float) getDisplayRate;
- (void)  displayRateSwitch:(float)refreshRate withDynamicRange:(int)dynamicRange;
- (void)  displayRateReset;
@end

extern MainController *g_xbmcController;

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

#import "system.h"

#import "Application.h"

#import "cores/AudioEngine/AEFactory.h"
#import "guilib/GUIWindowManager.h"
#import "input/Key.h"
#import "interfaces/AnnouncementManager.h"
#import "network/NetworkServices.h"
#import "messaging/ApplicationMessenger.h"
#import "platform/darwin/NSLogDebugHelpers.h"
#import "platform/darwin/tvos/MainEAGLView.h"
#import "platform/darwin/tvos/MainController.h"
#import "platform/darwin/tvos/MainApplication.h"
#import "platform/darwin/tvos/TVOSTopShelf.h"
#import "platform/darwin/ios-common/AnnounceReceiver.h"
#import "platform/MCRuntimeLib.h"
#import "platform/MCRuntimeLibContext.h"
#import "windowing/WindowingFactory.h"
#import "settings/Settings.h"
#import "services/lighteffects/LightEffectServices.h"
#import "utils/SeekHandler.h"

#import <MediaPlayer/MPMediaItem.h>
#import <MediaPlayer/MediaPlayer.h>
#import <MediaPlayer/MPNowPlayingInfoCenter.h>

#import <AVFoundation/AVAudioPlayer.h>
#import <AudioToolbox/AudioToolbox.h>

using namespace KODI::MESSAGING;

MainController *g_xbmcController;

//--------------------------------------------------------------
#pragma mark - MainController interface
@interface MainController ()
@property (strong, nonatomic) NSTimer *pressAutoRepeatTimer;
@property (strong, nonatomic) NSTimer *remoteIdleTimer;
@end

#pragma mark - MainController implementation
@implementation MainController

@synthesize m_lastGesturePoint;
@synthesize m_screenScale;
@synthesize m_screenIdx;
@synthesize m_screensize;
@synthesize m_nowPlayingInfo;
@synthesize m_directionOverride;
@synthesize m_direction;
@synthesize m_currentKey;
@synthesize m_clickResetPan;
@synthesize m_mimicAppleSiri;
@synthesize m_remoteIdleState;
@synthesize m_remoteIdleTimeout;
@synthesize m_shouldRemoteIdle;
@synthesize m_RemoteOSDSwipes;

#pragma mark - internal key press methods
//--------------------------------------------------------------
//--------------------------------------------------------------
- (void)sendKeyDownUp:(XBMCKey)key
{
  XBMC_Event newEvent = {0};
  newEvent.key.keysym.sym = key;

  newEvent.type = XBMC_KEYDOWN;
  CWinEvents::MessagePush(&newEvent);

  newEvent.type = XBMC_KEYUP;
  CWinEvents::MessagePush(&newEvent);
}
- (void)sendKeyDown:(XBMCKey)key
{
  XBMC_Event newEvent = {0};
  newEvent.type = XBMC_KEYDOWN;
  newEvent.key.keysym.sym = key;
  CWinEvents::MessagePush(&newEvent);
}
- (void)sendKeyUp:(XBMCKey)key
{
  XBMC_Event newEvent = {0};
  newEvent.type = XBMC_KEYUP;
  newEvent.key.keysym.sym = key;
  CWinEvents::MessagePush(&newEvent);
}

#pragma mark - remote idle timer
//--------------------------------------------------------------

- (void)startRemoteTimer
{
  m_remoteIdleState = false;

  //PRINT_SIGNATURE();
  if (self.remoteIdleTimer != nil)
    [self stopRemoteTimer];
  if (m_shouldRemoteIdle)
  {
    NSDate *fireDate = [NSDate dateWithTimeIntervalSinceNow:m_remoteIdleTimeout];
    NSTimer *timer = [[NSTimer alloc] initWithFireDate:fireDate
                                      interval:0.0
                                      target:self
                                      selector:@selector(setRemoteIdleState)
                                      userInfo:nil
                                      repeats:NO];
    
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
    self.remoteIdleTimer = timer;
  }
}

- (void)stopRemoteTimer
{
  //PRINT_SIGNATURE();
  if (self.remoteIdleTimer != nil)
  {
    [self.remoteIdleTimer invalidate];
    self.remoteIdleTimer = nil;
  }
  m_remoteIdleState = false;
}

- (void)setRemoteIdleState
{
  //PRINT_SIGNATURE();
  m_remoteIdleState = true;
}

#pragma mark - key press auto-repeat methods
//--------------------------------------------------------------
//--------------------------------------------------------------
// start repeating after 0.25s
#define REPEATED_KEYPRESS_DELAY_S 0.50
// pause 0.05s (50ms) between keypresses
#define REPEATED_KEYPRESS_PAUSE_S 0.05
//--------------------------------------------------------------

- (void)startKeyPressTimer:(XBMCKey)keyId
{
  [self startKeyPressTimer:keyId clickTime:REPEATED_KEYPRESS_PAUSE_S];
}

- (void)startKeyPressTimer:(XBMCKey)keyId clickTime:(NSTimeInterval)interval
{
  //PRINT_SIGNATURE();
  if (self.pressAutoRepeatTimer != nil)
    [self stopKeyPressTimer];

  [self sendKeyDown:keyId];

  NSNumber *number = [NSNumber numberWithInt:keyId];
  NSDate *fireDate = [NSDate dateWithTimeIntervalSinceNow:REPEATED_KEYPRESS_DELAY_S];

  // schedule repeated timer which starts after REPEATED_KEYPRESS_DELAY_S
  // and fires every REPEATED_KEYPRESS_PAUSE_S
  NSTimer *timer = [[NSTimer alloc] initWithFireDate:fireDate
    interval:interval
    target:self
    selector:@selector(keyPressTimerCallback:)
    userInfo:number
    repeats:YES];

  // schedule the timer to the runloop
  [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
  self.pressAutoRepeatTimer = timer;
}
- (void)stopKeyPressTimer
{
  //PRINT_SIGNATURE();
  if (self.pressAutoRepeatTimer != nil)
  {
    [self.pressAutoRepeatTimer invalidate];
    self.pressAutoRepeatTimer = nil;
  }
}
- (void)keyPressTimerCallback:(NSTimer*)theTimer
{
  //PRINT_SIGNATURE();
  // if queue is empty - skip this timer event before letting it process
  if (CWinEvents::GetQueueSize())
    return;

  NSNumber *keyId = [theTimer userInfo];
  [self sendKeyDown:(XBMCKey)[keyId intValue]];
}

#pragma mark - remote helpers

//--------------------------------------------------------------
- (XBMCKey)getPanDirectionKey:(CGPoint)translation
{
  int x = (int)translation.x;
  int y = (int)translation.y;
  int absX = x;
  int absY = y;
  
  if (absX < 0)
    absX *= -1;
  
  if (absY < 0)
    absY *= -1;
  
  bool horizontal, veritical;
  horizontal = ( absX > absY ) ;
  veritical = !horizontal;
  
  // Determine up, down, right, or left:
  bool swipe_up, swipe_down, swipe_left, swipe_right;
  swipe_left = (horizontal && x < 0);
  swipe_right = (horizontal && x >= 0);
  swipe_up = (veritical && y < 0);
  swipe_down = (veritical && y >= 0);
  
  if (swipe_down)
    return XBMCK_DOWN;
  if (swipe_up)
    return XBMCK_UP;
  if (swipe_left)
    return XBMCK_LEFT;
  if (swipe_right)
    return XBMCK_RIGHT;
  
  return XBMCK_UNKNOWN;
}

//--------------------------------------------------------------
- (UIPanGestureRecognizerDirection)getPanDirection:(CGPoint)translation
{
  int x = (int)translation.x;
  int y = (int)translation.y;
  int absX = x;
  int absY = y;
  
  if (absX < 0)
    absX *= -1;
  
  if (absY < 0)
    absY *= -1;
  
  bool horizontal, veritical;
  horizontal = ( absX > absY ) ;
  veritical = !horizontal;
  
  // Determine up, down, right, or left:
  bool swipe_up, swipe_down, swipe_left, swipe_right;
  swipe_left = (horizontal && x < 0);
  swipe_right = (horizontal && x >= 0);
  swipe_up = (veritical && y < 0);
  swipe_down = (veritical && y >= 0);
  
  if (swipe_down)
    return UIPanGestureRecognizerDirectionDown;
  if (swipe_up)
    return UIPanGestureRecognizerDirectionUp;
  if (swipe_left)
    return UIPanGestureRecognizerDirectionLeft;
  if (swipe_right)
    return UIPanGestureRecognizerDirectionRight;
  
  return UIPanGestureRecognizerDirectionUndefined;
  
}

//--------------------------------------------------------------
-(BOOL) shouldFastScroll
{
  // we dont want fast scroll in below windows, no point in going 15 places in home screen
  int window = g_windowManager.GetActiveWindow();
  
  if (window == WINDOW_HOME ||
      window == WINDOW_FULLSCREEN_LIVETV ||
      window == WINDOW_FULLSCREEN_VIDEO ||
      window == WINDOW_FULLSCREEN_RADIO ||
      (window >= WINDOW_SETTINGS_START && window <= WINDOW_SETTINGS_APPEARANCE)
      )
    return NO;
  
  return YES;
}

//--------------------------------------------------------------
-(BOOL) shouldOSDScroll
{
  // we might not want to scroll in below windows, fullscreen video playback is one
  int window = g_windowManager.GetActiveWindow();
  
  if (m_RemoteOSDSwipes &&
      (window == WINDOW_FULLSCREEN_LIVETV ||
      window == WINDOW_FULLSCREEN_VIDEO ||
      window == WINDOW_FULLSCREEN_RADIO
      ))
    return NO;
  
  return YES;
}

//--------------------------------------------------------------
- (void)setSiriRemote:(BOOL)enable
{
  m_mimicAppleSiri = enable;
}

//--------------------------------------------------------------
- (void)setRemoteIdleTimeout:(int)timeout
{
  m_remoteIdleTimeout = (float)timeout;
  [self startRemoteTimer];
}

- (void)setShouldRemoteIdle:(BOOL)idle
{
  //PRINT_SIGNATURE();
  m_shouldRemoteIdle = idle;
  [self startRemoteTimer];
}

- (void)setSiriRemoteOSDSwipes:(BOOL)enable
{
  //PRINT_SIGNATURE();
  m_RemoteOSDSwipes = enable;
}


//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - gesture methods
//--------------------------------------------------------------
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
  //PRINT_SIGNATURE();
  return YES;
}

//--------------------------------------------------------------
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{  
  if ([gestureRecognizer isKindOfClass:[UISwipeGestureRecognizer class]] && [otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]]) {
    return YES;
  }
  if ([gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]] && [otherGestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]]) {
    return YES;
  }
  if ([gestureRecognizer isKindOfClass:[UITapGestureRecognizer class]] && [otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]]) {
    return YES;
  }
  return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
  if (!m_mimicAppleSiri && [gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]] && ([otherGestureRecognizer isKindOfClass:[UISwipeGestureRecognizer class]] || [otherGestureRecognizer isKindOfClass:[UITapGestureRecognizer class]]))
  {
    return YES;
  }
  return NO;
}

//--------------------------------------------------------------
// called before pressesBegan:withEvent: is called on the gesture recognizer
// for a new press. return NO to prevent the gesture recognizer from seeing this press
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceivePress:(UIPress *)press
{
  //PRINT_SIGNATURE();
  BOOL handled = YES;
  switch (press.type)
  {
    // single press key, but also detect hold and back to tvos.
    case UIPressTypeMenu:
    {
      // menu is special.
      //  a) if at our home view, should return to atv home screen.
      //  b) if not, let it pass to us.
      int focusedWindowID = g_windowManager.GetFocusedWindow();
      if (focusedWindowID == WINDOW_HOME)
        handled = NO;
      break;
    }

    // single press keys
    case UIPressTypeSelect:
    case UIPressTypePlayPause:
      break;

    // auto-repeat keys
    case UIPressTypeUpArrow:
    case UIPressTypeDownArrow:
    case UIPressTypeLeftArrow:
    case UIPressTypeRightArrow:
      break;

    default:
      handled = NO;
  }

  return handled;
}

//--------------------------------------------------------------
- (void)createSwipeGestureRecognizers
{
  UISwipeGestureRecognizer *swipeLeft = [[UISwipeGestureRecognizer alloc]
                                         initWithTarget:self action:@selector(handleSwipe:)];

  swipeLeft.delaysTouchesBegan = NO;
  swipeLeft.direction = UISwipeGestureRecognizerDirectionLeft;
  swipeLeft.delegate = self;
  [m_glView addGestureRecognizer:swipeLeft];

  //single finger swipe right
  UISwipeGestureRecognizer *swipeRight = [[UISwipeGestureRecognizer alloc]
                                          initWithTarget:self action:@selector(handleSwipe:)];

  swipeRight.delaysTouchesBegan = NO;
  swipeRight.direction = UISwipeGestureRecognizerDirectionRight;
  swipeRight.delegate = self;
  [m_glView addGestureRecognizer:swipeRight];

  //single finger swipe up
  UISwipeGestureRecognizer *swipeUp = [[UISwipeGestureRecognizer alloc]
                                       initWithTarget:self action:@selector(handleSwipe:)];

  swipeUp.delaysTouchesBegan = NO;
  swipeUp.direction = UISwipeGestureRecognizerDirectionUp;
  swipeUp.delegate = self;
  [m_glView addGestureRecognizer:swipeUp];

  //single finger swipe down
  UISwipeGestureRecognizer *swipeDown = [[UISwipeGestureRecognizer alloc]
                                         initWithTarget:self action:@selector(handleSwipe:)];

  swipeDown.delaysTouchesBegan = NO;
  swipeDown.direction = UISwipeGestureRecognizerDirectionDown;
  swipeDown.delegate = self;
  [m_glView addGestureRecognizer:swipeDown];
}
  
//--------------------------------------------------------------
- (void)createPanGestureRecognizers
{
  //PRINT_SIGNATURE();
  // for pan gestures with one finger
  auto pan = [[UIPanGestureRecognizer alloc]
    initWithTarget:self action:@selector(handlePan:)];
  pan.delegate = self;
  [m_glView addGestureRecognizer:pan];
  m_clickResetPan = false;
}
//--------------------------------------------------------------
- (void)createTapGesturecognizers
{
  //PRINT_SIGNATURE();
  // tap side of siri remote pad
  auto upRecognizer = [[UITapGestureRecognizer alloc]
                       initWithTarget: self action: @selector(tapUpArrowPressed:)];
  upRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeUpArrow]];
  upRecognizer.delegate = self;
  [m_glView addGestureRecognizer: upRecognizer];
  
  auto downRecognizer = [[UITapGestureRecognizer alloc]
                         initWithTarget: self action: @selector(tapDownArrowPressed:)];
  downRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeDownArrow]];
  downRecognizer.delegate = self;
  [m_glView addGestureRecognizer: downRecognizer];
  
  auto leftRecognizer = [[UITapGestureRecognizer alloc]
                         initWithTarget: self action: @selector(tapLeftArrowPressed:)];
  leftRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeLeftArrow]];
  leftRecognizer.delegate = self;
  [m_glView addGestureRecognizer: leftRecognizer];
  
  auto rightRecognizer = [[UITapGestureRecognizer alloc]
                          initWithTarget: self action: @selector(tapRightArrowPressed:)];
  rightRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeRightArrow]];
  rightRecognizer.delegate = self;
  [m_glView addGestureRecognizer: rightRecognizer];
}
//--------------------------------------------------------------
- (void)createPressGesturecognizers
{
  //PRINT_SIGNATURE();
  // we need UILongPressGestureRecognizer here because it will give
  // UIGestureRecognizerStateBegan AND UIGestureRecognizerStateEnded
  // even if we hold down for a long time. UITapGestureRecognizer
  // will eat the ending on long holds and we never see it.
  auto upRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteUpArrowPressed:)];
  upRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeUpArrow]];
  upRecognizer.minimumPressDuration = 0.01;
  upRecognizer.delegate = self;
  [self.view addGestureRecognizer: upRecognizer];

  auto downRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteDownArrowPressed:)];
  downRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeDownArrow]];
  downRecognizer.minimumPressDuration = 0.01;
  downRecognizer.delegate = self;
  [self.view addGestureRecognizer: downRecognizer];

  auto leftRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteLeftArrowPressed:)];
  leftRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeLeftArrow]];
  leftRecognizer.minimumPressDuration = 0.01;
  leftRecognizer.delegate = self;
  [self.view addGestureRecognizer: leftRecognizer];

  auto rightRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteRightArrowPressed:)];
  rightRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeRightArrow]];
  rightRecognizer.minimumPressDuration = 0.01;
  rightRecognizer.delegate = self;
  [self.view addGestureRecognizer: rightRecognizer];
  
  // we always have these under tvos
  auto menuRecognizer = [[UITapGestureRecognizer alloc]
                         initWithTarget: self action: @selector(menuPressed:)];
  menuRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeMenu]];
  menuRecognizer.delegate  = self;
  [m_glView addGestureRecognizer: menuRecognizer];
  
  auto playPauseRecognizer = [[UITapGestureRecognizer alloc]
                              initWithTarget: self action: @selector(playPausePressed:)];
  playPauseRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypePlayPause]];
  playPauseRecognizer.delegate  = self;
  [m_glView addGestureRecognizer: playPauseRecognizer];
  
  auto selectRecognizer = [[UILongPressGestureRecognizer alloc]
                          initWithTarget: self action: @selector(selectPressed:)];
  selectRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeSelect]];
  selectRecognizer.minimumPressDuration = 0.001;
  selectRecognizer.delegate = self;
  [self.view addGestureRecognizer: selectRecognizer];
  
}

//--------------------------------------------------------------
- (void)buttonHoldSelect
{
  self.m_holdCounter++;
  [self.m_holdTimer invalidate];
  [self sendKeyDownUp:XBMCK_c];
}
//--------------------------------------------------------------
- (void) activateKeyboard:(UIView *)view
{
  //PRINT_SIGNATURE();
  [self.view addSubview:view];
  m_glView.userInteractionEnabled = NO;
}
//--------------------------------------------------------------
- (void) deactivateKeyboard:(UIView *)view
{
  //PRINT_SIGNATURE();
  [view removeFromSuperview];
  m_glView.userInteractionEnabled = YES; 
  [self becomeFirstResponder];
}
//--------------------------------------------------------------
- (void)menuPressed:(UITapGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      break;
    case UIGestureRecognizerStateChanged:
      break;
    case UIGestureRecognizerStateEnded:
      if (g_windowManager.GetFocusedWindow() == WINDOW_FULLSCREEN_VIDEO)
      {
        if (CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIBACK))
          CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_STOP);
        else
          [self sendKeyDownUp:XBMCK_ESCAPE];
      }
      else
        [self sendKeyDownUp:XBMCK_BACKSPACE];
      
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}
//--------------------------------------------------------------
- (void)selectPressed:(UILongPressGestureRecognizer *)sender
{
  // if we have clicked select while scrolling up/down we need to reset direction of pan
  m_clickResetPan = true;
  
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      self.m_holdCounter = 0;
      self.m_holdTimer = [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(buttonHoldSelect) userInfo:nil repeats:YES];
      break;
    case UIGestureRecognizerStateChanged:
      if (self.m_holdCounter > 1)
      {
        [self.m_holdTimer invalidate];
        [self sendKeyDownUp:XBMCK_c];
      }
      break;
    case UIGestureRecognizerStateEnded:
      [self.m_holdTimer invalidate];
      if (self.m_holdCounter < 1)
        [self sendKeyDownUp:XBMCK_RETURN];
      
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}

- (void)playPausePressed:(UITapGestureRecognizer *) sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      break;
    case UIGestureRecognizerStateChanged:
      break;
    case UIGestureRecognizerStateEnded:
      [self sendKeyDownUp:XBMCK_MEDIA_PLAY_PAUSE];
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}

//--------------------------------------------------------------
- (IBAction)IRRemoteUpArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      [self startKeyPressTimer:XBMCK_UP];
      break;
    case UIGestureRecognizerStateChanged:
      break;
    case UIGestureRecognizerStateEnded:
      [self stopKeyPressTimer];
      [self sendKeyUp:XBMCK_UP];
      
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}
//--------------------------------------------------------------
- (IBAction)IRRemoteDownArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      [self startKeyPressTimer:XBMCK_DOWN];
      break;
    case UIGestureRecognizerStateChanged:
      break;
    case UIGestureRecognizerStateEnded:
      [self stopKeyPressTimer];
      [self sendKeyUp:XBMCK_DOWN];
      
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}
//--------------------------------------------------------------
- (IBAction)IRRemoteLeftArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      [self startKeyPressTimer:XBMCK_LEFT];
      break;
    case UIGestureRecognizerStateChanged:
      break;
    case UIGestureRecognizerStateEnded:
      [self stopKeyPressTimer];
      [self sendKeyUp:XBMCK_LEFT];
      
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}
//--------------------------------------------------------------
- (IBAction)IRRemoteRightArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      [self startKeyPressTimer:XBMCK_RIGHT];
      break;
    case UIGestureRecognizerStateChanged:
      break;
    case UIGestureRecognizerStateEnded:
      [self stopKeyPressTimer];
      [self sendKeyUp:XBMCK_RIGHT];
      
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}

//--------------------------------------------------------------
- (IBAction)tapUpArrowPressed:(UIGestureRecognizer *)sender
{
  if (!m_remoteIdleState)
    [self sendKeyDownUp:XBMCK_UP];
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (IBAction)tapDownArrowPressed:(UIGestureRecognizer *)sender
{
  if (!m_remoteIdleState)
    [self sendKeyDownUp:XBMCK_DOWN];
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (IBAction)tapLeftArrowPressed:(UIGestureRecognizer *)sender
{
  if (!m_remoteIdleState)
    [self sendKeyDownUp:XBMCK_LEFT];
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (IBAction)tapRightArrowPressed:(UIGestureRecognizer *)sender
{
  if (!m_remoteIdleState)
    [self sendKeyDownUp:XBMCK_RIGHT];
  [self startRemoteTimer];
}

//--------------------------------------------------------------
- (IBAction)handlePan:(UIPanGestureRecognizer *)sender 
{
  if (!m_remoteIdleState)
  {
    if (m_appAlive == YES) //NO GESTURES BEFORE WE ARE UP AND RUNNING
    {
      if (m_mimicAppleSiri)
      {
        if ([self shouldOSDScroll])
        {
          static UIPanGestureRecognizerDirection direction = UIPanGestureRecognizerDirectionUndefined;
          // speed       == how many clicks full swipe will give us(1000x1000px)
          // minVelocity == min velocity to trigger fast scroll, add this to settings?
          float speed = 240.0;
          float minVelocity = 1300.0;
          switch (sender.state) {
              
            case UIGestureRecognizerStateBegan: {
              
              if (direction == UIPanGestureRecognizerDirectionUndefined)
              {
                m_lastGesturePoint = [sender translationInView:sender.view];
                m_lastGesturePoint.x = m_lastGesturePoint.x/1.92;
                m_lastGesturePoint.y = m_lastGesturePoint.y/1.08;
                
                m_direction = [self getPanDirection:m_lastGesturePoint];
                m_directionOverride = false;
              }
              
              break;
            }
              
            case UIGestureRecognizerStateChanged:
            {
              CGPoint gesturePoint = [sender translationInView:sender.view];
              gesturePoint.x = gesturePoint.x/1.92;
              gesturePoint.y = gesturePoint.y/1.08;
              
              CGPoint gestureMovement;
              gestureMovement.x = gesturePoint.x - m_lastGesturePoint.x;
              gestureMovement.y = gesturePoint.y - m_lastGesturePoint.y;
              direction = [self getPanDirection:gestureMovement];
              
              CGPoint velocity = [sender velocityInView:sender.view];
              CGFloat velocityX = (0.2*velocity.x);
              CGFloat velocityY = (0.2*velocity.y);
              
              if (ABS(velocityY) > minVelocity || ABS(velocityX) > minVelocity || m_directionOverride)
              {
                direction = m_direction;
                // Override direction to correct swipe errors
                m_directionOverride = true;
              }
              
              switch (direction)
              {
                case UIPanGestureRecognizerDirectionUp:
                {
                  if ((ABS(m_lastGesturePoint.y - gesturePoint.y) > speed) || ABS(velocityY) > minVelocity )
                  {
                    [self sendKeyDownUp:XBMCK_UP];
                    if (ABS(velocityY) > minVelocity && [self shouldFastScroll])
                      [self sendKeyDownUp:XBMCK_UP];
                    m_lastGesturePoint = gesturePoint;
                  }
                  break;
                }
                case UIPanGestureRecognizerDirectionDown:
                {
                  if ((ABS(m_lastGesturePoint.y - gesturePoint.y) > speed) || ABS(velocityY) > minVelocity)
                  {
                    [self sendKeyDownUp:XBMCK_DOWN];
                    if (ABS(velocityY) > minVelocity && [self shouldFastScroll])
                      [self sendKeyDownUp:XBMCK_DOWN];
                    m_lastGesturePoint = gesturePoint;
                  }
                  break;
                }
                case UIPanGestureRecognizerDirectionLeft:
                {
                  // add 80 px to slow left/right swipes, it matched up down better
                  if ((ABS(m_lastGesturePoint.x - gesturePoint.x) > speed+80) || ABS(velocityX) > minVelocity)
                  {
                    [self sendKeyDownUp:XBMCK_LEFT];
                    if (ABS(velocityX) > minVelocity && [self shouldFastScroll])
                      [self sendKeyDownUp:XBMCK_LEFT];
                    m_lastGesturePoint = gesturePoint;
                  }
                  break;
                }
                case UIPanGestureRecognizerDirectionRight:
                {
                  // add 80 px to slow left/right swipes, it matched up down better
                  if ((ABS(m_lastGesturePoint.x - gesturePoint.x) > speed+80) || ABS(velocityX) > minVelocity)
                  {
                    [self sendKeyDownUp:XBMCK_RIGHT];
                    if (ABS(velocityX) > minVelocity && [self shouldFastScroll])
                      [self sendKeyDownUp:XBMCK_RIGHT];
                    m_lastGesturePoint = gesturePoint;
                  }
                  break;
                }
                default:
                {
                  break;
                }
              }
            }
              
            case UIGestureRecognizerStateEnded: {
              direction = UIPanGestureRecognizerDirectionUndefined;
              // start remote idle timer
              [self startRemoteTimer];
              break;
            }
              
            default:
              break;
          }
        }
      }
      else // dont mimic apple siri remote
      {
        XBMCKey key;
        switch (sender.state)
        {
          case UIGestureRecognizerStateBegan:
          {
            m_currentClick = -1;
            m_currentKey = XBMCK_UNKNOWN;
            break;
          }
          case UIGestureRecognizerStateChanged:
          {
            CGPoint gesturePoint = [sender translationInView:m_glView];
            gesturePoint.x = gesturePoint.x/1.92;
            gesturePoint.y = gesturePoint.y/1.08;
            
            key = [self getPanDirectionKey:gesturePoint];
            
            // ignore UP/DOWN swipes while in full screen playback
            if (g_windowManager.GetFocusedWindow() != WINDOW_FULLSCREEN_VIDEO ||
                key == XBMCK_LEFT ||
                key == XBMCK_RIGHT)
            {
              int click;
              int absX = gesturePoint.x;
              int absY = gesturePoint.y;
              
              if (absX < 0)
                absX *= -1;
              
              if (absY < 0)
                absY *= -1;
              
              if (key == XBMCK_RIGHT || key == XBMCK_LEFT)
              {
                if (absX > 200)
                  click = 2;
                else if (absX > 70)
                  click = 1;
                else
                  click = 0;
              }
              else
              {
                if (absY > 200)
                  click = 2;
                else if (absY > 100)
                  click = 1;
                else
                  click = 0;
              }
              
              if (m_clickResetPan || m_currentKey != key || click != m_currentClick)
              {
                [self stopKeyPressTimer];
                [self sendKeyUp:m_currentKey];
                
                if (click != m_currentClick)
                {
                  m_currentClick = click;
                }
                if (m_currentKey == XBMCK_UNKNOWN || m_clickResetPan ||
                    ((m_currentKey == XBMCK_RIGHT && key == XBMCK_LEFT) ||
                     (m_currentKey == XBMCK_LEFT && key == XBMCK_RIGHT) ||
                     (m_currentKey == XBMCK_UP && key == XBMCK_DOWN) ||
                     (m_currentKey == XBMCK_DOWN && key == XBMCK_UP))
                    )
                {
                  m_clickResetPan = false;
                  m_currentKey = key;
                }
                
                if (m_currentClick == 2)
                {
                  //fast click
                  [self startKeyPressTimer:m_currentKey clickTime:0.20];
                  LOG("fast click");
                }
                else if (m_currentClick == 1)
                {
                  // slow click
                  [self startKeyPressTimer:m_currentKey clickTime:0.80];
                  LOG("slow click");
                }
              }
            }
            break;
          }
          case UIGestureRecognizerStateEnded:
          case UIGestureRecognizerStateCancelled:
            [self stopKeyPressTimer];
            // start remote idle timer
            [self startRemoteTimer];
            break;
          default:
            break;
        }
      }
    }
  }
}

//--------------------------------------------------------------
- (IBAction)handleSwipe:(UISwipeGestureRecognizer *)sender
{
  if (!m_remoteIdleState)
  {
    if(!m_mimicAppleSiri && m_appAlive == YES)//NO GESTURES BEFORE WE ARE UP AND RUNNING
    {
      switch ([sender direction])
      {
        case UISwipeGestureRecognizerDirectionRight:
          [self sendKeyDownUp:XBMCK_RIGHT];
          break;
        case UISwipeGestureRecognizerDirectionLeft:
          [self sendKeyDownUp:XBMCK_LEFT];
          break;
        case UISwipeGestureRecognizerDirectionUp:
          [self sendKeyDownUp:XBMCK_UP];
          break;
        case UISwipeGestureRecognizerDirectionDown:
          [self sendKeyDownUp:XBMCK_DOWN];
          break;
      }
    }
  }
  // start remote idle timer
  [self startRemoteTimer];
}

#pragma mark -
- (void) insertVideoView:(UIView*)view
{
  [self.view insertSubview:view belowSubview:m_glView];
  [self.view setNeedsDisplay];
}

- (void) removeVideoView:(UIView*)view
{
  [view removeFromSuperview];
}

- (id)initWithFrame:(CGRect)frame withScreen:(UIScreen *)screen
{ 
  m_screenIdx = 0;
  self = [super init];
  if (!self)
    return nil;

  m_pause = FALSE;
  m_appAlive = FALSE;
  m_animating = FALSE;
  m_controllerState = MC_NONE;

  m_bgTask = UIBackgroundTaskInvalid;

  m_window = [[UIWindow alloc] initWithFrame:frame];
  [m_window setRootViewController:self];  
  m_window.screen = screen;
  m_window.backgroundColor = [UIColor blackColor];
  // Turn off autoresizing
  m_window.autoresizingMask = 0;
  m_window.autoresizesSubviews = NO;

  [self enableScreenSaver];

  [m_window makeKeyAndVisible];
  g_xbmcController = self;  

  CAnnounceReceiver::GetInstance().Initialize();
  //NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  //[center addObserver: self
  //   selector: @selector(observeDefaultCenterStuff:) name: nil object: nil];

  return self;
}
//--------------------------------------------------------------
- (void)dealloc
{
  // stop background task (if running)
  [self disableBackGroundTask];

  CAnnounceReceiver::GetInstance().DeInitialize();

  [self stopAnimation];
}
//--------------------------------------------------------------
- (void)loadView
{
  [super loadView];

  self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.view.autoresizesSubviews = YES;
  
  m_glView = [[MainEAGLView alloc] initWithFrame:self.view.bounds withScreen:[UIScreen mainScreen]];

  // Check if screen is Retina
  // m_screenScale = [[UIScreen mainScreen] nativeScale];
  m_screenScale = 1.0;
  [self.view addSubview: m_glView];
}
//--------------------------------------------------------------
- (void)viewDidLoad
{
  [super viewDidLoad];
  
  [self createSwipeGestureRecognizers];
  [self createPanGestureRecognizers];
  [self createPressGesturecognizers];
  [self createTapGesturecognizers];
  [self createCustomControlCenter];
  
}
//--------------------------------------------------------------
- (void)viewWillAppear:(BOOL)animated
{
  [self resumeAnimation];
  [super viewWillAppear:animated];
}
//--------------------------------------------------------------
- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];
  [self becomeFirstResponder];
}
//--------------------------------------------------------------
- (void)viewWillDisappear:(BOOL)animated
{  
  [self pauseAnimation];
  [super viewWillDisappear:animated];
}
//--------------------------------------------------------------
- (void)viewDidUnload
{
  [[UIApplication sharedApplication] endReceivingRemoteControlEvents];
  [self resignFirstResponder];
  [super viewDidUnload];
}
//--------------------------------------------------------------
- (UIView *)inputView
{
  // override our input view to an empty view
  // this prevents the on screen keyboard
  // which would be shown whenever this UIResponder
  // becomes the first responder (which is always the case!)
  // caused by implementing the UIKeyInput protocol
  return [[UIView alloc] initWithFrame:CGRectZero];
}
//--------------------------------------------------------------
- (BOOL)canBecomeFirstResponder
{
  return YES;
}
//--------------------------------------------------------------
- (void)setFramebuffer
{
  if (!m_pause)
    [m_glView setFramebuffer];
}
//--------------------------------------------------------------
- (bool)presentFramebuffer
{
  if (!m_pause)
    return [m_glView presentFramebuffer];
  else
    return FALSE;
}
//--------------------------------------------------------------
- (CGSize)getScreenSize
{
  m_screensize.width  = m_glView.bounds.size.width  * m_screenScale;
  m_screensize.height = m_glView.bounds.size.height * m_screenScale;
  return m_screensize;
}

//--------------------------------------------------------------
- (void)didReceiveMemoryWarning
{
  PRINT_SIGNATURE();
  // Releases the view if it doesn't have a superview.
  [super didReceiveMemoryWarning];
  // Release any cached data, images, etc. that aren't in use.
}
//--------------------------------------------------------------
- (void)enableBackGroundTask
{
  PRINT_SIGNATURE();
  if (m_bgTask != UIBackgroundTaskInvalid)
  {
    [[UIApplication sharedApplication] endBackgroundTask: m_bgTask];
    m_bgTask = UIBackgroundTaskInvalid;
  }
  LOG(@"%s: beginBackgroundTask", __PRETTY_FUNCTION__);
  // we have to alloc the background task for keep network working after screen lock and dark.
  m_bgTask = [[UIApplication sharedApplication] beginBackgroundTaskWithExpirationHandler:^{
    [self disableBackGroundTask];
  }];
}
//--------------------------------------------------------------
- (void)disableBackGroundTask
{
  PRINT_SIGNATURE();
  if (m_bgTask != UIBackgroundTaskInvalid)
  {
    LOG(@"%s: endBackgroundTask", __PRETTY_FUNCTION__);
    [[UIApplication sharedApplication] endBackgroundTask: m_bgTask];
    m_bgTask = UIBackgroundTaskInvalid;
  }
}
//--------------------------------------------------------------
- (void)disableSystemSleep
{
}
//--------------------------------------------------------------
- (void)enableSystemSleep
{
}
//--------------------------------------------------------------
- (void)disableScreenSaver
{
  m_disableIdleTimer = YES;
  [[UIApplication sharedApplication] setIdleTimerDisabled:YES];
  [self resetSystemIdleTimer];
}
//--------------------------------------------------------------
- (void)enableScreenSaver
{
  m_disableIdleTimer = NO;
  [[UIApplication sharedApplication] setIdleTimerDisabled:NO];
}

//--------------------------------------------------------------
- (bool)resetSystemIdleTimer
{
  // this is silly :)
  // when system screen saver kicks off, we switch to UIApplicationStateInactive, the only way
  // to get out of the screensaver is to call ourself to open an custom URL that is registered
  // in our Info.plist. The openURL method of UIApplication must be supported but we can just
  // reply NO and we get restored to UIApplicationStateActive.
  bool inActive = [UIApplication sharedApplication].applicationState == UIApplicationStateInactive;
  if (inActive)
  {
    NSURL *url = [NSURL URLWithString:@"mrmc://wakeup"];
    [[UIApplication sharedApplication] openURL:url];
  }

  return inActive;
}

//--------------------------------------------------------------
- (UIScreenMode*) preferredScreenMode:(UIScreen*) screen
{
  // tvOS only support one mode, the current one.
  return [screen currentMode];
}

//--------------------------------------------------------------
- (NSArray<UIScreenMode *> *) availableScreenModes:(UIScreen*) screen
{
  // tvOS only support one mode, the current one,
  // pass back an array with this inside.
  NSMutableArray *array = [[NSMutableArray alloc] initWithCapacity:1];
  [array addObject:[screen currentMode]];
  return array;
}

//--------------------------------------------------------------
- (bool)changeScreen:(unsigned int)screenIdx withMode:(UIScreenMode *)mode
{
  return true;
}
//--------------------------------------------------------------
- (void)enterForeground
{
  PRINT_SIGNATURE();
}
//--------------------------------------------------------------
- (void)enterActiveDelayed:(id)arg
{
  // the only way to tell if we are really going active is to delay for
  // two seconds, then test. Otherwise we might be doing a forced sleep
  // and that will blip us active for a short time before going inactive/background.
  if ([UIApplication sharedApplication].applicationState != UIApplicationStateActive)
    return;

  PRINT_SIGNATURE();

  // MCRuntimeLib_Initialized is only true if
  // we were running and got moved to background
  while(!MCRuntimeLib_Initialized())
    usleep(50*1000);

  g_Windowing.OnAppFocusChange(true);

  // this will fire only if we are already alive and have 'menu'ed out and back
  ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::System, "xbmc", "OnWake");

  // restart ZeroConfig (if stopped)
  CNetworkServices::GetInstance().StartZeroconf();
  CNetworkServices::GetInstance().StartPlexServices();
  CNetworkServices::GetInstance().StartLightEffectServices();

  // wait for AE to wake
  XbmcThreads::EndTime timer(2000);
  while (CAEFactory::IsSuspended() && !timer.IsTimePast())
    usleep(250*1000);

  // handles a push into foreground by a topshelf item select/play
  // returns false if there is no topshelf item
  if (!CTVOSTopShelf::GetInstance().RunTopShelf())
  {
    // no topshelf item, check others
    switch(m_controllerState)
    {
      default:
      case MC_INACTIVE:
        CAEFactory::DeviceChange();
        break;
      case MC_INACTIVE_WASPAUSED:
        CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_UNPAUSE);
        break;
      case MC_BACKGROUND_RESTORE:
        if (!g_application.LastProgressTrackingItem().GetPath().empty())
        {
          CFileItem *fileitem = new CFileItem(g_application.LastProgressTrackingItem());
          if (!fileitem->IsLiveTV())
          {
            // m_lStartOffset always gets multiplied by 75, magic numbers :)
            fileitem->m_lStartOffset = m_wasPlayingTime * 75;
          }
          CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_PLAY, 0, 0, static_cast<void*>(fileitem));
        }
        break;
    }
  }
  m_controllerState = MC_ACTIVE;

}
- (void)becomeActive
{
  PRINT_SIGNATURE();
  // stop background task (if running)
  [self disableBackGroundTask];

  SEL singleParamSelector = @selector(enterActiveDelayed:);
  [g_xbmcController performSelector:singleParamSelector withObject:nil afterDelay:2.0];
}
//--------------------------------------------------------------
- (void)becomeInactive
{
  PRINT_SIGNATURE();

  m_controllerState = MC_INACTIVE;
  if (g_application.m_pPlayer->IsPlayingVideo())
  {
    m_controllerState = MC_INACTIVE_ISPAUSED;
    // we might or might not be paused.
    if (!g_application.m_pPlayer->IsPaused())
    {
      m_controllerState = MC_INACTIVE_WASPAUSED;
      CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_PAUSE_IF_PLAYING);
    }
  }
}


//--------------------------------------------------------------
- (void)enterBackgroundDetached:(id)arg
{
  PRINT_SIGNATURE();
  switch(m_controllerState)
  {
    default:
    case MC_INACTIVE:
      m_controllerState = MC_BACKGROUND;
      break;
    case MC_INACTIVE_ISPAUSED:
    case MC_INACTIVE_WASPAUSED:
      {
        m_controllerState = MC_BACKGROUND_RESTORE;
        // get the current playing time but backup a little, it seems better
        m_wasPlayingTime = g_application.GetTime() - 1.50;
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_STOP)));
        // wait until we stop playing.
        while(g_application.m_pPlayer->IsPlaying())
          usleep(50*1000);
      }
      break;
  }

  g_Windowing.OnAppFocusChange(false);

  // Apple says to disable ZeroConfig when moving to background
  CNetworkServices::GetInstance().StopZeroconf();
  CNetworkServices::GetInstance().StopPlexServices();
  CNetworkServices::GetInstance().StopLightEffectServices();

  if (m_controllerState != MC_BACKGROUND)
  {
    // if we are not playing/pause when going to background
    // close out network shares as we can get fully suspended.
    g_application.CloseNetworkShares();
    [self disableBackGroundTask];
  }

  // OnAppFocusChange triggers an AE suspend.
  // Wait for AE to suspend and delete the audio sink, this allows
  // AudioOutputUnitStop to complete.
  // Note that to user, we moved into background to user but we
  // are really waiting here for AE to suspend.
  while (!CAEFactory::IsSuspended())
    usleep(250*1000);
}

- (void)enterBackground
{
  PRINT_SIGNATURE();
  if (m_controllerState != MC_INACTIVE)
    [self enableBackGroundTask];
  [NSThread detachNewThreadSelector:@selector(enterBackgroundDetached:) toTarget:self withObject:nil];
}

//--------------------------------------------------------------
- (void)audioRouteChanged
{
  PRINT_SIGNATURE();
  if (MCRuntimeLib_Initialized())
    CAEFactory::DeviceChange();
}
- (EAGLContext*) getEAGLContextObj
{
  return [m_glView getContext];
}

#pragma mark - MCRuntimeLib routines
//--------------------------------------------------------------
- (void)pauseAnimation
{
  //PRINT_SIGNATURE();
  m_pause = TRUE;
  MCRuntimeLib_SetRenderGUI(false);
}
//--------------------------------------------------------------
- (void)resumeAnimation
{
  //PRINT_SIGNATURE();
  m_pause = FALSE;
  MCRuntimeLib_SetRenderGUI(true);
}
//--------------------------------------------------------------
- (void)startAnimation
{
  //PRINT_SIGNATURE();
  if (m_animating == NO && [m_glView getContext])
  {
    // kick off an animation thread
    m_animationThreadLock = [[NSConditionLock alloc] initWithCondition: FALSE];
    m_animationThread = [[NSThread alloc] initWithTarget:self
      selector:@selector(runAnimation:) object:m_animationThreadLock];
    [m_animationThread start];
    m_animating = TRUE;
  }
}
//--------------------------------------------------------------
- (void)stopAnimation
{
  //PRINT_SIGNATURE();
  if ([m_glView getContext])
  {
    m_appAlive = FALSE;
    m_animating = FALSE;
    if (MCRuntimeLib_Running())
      CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
    // wait for animation thread to die
    if ([m_animationThread isFinished] == NO)
      [m_animationThreadLock lockWhenCondition:TRUE];
  }
}

//--------------------------------------------------------------
- (void)runAnimation:(id)arg
{
  [[NSThread currentThread] setName:@"MCRuntimeLib"];
  [[NSThread currentThread] setThreadPriority:0.75];

  // signal the thread is alive
  NSConditionLock* myLock = arg;
  [myLock lock];
  
  // Prevent child processes from becoming zombies on exit
  // if not waited upon. See also Util::Command
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_NOCLDWAIT;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &sa, NULL);
  
  setlocale(LC_NUMERIC, "C");
  
  int status = 0;
  try
  {
    // set up some MCRuntimeLib specific relationships
    MCRuntimeLib::Context run_context;
    m_appAlive = TRUE;
    // start up with gui enabled
    status = MCRuntimeLib_Run(true);
    // we exited or died.
    MCRuntimeLib_SetRenderGUI(false);
  }
  catch(...)
  {
    m_appAlive = FALSE;
    ELOG(@"%sException caught on main loop status=%d. Exiting", __PRETTY_FUNCTION__, status);
  }
  
  // signal the thread is dead
  [myLock unlockWithCondition:TRUE];
  
  [self enableScreenSaver];
  [self enableSystemSleep];
}

//--------------------------------------------------------------
- (void)remoteControlReceivedWithEvent:(UIEvent*)receivedEvent
{
  PRINT_SIGNATURE();
  if (receivedEvent.type == UIEventTypeRemoteControl)
  {
    switch (receivedEvent.subtype)
    {
      case UIEventSubtypeRemoteControlTogglePlayPause:
        // check if not in background, we can get this if sleep is forced
        if (m_controllerState < MC_BACKGROUND)
          CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_PLAYPAUSE)));
        break;
      case UIEventSubtypeRemoteControlPlay:
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_PLAY)));
        break;
      case UIEventSubtypeRemoteControlPause:
        // ACTION_PAUSE sometimes cause unpause, use MediaPauseIfPlaying to make sure pause only
        //CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_PAUSE);
        // warning, something is wacky, in tvOS we only get this if play/pause button is pushed
        // the playPausePressed method should be getting called and it does, sometimes. WTF ?
        // check if not in background, we can get this if sleep is forced
        if (m_controllerState < MC_BACKGROUND)
          CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_PLAYPAUSE)));
        break;
      case UIEventSubtypeRemoteControlStop:
        CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_STOP);
        break;
      case UIEventSubtypeRemoteControlNextTrack:
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_NEXT_ITEM)));
        break;
      case UIEventSubtypeRemoteControlPreviousTrack:
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PREV_ITEM)));
        break;
      case UIEventSubtypeRemoteControlBeginSeekingForward:
        // use 4X speed forward.
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_FORWARD)));
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_FORWARD)));
        break;
      case UIEventSubtypeRemoteControlBeginSeekingBackward:
        // use 4X speed rewind.
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_REWIND)));
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_REWIND)));
        break;
      case UIEventSubtypeRemoteControlEndSeekingForward:
      case UIEventSubtypeRemoteControlEndSeekingBackward:
        // restore to normal playback speed.
        if (g_application.m_pPlayer->IsPlaying() && !g_application.m_pPlayer->IsPaused())
          CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_PLAY)));
        break;
      default:
        LOG(@"unhandled subtype: %d", (int)receivedEvent.subtype);
        break;
    }
    // start remote timeout
    [self startRemoteTimer];
  }
}

#pragma mark - Now Playing routines
//--------------------------------------------------------------
- (void)setIOSNowPlayingInfo:(NSDictionary *)info
{
  //PRINT_SIGNATURE();
  self.m_nowPlayingInfo = info;
  dispatch_async(dispatch_get_main_queue(),^{
    [[MPNowPlayingInfoCenter defaultCenter] setNowPlayingInfo:self.m_nowPlayingInfo];
    //LOG(@"setIOSNowPlayingInfo: after  %@", [[MPNowPlayingInfoCenter defaultCenter]  nowPlayingInfo]);
  });
}
//--------------------------------------------------------------
- (void)onPlayDelayed:(NSDictionary *)item
{
  // we want to delay playback report, helps us get the current timeline
  //PRINT_SIGNATURE();
  SEL singleParamSelector = @selector(onPlay:);
  [g_xbmcController performSelector:singleParamSelector withObject:item afterDelay:2];
}

- (void)onPlay:(NSDictionary *)item
{
  //PRINT_SIGNATURE();
  NSMutableDictionary * dict = [[NSMutableDictionary alloc] init];

  NSString *title = [item objectForKey:@"title"];
  if (title && title.length > 0)
    [dict setObject:title forKey:MPMediaItemPropertyTitle];
  NSString *album = [item objectForKey:@"album"];
  if (album && album.length > 0)
    [dict setObject:album forKey:MPMediaItemPropertyAlbumTitle];
  NSString *artists = [item objectForKey:@"artist"];
  if (artists && artists.length > 0)
    [dict setObject:artists forKey:MPMediaItemPropertyArtist];
  NSNumber *track = [item objectForKey:@"track"];
  if (track)
    [dict setObject:track forKey:MPMediaItemPropertyAlbumTrackNumber];
  NSNumber *duration = [item objectForKey:@"duration"];
  if (duration)
    [dict setObject:duration forKey:MPMediaItemPropertyPlaybackDuration];
  NSArray *genres = [item objectForKey:@"genre"];
  if (genres && genres.count > 0)
    [dict setObject:[genres componentsJoinedByString:@" "] forKey:MPMediaItemPropertyGenre];

  UIImage *image = [item objectForKey:@"thumb"];
  if (image)
  {
    MPMediaItemArtwork *mArt = [[MPMediaItemArtwork alloc] initWithImage:image];
    if (mArt)
    {
      [dict setObject:mArt forKey:MPMediaItemPropertyArtwork];
    }
  }
  
  NSNumber *elapsed = [NSNumber numberWithDouble:g_application.GetTime()];
  if (elapsed)
    [dict setObject:elapsed forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
  NSNumber *speed = [item objectForKey:@"speed"];
  if (speed)
    [dict setObject:speed forKey:MPNowPlayingInfoPropertyDefaultPlaybackRate];
  NSNumber *current = [item objectForKey:@"current"];
  if (current)
    [dict setObject:current forKey:MPNowPlayingInfoPropertyPlaybackQueueIndex];
  NSNumber *total = [item objectForKey:@"total"];
  if (total)
    [dict setObject:total forKey:MPNowPlayingInfoPropertyPlaybackQueueCount];
  /*
   other properities can be set:
   MPMediaItemPropertyAlbumTrackCount
   MPMediaItemPropertyComposer
   MPMediaItemPropertyDiscCount
   MPMediaItemPropertyDiscNumber
   MPMediaItemPropertyPersistentID

   Additional metadata properties:
   MPNowPlayingInfoPropertyChapterNumber;
   MPNowPlayingInfoPropertyChapterCount;
   */

  [self setIOSNowPlayingInfo:dict];

}
//--------------------------------------------------------------
- (void)onSpeedChanged:(NSDictionary *)item
{
  //PRINT_SIGNATURE();
  NSMutableDictionary *info = [self.m_nowPlayingInfo mutableCopy];
  NSNumber *elapsed = [NSNumber numberWithDouble:g_application.GetTime()];
  if (elapsed)
    [info setObject:elapsed forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
  NSNumber *speed = [item objectForKey:@"speed"];
  if (speed)
    [info setObject:speed forKey:MPNowPlayingInfoPropertyDefaultPlaybackRate];

  [self setIOSNowPlayingInfo:info];
}

- (void)onSeekDelayed:(id)arg
{
  // wait until any delayed seek fires and we come out of paused and are really playing again.
  while(CSeekHandler::GetInstance().InProgress() || g_application.m_pPlayer->IsPaused())
    usleep(50*1000);

  //PRINT_SIGNATURE();
  NSMutableDictionary *info = [self.m_nowPlayingInfo mutableCopy];

  NSNumber *elapsed = [NSNumber numberWithDouble:g_application.GetTime()];
  if (elapsed)
    [info setObject:elapsed forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
  NSNumber *speed = [NSNumber numberWithDouble:1.0f];
  if (speed)
    [info setObject:speed forKey:MPNowPlayingInfoPropertyDefaultPlaybackRate];

  [self setIOSNowPlayingInfo:info];
}

- (void)onSeekPlaying
{
  PRINT_SIGNATURE();
  [NSThread detachNewThreadSelector:@selector(onSeekDelayed:) toTarget:self withObject:nil];
}
//--------------------------------------------------------------
- (void)onPausePlaying:(NSDictionary *)item
{
  PRINT_SIGNATURE();
  NSMutableDictionary *info = [self.m_nowPlayingInfo mutableCopy];
  NSNumber *speed = [NSNumber numberWithDouble:0.000001f];
  if (speed)
    [info setObject:speed forKey:MPNowPlayingInfoPropertyDefaultPlaybackRate];
  NSNumber *elapsed = [NSNumber numberWithDouble:g_application.GetTime()];
  if (elapsed)
    [info setObject:elapsed forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
  [self setIOSNowPlayingInfo:info];
}
//--------------------------------------------------------------
- (void)onStopPlaying:(NSDictionary *)item
{
  PRINT_SIGNATURE();
  [self setIOSNowPlayingInfo:nil];
}

#pragma mark - control center

- (void)observeDefaultCenterStuff: (NSNotification *) notification
{
  //LOG(@"default: %@", [notification name]);
  //LOG(@"userInfo: %@", [notification userInfo]);
}

- (void)createCustomControlCenter
{
  //PRINT_SIGNATURE();
  
  MPRemoteCommandCenter *commandCenter = [MPRemoteCommandCenter sharedCommandCenter];

  // disable stop button
  commandCenter.stopCommand.enabled = NO;
  [commandCenter.stopCommand addTarget:self action:@selector(onCCStop:)];
  // enable play button
  commandCenter.playCommand.enabled = YES;
  [commandCenter.playCommand addTarget:self action:@selector(onCCPlay:)];
   
  // enable seek
  MPRemoteCommand *seekBackwardIntervalCommand = [commandCenter seekForwardCommand];
  [seekBackwardIntervalCommand setEnabled:YES];
  [seekBackwardIntervalCommand addTarget:self action:@selector(onCCFF:)];
  
  MPRemoteCommand *seekForwardIntervalCommand = [commandCenter seekBackwardCommand];
  [seekForwardIntervalCommand setEnabled:YES];
  [seekForwardIntervalCommand addTarget:self action:@selector(onCCREW:)];
  
  // enable next/previous
  MPRemoteCommand *previousTrackIntervalCommand = [commandCenter previousTrackCommand];
  [previousTrackIntervalCommand setEnabled:YES];
  [previousTrackIntervalCommand addTarget:self action:@selector(onCCPrev:)];
  
  MPRemoteCommand *nextTrackIntervalCommand = [commandCenter nextTrackCommand];
  [nextTrackIntervalCommand setEnabled:YES];
  [nextTrackIntervalCommand addTarget:self action:@selector(onCCNext:)];
  
  // disable skip but set selector
  MPSkipIntervalCommand *skipBackwardIntervalCommand = [commandCenter skipBackwardCommand];
  [skipBackwardIntervalCommand setEnabled:NO];
  [skipBackwardIntervalCommand addTarget:self action:@selector(onCCSkipPrev:)];
  skipBackwardIntervalCommand.preferredIntervals = @[@(42)];  // Set your own interval
  
  MPSkipIntervalCommand *skipForwardIntervalCommand = [commandCenter skipForwardCommand];
  skipForwardIntervalCommand.preferredIntervals = @[@(42)];  // Max 99
  [skipForwardIntervalCommand setEnabled:NO];
  [skipForwardIntervalCommand addTarget:self action:@selector(onCCSkipNext:)];
  
  // seek bar
  [commandCenter.changePlaybackPositionCommand addTarget:self action:@selector(onCCPlaybackPossition:)];
  
}
- (MPRemoteCommandHandlerStatus)onCCPlaybackPossition:(MPChangePlaybackPositionCommandEvent *) event
{
  g_application.SeekTime(event.positionTime);
  
  return MPRemoteCommandHandlerStatusSuccess;
}
- (void)onCCPlay:(MPRemoteCommandHandlerStatus*)event
{
  //PRINT_SIGNATURE();
}
- (void)onCCStop:(MPRemoteCommandHandlerStatus*)event
{
  //PRINT_SIGNATURE();
}
- (void)onCCFF:(MPRemoteCommandHandlerStatus*)event
{
  //PRINT_SIGNATURE();
}
- (void)onCCREW:(MPRemoteCommandHandlerStatus*)event
{
  //PRINT_SIGNATURE();
}
- (void)onCCNext:(MPRemoteCommandHandlerStatus*)event
{
 // PRINT_SIGNATURE();
}
- (void)onCCPrev:(MPRemoteCommandHandlerStatus*)event
{
 // PRINT_SIGNATURE();
}
- (void)onCCSkipNext:(MPRemoteCommandHandlerStatus*)event
{
  CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_STEP_FORWARD)));
 // PRINT_SIGNATURE();
}
- (void)onCCSkipPrev:(MPRemoteCommandHandlerStatus*)event
{
  CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_STEP_BACK)));
 // PRINT_SIGNATURE();
}

@end

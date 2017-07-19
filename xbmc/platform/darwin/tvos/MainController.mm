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

#import "system.h"

#import "Application.h"

#import "CompileInfo.h"
#import "cores/AudioEngine/AEFactory.h"
#import "guilib/GUIWindowManager.h"
#import "input/Key.h"
#import "input/ButtonTranslator.h"
#import "input/InputManager.h"
#import "input/touch/ITouchActionHandler.h"
#import "input/touch/generic/GenericTouchActionHandler.h"
#import "interfaces/AnnouncementManager.h"
#import "network/NetworkServices.h"
#import "messaging/ApplicationMessenger.h"
#import "platform/darwin/FocusEngineHandler.h"
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
#import "utils/log.h"

#import <MediaPlayer/MPMediaItem.h>
#import <MediaPlayer/MediaPlayer.h>
#import <MediaPlayer/MPNowPlayingInfoCenter.h>
#import <GameController/GameController.h>

// these MUST match those in system/keymaps/customcontroller.SiriRemote.xml
typedef enum SiriRemoteTypes
{
  SiriRemote_UpTap = 1,
  SiriRemote_DownTap = 2,
  SiriRemote_LeftTap = 3,
  SiriRemote_RightTap = 4,
  SiriRemote_CenterClick = 5,
  SiriRemote_MenuClick = 6,
  SiriRemote_CenterHold = 7,
  SiriRemote_UpSwipe = 8,
  SiriRemote_DownSwipe = 9,
  SiriRemote_LeftSwipe = 10,
  SiriRemote_RightSwipe = 11,
  SiriRemote_PausePlayClick = 12,
  SiriRemote_IR_Play = 13,
  SiriRemote_IR_Pause= 14,
  SiriRemote_IR_Stop = 15,
  SiriRemote_IR_NextTrack = 16,
  SiriRemote_IR_PreviousTrack = 17,
  SiriRemote_IR_FastForward = 18,
  SiriRemote_IR_Rewind = 19,
  SiriRemote_MenuClickAtHome = 20,
  SiriRemote_UpScroll = 21,
  SiriRemote_DownScroll = 22,
  SiriRemote_PageUp = 23,
  SiriRemote_PageDown = 24
} SiriRemoteTypes;

using namespace KODI::MESSAGING;

MainController *g_xbmcController;

//--------------------------------------------------------------
#pragma mark - MainController interface
@interface MainController ()
@property (strong, nonatomic) NSTimer *pressAutoRepeatTimer;
@property (strong, nonatomic) NSTimer *remoteIdleTimer;
@property (strong) GCController* gcController;

@end

#pragma mark - MainController implementation
@implementation MainController

@synthesize m_screenScale;
@synthesize m_screenIdx;
@synthesize m_screensize;
@synthesize m_nowPlayingInfo;
@synthesize m_remoteIdleState;
@synthesize m_remoteIdleTimeout;
@synthesize m_enableRemoteIdle;
@synthesize m_allowTap;

#pragma mark - internal key press methods
- (void)sendButtonPressed:(int)buttonId
{
  // if native keyboard is up, we don't want to send any button presses to MrMC
  if (m_nativeKeyboardActive)
    return;
  
  int actionID;
  std::string actionName;
  // Translate using custom controller translator.
  if (CButtonTranslator::GetInstance().TranslateCustomControllerString(
    CFocusEngineHandler::GetInstance().GetFocusWindowID(),
    "SiriRemote", buttonId, actionID, actionName))
  {
    CInputManager::GetInstance().QueueAction(CAction(actionID, 1.0f, 0.0f, actionName), true);
  }
  else
    CLog::Log(LOGDEBUG, "sendButtonPressed, ERROR mapping customcontroller action. CustomController: %s %i", "SiriRemote", buttonId);
}

#pragma mark - remote idle timer
//--------------------------------------------------------------

- (void)startRemoteTimer
{
  m_remoteIdleState = false;

  //PRINT_SIGNATURE();
  if (self.remoteIdleTimer != nil)
    [self stopRemoteTimer];
  if (m_enableRemoteIdle)
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
#define REPEATED_KEYPRESS_DELAY_S 0.25
// pause 0.05s (50ms) between keypresses
#define REPEATED_KEYPRESS_PAUSE_S 0.15
//--------------------------------------------------------------
static CFAbsoluteTime keyPressTimerStartSeconds;

//- (void)startKeyPressTimer:(XBMCKey)keyId
- (void)startKeyPressTimer:(int)keyId
{
  [self startKeyPressTimer:keyId doBeforeDelay:true withDelay:REPEATED_KEYPRESS_DELAY_S];
}

- (void)startKeyPressTimer:(int)keyId doBeforeDelay:(bool)doBeforeDelay
{
  [self startKeyPressTimer:keyId doBeforeDelay:doBeforeDelay withDelay:REPEATED_KEYPRESS_DELAY_S withInterval:REPEATED_KEYPRESS_PAUSE_S];
}

- (void)startKeyPressTimer:(int)keyId doBeforeDelay:(bool)doBeforeDelay withDelay:(NSTimeInterval)delay
{
  [self startKeyPressTimer:keyId doBeforeDelay:doBeforeDelay withDelay:delay withInterval:REPEATED_KEYPRESS_PAUSE_S];
}

- (void)startKeyPressTimer:(int)keyId doBeforeDelay:(bool)doBeforeDelay withInterval:(NSTimeInterval)interval
{
  [self startKeyPressTimer:keyId doBeforeDelay:doBeforeDelay withDelay:REPEATED_KEYPRESS_DELAY_S withInterval:interval];
}

static int keyPressTimerFiredCount = 0;
- (void)startKeyPressTimer:(int)keyId doBeforeDelay:(bool)doBeforeDelay withDelay:(NSTimeInterval)delay withInterval:(NSTimeInterval)interval
{
  //PRINT_SIGNATURE();
  if (self.pressAutoRepeatTimer != nil)
    [self stopKeyPressTimer];

  if (doBeforeDelay)
    [self sendButtonPressed:keyId];

  NSNumber *number = [NSNumber numberWithInt:keyId];
  NSDate *fireDate = [NSDate dateWithTimeIntervalSinceNow:delay];

  keyPressTimerFiredCount = 0;
  keyPressTimerStartSeconds = CFAbsoluteTimeGetCurrent() + delay;
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
- (int)getKeyPressTimerCount
{
  return keyPressTimerFiredCount;
}
- (void)keyPressTimerCallback:(NSTimer*)theTimer
{
  //PRINT_SIGNATURE();
  // if queue is not empty - skip this timer event before letting it process
  if (CWinEvents::GetQueueSize())
    return;

  NSNumber *keyId = [theTimer userInfo];
  CFAbsoluteTime secondsFromStart = CFAbsoluteTimeGetCurrent() - keyPressTimerStartSeconds;
  if ([self canDoScrollUpDown] && secondsFromStart > 1.5f)
  {
    switch([keyId intValue])
    {
      case SiriRemote_UpTap:
      case SiriRemote_LeftTap:
        [self sendButtonPressed:SiriRemote_UpScroll];
        break;
      case SiriRemote_DownTap:
      case SiriRemote_RightTap:
        [self sendButtonPressed:SiriRemote_DownScroll];
        break;
      default:
        [self sendButtonPressed:[keyId intValue]];
        break;
    }
  }
  else
  {
    [self sendButtonPressed:[keyId intValue]];
  }
  keyPressTimerFiredCount++;
}

#pragma mark - remote helpers

//--------------------------------------------------------------
-(bool)shouldFastScroll
{
  // we dont want fast scroll in below windows, no point in going 15 places in home screen
  int window = CFocusEngineHandler::GetInstance().GetFocusWindowID();

  if (window == WINDOW_HOME ||
      window == WINDOW_FULLSCREEN_LIVETV ||
      window == WINDOW_FULLSCREEN_VIDEO ||
      window == WINDOW_FULLSCREEN_RADIO ||
      (window >= WINDOW_SETTINGS_START && window <= WINDOW_SETTINGS_APPEARANCE)
      )
    return false;
  
  return true;
}

-(ORIENTATION)getFocusedOrientation
{
  return CFocusEngineHandler::GetInstance().GetFocusOrientation();
}

-(bool)canDoScrollUpDown
{
  // we dont want fast scroll in below windows, no point in going 15 places in home screen
  CGUIWindow* pWindow = g_windowManager.GetWindow(g_windowManager.GetFocusedWindow());
  CGUIControl *focusedControl = pWindow->GetFocusedControl();
  if (focusedControl)
  {
    if (focusedControl->GetControlType() == CGUIControl::GUICONTROL_SCROLLBAR)
      return false;
  }
  return true;
}

//--------------------------------------------------------------
- (void)setRemoteIdleTimeout:(int)timeout
{
  m_remoteIdleTimeout = (float)timeout;
  [self startRemoteTimer];
}

- (void)enableRemoteIdle:(BOOL)enable
{
  //PRINT_SIGNATURE();
  m_enableRemoteIdle = enable;
  [self startRemoteTimer];
}

- (void)enableRemotePanSwipe:(BOOL)enable
{
  //PRINT_SIGNATURE();
  siriRemoteInfo.enablePanSwipe = enable;
}

//--------------------------------------------------------------
#pragma mark - gesture methods
//--------------------------------------------------------------
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
  // important, this lets our view get touch events
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
  if ([gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]] && ([otherGestureRecognizer isKindOfClass:[UISwipeGestureRecognizer class]] || [otherGestureRecognizer isKindOfClass:[UITapGestureRecognizer class]]))
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
}
//--------------------------------------------------------------
- (void)createTapGestureRecognizers
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
  
  // we always have these under tvos, both ir and siri remotes respond to these
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
- (void) nativeKeyboardActive: (bool)active;
{
  m_nativeKeyboardActive = active;
}
//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - gesture handlers
- (void)menuPressed:(UITapGestureRecognizer *)sender
{
  PRINT_SIGNATURE();
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
          [self sendButtonPressed:SiriRemote_MenuClickAtHome];
      }
      else
      {
        [self sendButtonPressed:SiriRemote_MenuClick];
      }
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}
//--------------------------------------------------------------
- (void)selectButtonHold
{
  self.m_holdCounter++;
  [self.m_holdTimer invalidate];
  [self sendButtonPressed:SiriRemote_CenterHold];
}
//--------------------------------------------------------------
- (void)selectPressed:(UITapGestureRecognizer *)sender
{
  PRINT_SIGNATURE();
  
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      self.m_holdCounter = 0;
      self.m_holdTimer = [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(selectButtonHold) userInfo:nil repeats:YES];
      break;
    case UIGestureRecognizerStateChanged:
      if (self.m_holdCounter > 1)
      {
        [self.m_holdTimer invalidate];
        [self sendButtonPressed:SiriRemote_CenterHold];
      }
      break;
    case UIGestureRecognizerStateEnded:
      [self.m_holdTimer invalidate];
      if (self.m_holdCounter < 1)
        [self sendButtonPressed:SiriRemote_CenterClick];
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}

- (void)playPausePressed:(UITapGestureRecognizer *) sender
{
  PRINT_SIGNATURE();
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      break;
    case UIGestureRecognizerStateChanged:
      break;
    case UIGestureRecognizerStateEnded:
      [self sendButtonPressed:SiriRemote_PausePlayClick];
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}

#define REPEATED_IRPRESS_DELAY_S 0.35
- (IBAction)IRRemoteUpArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      [self sendButtonPressed:SiriRemote_UpTap];
      if ([self shouldFastScroll] && [self getFocusedOrientation] == VERTICAL)
      {
        [self startKeyPressTimer:SiriRemote_UpTap doBeforeDelay:false withDelay:REPEATED_IRPRESS_DELAY_S];
      }
      break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateChanged:
    case UIGestureRecognizerStateCancelled:
      [self stopKeyPressTimer];
      // restart remote timeout
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
      [self sendButtonPressed:SiriRemote_DownTap];
      if ([self shouldFastScroll] && [self getFocusedOrientation] == VERTICAL)
      {
        [self startKeyPressTimer:SiriRemote_DownTap doBeforeDelay:false withDelay:REPEATED_IRPRESS_DELAY_S];
      }
      break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateChanged:
    case UIGestureRecognizerStateCancelled:
      [self stopKeyPressTimer];
      // restart remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}
- (IBAction)IRRemoteLeftArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
     [self sendButtonPressed:SiriRemote_LeftTap];
      if ([self shouldFastScroll] && [self getFocusedOrientation] == HORIZONTAL)
      {
        [self startKeyPressTimer:SiriRemote_LeftTap doBeforeDelay:false withDelay:REPEATED_IRPRESS_DELAY_S];
      }
      break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateChanged:
    case UIGestureRecognizerStateCancelled:
      [self stopKeyPressTimer];
      // restart remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}
- (IBAction)IRRemoteRightArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      [self sendButtonPressed:SiriRemote_RightTap];
      if ([self shouldFastScroll] && [self getFocusedOrientation] == HORIZONTAL)
      {
        [self startKeyPressTimer:SiriRemote_RightTap doBeforeDelay:false withDelay:REPEATED_IRPRESS_DELAY_S];
      }
      break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateChanged:
    case UIGestureRecognizerStateCancelled:
      [self stopKeyPressTimer];
      // restart remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}

//--------------------------------------------------------------
- (IBAction)tapUpArrowPressed:(UIGestureRecognizer *)sender
{
  //if (!m_remoteIdleState)
  NSLog(@"microGamepad: tapUpArrowPressed");
  if (m_allowTap)
    [self sendButtonPressed:SiriRemote_UpTap];
  m_allowTap = true;
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (IBAction)tapDownArrowPressed:(UIGestureRecognizer *)sender
{
  //if (!m_remoteIdleState)
  NSLog(@"microGamepad: tapDownArrowPressed");
  if (m_allowTap)
    [self sendButtonPressed:SiriRemote_DownTap];
  m_allowTap = true;
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (IBAction)tapLeftArrowPressed:(UIGestureRecognizer *)sender
{
  //if (!m_remoteIdleState)
  NSLog(@"microGamepad: tapLeftArrowPressed");
  if (m_allowTap)
    [self sendButtonPressed:SiriRemote_LeftTap];
  m_allowTap = true;
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (IBAction)tapRightArrowPressed:(UIGestureRecognizer *)sender
{
  //if (!m_remoteIdleState)
  NSLog(@"microGamepad: tapRightArrowPressed");
  if (m_allowTap)
    [self sendButtonPressed:SiriRemote_RightTap];
  m_allowTap = true;
  [self startRemoteTimer];
}

//--------------------------------------------------------------
- (IBAction)handleSwipe:(UISwipeGestureRecognizer *)sender
{
  if (!m_remoteIdleState)
  {
    if (m_appAlive == YES)//NO GESTURES BEFORE WE ARE UP AND RUNNING
    {
      switch (sender.state)
      {
        case UIGestureRecognizerStateBegan:
          NSLog(@"microGamepad: handleSwipe:UIGestureRecognizerStateBegan");
          break;
        case UIGestureRecognizerStateChanged:
          NSLog(@"microGamepad: handleSwipe:UIGestureRecognizerStateChanged");
          break;
        case UIGestureRecognizerStateEnded:
          NSLog(@"microGamepad: handleSwipe:UIGestureRecognizerStateEnded");
          break;
        case UIGestureRecognizerStateCancelled:
          NSLog(@"microGamepad: handleSwipe:UIGestureRecognizerStateCancelled");
          break;
        default:
          break;
      }
      switch ([sender direction])
      {
        case UISwipeGestureRecognizerDirectionRight:
          NSLog(@"microGamepad: handleSwipe:UISwipeGestureRecognizerDirectionRight");
          break;
        case UISwipeGestureRecognizerDirectionLeft:
          NSLog(@"microGamepad: handleSwipe:UISwipeGestureRecognizerDirectionLeft");
          break;
        case UISwipeGestureRecognizerDirectionUp:
          NSLog(@"microGamepad: handleSwipe:UISwipeGestureRecognizerDirectionUp");
          break;
        case UISwipeGestureRecognizerDirectionDown:
          NSLog(@"microGamepad: handleSwipe:UISwipeGestureRecognizerDirectionDown");
          break;
      }
    }
  }
  // start remote idle timer
  [self startRemoteTimer];
}

//--------------------------------------------------------------
- (IBAction)handlePan:(UIPanGestureRecognizer *)sender
{
  if (!m_remoteIdleState)
  {
    if (m_appAlive == YES)//NO GESTURES BEFORE WE ARE UP AND RUNNING
    {
      switch (sender.state)
      {
        case UIGestureRecognizerStateBegan:
          NSLog(@"microGamepad: handlePan:UIGestureRecognizerStateBegan");
          break;
        case UIGestureRecognizerStateChanged:
          NSLog(@"microGamepad: handlePan:UIGestureRecognizerStateChanged");
          break;
        case UIGestureRecognizerStateEnded:
          NSLog(@"microGamepad: handlePan:UIGestureRecognizerStateEnded");
          break;
        case UIGestureRecognizerStateCancelled:
          NSLog(@"microGamepad: handlePan:UIGestureRecognizerStateCancelled");
          break;
        default:
          break;
      }
    }
  }
  // start remote idle timer
  [self startRemoteTimer];
}

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - microGamepad methods
typedef enum SiriRemoteState
{
  SiriRemoteIdle,
  SiriRemoteSelect,
  SiriRemoteTapTimer,
  SiriRemotePanSwipe,
  SiriRemotePan,
} SiriRemoteState;

typedef struct
{
  float dt, dx, dy;
  bool  debug = false;
  CGPoint startPoint;
  CGPoint movedPoint;
  float   tapbounts = 0.18f;
  // // movement to outside w/2 or h/2 determines jump to next item.
  CGRect  panningRect;
  // default panning rect size, this will
  // get offset as user pans across items.
  CGRect  panningRectStart = {0.0f, 0.0f, 0.85f, 0.85f};
  // points outside are in 'pinned' area
  CGRect  panningPinnedRect = {0.25f, 0.25f, 1.50f, 1.50f};
  // points outside are in horizontal 'pinned' area.
  // strips at top and bottom that spans from left to right.
  CGRect  panningPinnedHorzRect = {0.00f, 0.25f, 2.00f, 1.50f};
  // points outside are in horizontal 'pinned' area.
  // strips at left and right that spans from top to bottom.
  CGRect  panningPinnedVertRect = {0.25f, 0.00f, 1.50f, 2.00f};
  CFAbsoluteTime startSeconds;
  CFAbsoluteTime movedSeconds;
  bool enablePanSwipe;
  float ignoreAfterSwipeSeconds;
  FocusEngineAnimate focusAnimate;
  SiriRemoteState state = SiriRemoteIdle;
} SiriRemoteInfo;
static SiriRemoteInfo siriRemoteInfo;

-(void)startTapRepeatTimer:(SiriRemoteInfo&)remote withPoint:(CGPoint)point withdelay:(NSTimeInterval)delayTime
{
  if (![self shouldFastScroll])
    return;

  // absolute coordinate system is 0 to +2 with left/bottom = (0,0)
  // transform coordinates to left/bottom = (-1, -1) to make checks easy
  CGPoint centerStart = CGPointMake(
    point.x - 1.0,
    point.y - 1.0);
  NSLog(@"microGamepad: tap timer started");
  // tap detected, use staring location to determine
  // where tap occurred, ending location could be a slip of the finger
  if (fabs(centerStart.x) >= fabs(centerStart.y))
  {
    if (point.x < 1.0)
    {
      if (remote.debug)
        NSLog(@"microGamepad: tap repeat left");
      if ([self shouldFastScroll] && [self getFocusedOrientation] == HORIZONTAL)
      {
        [self startKeyPressTimer:SiriRemote_LeftTap doBeforeDelay:false withDelay:delayTime];
      }
      else
        [self startKeyPressTimer:SiriRemote_UpTap doBeforeDelay:false withDelay:delayTime];
    }
    else
    {
      if (remote.debug)
        NSLog(@"microGamepad: tap repeat right");
      if ([self shouldFastScroll] && [self getFocusedOrientation] == HORIZONTAL)
      {
        [self startKeyPressTimer:SiriRemote_RightTap doBeforeDelay:false withDelay:delayTime];
      }
      else
        [self startKeyPressTimer:SiriRemote_DownTap doBeforeDelay:false withDelay:delayTime];
    }
  }
  else
  {
    if (point.y >= 1.0)
    {
      if (remote.debug)
        NSLog(@"microGamepad: tap repeat up");
      if ([self getFocusedOrientation] == VERTICAL)
        [self startKeyPressTimer:SiriRemote_UpTap doBeforeDelay:false withDelay:delayTime];
    }
    else
    {
      if (remote.debug)
        NSLog(@"microGamepad: tap repeat down");
      if ([self getFocusedOrientation] == VERTICAL)
        [self startKeyPressTimer:SiriRemote_DownTap doBeforeDelay:false withDelay:delayTime];
    }
  }
}
-(bool)isTapRepeatTimerActive
{
  return self.pressAutoRepeatTimer != nil;
}

-(void)stopTapRepeatTimer
{
  if (self.pressAutoRepeatTimer != nil)
  {
    if (siriRemoteInfo.debug)
      NSLog(@"microGamepad: tap timer stopped");
    [self stopKeyPressTimer];
  }
}

-(void)processPanEvent:(SiriRemoteInfo&)remote
{
  if (!siriRemoteInfo.enablePanSwipe)
    return;

  if (!CGRectContainsPoint(remote.panningPinnedRect, remote.movedPoint))
  {
    // if outside panningPinnedRect,
    // we are at edges of trackpad and considered 'pinned'
    if (siriRemoteInfo.debug)
        NSLog(@"microGamepad: processPanPinned");

    if (![self isTapRepeatTimerActive])
    {
      // use moved point to determine direction
      [self startTapRepeatTimer:remote withPoint:remote.movedPoint withdelay:REPEATED_KEYPRESS_DELAY_S];
      CFocusEngineHandler::GetInstance().ClearAnimation();
    }
  }
  else
  {
    if ([self isTapRepeatTimerActive])
    {
      // have been auto-repeating, kill the repeat timer
      [self stopTapRepeatTimer];
      // if we are fast scrolling, item focus might be on the group
      // wait a little bit until gui catches up and sets the real item focus
      usleep((int)(0.15f * 1000) * 1000);
    }

    // update focus engine visual effect
    float dx = remote.movedPoint.x - CGRectGetMidX(remote.panningRect);
    float dy = remote.movedPoint.y - CGRectGetMidY(remote.panningRect);
    // check if focus visual effect needs updating
    if (!CGPointEqualToPoint(remote.movedPoint, remote.startPoint))
    {
      remote.startPoint = remote.movedPoint;
      if (remote.debug)
        NSLog(@"microGamepad: focus dx(%f), dy(%f)", dx, dy);
      remote.focusAnimate.slideX = dx;
      remote.focusAnimate.slideY = dy;
      CFocusEngineHandler::GetInstance().UpdateAnimation(remote.focusAnimate);
    }

    if (!CGRectContainsPoint(remote.panningRect, remote.movedPoint))
    {
      // check if moved point is outside panning rect
      // absolute coordinate system is 0 to +2 with left/bottom = (0,0)
      // use SiriRemote_xxxxSwipe here so we can block them when playing videos
      // check if inside panning rect. if not, we moved outside and need to move focus.
      [self stopTapRepeatTimer];
      if (remote.debug)
      {
        NSLog(@"microGamepad: x(%f), y(%f), L(%f), R(%f), T(%f), B(%f)",
          remote.movedPoint.x, remote.movedPoint.y,
          CGRectGetMinX(remote.panningRect),
          CGRectGetMaxX(remote.panningRect),
          CGRectGetMaxY(remote.panningRect),
          CGRectGetMinY(remote.panningRect));
      }

      bool moved = false;
      float delaySeconds = 0.15f;
      // if user pans fast, might have panned more than item
      // check if dx or dy is some multiple the panning rect item bounds.
      int cycleX = lround(fabs(dx) / CGRectGetWidth(remote.panningRect));
      int cycleY = lround(fabs(dy) / CGRectGetHeight(remote.panningRect));
      // check if moving left/right or up/down ?
      if (fabs(dx) >= fabs(dy))
      {
        if (remote.movedPoint.x <= CGRectGetMinX(remote.panningRect))
        {
          if (remote.debug)
          {
            NSLog(@"microGamepad: pan left  dt(%f), dx(%f), dy(%f), cycleX(%d)",
              remote.dt, dx, dy, cycleX);
          }
          moved = true;
          for (int i = 0; i < cycleX; ++i)
          {
            [self sendButtonPressed:SiriRemote_LeftSwipe];
            usleep((int)(delaySeconds * 1000) * 1000);
          }
        }
        else if (remote.movedPoint.x >= CGRectGetMaxX(remote.panningRect))
        {
          if (remote.debug)
          {
            NSLog(@"microGamepad: pan right  dt(%f), dx(%f), dy(%f), cycleX(%d)",
              remote.dt, dx, dy, cycleX);
          }
          moved = true;
          for (int i = 0; i < cycleX; ++i)
          {
            [self sendButtonPressed:SiriRemote_RightSwipe];
            usleep((int)(delaySeconds * 1000) * 1000);
          }
        }
      }
      else
      {
        if (remote.movedPoint.y >= CGRectGetMaxY(remote.panningRect))
        {
          if (remote.debug)
          {
            NSLog(@"microGamepad: pan up  dt(%f), dx(%f), dy(%f), cycleY(%d)",
              remote.dt, dx, dy, cycleY);
          }
          moved = true;
          for (int i = 0; i < cycleY; ++i)
          {
            [self sendButtonPressed:SiriRemote_UpSwipe];
            usleep((int)(delaySeconds * 1000) * 1000);
          }
        }
        else if (remote.movedPoint.y <= CGRectGetMinY(remote.panningRect))
        {
          if (remote.debug)
          {
            NSLog(@"microGamepad: pan down  dt(%f), dx(%f), dy(%f), cycleY(%d)",
              remote.dt, dx, dy, cycleY);
          }
          moved = true;
          for (int i = 0; i < cycleY; ++i)
          {
            [self sendButtonPressed:SiriRemote_DownSwipe];
            usleep((int)(delaySeconds * 1000) * 1000);
          }
        }
      }

      // only update if we actually moved focus
      if (moved)
      {
        remote.panningRect.origin.x += copysignf(0.5 * CGRectGetWidth(remote.panningRect) * cycleX, dx);
        remote.panningRect.origin.y += copysignf(0.5 * CGRectGetHeight(remote.panningRect) * cycleY, dy);
        if (remote.debug)
        {
          NSLog(@"microGamepad: x(%f), y(%f), L(%f), R(%f), T(%f), B(%f)",
            remote.movedPoint.x, remote.movedPoint.y,
            CGRectGetMinX(remote.panningRect),
            CGRectGetMaxX(remote.panningRect),
            CGRectGetMaxY(remote.panningRect),
            CGRectGetMinY(remote.panningRect));
        }
      }
    }
  }
}

-(void)processSwipeEvent:(SiriRemoteInfo&)remote withCycles:(int)cycles
{
  if (!siriRemoteInfo.enablePanSwipe)
    return;

  // absolute coordinate system is 0 to +2 with left/bottom = (0,0)
  // use SiriRemote_xxxxSwipe here so we can block them when playing videos
  float delaySeconds = 0.2f;
  if ([self shouldFastScroll])
    delaySeconds = 0.05f;

  // check for swipe left/right or up/down ?
  if (remote.dx >= remote.dy)
  {
    if (CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRISWIPEONCE) ||
        [self getFocusedOrientation] != HORIZONTAL)
    {
      cycles = 1;
      delaySeconds = 0.0f;
    }

    float vx = (remote.movedPoint.x - remote.startPoint.x) / remote.dt;
    if (remote.movedPoint.x < remote.startPoint.x)
    {
      if (remote.debug)
      {
        NSLog(@"microGamepad: swipe left  dt(%f), dx(%f), dy(%f), vx(%f)",
          remote.dt, remote.dx, remote.dy, vx);
      }
      int keyId = SiriRemote_LeftSwipe;
      if (remote.dx < 2.0f && !CGRectContainsPoint(remote.panningPinnedRect, remote.startPoint) &&
          !CGRectContainsPoint(remote.panningPinnedRect, remote.movedPoint) &&
          !CGRectContainsPoint(remote.panningPinnedHorzRect, remote.movedPoint))
      {
        keyId = SiriRemote_PageUp;
      }
      for (int i = 0; i < cycles; ++i)
      {
        [self sendButtonPressed:keyId];
        usleep((int)(delaySeconds * 1000) * 1000);
      }
    }
    else
    {
      if (remote.debug)
      {
        NSLog(@"microGamepad: swipe right dt(%f), dx(%f), dy(%f), vx(%f)",
          remote.dt, remote.dx, remote.dy, vx);
      }
      int keyId = SiriRemote_RightSwipe;
      if (remote.dx < 2.0f && !CGRectContainsPoint(remote.panningPinnedRect, remote.startPoint) &&
          !CGRectContainsPoint(remote.panningPinnedRect, remote.movedPoint) &&
          !CGRectContainsPoint(remote.panningPinnedHorzRect, remote.movedPoint))
      {
        keyId = SiriRemote_PageDown;
      }
      for (int i = 0; i < cycles; ++i)
      {
        [self sendButtonPressed:keyId];
        usleep((int)(delaySeconds * 1000) * 1000);
      }
    }
  }
  else
  {
    if (CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRISWIPEONCE) ||
        [self getFocusedOrientation] != VERTICAL)
    {
      cycles = 1;
      delaySeconds = 0.0f;
    }

    float vy = (remote.movedPoint.y - remote.startPoint.y) / remote.dt;
    if (remote.movedPoint.y > remote.startPoint.y)
    {
      if (remote.debug)
      {
        NSLog(@"microGamepad: swipe up    dt(%f), dx(%f), dy(%f), vy(%f)",
          remote.dt, remote.dx, remote.dy, vy);
      }
      int keyId = SiriRemote_UpSwipe;
      if (remote.dy < 2.0f && !CGRectContainsPoint(remote.panningPinnedRect, remote.startPoint) &&
          !CGRectContainsPoint(remote.panningPinnedRect, remote.movedPoint) &&
          !CGRectContainsPoint(remote.panningPinnedVertRect, remote.movedPoint))
      {
        keyId = SiriRemote_PageUp;
      }
      for (int i = 0; i < cycles; ++i)
      {
        [self sendButtonPressed:keyId];
        usleep((int)(delaySeconds * 1000) * 1000);
      }
    }
    else
    {
      if (remote.debug)
      {
        NSLog(@"microGamepad: swipe down  dt(%f), dx(%f), dy(%f), vy(%f)",
          remote.dt, remote.dx, remote.dy, vy);
      }
      int keyId = SiriRemote_DownSwipe;
      if (remote.dy < 2.0f && !CGRectContainsPoint(remote.panningPinnedRect, remote.startPoint) &&
          !CGRectContainsPoint(remote.panningPinnedRect, remote.movedPoint) &&
          !CGRectContainsPoint(remote.panningPinnedVertRect, remote.movedPoint))
      {
        keyId = SiriRemote_PageDown;
      }
      for (int i = 0; i < cycles; ++i)
      {
        [self sendButtonPressed:keyId];
        usleep((int)(delaySeconds * 1000) * 1000);
      }
    }
  }
}

-(void)processTapEvent:(SiriRemoteInfo&)remote
{
  if (remote.startPoint.x == 0.0f ||
      remote.startPoint.y == 0.0f ||
      remote.startPoint.x == 2.0f ||
      remote.startPoint.y == 2.0f)
  {
    //apple remote app deadzone
    return;
  }

  // absolute coordinate system is 0 to +2 with left/bottom = (0,0)
  // transform coordinates to left/bottom = (-1, -1) to make checks easy
  CGPoint centerStart = CGPointMake(
    remote.startPoint.x - 1.0,
    remote.startPoint.y - 1.0);
  if (fabs(centerStart.x) < remote.tapbounts && fabs(centerStart.y) < remote.tapbounts)
  {
    // tap in center, ignore it.
    if (remote.debug)
      NSLog(@"microGamepad: tap center, ignored  dt(%f)", remote.dt);
  }
  else
  {
    // tap detected, use begining location to determine
    // where tap occurred, ending location could be a slip of the finger
    if (fabs(centerStart.x) >= fabs(centerStart.y))
    {
      if (remote.startPoint.x < 1.0)
      {
        if (remote.debug)
          NSLog(@"microGamepad: tap left  dt(%f)", remote.dt);
        [self sendButtonPressed:SiriRemote_LeftTap];
      }
      else
      {
        if (remote.debug)
          NSLog(@"microGamepad: tap right dt(%f)", remote.dt);
        [self sendButtonPressed:SiriRemote_RightTap];
      }
    }
    else
    {
      if (remote.startPoint.y >= 1.0)
      {
        if (remote.debug)
          NSLog(@"microGamepad: tap up    dt(%f)", remote.dt);
        [self sendButtonPressed:SiriRemote_UpTap];
      }
      else
      {
        if (remote.debug)
          NSLog(@"microGamepad: tap down  dt(%f)", remote.dt);
        [self sendButtonPressed:SiriRemote_DownTap];
      }
    }
  }
}

-(void)updateRemoteStartInfo:(SiriRemoteInfo&)remote withGamePad:(GCMicroGamepad*)gamepad
{
  // dpad values range from 0.0 to 1.0
  // convert begining up/down/left/right into horizontal/vertical axis (-1, +1)
  // and transform to 0 to +2 with left/bottom = (0,0), makes calcs easy
  remote.startPoint = CGPointMake(
    -gamepad.dpad.left.value + gamepad.dpad.right.value,
    -gamepad.dpad.down.value + gamepad.dpad.up.value);
  remote.startPoint.x += 1.0;
  remote.startPoint.y += 1.0;
  /*
  NSLog(@"microGamepad: U(%d), D(%d), L(%d), R(%d), point %@",
    gamepad.dpad.up.pressed,
    gamepad.dpad.down.pressed,
    gamepad.dpad.left.pressed,
    gamepad.dpad.right.pressed,
    NSStringFromCGPoint(remote));
  */
  remote.movedPoint = remote.startPoint;
  remote.panningRect = remote.panningRectStart;
  remote.panningRect.origin.x = remote.startPoint.x - CGRectGetWidth(remote.panningRect) / 2.0;
  remote.panningRect.origin.y = remote.startPoint.y - CGRectGetHeight(remote.panningRect) / 2.0;
  if (remote.debug)
  {
    NSLog(@"microGamepad: x(%f), y(%f), L(%f), R(%f), T(%f), B(%f)",
      remote.movedPoint.x, remote.movedPoint.y,
      CGRectGetMinX(remote.panningRect),
      CGRectGetMaxX(remote.panningRect),
      CGRectGetMaxY(remote.panningRect),
      CGRectGetMinY(remote.panningRect));
  }
  remote.startSeconds = CFAbsoluteTimeGetCurrent();
  remote.movedSeconds = remote.startSeconds;
  remote.ignoreAfterSwipeSeconds = 0.0f;
  remote.dt = 0.0f;
  remote.dx = 0.0f;
  remote.dy = 0.0f;
}

-(void)updateRemoteMovedInfo:(SiriRemoteInfo&)remote withGamePad:(GCMicroGamepad*)gamepad
{
  // dpad values range from 0.0 to 1.0
  // referenced from center of touchpad.
  // convert begining up/down/left/right into horizontal/vertical axis (-1, +1)
  // and transform to 0 to +2 with left/bottom = (0,0), makes calcs easy
  remote.movedPoint = CGPointMake(
    -gamepad.dpad.left.value + gamepad.dpad.right.value,
    -gamepad.dpad.down.value + gamepad.dpad.up.value);
  remote.movedPoint.x += 1.0;
  remote.movedPoint.y += 1.0;
  remote.movedSeconds = CFAbsoluteTimeGetCurrent();
  remote.dt = remote.movedSeconds - remote.startSeconds;
  remote.dx = fabs(remote.movedPoint.x - remote.startPoint.x);
  remote.dy = fabs(remote.movedPoint.y - remote.startPoint.y);
}

-(void)cgControllerDidDisconnect:(NSNotification *)notification
{
  self.gcController = nil;
  if (siriRemoteInfo.debug)
    NSLog(@"microGamepad: did disconnect");
}

-(void)cgControllerDidConnect:(NSNotification*)notification
{
  if( notification.object != nil )
  {
    self.gcController = notification.object;
    if (self.gcController.microGamepad)
    {
      siriRemoteInfo.state = SiriRemoteIdle;
      siriRemoteInfo.startSeconds = CFAbsoluteTimeGetCurrent();
      siriRemoteInfo.movedSeconds = siriRemoteInfo.startSeconds;
      // Capturing 'self' strongly in this block is likely to lead to a retain cycle
      // so creating a weak reference to self for access inside the block
      __weak MainController *weakSelf = self;

      //self.gcController.microGamepad.allowsRotation = allowsRotation ? YES : NO;
      self.gcController.microGamepad.reportsAbsoluteDpadValues = YES;
#if 0
      self.gcController.microGamepad.valueChangedHandler = ^(GCMicroGamepad *gamepad, GCControllerElement *element)
      {
        siriRemoteInfo.startPoint = CGPointMake(
          -gamepad.dpad.left.value + gamepad.dpad.right.value,
          -gamepad.dpad.down.value + gamepad.dpad.up.value);
        siriRemoteInfo.startPoint.x += 1.0;
        siriRemoteInfo.startPoint.y += 1.0;
        NSLog(@"microGamepad: A(%d), U(%d), D(%d), L(%d), R(%d), point %@",
          gamepad.buttonA.pressed,
          gamepad.dpad.up.pressed,
          gamepad.dpad.down.pressed,
          gamepad.dpad.left.pressed,
          gamepad.dpad.right.pressed,
          NSStringFromCGPoint(siriRemoteInfo.startPoint));
      };
#else
      self.gcController.microGamepad.valueChangedHandler = ^(GCMicroGamepad *gamepad, GCControllerElement *element)
      {
        m_allowTap = false;
        // buttonA is the 'select' button,
        // if pressed bypass any touch handling
        if (gamepad.buttonA.pressed)
        {
          [weakSelf stopTapRepeatTimer];
          siriRemoteInfo.state = SiriRemoteSelect;
        }

        // check for other 'ignore' conditions
        if (siriRemoteInfo.state == SiriRemoteIdle)
        {
          // check for spurious touch after swipe
          // not sure why this can sometimes happen
          if (siriRemoteInfo.ignoreAfterSwipeSeconds > 0.0f)
          {
            float dt = CFAbsoluteTimeGetCurrent() - siriRemoteInfo.movedSeconds;
            if (dt > siriRemoteInfo.ignoreAfterSwipeSeconds)
              siriRemoteInfo.ignoreAfterSwipeSeconds = 0.0f;
            else
              return;
          }
          // if siri remote idle timeout is active,
          // ignore the 1st touch and pretend to wake up
          if (weakSelf.m_remoteIdleState)
          {
            // start remote idle timer
            [weakSelf startRemoteTimer];
            return;
          }
        }
        // we only care that some dpad is pressed or not. touch directions are
        // determined by the dpad values. The reason for this is when a finger
        // is down, we get two signaling down and we have to track all four.
        // we also track buttonA.pressed so we can ignore it.
        // when user does a select, finger down (up/down/right/left pressed),
        // click down (buttonA.pressed), click up and finger is still
        // down (up/down/right/left pressed), then finger goes up. Need to also eat the
        // finger still down.
        BOOL pressed = gamepad.buttonA.pressed ||
                       gamepad.dpad.up.pressed ||
                       gamepad.dpad.down.pressed ||
                       gamepad.dpad.left.pressed ||
                       gamepad.dpad.right.pressed;


        if (pressed && siriRemoteInfo.state != SiriRemoteIdle)
          [weakSelf updateRemoteMovedInfo:siriRemoteInfo withGamePad:gamepad];

        switch(siriRemoteInfo.state)
        {
          case SiriRemoteIdle:
            if (siriRemoteInfo.debug)
              NSLog(@"microGamepad: idle, pressed(%d), dt(%f)", pressed, siriRemoteInfo.dt);
            [weakSelf stopTapRepeatTimer];
            siriRemoteInfo.focusAnimate = FocusEngineAnimate();
            siriRemoteInfo.focusAnimate.zoomX = 105.0f;
            siriRemoteInfo.focusAnimate.zoomY = 105.0f;
            CFocusEngineHandler::GetInstance().ClearAnimation();
            CFocusEngineHandler::GetInstance().UpdateAnimation(siriRemoteInfo.focusAnimate);
            if (pressed)
            {
              [weakSelf updateRemoteStartInfo:siriRemoteInfo withGamePad:gamepad];
              // absolute coordinate system is 0 to +2 with left/bottom = (0,0)
              // transform coordinates to left/bottom = (-1, -1) to make checks easy
              CGPoint centerStart = CGPointMake(
                siriRemoteInfo.startPoint.x - 1.0,
                siriRemoteInfo.startPoint.y - 1.0);
              if (fabs(centerStart.x) < siriRemoteInfo.tapbounts && fabs(centerStart.y) < siriRemoteInfo.tapbounts)
              {
                // tap in center, ignore it.
                if (siriRemoteInfo.debug)
                  NSLog(@"microGamepad: tap center ignored");
              }
              else
              {
                // fire off tap hold/repeat timer
                [weakSelf startTapRepeatTimer:siriRemoteInfo  withPoint:siriRemoteInfo.startPoint withdelay:0.75];
              }
              siriRemoteInfo.state = SiriRemoteTapTimer;
            }
            break;
          case SiriRemoteSelect:
            // Selects are handled by gesture handler, eat these
            break;
          case SiriRemoteTapTimer:
            if (siriRemoteInfo.debug)
            {
              NSLog(@"microGamepad: tap timer, pressed(%d), dt(%f), dx(%f), dy(%f)",
                pressed, siriRemoteInfo.dt, siriRemoteInfo.dx, siriRemoteInfo.dy);
            }
            // finger moved from initial start position (value changed fired)
            if (pressed)
            {
              // check if we are moving and outside tap bounds
              if (siriRemoteInfo.dx > siriRemoteInfo.tapbounts || siriRemoteInfo.dy > siriRemoteInfo.tapbounts)
              {
                // cancel tap hold/repeat timer
                [weakSelf stopTapRepeatTimer];
                // could be pan or start of swipe
                // we can not tell which yet so update moved info and move to pan/swipe state
                siriRemoteInfo.state = SiriRemotePanSwipe;
              }
            }
            else
            {
              if (siriRemoteInfo.dx < siriRemoteInfo.tapbounts && siriRemoteInfo.dy < siriRemoteInfo.tapbounts)
              {
                // if we did not move at all, dt will not get updated and will be
                // zero on touch release, so use guardSeconds to calc tap duration.
                // if tap duration is longer than limit, ignore it
                float tapSeconds = CFAbsoluteTimeGetCurrent() - siriRemoteInfo.startSeconds;
                if (tapSeconds < 1.0f)
                  [weakSelf processTapEvent:siriRemoteInfo];
                else if (![weakSelf isTapRepeatTimerActive])
                {
                  // if pressAutoRepeatTimer is not alive, it got invalidated
                  // during call to startTapRepeatTimer in touch pressed
                  // from a match to [self getFocusedOrientation], i.e
                  // tried to autorepeat in a non valid direction. Normally
                  // we just ignore taps if tap duration is longer than limit,
                  // but in this case, permit a tap.
                  [weakSelf processTapEvent:siriRemoteInfo];
                }
              }
            }
            break;
          case SiriRemotePanSwipe:
            if (siriRemoteInfo.debug)
            {
              NSLog(@"microGamepad: pan or swipe?, pressed(%d), dt(%f), dx(%f), dy(%f)",
                pressed, siriRemoteInfo.dt, siriRemoteInfo.dx, siriRemoteInfo.dy);
            }
            if (pressed)
            {
              // swipes are complete by 0.5 seconds, so this must be a pan
              if (siriRemoteInfo.dt >= 0.50f)
              {
                [weakSelf processPanEvent:siriRemoteInfo];
                siriRemoteInfo.state = SiriRemotePan;
              }
            }
            else
            {
              // check if this looks like a swipe, which have a min distance then
              // the finger is down. Also swipes that take longer than 1 second are ignored.
              // should not need the time check, if touch is down longer than 0.5 seconds, it is a pan.
              if (siriRemoteInfo.dt <= 1.0f &&
                 (siriRemoteInfo.dx >= siriRemoteInfo.tapbounts || siriRemoteInfo.dy >= siriRemoteInfo.tapbounts))
              {
                // swipe
                // vx, xy range is typically 0 to 12+
                // swiping down is typically a slower operation so
                // give it a little more velocity help
                float vx = (siriRemoteInfo.movedPoint.x - siriRemoteInfo.startPoint.x) / siriRemoteInfo.dt;
                float vy = (siriRemoteInfo.movedPoint.y - siriRemoteInfo.startPoint.y) / siriRemoteInfo.dt;
                int vcycles = fmaxf(fabsf(vx) / 4.0f, fabsf(vy) / 2.5f);
                // a swipe should generate repeats
                // if swipe length is long and/or the swipe was fast
                int cycles = vcycles + (fmaxf(fabsf(siriRemoteInfo.dx), fabsf(siriRemoteInfo.dy)) / 0.9f);
                if (cycles <= 0) cycles = 1;
                if (siriRemoteInfo.debug)
                  NSLog(@"microGamepad: dt(%f), vcycles(%d), cycles(%d)", siriRemoteInfo.dt, vcycles, cycles);

                [weakSelf processSwipeEvent:siriRemoteInfo withCycles:cycles];
                siriRemoteInfo.ignoreAfterSwipeSeconds = 0.25;
              }
              else
              {
                if (siriRemoteInfo.debug)
                  NSLog(@"microGamepad: should never get here, pressed(%d)", pressed);
              }
            }
            break;
          case SiriRemotePan:
            if (siriRemoteInfo.debug)
            {
              NSLog(@"microGamepad: pan, pressed(%d), dt(%f), dx(%f), dy(%f)",
                pressed, siriRemoteInfo.dt, siriRemoteInfo.dx, siriRemoteInfo.dy);
            }
            // pans only happen when pressed, we could (in future) add auto-scroll if
            // pan is pinned to same panningRect or some other check.
            if (pressed)
              [weakSelf processPanEvent:siriRemoteInfo];
            break;
        }

        // if not pressed, we are done and will not
        // get called again until the next touch changed event
        if (!pressed)
        {
          // start remote idle timer
          [weakSelf startRemoteTimer];
          // always cancel tap repeat timer
          [weakSelf stopTapRepeatTimer];
          CFocusEngineHandler::GetInstance().ClearAnimation();
          // always return to SiriRemoteIdle2
          siriRemoteInfo.state = SiriRemoteIdle;
          if (siriRemoteInfo.debug)
            NSLog(@"microGamepad: idle");
        }
      };
#endif
    }
    if (siriRemoteInfo.debug)
      NSLog(@"microGamepad: did connect");
  }
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
  [self createTapGestureRecognizers];

  // for IR remotes
  [self createPressGesturecognizers];
  [self createCustomControlCenter];
  
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(cgControllerDidConnect:) name:GCControllerDidConnectNotification object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(cgControllerDidDisconnect:) name:GCControllerDidDisconnectNotification object:nil];
  [GCController startWirelessControllerDiscoveryWithCompletionHandler:nil];
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
  [self becomeFirstResponder];
  [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];
  [GCController startWirelessControllerDiscoveryWithCompletionHandler:nil];
}
//--------------------------------------------------------------
- (void)viewWillDisappear:(BOOL)animated
{  
  [self pauseAnimation];
  [super viewWillDisappear:animated];
  [[NSNotificationCenter defaultCenter] removeObserver:self name:GCControllerDidConnectNotification object:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:self name:GCControllerDidDisconnectNotification object:nil];  
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
#if defined(APP_PACKAGE_LITE)
    NSURL *url = [NSURL URLWithString:@"mrmclite://wakeup"];
#else
    NSURL *url = [NSURL URLWithString:@"mrmc://wakeup"];
#endif
    [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
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
        [self sendButtonPressed:SiriRemote_IR_Play];
        break;
      case UIEventSubtypeRemoteControlPause:
        // check if not in background, we can get this if sleep is forced
        if (m_controllerState < MC_BACKGROUND)
          [self sendButtonPressed:SiriRemote_IR_Pause];
        break;
      case UIEventSubtypeRemoteControlStop:
        [self sendButtonPressed:SiriRemote_IR_Stop];
        break;
      case UIEventSubtypeRemoteControlNextTrack:
        [self sendButtonPressed:SiriRemote_IR_NextTrack];
        break;
      case UIEventSubtypeRemoteControlPreviousTrack:
        [self sendButtonPressed:SiriRemote_IR_PreviousTrack];
        break;
      case UIEventSubtypeRemoteControlBeginSeekingForward:
        // use 4X speed forward.
        [self sendButtonPressed:SiriRemote_IR_FastForward];
        [self sendButtonPressed:SiriRemote_IR_FastForward];
        break;
      case UIEventSubtypeRemoteControlBeginSeekingBackward:
        // use 4X speed rewind.
        [self sendButtonPressed:SiriRemote_IR_Rewind];
        [self sendButtonPressed:SiriRemote_IR_Rewind];
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
    MPMediaItemArtwork *mArt = [[MPMediaItemArtwork alloc] initWithBoundsSize:image.size requestHandler:^UIImage * _Nonnull(CGSize size) { return image;}];
    if (mArt)
    {
      [dict setObject:mArt forKey:MPMediaItemPropertyArtwork];
    }
  }
  
  if (NSClassFromString(@"MPNowPlayingInfoCenter"))
  {
    NSNumber *elapsed = [item objectForKey:@"elapsed"];
    if (elapsed)
      [dict setObject:elapsed forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    NSNumber *speed = [item objectForKey:@"speed"];
    if (speed)
      [dict setObject:speed forKey:MPNowPlayingInfoPropertyPlaybackRate];
    NSNumber *current = [item objectForKey:@"current"];
    if (current)
      [dict setObject:current forKey:MPNowPlayingInfoPropertyPlaybackQueueIndex];
    NSNumber *total = [item objectForKey:@"total"];
    if (total)
      [dict setObject:total forKey:MPNowPlayingInfoPropertyPlaybackQueueCount];
  }
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

  if (![[[NSBundle mainBundle] bundleIdentifier] hasPrefix:[NSString stringWithUTF8String:CCompileInfo::GetPackage()]])
      CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
}
//--------------------------------------------------------------
- (void)onSpeedChanged:(NSDictionary *)item
{
  PRINT_SIGNATURE();
  if (NSClassFromString(@"MPNowPlayingInfoCenter"))
  {
    NSMutableDictionary *info = [self.m_nowPlayingInfo mutableCopy];
    NSNumber *elapsed = [item objectForKey:@"elapsed"];
    if (elapsed)
      [info setObject:elapsed forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    NSNumber *speed = [item objectForKey:@"speed"];
    if (speed)
      [info setObject:speed forKey:MPNowPlayingInfoPropertyPlaybackRate];

    [self setIOSNowPlayingInfo:info];
  }
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
  if (NSClassFromString(@"MPNowPlayingInfoCenter"))
  {
    NSMutableDictionary *info = [self.m_nowPlayingInfo mutableCopy];
    NSNumber *speed = [NSNumber numberWithDouble:0.000001f];
    if (speed)
      [info setObject:speed forKey:MPNowPlayingInfoPropertyDefaultPlaybackRate];
    NSNumber *elapsed = [NSNumber numberWithDouble:g_application.GetTime()];
    if (elapsed)
      [info setObject:elapsed forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    [self setIOSNowPlayingInfo:info];
  }
}
//--------------------------------------------------------------
- (void)onStopPlaying:(NSDictionary *)item
{
  PRINT_SIGNATURE();
  [self setIOSNowPlayingInfo:nil];
}

#pragma mark - control center

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

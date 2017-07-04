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
#import "CompileInfo.h"
#import "FileItem.h"
#import "MusicInfoTag.h"
#import "SpecialProtocol.h"
#import "PlayList.h"
#import "TextureCache.h"

#import "cores/AudioEngine/AEFactory.h"
#import "input/Key.h"
#import "input/touch/generic/GenericTouchActionHandler.h"
#import "messaging/ApplicationMessenger.h"
#import "network/NetworkServices.h"
#import "platform/darwin/DarwinUtils.h"
#import "platform/darwin/NSLogDebugHelpers.h"
#import "platform/darwin/ios/IOSEAGLView.h"
#import "platform/darwin/ios/XBMCController.h"
#import "platform/darwin/ios/IOSScreenManager.h"
#import "platform/darwin/ios-common/AnnounceReceiver.h"
#import "platform/MCRuntimeLib.h"
#import "platform/MCRuntimeLibContext.h"
#import "windowing/WindowingFactory.h"
#import "utils/SeekHandler.h"

#import <MediaPlayer/MPMediaItem.h>
#import <MediaPlayer/MPNowPlayingInfoCenter.h>

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795028842
#endif
#define RADIANS_TO_DEGREES(radians) ((radians) * (180.0 / M_PI))

using namespace KODI::MESSAGING;

XBMCController *g_xbmcController;
//--------------------------------------------------------------
//

@interface XBMCController ()
- (void)rescheduleNetworkAutoSuspend;
@end

@implementation XBMCController
@synthesize m_lastGesturePoint;
@synthesize m_screenScale;
@synthesize m_touchBeginSignaled;
@synthesize m_screenIdx;
@synthesize m_screensize;
@synthesize m_networkAutoSuspendTimer;
@synthesize m_nowPlayingInfo;

#pragma mark - internal key press methods
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
- (void) sendKeypressEvent: (XBMC_Event) event
{
  event.type = XBMC_KEYDOWN;
  CWinEvents::MessagePush(&event);

  event.type = XBMC_KEYUP;
  CWinEvents::MessagePush(&event);
}

// START OF UIKeyInput protocol
- (BOOL)hasText
{
  return NO;
}

- (void)insertText:(NSString *)text
{
  // in case the native touch keyboard is active
  // don't do anything here
  // we are only supposed to be called when
  // using an external bt keyboard...
  if (m_nativeKeyboardActive)
    return;

  unichar currentKey = [text characterAtIndex:0];
  // handle return
  if (currentKey == '\n' || currentKey == '\r')
    return;
  // This was passed to us from keyboard, we do not want
  // to handle "return" from onscreen keyboard
  // keeping it here just in case something goes wrong
  //  currentKey = XBMCK_RETURN;
  
  XBMC_Event newEvent;
  memset(&newEvent, 0, sizeof(newEvent));

  // handle upper case letters
  if (currentKey >= 'A' && currentKey <= 'Z')
  {
    newEvent.key.keysym.mod = XBMCKMOD_LSHIFT;
    currentKey += 0x20;// convert to lower case
  }

  newEvent.key.keysym.sym = (XBMCKey)currentKey;
  newEvent.key.keysym.unicode = currentKey;

  [self sendKeypressEvent:newEvent];
}

- (void)deleteBackward
{
  [self sendKey:XBMCK_BACKSPACE];
}
// END OF UIKeyInput protocol

-(void)sendKey:(XBMCKey) key
{
  XBMC_Event newEvent;
  memset(&newEvent, 0, sizeof(newEvent));
  
  //newEvent.key.keysym.unicode = key;
  newEvent.key.keysym.sym = key;
  [self sendKeypressEvent:newEvent];
}

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - gesture methods
//--------------------------------------------------------------
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
  if ([gestureRecognizer isKindOfClass:[UIRotationGestureRecognizer class]] && [otherGestureRecognizer isKindOfClass:[UIPinchGestureRecognizer class]]) {
    return YES;
  }

  if ([gestureRecognizer isKindOfClass:[UISwipeGestureRecognizer class]] && [otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]]) {
    return YES;
  }

  return NO;
}

//--------------------------------------------------------------
- (void)createGestureRecognizers 
{
  //1 finger single tab
  UITapGestureRecognizer *singleFingerSingleTap = [[UITapGestureRecognizer alloc]
                                                   initWithTarget:self action:@selector(handleSingleFingerSingleTap:)];

  singleFingerSingleTap.delaysTouchesBegan = NO;
  singleFingerSingleTap.numberOfTapsRequired = 1;
  singleFingerSingleTap.numberOfTouchesRequired = 1;

  [m_glView addGestureRecognizer:singleFingerSingleTap];

  //2 finger single tab - right mouse
  //single finger double tab delays single finger single tab - so we
  //go for 2 fingers here - so single finger single tap is instant
  UITapGestureRecognizer *doubleFingerSingleTap = [[UITapGestureRecognizer alloc]
    initWithTarget:self action:@selector(handleDoubleFingerSingleTap:)];  

  doubleFingerSingleTap.delaysTouchesBegan = NO;
  doubleFingerSingleTap.numberOfTapsRequired = 1;
  doubleFingerSingleTap.numberOfTouchesRequired = 2;
  [m_glView addGestureRecognizer:doubleFingerSingleTap];

  //1 finger single long tab - right mouse - alernative
  UILongPressGestureRecognizer *singleFingerSingleLongTap = [[UILongPressGestureRecognizer alloc]
    initWithTarget:self action:@selector(handleSingleFingerSingleLongTap:)];  

  singleFingerSingleLongTap.delaysTouchesBegan = NO;
  singleFingerSingleLongTap.delaysTouchesEnded = NO;
  [m_glView addGestureRecognizer:singleFingerSingleLongTap];

  //double finger swipe left for backspace ... i like this fast backspace feature ;)
  UISwipeGestureRecognizer *swipeLeft2 = [[UISwipeGestureRecognizer alloc]
                                            initWithTarget:self action:@selector(handleSwipe:)];

  swipeLeft2.delaysTouchesBegan = NO;
  swipeLeft2.numberOfTouchesRequired = 2;
  swipeLeft2.direction = UISwipeGestureRecognizerDirectionLeft;
  swipeLeft2.delegate = self;
  [m_glView addGestureRecognizer:swipeLeft2];

  //single finger swipe left
  UISwipeGestureRecognizer *swipeLeft = [[UISwipeGestureRecognizer alloc]
                                          initWithTarget:self action:@selector(handleSwipe:)];

  swipeLeft.delaysTouchesBegan = NO;
  swipeLeft.numberOfTouchesRequired = 1;
  swipeLeft.direction = UISwipeGestureRecognizerDirectionLeft;
  swipeLeft.delegate = self;
  [m_glView addGestureRecognizer:swipeLeft];
  
  //single finger swipe right
  UISwipeGestureRecognizer *swipeRight = [[UISwipeGestureRecognizer alloc]
                                         initWithTarget:self action:@selector(handleSwipe:)];
  
  swipeRight.delaysTouchesBegan = NO;
  swipeRight.numberOfTouchesRequired = 1;
  swipeRight.direction = UISwipeGestureRecognizerDirectionRight;
  swipeRight.delegate = self;
  [m_glView addGestureRecognizer:swipeRight];
  
  //single finger swipe up
  UISwipeGestureRecognizer *swipeUp = [[UISwipeGestureRecognizer alloc]
                                         initWithTarget:self action:@selector(handleSwipe:)];
  
  swipeUp.delaysTouchesBegan = NO;
  swipeUp.numberOfTouchesRequired = 1;
  swipeUp.direction = UISwipeGestureRecognizerDirectionUp;
  swipeUp.delegate = self;
  [m_glView addGestureRecognizer:swipeUp];

  //single finger swipe down
  UISwipeGestureRecognizer *swipeDown = [[UISwipeGestureRecognizer alloc]
                                         initWithTarget:self action:@selector(handleSwipe:)];
  
  swipeDown.delaysTouchesBegan = NO;
  swipeDown.numberOfTouchesRequired = 1;
  swipeDown.direction = UISwipeGestureRecognizerDirectionDown;
  swipeDown.delegate = self;
  [m_glView addGestureRecognizer:swipeDown];
  
  //for pan gestures with one finger
  UIPanGestureRecognizer *pan = [[UIPanGestureRecognizer alloc]
    initWithTarget:self action:@selector(handlePan:)];

  pan.delaysTouchesBegan = NO;
  pan.maximumNumberOfTouches = 1;
  [m_glView addGestureRecognizer:pan];

  //for zoom gesture
  UIPinchGestureRecognizer *pinch = [[UIPinchGestureRecognizer alloc]
    initWithTarget:self action:@selector(handlePinch:)];

  pinch.delaysTouchesBegan = NO;
  pinch.delegate = self;
  [m_glView addGestureRecognizer:pinch];

  //for rotate gesture
  UIRotationGestureRecognizer *rotate = [[UIRotationGestureRecognizer alloc]
                                         initWithTarget:self action:@selector(handleRotate:)];

  rotate.delaysTouchesBegan = NO;
  rotate.delegate = self;
  [m_glView addGestureRecognizer:rotate];
}
//--------------------------------------------------------------
- (void) activateKeyboard:(UIView *)view
{
  [self.view addSubview:view];
  m_glView.userInteractionEnabled = NO;
}
//--------------------------------------------------------------
- (void) deactivateKeyboard:(UIView *)view
{
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
-(void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
  if (m_appAlive && m_glView)//NO GESTURES BEFORE WE ARE UP AND RUNNING
  {
    UITouch *touch = (UITouch *)[[touches allObjects] objectAtIndex:0];
    CGPoint point = [touch locationInView:m_glView];
    point.x *= m_screenScale;
    point.y *= m_screenScale;
    CGenericTouchActionHandler::GetInstance().OnSingleTouchStart(point.x, point.y);
  }
}
//--------------------------------------------------------------
-(void)handlePinch:(UIPinchGestureRecognizer*)sender
{
  if (m_appAlive && m_glView && sender.numberOfTouches)//NO GESTURES BEFORE WE ARE UP AND RUNNING
  {
    CGPoint point = [sender locationOfTouch:0 inView:m_glView];  
    point.x *= m_screenScale;
    point.y *= m_screenScale;

    switch(sender.state)
    {
      case UIGestureRecognizerStateBegan:
        CGenericTouchActionHandler::GetInstance().OnTouchGestureStart(point.x, point.y);
        break;
      case UIGestureRecognizerStateChanged:
        CGenericTouchActionHandler::GetInstance().OnZoomPinch(point.x, point.y, [sender scale]);
        break;
      case UIGestureRecognizerStateEnded:
      case UIGestureRecognizerStateCancelled:
        CGenericTouchActionHandler::GetInstance().OnTouchGestureEnd(point.x, point.y, 0, 0, 0, 0);
        break;
      default:
        break;
    }
  }
}
//--------------------------------------------------------------
-(void)handleRotate:(UIRotationGestureRecognizer*)sender
{
  if (m_appAlive && m_glView && sender.numberOfTouches)//NO GESTURES BEFORE WE ARE UP AND RUNNING
  {
    CGPoint point = [sender locationOfTouch:0 inView:m_glView];
    point.x *= m_screenScale;
    point.y *= m_screenScale;

    switch(sender.state)
    {
      case UIGestureRecognizerStateBegan:
        CGenericTouchActionHandler::GetInstance().OnTouchGestureStart(point.x, point.y);
        break;
      case UIGestureRecognizerStateChanged:
        CGenericTouchActionHandler::GetInstance().OnRotate(point.x, point.y, RADIANS_TO_DEGREES([sender rotation]));
        break;
      case UIGestureRecognizerStateEnded:
        CGenericTouchActionHandler::GetInstance().OnTouchGestureEnd(point.x, point.y, 0, 0, 0, 0);
        break;
      default:
        break;
    }
  }
}
//--------------------------------------------------------------
- (IBAction)handlePan:(UIPanGestureRecognizer *)sender 
{
  if (m_appAlive && m_glView)//NO GESTURES BEFORE WE ARE UP AND RUNNING
  { 
    CGPoint velocity = [sender velocityInView:m_glView];

    if ([sender state] == UIGestureRecognizerStateBegan && sender.numberOfTouches)
    {
      CGPoint point = [sender locationOfTouch:0 inView:m_glView];
      point.x *= m_screenScale;
      point.y *= m_screenScale;
      m_touchBeginSignaled = false;
      m_lastGesturePoint = point;
    }

    if ([sender state] == UIGestureRecognizerStateChanged && sender.numberOfTouches)
    {
      CGPoint point = [sender locationOfTouch:0 inView:m_glView];
      point.x *= m_screenScale;
      point.y *= m_screenScale;
      bool bNotify = false;
      CGFloat yMovement=point.y - m_lastGesturePoint.y;
      CGFloat xMovement=point.x - m_lastGesturePoint.x;
      
      if (xMovement)
        bNotify = true;
      
      if (yMovement)
        bNotify = true;
      
      if (bNotify)
      {
        if (!m_touchBeginSignaled)
        {
          CGenericTouchActionHandler::GetInstance().OnTouchGestureStart((float)point.x, (float)point.y);
          m_touchBeginSignaled = true;
        }

        CGenericTouchActionHandler::GetInstance().OnTouchGesturePan((float)point.x, (float)point.y,
                                                            (float)xMovement, (float)yMovement, 
                                                            (float)velocity.x, (float)velocity.y);
        m_lastGesturePoint = point;
      }
    }
    
    if (m_touchBeginSignaled && ([sender state] == UIGestureRecognizerStateEnded || [sender state] == UIGestureRecognizerStateCancelled))
    {
      //signal end of pan - this will start inertial scrolling with deacceleration in CApplication
      CGenericTouchActionHandler::GetInstance().OnTouchGestureEnd((float)m_lastGesturePoint.x, (float)m_lastGesturePoint.y,
                                                             (float)0.0, (float)0.0, 
                                                             (float)velocity.x, (float)velocity.y);

      m_touchBeginSignaled = false;
    }
  }
}
//--------------------------------------------------------------
- (IBAction)handleSwipe:(UISwipeGestureRecognizer *)sender
{
  if (m_appAlive && m_glView && sender.numberOfTouches)//NO GESTURES BEFORE WE ARE UP AND RUNNING
  {
    if (sender.state == UIGestureRecognizerStateRecognized)
    {
      CGPoint point = [sender locationOfTouch:0 inView:m_glView];
      point.x *= m_screenScale;
      point.y *= m_screenScale;

      TouchMoveDirection direction = TouchMoveDirectionNone;
      switch ([sender direction])
      {
        case UISwipeGestureRecognizerDirectionRight:
          direction = TouchMoveDirectionRight;
          break;
        case UISwipeGestureRecognizerDirectionLeft:
          direction = TouchMoveDirectionLeft;
          break;
        case UISwipeGestureRecognizerDirectionUp:
          direction = TouchMoveDirectionUp;
          break;
        case UISwipeGestureRecognizerDirectionDown:
          direction = TouchMoveDirectionDown;
          break;
      }
      CGenericTouchActionHandler::GetInstance().OnSwipe(direction,
                                                0.0, 0.0,
                                                point.x, point.y, 0, 0,
                                                [sender numberOfTouches]);
    }
  }
}
//--------------------------------------------------------------
- (IBAction)handleSingleFingerSingleTap:(UIGestureRecognizer *)sender 
{
  //Allow the tap gesture during init
  //(for allowing the user to tap away any messagboxes during init)
  if (m_readyToRun && sender.numberOfTouches)
  {
    CGPoint point = [sender locationOfTouch:0 inView:m_glView];
    point.x *= m_screenScale;
    point.y *= m_screenScale;
    //NSLog(@"%s singleTap", __PRETTY_FUNCTION__);
    CGenericTouchActionHandler::GetInstance().OnTap((float)point.x, (float)point.y, [sender numberOfTouches]);
  }
}
//--------------------------------------------------------------
- (IBAction)handleDoubleFingerSingleTap:(UIGestureRecognizer *)sender
{
  if (m_appAlive && m_glView && sender.numberOfTouches)//NO GESTURES BEFORE WE ARE UP AND RUNNING
  {
    CGPoint point = [sender locationOfTouch:0 inView:m_glView];
    point.x *= m_screenScale;
    point.y *= m_screenScale;
    //NSLog(@"%s toubleTap", __PRETTY_FUNCTION__);
    CGenericTouchActionHandler::GetInstance().OnTap((float)point.x, (float)point.y, [sender numberOfTouches]);
  }
}
//--------------------------------------------------------------
- (IBAction)handleSingleFingerSingleLongTap:(UIGestureRecognizer *)sender
{
  if (m_appAlive && m_glView && sender.numberOfTouches)//NO GESTURES BEFORE WE ARE UP AND RUNNING
  {
    CGPoint point = [sender locationOfTouch:0 inView:m_glView];
    point.x *= m_screenScale;
    point.y *= m_screenScale;

    if (sender.state == UIGestureRecognizerStateBegan)
    {
      m_lastGesturePoint = point;
      // mark the control
      //CGenericTouchActionHandler::GetInstance().OnSingleTouchStart((float)point.x, (float)point.y);
    }

    if (sender.state == UIGestureRecognizerStateEnded)
    {
      CGenericTouchActionHandler::GetInstance().OnSingleTouchMove((float)point.x, (float)point.y, point.x - m_lastGesturePoint.x, point.y - m_lastGesturePoint.y, 0, 0);
    }
    
    if (sender.state == UIGestureRecognizerStateEnded)
    {	
      CGenericTouchActionHandler::GetInstance().OnLongPress((float)point.x, (float)point.y);
    }
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

//--------------------------------------------------------------
- (id)initWithFrame:(CGRect)frame withScreen:(UIScreen *)screen
{ 
  m_screenIdx = 0;
  self = [super init];
  if (!self)
    return nil;

  m_pause = FALSE;
  m_glView = NULL;
  m_appAlive = FALSE;
  m_animating = FALSE;
  m_readyToRun = FALSE;

  m_isPlayingBeforeInactive = FALSE;
  m_bgTask = UIBackgroundTaskInvalid;
  m_playbackState = IOS_PLAYBACK_STOPPED;

  m_window = [[UIWindow alloc] initWithFrame:frame];
  [m_window setRootViewController:self];  
  m_window.screen = screen;
  m_window.backgroundColor = [UIColor blackColor];
  // Turn off autoresizing
  m_window.autoresizingMask = 0;
  m_window.autoresizesSubviews = NO;
  
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center addObserver: self
     selector: @selector(observeDefaultCenterStuff:) name: nil object: nil];

  [m_window makeKeyAndVisible];
  g_xbmcController = self;  
  
  CAnnounceReceiver::GetInstance().Initialize();

  return self;
}
//--------------------------------------------------------------
- (void)loadView
{
  [super loadView];
  if (CDarwinUtils::GetIOSVersion() >= 8.0)
  {
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view.autoresizesSubviews = YES;
  
    m_glView = [[IOSEAGLView alloc] initWithFrame:self.view.bounds withScreen:[UIScreen mainScreen]];
    [[IOSScreenManager sharedInstance] setView:m_glView];
    [m_glView setMultipleTouchEnabled:YES];
  
    /* Check if screen is Retina */
    m_screenScale = [[UIScreen mainScreen] nativeScale];
  
    m_glView.opaque = NO;
    m_glView.backgroundColor = [UIColor clearColor];
    [self.view addSubview: m_glView];

    [self.view setNeedsDisplay];

    [self createGestureRecognizers];
  }
}
//--------------------------------------------------------------
-(void)viewDidLoad
{
  [super viewDidLoad];
}
//--------------------------------------------------------------
- (void)dealloc
{
  // stop background task
  [m_networkAutoSuspendTimer invalidate];
  [self enableNetworkAutoSuspend:nil];

  CAnnounceReceiver::GetInstance().DeInitialize();

  [self stopAnimation];
  m_glView = nil;
  m_window = nil;

  NSNotificationCenter *center;
  // take us off the default center for our app
  center = [NSNotificationCenter defaultCenter];
  [center removeObserver: self];
}
//--------------------------------------------------------------
- (void)viewWillAppear:(BOOL)animated
{
  // move this later into CocoaPowerSyscall
  [[UIApplication sharedApplication] setIdleTimerDisabled:YES];
  
  [self resumeAnimation];
  [super viewWillAppear:animated];
}
//--------------------------------------------------------------
-(void) viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  [self becomeFirstResponder];
  [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];
}
//--------------------------------------------------------------
- (void)viewWillDisappear:(BOOL)animated
{  
  [self pauseAnimation];
  [super viewWillDisappear:animated];
}
//--------------------------------------------------------------
-(UIView *)inputView
{
  // override our input view to an empty view
  // this prevents the on screen keyboard
  // which would be shown whenever this UIResponder
  // becomes the first responder (which is always the case!)
  // caused by implementing the UIKeyInput protocol
  return [[UIView alloc] initWithFrame:CGRectZero];
}
//--------------------------------------------------------------
- (BOOL) canBecomeFirstResponder
{
  return YES;
}
//--------------------------------------------------------------
- (void)viewDidUnload
{
  [[UIApplication sharedApplication] endReceivingRemoteControlEvents];
  [self resignFirstResponder];

	[super viewDidUnload];	
}
//--------------------------------------------------------------
- (void) setFramebuffer
{
  if (!m_pause)
    [m_glView setFramebuffer];
}
//--------------------------------------------------------------
- (bool) presentFramebuffer
{
  if (!m_pause)
    return [m_glView presentFramebuffer];
  else
    return FALSE;
}
//--------------------------------------------------------------
- (CGSize) getScreenSize
{
  m_screensize.width  = m_glView.bounds.size.width * m_screenScale;
  m_screensize.height = m_glView.bounds.size.height * m_screenScale;
  return m_screensize;
}
//--------------------------------------------------------------
- (CGFloat) getScreenScale:(UIScreen *)screen;
{
  return [m_glView getScreenScale:screen];
}
//--------------------------------------------------------------
//--------------------------------------------------------------
- (BOOL) recreateOnReselect
{ 
  return YES;
}
//--------------------------------------------------------------
- (void)didReceiveMemoryWarning
{
  // Releases the view if it doesn't have a superview.
  [super didReceiveMemoryWarning];
  // Release any cached data, images, etc. that aren't in use.
}
//--------------------------------------------------------------
- (void) disableSystemSleep
{
}
//--------------------------------------------------------------
- (void) enableSystemSleep
{
}
//--------------------------------------------------------------
- (void)disableScreenSaver
{
  if ([UIApplication sharedApplication].idleTimerDisabled == NO)
    [[UIApplication sharedApplication] setIdleTimerDisabled:YES];
}
//--------------------------------------------------------------
- (void)enableScreenSaver
{
  if ([UIApplication sharedApplication].idleTimerDisabled == YES)
    [[UIApplication sharedApplication] setIdleTimerDisabled:NO];
}
//--------------------------------------------------------------
- (bool)resetSystemIdleTimer
{
  return false;
}
//--------------------------------------------------------------
- (UIScreenMode*) preferredScreenMode:(UIScreen*) screen
{
  // present because preferredMode is prohibited under tvOS
  // and we factor it out from WinSystemIOS.mm
  return [screen preferredMode];
}
//--------------------------------------------------------------
- (NSArray<UIScreenMode *> *) availableScreenModes:(UIScreen*) screen
{
  // present because availableModes is prohibited under tvOS
  // and we factor it out from WinSystemIOS.mm
  return [screen availableModes];
}
//--------------------------------------------------------------
- (bool) changeScreen: (unsigned int)screenIdx withMode:(UIScreenMode *)mode
{
  bool ret = [[IOSScreenManager sharedInstance] changeScreen:screenIdx withMode:mode];
  return ret;
}
//--------------------------------------------------------------
- (void) activateScreen: (UIScreen *)screen  withOrientation:(UIInterfaceOrientation)newOrientation
{
  // Since ios7 we have to handle the orientation manually
  // it differs by 90 degree between internal and external screen
  float   angle = 0;
  UIView *view = [m_window.subviews objectAtIndex:0];
  switch(newOrientation)
  {
    case UIInterfaceOrientationUnknown:
    case UIInterfaceOrientationPortrait:
      angle = 0;
      break;
    case UIInterfaceOrientationPortraitUpsideDown:
      angle = M_PI;
      break;
    case UIInterfaceOrientationLandscapeLeft:
      angle = -M_PI_2;
      break;
    case UIInterfaceOrientationLandscapeRight:
      angle = M_PI_2;
      break;
  }
  // reset the rotation of the view
  view.layer.transform = CATransform3DMakeRotation(angle, 0, 0.0, 1.0);
  view.layer.bounds = view.bounds;
  m_window.screen = screen;
  [view setFrame:m_window.frame];
}
//--------------------------------------------------------------
- (void)enterBackground
{
  if (g_application.m_pPlayer->IsPlaying() && !g_application.m_pPlayer->IsPaused())
  {
    m_isPlayingBeforeInactive = TRUE;
    CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_PAUSE_IF_PLAYING);
  }
  g_Windowing.OnAppFocusChange(false);
}

- (void)enterForeground
{
  g_Windowing.OnAppFocusChange(true);
  // when we come back, restore playing if we were.
  if (m_isPlayingBeforeInactive)
  {
    CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_UNPAUSE);
    m_isPlayingBeforeInactive = FALSE;
  }
  CNetworkServices::GetInstance().StartPlexServices();
}

- (void)becomeInactive
{
  // if we were interrupted, already paused here
  // else if user background us or lock screen, only pause video here, audio keep playing.
  if (g_application.m_pPlayer->IsPlayingVideo() &&
     !g_application.m_pPlayer->IsPaused())
  {
    m_isPlayingBeforeInactive = TRUE;
    CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_PAUSE_IF_PLAYING);
  }
  // check whether we need disable network auto suspend.
  [self rescheduleNetworkAutoSuspend];
  CNetworkServices::GetInstance().StopPlexServices();
}

//--------------------------------------------------------------
- (void)audioRouteChanged
{
  if (MCRuntimeLib_Initialized())
    CAEFactory::DeviceChange();
}

- (NSArray *)keyCommands
{
  UIKeyCommand *upArrow = [UIKeyCommand keyCommandWithInput: UIKeyInputUpArrow modifierFlags: 0 action: @selector(handleKeyCursors:)];
  UIKeyCommand *dnArrow = [UIKeyCommand keyCommandWithInput: UIKeyInputDownArrow modifierFlags: 0 action: @selector(handleKeyCursors:)];
  UIKeyCommand *ltArrow = [UIKeyCommand keyCommandWithInput: UIKeyInputLeftArrow modifierFlags: 0 action: @selector(handleKeyCursors:)];
  UIKeyCommand *rtArrow = [UIKeyCommand keyCommandWithInput: UIKeyInputRightArrow modifierFlags: 0 action: @selector(handleKeyCursors:)];

  return [[NSArray alloc] initWithObjects: upArrow, dnArrow, ltArrow, rtArrow, nil];
}

- (void)handleKeyCursors:(UIKeyCommand *)keyCommand
{
  NSString *input = keyCommand.input;

  XBMCKey key = XBMCK_UNKNOWN;
  if (input == UIKeyInputUpArrow)
    key = XBMCK_RIGHT;
  else if (input == UIKeyInputDownArrow)
    key = XBMCK_DOWN;
  else if (input == UIKeyInputLeftArrow)
    key = XBMCK_LEFT;
  else if (input == UIKeyInputRightArrow)
    key = XBMCK_DOWN;
  else
  {
    //LOG(@"%s: tmp key unsupported :(", __PRETTY_FUNCTION__);
    return; // not supported by us - return...
  }

  [self sendKey:key];
}


#pragma mark - MCRuntimeLib routines
//--------------------------------------------------------------
- (void)pauseAnimation
{
  m_pause = TRUE;
  MCRuntimeLib_SetRenderGUI(false);
}
//--------------------------------------------------------------
- (void)resumeAnimation
{
  m_pause = FALSE;
  MCRuntimeLib_SetRenderGUI(true);
}
//--------------------------------------------------------------
- (void)startAnimation
{
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

  m_readyToRun = TRUE;
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

#pragma mark - remote control routines
//--------------------------------------------------------------
- (void)disableNetworkAutoSuspend
{
  if (m_bgTask != UIBackgroundTaskInvalid)
  {
    [[UIApplication sharedApplication] endBackgroundTask: m_bgTask];
    m_bgTask = UIBackgroundTaskInvalid;
  }
  // we have to alloc the background task for keep network working after screen lock and dark.
  UIBackgroundTaskIdentifier newTask = [[UIApplication sharedApplication] beginBackgroundTaskWithExpirationHandler:nil];
  m_bgTask = newTask;

  if (m_networkAutoSuspendTimer)
  {
    [m_networkAutoSuspendTimer invalidate];
    self.m_networkAutoSuspendTimer = nil;
  }
}
//--------------------------------------------------------------
- (void)enableNetworkAutoSuspend:(id)obj
{
  if (m_bgTask != UIBackgroundTaskInvalid)
  {
    [[UIApplication sharedApplication] endBackgroundTask: m_bgTask];
    m_bgTask = UIBackgroundTaskInvalid;
  }
}

//--------------------------------------------------------------
- (void)remoteControlReceivedWithEvent:(UIEvent*)receivedEvent
{
  if (receivedEvent.type == UIEventTypeRemoteControl)
  {
    [self disableNetworkAutoSuspend];
    switch (receivedEvent.subtype)
    {
      case UIEventSubtypeRemoteControlTogglePlayPause:
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
        //LOG(@"unhandled subtype: %d", (int)receivedEvent.subtype);
        break;
    }
    // start remote timeout
    [self rescheduleNetworkAutoSuspend];
  }
}

#pragma mark - Now Playing routines
//--------------------------------------------------------------
- (void)setIOSNowPlayingInfo:(NSDictionary *)info
{
  self.m_nowPlayingInfo = info;
  [[MPNowPlayingInfoCenter defaultCenter] setNowPlayingInfo:self.m_nowPlayingInfo];
}
//--------------------------------------------------------------

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

  if (NSClassFromString(@"MPNowPlayingInfoCenter"))
  {
    UIImage *image = [item objectForKey:@"thumb"];
    if (image)
    {
      MPMediaItemArtwork *mArt = [[MPMediaItemArtwork alloc] initWithImage:image];
      if (mArt)
        [dict setObject:mArt forKey:MPMediaItemPropertyArtwork];
    }
    // these proprity keys are ios5+ only
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

  m_playbackState = IOS_PLAYBACK_PLAYING;
  [self disableNetworkAutoSuspend];

  if (![[[NSBundle mainBundle] bundleIdentifier] hasPrefix:[NSString stringWithUTF8String:CCompileInfo::GetPackage()]])
    CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
}
//--------------------------------------------------------------
- (void)onSpeedChanged:(NSDictionary *)item
{
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
//--------------------------------------------------------------
- (void)onPausePlaying:(NSDictionary *)item
{
  m_playbackState = IOS_PLAYBACK_PAUSED;
  // schedule set network auto suspend state for save power if idle.
  [self rescheduleNetworkAutoSuspend];
}
//--------------------------------------------------------------
- (void)onStopPlaying:(NSDictionary *)item
{
  [self setIOSNowPlayingInfo:nil];

  m_playbackState = IOS_PLAYBACK_STOPPED;
  // delay set network auto suspend state in case we are switching playing item.
  [self rescheduleNetworkAutoSuspend];
}
//--------------------------------------------------------------
- (void)onSeekPlaying
{
  PRINT_SIGNATURE();
  [NSThread detachNewThreadSelector:@selector(onSeekDelayed:) toTarget:self withObject:nil];
}
//--------------------------------------------------------------

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

- (void)rescheduleNetworkAutoSuspend
{
  //LOG(@"%s: playback state: %d", __PRETTY_FUNCTION__,  m_playbackState);
  if (m_playbackState == IOS_PLAYBACK_PLAYING)
  {
    [self disableNetworkAutoSuspend];
    return;
  }
  if (m_networkAutoSuspendTimer)
    [m_networkAutoSuspendTimer invalidate];

  int delay = m_playbackState == IOS_PLAYBACK_PAUSED ? 60 : 30;  // wait longer if paused than stopped
  self.m_networkAutoSuspendTimer = [NSTimer scheduledTimerWithTimeInterval:delay target:self selector:@selector(enableNetworkAutoSuspend:) userInfo:nil repeats:NO];
}

#pragma mark private helper methods
//
- (void)observeDefaultCenterStuff: (NSNotification *) notification
{
//  LOG(@"default: %@", [notification name]);
//  LOG(@"userInfo: %@", [notification userInfo]);
}
- (EAGLContext*) getEAGLContextObj
{
  return [m_glView getContext];
}
- (BOOL)shouldAutorotate
{
  UIInterfaceOrientation orient = [[UIApplication sharedApplication] statusBarOrientation];
  
  return (orient == UIInterfaceOrientationLandscapeLeft ||
          orient == UIInterfaceOrientationLandscapeRight);
}

-(NSUInteger)supportedInterfaceOrientations
{
  return UIInterfaceOrientationMaskLandscape;
}
@end

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
 *  Refactored. Copyright (C) 2015-2018 Team MrMC
 *  https://github.com/MrMC
 *
 */

#define dumpviewsonload 0

#import "system.h"

#import "Application.h"

#import "CompileInfo.h"
#import "GUIInfoManager.h"
#import "Util.h"
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
#import "platform/darwin/DarwinUtils.h"
#import "platform/darwin/FocusEngineHandler.h"
#import "platform/darwin/NSLogDebugHelpers.h"
#import "platform/darwin/tvos/MainEAGLView.h"
#import "platform/darwin/tvos/FocusLayerView.h"
#import "platform/darwin/tvos/FocusLayerViewPlayerProgress.h"
#import "platform/darwin/tvos/MainController.h"
#import "platform/darwin/tvos/MainApplication.h"
#import "platform/darwin/tvos/TVOSTopShelf.h"
#import "platform/darwin/tvos/MainKeyboardView.h"
#import "platform/darwin/ios-common/AnnounceReceiver.h"
#import "platform/MCRuntimeLib.h"
#import "platform/MCRuntimeLibContext.h"
#import "windowing/WindowingFactory.h"
#import "settings/Settings.h"
#import "services/lighteffects/LightEffectServices.h"
#import "utils/LiteUtils.h"
#import "utils/StringObfuscation.h"
#import "utils/SeekHandler.h"
#import "utils/URIUtils.h"
#import "utils/log.h"

#import <MediaPlayer/MPMediaItem.h>
#import <MediaPlayer/MediaPlayer.h>
#import <MediaPlayer/MPNowPlayingInfoCenter.h>
#import <GameController/GameController.h>

#if __TVOS_11_2
  #import <AVFoundation/AVDisplayCriteria.h>
  #import <AVKit/AVDisplayManager.h>
  #import <AVKit/UIWindow.h>

  @interface AVDisplayCriteria()
  @property(readonly) int videoDynamicRange;
  @property(readonly, nonatomic) float refreshRate;
  - (id)initWithRefreshRate:(float)arg1 videoDynamicRange:(int)arg2;
  @end
#else
  @interface AVDisplayCriteria : NSObject <NSCopying>
  @property(readonly) int videoDynamicRange;
  @property(readonly, nonatomic) float refreshRate;
  - (id)initWithRefreshRate:(float)arg1 videoDynamicRange:(int)arg2;
  @end

  @interface AVDisplayManager : NSObject
  @property(nonatomic, readonly, getter=isDisplayModeSwitchInProgress) BOOL displayModeSwitchInProgress;
  @property(nonatomic, copy) AVDisplayCriteria *preferredDisplayCriteria;
  @end

  @interface UIWindow (AVAdditions)
  @property(nonatomic, readonly) AVDisplayManager *avDisplayManager;
  @end
#endif

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
@property (nonatomic, strong) CADisplayLink *displayLink;
@property (nonatomic, assign) float displayRate;
@property (nonatomic, nullable) FocusLayerView *focusView;
@property (nonatomic, nullable) FocusLayerView *focusViewLeft;
@property (nonatomic, nullable) FocusLayerView *focusViewRight;
@property (nonatomic, nullable) FocusLayerView *focusViewTop;
@property (nonatomic, nullable) FocusLayerView *focusViewBottom;
@property (nonatomic, assign) FocusLayer focusLayer;
@property (strong, nonatomic) NSTimer *focusIdleTimer;
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
@synthesize m_focusIdleState;
@synthesize m_enableRemoteExpertMode;
@synthesize m_stopPlaybackOnMenu;
@synthesize m_clickDirection;

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - MainController methods
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

  m_clickDirection = CLICK_SELECT_OR_RELEASED;

  [self enableScreenSaver];

  [m_window makeKeyAndVisible];
  g_xbmcController = self;  

  CAnnounceReceiver::GetInstance().Initialize();

  // The AppleTV4K has a rock solid reported duration. The
  // AppleTV4 wanders and is quantized for display rate tracking.
  self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkTick:)];
  // we want the native cadence of the display hardware.
  self.displayLink.preferredFramesPerSecond = 0;
  [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];

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
  m_screenScale = [m_glView getScreenScale:[UIScreen mainScreen]];
  [self.view addSubview: m_glView];

  CGRect focusRect = CGRectMake(0, 0, m_glView.bounds.size.width, m_glView.bounds.size.height);
  // virtual views, these are outside focusView (display bounds)
  // and used to detect up/down/right/left focus movements for pop/slide out views
  // we trap/cancel the focus move in shouldUpdateFocusInContext
  // if the focused core control can do the move, it will and we will get a focus update
  // from core which we then use to adjust focus in didUpdateFocusInContext.
  // That's the theory anyway :)
  CGRect focusRectTop = focusRect;
  focusRectTop.origin.y -= 200;
  focusRectTop.size.height = 200;
  self.focusViewTop = [[FocusLayerView alloc] initWithFrame:focusRectTop];
  [self.focusViewTop setFocusable:true];
  [self.focusViewTop setViewVisible:false];

  CGRect focusRectLeft = focusRect;
  focusRectLeft.origin.x -= 200;
  focusRectLeft.size.width = 200;
  self.focusViewLeft = [[FocusLayerView alloc] initWithFrame:focusRectLeft];
  [self.focusViewLeft setFocusable:true];
  [self.focusViewLeft setViewVisible:false];

  CGRect focusRectRight = focusRect;
  focusRectRight.origin.x += focusRect.size.width;
  focusRectRight.size.width = 200;
  self.focusViewRight = [[FocusLayerView alloc] initWithFrame:focusRectRight];
  [self.focusViewRight setFocusable:true];
  [self.focusViewRight setViewVisible:false];

  CGRect focusRectBottom = focusRect;
  focusRectBottom.origin.y += focusRect.size.height;
  focusRectBottom.size.height = 200;
  self.focusViewBottom = [[FocusLayerView alloc] initWithFrame:focusRectBottom];
  [self.focusViewBottom setFocusable:true];
  [self.focusViewBottom setViewVisible:false];

  self.focusView = [[FocusLayerView alloc] initWithFrame:focusRect];
  [self.focusView setFocusable:true];
  [self.focusView setViewVisible:false];
  // focus layer lives above m_glView
  [self.view insertSubview:self.focusView aboveSubview:m_glView];

  [self.focusView addSubview:self.focusViewTop];
  [self.focusView addSubview:self.focusViewLeft];
  [self.focusView addSubview:self.focusViewRight];
  [self.focusView addSubview:self.focusViewBottom];
}
//--------------------------------------------------------------
- (void)viewDidLoad
{
  [super viewDidLoad];

  // safe time to update screensize, loadView is too early
  m_screensize.width  = m_glView.bounds.size.width  * m_screenScale;
  m_screensize.height = m_glView.bounds.size.height * m_screenScale;

  [self createSiriPressGesturecognizers];
  [self createSiriSwipeGestureRecognizers];
  [self createSiriPanGestureRecognizers];
  [self createSiriTapGestureRecognizers];
  [self createCustomControlCenter];
  [self initGameController];
  // startup with idle timer running
  [self startRemoteTimer];

  if (__builtin_available(tvOS 11.2, *))
  {
    if ([m_window respondsToSelector:@selector(avDisplayManager)])
    {
      auto avDisplayManager = [m_window avDisplayManager];
      [avDisplayManager addObserver:self forKeyPath:@"displayModeSwitchInProgress" options:NSKeyValueObservingOptionNew context:nullptr];
    }
  }
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
}
//--------------------------------------------------------------
- (void)viewWillDisappear:(BOOL)animated
{  
  [self pauseAnimation];
  [super viewWillDisappear:animated];

  if (__builtin_available(tvOS 11.2, *))
  {
    if ([m_window respondsToSelector:@selector(avDisplayManager)])
    {
      auto avDisplayManager = [m_window avDisplayManager];
      [avDisplayManager removeObserver:self forKeyPath:@"displayModeSwitchInProgress"];
    }
  }
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
  return m_screensize;
}

//--------------------------------------------------------------
- (void)didReceiveMemoryWarning
{
  CLog::Log(LOGDEBUG, "didReceiveMemoryWarning");
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
  if ([NSThread currentThread] != [NSThread mainThread])
  {
    dispatch_async(dispatch_get_main_queue(),^{
      m_disableIdleTimer = YES;
      [[UIApplication sharedApplication] setIdleTimerDisabled:YES];
      [self resetSystemIdleTimer];
    });
  }
  else
  {
    m_disableIdleTimer = YES;
    [[UIApplication sharedApplication] setIdleTimerDisabled:YES];
    [self resetSystemIdleTimer];
  }
}
//--------------------------------------------------------------
- (void)enableScreenSaver
{
  if ([NSThread currentThread] != [NSThread mainThread])
  {
    dispatch_async(dispatch_get_main_queue(),^{
      m_disableIdleTimer = NO;
      [[UIApplication sharedApplication] setIdleTimerDisabled:NO];
    });
  }
  else
  {
    m_disableIdleTimer = NO;
    [[UIApplication sharedApplication] setIdleTimerDisabled:NO];
  }
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
    if (CLiteUtils::IsLite())
      url = [NSURL URLWithString:@"mrmclite://wakeup"];
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
        CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_UNPAUSE);
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
    [self performSelectorOnMainThread:@selector(updateFocusLayer) withObject:nil  waitUntilDone:NO];
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
//--------------------------------------------------------------
#pragma mark - helper methods/routines
//--------------------------------------------------------------
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

- (void)enableRemoteExpertMode:(BOOL)enable
{
  m_enableRemoteExpertMode = enable;
  [self startRemoteTimer];
}

- (bool)hasPlayerProgressScrubbing
{
  if (m_enableRemoteExpertMode)
    return false;

  CGUIWindow *focusWindow = CFocusEngineHandler::GetInstance().GetFocusWindow();
  if (focusWindow && focusWindow->GetID() != WINDOW_FULLSCREEN_VIDEO)
    return false;

  if (g_application.m_pPlayer->IsPlayingVideo() && !g_application.m_pPlayer->CanSeek())
    return false;

  CFileItem &fileItem = g_application.CurrentFileItem();
  if (fileItem.IsLiveTV()
  ||  URIUtils::IsUPnP(fileItem.GetPath())
  ||  URIUtils::IsBluray(fileItem.GetPath())
  ||  fileItem.IsBDFile()
  ||  fileItem.IsDVD()
  ||  fileItem.IsDiscImage()
  ||  fileItem.IsDVDFile(false, true)
  ||  fileItem.IsDiscStub()
  ||  fileItem.IsPlayList())
    return false;

  return true;
}

- (void)stopPlaybackOnMenu:(BOOL)enable
{
  m_stopPlaybackOnMenu = enable;
  [self startRemoteTimer];
}

//--------------------------------------------------------------
- (void) activateKeyboard:(UIView *)view
{
  //PRINT_SIGNATURE();
  [self.focusView setFocusable:true];
  [self.focusView addSubview:view];
  self.focusView.userInteractionEnabled = NO;
}
//--------------------------------------------------------------
- (void) deactivateKeyboard:(UIView *)view
{
  //PRINT_SIGNATURE();
  // this will remove the native keyboad
  [[UIApplication sharedApplication] sendAction:@selector(resignFirstResponder) to:nil from:nil forEvent:nil];
  [view removeFromSuperview];
  // keep self.focusView focusable
  self.focusView.userInteractionEnabled = YES;
  [self becomeFirstResponder];
  [self setNeedsFocusUpdate];
  [self updateFocusIfNeeded];
}
//--------------------------------------------------------------
- (void) nativeKeyboardActive: (bool)active;
{
  //PRINT_SIGNATURE();
  m_nativeKeyboardActive = active;
}

//--------------------------------------------------------------
//--------------------------------------------------------------
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
//--------------------------------------------------------------
#pragma mark - AVDisplayLayer methods
//--------------------------------------------------------------
- (void) insertVideoView:(UIView*)view
{
  [self.view insertSubview:view belowSubview:m_glView];
  [self.view setNeedsDisplay];
}
//--------------------------------------------------------------
- (void) removeVideoView:(UIView*)view
{
  [view removeFromSuperview];
}

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - display switching routines
//--------------------------------------------------------------
- (float)getDisplayRate
{
  if (self.displayRate > 0.0f)
    return self.displayRate;

  return 60.0f;
}

//--------------------------------------------------------------
- (void)displayLinkTick:(CADisplayLink *)sender
{
  if (self.displayLink.duration > 0.0f)
  {
    static float oldDisplayRate = 0.0f;
    if (CDarwinUtils::IsAppleTV4KOrAbove())
    {
      // The AppleTV4K has a rock solid reported duration.
      // we want fps, not duration in seconds.
      self.displayRate = 1.0f / self.displayLink.duration;
    }
    else
    {
      // AppleTV4 wanders and we have to quantize to get it.
      // Since AppleTV4 cannot disaply rate switch, we only
      // need to support a small set of standard display rates.
      float displayFPS = 0.0f;
      int duration = 1000000.0f * self.displayLink.duration;
      switch(duration)
      {
        default:
          displayFPS = 0.0f;
          break;
        case 16000 ... 17000:
          // 59.940 (16683.333333)
          displayFPS = 60000.0f / 1001.0f;
          break;
        case 32000 ... 35000:
          // 29.970 (33366.666656)
          displayFPS = 30000.0f / 1001.0f;
          break;
        case 19000 ... 21000:
          // 50.000 (20000.000000)
          displayFPS = 50000.0f / 1000.0f;
          break;
        case 35500 ... 41000:
          // 25.000 (40000.000000)
          displayFPS = 25000.0f / 1000.0f;
          break;
      }
      self.displayRate = displayFPS;
    }

    if (self.displayRate != oldDisplayRate)
    {
      // track and log changes
      oldDisplayRate = self.displayRate;
      CLog::Log(LOGDEBUG, "%s: displayRate = %f", __PRETTY_FUNCTION__, self.displayRate);
    }
  }

  //if (m_animating)
  //  [self performSelectorOnMainThread:@selector(updateFocusLayer) withObject:nil  waitUntilDone:NO];
}

//--------------------------------------------------------------
- (void)displayRateSwitch:(float)refreshRate withDynamicRange:(int)dynamicRange
{
  if (CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOPLAYER_ADJUSTREFRESHRATE) != ADJUST_REFRESHRATE_OFF)
  {
    if (__builtin_available(tvOS 11.2, *))
    {
      // avDisplayManager is only in 11.2 beta4 so we need to also
      // trap out for older 11.2 betas. This can be changed once
      // tvOS 11.2 gets released.
      if ([m_window respondsToSelector:@selector(avDisplayManager)])
      {
        auto avDisplayManager = [m_window avDisplayManager];
        if (refreshRate > 0.0)
        {
          // initWithRefreshRate is private in 11.2 beta4 but apple
          // will move it public at some time.
          // videoDynamicRange values are based on watching
          // console log when forcing different values.
          // search for "Native Mode Requested" and pray :)
          // searches for "FBSDisplayConfiguration" and "currentMode" will show the actual
          // for example, currentMode = <FBSDisplayMode: 0x1c4298100; 1920x1080@2x (3840x2160/2) 24Hz p3 HDR10>
          // SDR == 0, 1 (1 is what tests with loading assets show)
          // HDR == 2, 3 (3 is what tests with loading assets show)
          // DoblyVision == 4
          // infer initWithRefreshRate in case it ever changes
          using namespace StringObfuscation;
          std::string neveryyoumind = ObfuscateString("AVDisplayCriteria");
          Class AVDisplayCriteriaClass = NSClassFromString([NSString stringWithUTF8String: neveryyoumind.c_str()]);
          AVDisplayCriteria *displayCriteria = [[AVDisplayCriteriaClass alloc] initWithRefreshRate:refreshRate videoDynamicRange:dynamicRange];
          if (displayCriteria)
          {
            // setting preferredDisplayCriteria will trigger a display rate switch
            avDisplayManager.preferredDisplayCriteria = displayCriteria;
          }
        }
        else
        {
          // switch back to tvOS defined user settings if we get
          // zero or less than value for refreshRate. Should never happen :)
          avDisplayManager.preferredDisplayCriteria = nil;
        }
        std::string dynamicRangeString = "Unknown";
        switch(dynamicRange)
        {
          case 0 ... 1:
            dynamicRangeString = "SDR";
            break;
          case 2 ... 3:
            dynamicRangeString = "HDR10";
            break;
          case 4:
            dynamicRangeString = "DolbyVision";
            break;
        }
        CLog::Log(LOGDEBUG, "displayRateSwitch request: refreshRate = %.2f, dynamicRange = %s", refreshRate, dynamicRangeString.c_str());
      }
    }
  }
}

//--------------------------------------------------------------
- (void)displayRateReset
{
  CLog::Log(LOGDEBUG, "displayRateReset");
  if (CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOPLAYER_ADJUSTREFRESHRATE) != ADJUST_REFRESHRATE_OFF)
  {
    if (__builtin_available(tvOS 11.2, *))
    {
      if ([m_window respondsToSelector:@selector(avDisplayManager)])
      {
        // setting preferredDisplayCriteria to nil will
        // switch back to tvOS defined user settings
        auto avDisplayManager = [m_window avDisplayManager];
        avDisplayManager.preferredDisplayCriteria = nil;
      }
    }
  }
}

//--------------------------------------------------------------
- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
  if ([keyPath isEqualToString:@"displayModeSwitchInProgress"])
  {
    // tracking displayModeSwitchInProgress via NSKeyValueObservingOptionNew,
    // any changes in displayModeSwitchInProgress will fire this callback.
    if (__builtin_available(tvOS 11.2, *))
    {
      std::string switchState = "NO";
      int dynamicRange = 0;
      float refreshRate = self.getDisplayRate;
      if ([m_window respondsToSelector:@selector(avDisplayManager)])
      {
        auto avDisplayManager = [m_window avDisplayManager];
        auto displayCriteria = avDisplayManager.preferredDisplayCriteria;
        // preferredDisplayCriteria can be nil, this is NOT an error
        // and just indicates tvOS defined user settings which we cannot see.
        if (displayCriteria != nil)
        {
          refreshRate = displayCriteria.refreshRate;
          dynamicRange = displayCriteria.videoDynamicRange;
        }
        if ([avDisplayManager isDisplayModeSwitchInProgress] == YES)
        {
          switchState = "YES";
          g_Windowing.AnnounceOnLostDevice();
          g_Windowing.StartLostDeviceTimer();
        }
        else
        {
          switchState = "DONE";
          g_Windowing.StopLostDeviceTimer();
          g_Windowing.AnnounceOnResetDevice();
          // displayLinkTick is tracking actual refresh duration.
          // when isDisplayModeSwitchInProgress == NO, we have switched
          // and stablized. We might have switched to some other
          // rate than what we requested. setting preferredDisplayCriteria is
          // only a request. For example, 30Hz might only be avaliable in HDR
          // and asking for 30Hz/SDR might result in 60Hz/SDR and
          // g_graphicsContext.SetFPS needs the actual refresh rate.
          refreshRate = self.getDisplayRate;
       }
      }
      g_graphicsContext.SetFPS(refreshRate);
      std::string dynamicRangeString = "Unknown";
      switch(dynamicRange)
      {
        case 0 ... 1:
          dynamicRangeString = "SDR";
          break;
        case 2 ... 3:
          dynamicRangeString = "HDR10";
          break;
        case 4:
          dynamicRangeString = "DolbyVision";
          break;
      }
      CLog::Log(LOGDEBUG, "displayModeSwitchInProgress == %s, refreshRate = %.2f, dynamicRange = %s",
        switchState.c_str(), refreshRate, dynamicRangeString.c_str());
    }
  }
}

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - remote idle timer
//--------------------------------------------------------------
// ignore remote/siri events if the timer is expired
- (void)startRemoteTimer
{
  m_remoteIdleState = false;
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
  if (self.remoteIdleTimer != nil)
  {
    [self.remoteIdleTimer invalidate];
    self.remoteIdleTimer = nil;
  }
  m_remoteIdleState = false;
}

- (void)setRemoteIdleState
{
  m_remoteIdleState = true;
}

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - gesture creators/recognizers
//--------------------------------------------------------------
- (void)createSiriSwipeGestureRecognizers
{
  // these are for tracking tap/pan/swipe state only,
  // tvOS focus engine will handle the navigation.
  UISwipeGestureRecognizer *swipeLeft = [[UISwipeGestureRecognizer alloc]
    initWithTarget:self action:@selector(SiriSwipeHandler:)];
  swipeLeft.delaysTouchesBegan = NO;
  swipeLeft.direction = UISwipeGestureRecognizerDirectionLeft;
  swipeLeft.delegate = self;
  [self.focusView  addGestureRecognizer:swipeLeft];

  //single finger swipe right
  UISwipeGestureRecognizer *swipeRight = [[UISwipeGestureRecognizer alloc]
    initWithTarget:self action:@selector(SiriSwipeHandler:)];
  swipeRight.delaysTouchesBegan = NO;
  swipeRight.direction = UISwipeGestureRecognizerDirectionRight;
  swipeRight.delegate = self;
  [self.focusView  addGestureRecognizer:swipeRight];

  //single finger swipe up
  UISwipeGestureRecognizer *swipeUp = [[UISwipeGestureRecognizer alloc]
    initWithTarget:self action:@selector(SiriSwipeHandler:)];
  swipeUp.delaysTouchesBegan = NO;
  swipeUp.direction = UISwipeGestureRecognizerDirectionUp;
  swipeUp.delegate = self;
  [self.focusView  addGestureRecognizer:swipeUp];

  //single finger swipe down
  UISwipeGestureRecognizer *swipeDown = [[UISwipeGestureRecognizer alloc]
    initWithTarget:self action:@selector(SiriSwipeHandler:)];
  swipeDown.delaysTouchesBegan = NO;
  swipeDown.direction = UISwipeGestureRecognizerDirectionDown;
  swipeDown.delegate = self;
  [self.focusView  addGestureRecognizer:swipeDown];
}
//--------------------------------------------------------------
- (void)createSiriPanGestureRecognizers
{
  // these are for tracking tap/pan/swipe state only,
  // tvOS focus engine will handle the navigation.
  auto pan = [[UIPanGestureRecognizer alloc]
    initWithTarget:self action:@selector(SiriPanHandler:)];
  pan.delegate = self;
  [self.focusView addGestureRecognizer:pan];
}
//--------------------------------------------------------------
- (void)createSiriTapGestureRecognizers
{
  auto singletap = [[UITapGestureRecognizer alloc]
    initWithTarget:self action:@selector(SiriSingleTapHandler:)];
  singletap.numberOfTapsRequired = 1;
  // The default press type is select, when this property is set to an empty array,
  // the gesture recognizer will respond to taps like a touch pad like surface
  singletap.allowedPressTypes = @[];
  singletap.allowedTouchTypes = @[@(UITouchTypeIndirect)];
  singletap.delegate = self;
  [self.focusView addGestureRecognizer:singletap];

  auto doubletap = [[UITapGestureRecognizer alloc]
    initWithTarget:self action:@selector(SiriDoubleTapHandler:)];
  doubletap.numberOfTapsRequired = 2;
  doubletap.allowedPressTypes = @[];
  doubletap.allowedTouchTypes = @[@(UITouchTypeIndirect)];
  [self.focusView addGestureRecognizer:doubletap];

  auto tripletap = [[UITapGestureRecognizer alloc]
    initWithTarget:self action:@selector(SiriTripleTapHandler:)];
  tripletap.numberOfTapsRequired = 3;
  tripletap.allowedPressTypes = @[];
  tripletap.allowedTouchTypes = @[@(UITouchTypeIndirect)];
  tripletap.delegate = self;
  [self.focusView addGestureRecognizer:tripletap];

  [singletap requireGestureRecognizerToFail:doubletap];
  [doubletap requireGestureRecognizerToFail:tripletap];
}
//--------------------------------------------------------------
- (void)createSiriPressGesturecognizers
{
  // we always have these under tvos,
  // both ir and siri remotes respond to these
  auto menuRecognizer = [[UITapGestureRecognizer alloc]
    initWithTarget: self action: @selector(SiriMenuHandler:)];
  menuRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeMenu]];
  menuRecognizer.delegate  = self;
  [self.focusView addGestureRecognizer: menuRecognizer];
  
  auto longSelectRecognizer = [[UILongPressGestureRecognizer alloc]
                           initWithTarget: self action: @selector(SiriLongSelectHandler:)];
  longSelectRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeSelect]];
  longSelectRecognizer.minimumPressDuration = 0.001;
  longSelectRecognizer.delegate = self;
  [self.focusView addGestureRecognizer: longSelectRecognizer];

  auto playPauseRecognizer = [[UITapGestureRecognizer alloc]
    initWithTarget: self action: @selector(SiriPlayPauseHandler:)];
  playPauseRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypePlayPause]];
  playPauseRecognizer.delegate  = self;
  [self.focusView addGestureRecognizer: playPauseRecognizer];

  // ir remote presses only, left/right/up/down
  auto upRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteUpArrowPressed:)];
  upRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeUpArrow]];
  upRecognizer.minimumPressDuration = 0.001;
  upRecognizer.delegate = self;
  [self.focusView addGestureRecognizer: upRecognizer];

  auto downRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteDownArrowPressed:)];
  downRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeDownArrow]];
  downRecognizer.minimumPressDuration = 0.001;
  downRecognizer.delegate = self;
  [self.focusView addGestureRecognizer: downRecognizer];

  auto leftRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteLeftArrowPressed:)];
  leftRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeLeftArrow]];
  leftRecognizer.minimumPressDuration = 0.001;
  leftRecognizer.delegate = self;
  [self.focusView addGestureRecognizer: leftRecognizer];

  auto rightRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteRightArrowPressed:)];
  rightRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeRightArrow]];
  rightRecognizer.minimumPressDuration = 0.001;
  rightRecognizer.delegate = self;
  [self.focusView addGestureRecognizer: rightRecognizer];
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
//--------------------------------------------------------------
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
  if ([gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]] && ([otherGestureRecognizer isKindOfClass:[UISwipeGestureRecognizer class]] || [otherGestureRecognizer isKindOfClass:[UITapGestureRecognizer class]]))
  {
    return YES;
  }
  if ([gestureRecognizer isKindOfClass:[UITapGestureRecognizer class]] && [otherGestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]])
  {
    return YES;
  }
  return NO;
}
//--------------------------------------------------------------
// GestureRecognizers are used to manage the focus action type state machine.
// There are three types, tap, pan and swipe. For taps, these
// can be menu, select, play/pause buttons or up/down/right/left taps
// on trackpad, or up/down/right/left on IR remote. One could call
// them presses but it's easier to just deal with them all as taps.
// (ie directional taps on trackpad are similar to directional presses on ir remotes)
// The tvOS focus engine will call shouldUpdateFocusInContext/didUpdateFocusInContext
// but we need to know which focus action type so we can do the right thing.
static const char* focusActionTypeNames[] = {
  "none",
  "tap",
  "pan",
  "swipe",
};
typedef enum FocusActionTypes
{
  FocusActionTap  = 1,
  FocusActionPan  = 2,
  FocusActionSwipe = 3,
} FocusActionTypes;
// default action is FocusActionTap, gestureRecognizers will
// set the correct type before shouldUpdateFocusInContext is hit
int focusActionType = FocusActionTap;

bool tapNoMore = false;
bool panNoMore = false;
bool swipeNoMore = false;
int swipeCounter = 0;
CGRect swipeStartingParentViewRect;
FocusLayerView *swipeStartingParent;
//--------------------------------------------------------------
// called before touchesBegan:withEvent: is called on the gesture recognizer
// for a new touch. return NO to prevent the gesture recognizer from seeing this touch
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
  // Block the recognition of tap gestures from other views
  if ( [touch.view isKindOfClass:[KeyboardView class]] )
    return NO;

  // same for FocusLayerViewPlayerProgress
  if ( [touch.view isKindOfClass:[FocusLayerViewPlayerProgress class]] )
    return NO;

  // important, this gestureRecognizer gets called before any other tap/pas/swipe handler
  // including shouldUpdateFocusInContext/didUpdateFocusInContext. So we can
  // setup the initial focusActionType to tap.
  //CLog::Log(LOGDEBUG, "shouldReceiveTouch:FocusActionTap, %ld", (long)gestureRecognizer.state);
  focusActionType = FocusActionTap;
  return YES;
}
//--------------------------------------------------------------
// called before pressesBegan:withEvent: is called on the gesture recognizer
// for a new press. return NO to prevent the gesture recognizer from seeing this press
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceivePress:(UIPress *)press
{
  //PRINT_SIGNATURE();
  // Block the recognition of press gestures from other views
  if ( [press.responder isKindOfClass:[KeyboardView class]] )
    return NO;
  // same for FocusLayerViewPlayerProgress
  if ( [press.responder isKindOfClass:[FocusLayerViewPlayerProgress class]] )
  {
    switch (press.type)
    {
      // we handle those here
      case UIPressTypeMenu:
      case UIPressTypeSelect:
      case UIPressTypePlayPause:
        break;
      default:
      return NO;
    }
  }
  /*
  if ( [_focusLayer.infocus.view isKindOfClass:[FocusLayerViewPlayerProgress class]] )
    return NO;
  */

  BOOL handled = YES;
  // important, this gestureRecognizer gets called before any other press handler
  // including shouldUpdateFocusInContext/didUpdateFocusInContext. So we can
  // setup the initial focusActionType to tap.
  //CLog::Log(LOGDEBUG, "shouldReceivePress:FocusActionTap, %ld", (long)gestureRecognizer.state);
  focusActionType = FocusActionTap;
  switch (press.type)
  {
    // single press key, but also detect hold and back to tvos.
    case UIPressTypeMenu:
    {
      // menu is special.
      //  a) if at our home view, should return to atv home screen
      //  b) if not, let it pass to us
      int focusedWindowID = CFocusEngineHandler::GetInstance().GetFocusWindowID();
      if (focusedWindowID == WINDOW_HOME)
      {
        CLog::Log(LOGDEBUG, "shouldReceivePress:focusedWindowID == WINDOW_HOME");
        handled = NO;
      }
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
//--------------------------------------------------------------
// The whole purpose of this function is to break a single
// select click into left, right and center clicks.
// This is the only way to detect the location of click.
// It could also be used to detect if a use has a finger
// resting on the track pad.
- (void)initGameController
{
  // dpad axis values ranges from -1.0 to 1.0
  // where -1,-1 is top, left on trackpad.
  // referenced from center of touchpad.
  [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification
    object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification * _Nonnull note)
  {
    self.gcController = note.object;
    self.gcController.microGamepad.reportsAbsoluteDpadValues = YES;
    self.gcController.microGamepad.valueChangedHandler = ^(GCMicroGamepad *gamepad, GCControllerElement *element)
    {
      CGPoint startPoint = CGPointMake(
        gamepad.dpad.xAxis.value, gamepad.dpad.yAxis.value);
      /*
      NSLog(@"microGamepad: A(%d), U(%d), D(%d), L(%d), R(%d), point %@",
        gamepad.buttonA.pressed,
        gamepad.dpad.up.pressed,
        gamepad.dpad.down.pressed,
        gamepad.dpad.left.pressed,
        gamepad.dpad.right.pressed,
        NSStringFromCGPoint(startPoint));
      */
      if (startPoint.x > 0.65)
      {
        if (gamepad.buttonA.pressed)
        {
          m_clickDirection = CLICK_RIGHT;
          //NSLog(@"microGamepad: user clicked finger near right side of remote");
        }
      }

      if (startPoint.x < -0.65)
      {
        if (gamepad.buttonA.pressed)
        {
          m_clickDirection = CLICK_LEFT;
          //NSLog(@"microGamepad: user clicked finger near left side of remote");
        }
      }

      if (startPoint.y > 0.65)
      {
        if (gamepad.buttonA.pressed)
        {
          m_clickDirection = CLICK_UP;
          //NSLog(@"microGamepad: user clicked finger near top of remote");
        }
      }

      if (startPoint.y < -0.65)
      {
        if (gamepad.buttonA.pressed)
        {
          m_clickDirection = CLICK_DOWN;
          //NSLog(@"microGamepad: user clicked finger near bottom of remote");
        }
      }

      if (!gamepad.buttonA.pressed)
      {
        m_clickDirection = CLICK_SELECT_OR_RELEASED;
        //NSLog(@"microGamepad: user released finger from touch surface");
      }
    };
  }];
}

//--------------------------------------------------------------
//--------------------------------------------------------------
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
    CInputManager::GetInstance().QueueAction(CAction(actionID, 1.0f, 0.0f, actionName, 0, buttonId), true);
  }
  else
    CLog::Log(LOGDEBUG, "sendButtonPressed, ERROR mapping customcontroller action. CustomController: %s %i", "SiriRemote", buttonId);
}

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - touch/gesture handlers
//--------------------------------------------------------------
- (IBAction)SiriSwipeHandler:(UISwipeGestureRecognizer *)sender
{
  // for state tracking only, tvOS focus engine will handle the navigation
  if (!m_remoteIdleState)
  {
    if (m_appAlive == YES)//NO GESTURES BEFORE WE ARE UP AND RUNNING
    {
      switch (sender.state)
      {
        case UIGestureRecognizerStateRecognized:
          {
            swipeNoMore = false;
            swipeCounter = 0;
            focusActionType = FocusActionSwipe;
            CLog::Log(LOGDEBUG, "SiriSwipeHandler:StateRecognized:FocusActionSwipe");
            swipeStartingParent = [self findParentView:_focusLayer.infocus.view];
            swipeStartingParentViewRect = swipeStartingParent.bounds;
            CLog::Log(LOGDEBUG, "SiriSwipeHandler:StateRecognized: %f, %f, %f, %f",
              swipeStartingParentViewRect.origin.x,
              swipeStartingParentViewRect.origin.y,
              swipeStartingParentViewRect.origin.x + swipeStartingParentViewRect.size.width,
              swipeStartingParentViewRect.origin.y + swipeStartingParentViewRect.size.height);
          }
          break;
        default:
          break;
      }
    }
  }
  // start remote timeout
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (IBAction)SiriPanHandler:(UIPanGestureRecognizer *)sender
{
  // for state tracking only, tvOS focus engine will handle the navigation
  if (!m_remoteIdleState)
  {
    if (m_appAlive == YES)//NO GESTURES BEFORE WE ARE UP AND RUNNING
    {
      switch (sender.state)
      {
        case UIGestureRecognizerStateBegan:
          {
            panNoMore = false;
            focusActionType = FocusActionPan;
            CLog::Log(LOGDEBUG, "SiriPanHandler:StateBegan:FocusActionPan");
            FocusLayerView *parentView = [self findParentView:_focusLayer.infocus.view];
            swipeStartingParentViewRect = parentView.bounds;
            swipeStartingParentViewRect = parentView.bounds;
            CLog::Log(LOGDEBUG, "SiriPanHandler:StateBegan: %f, %f, %f, %f",
              swipeStartingParentViewRect.origin.x,
              swipeStartingParentViewRect.origin.y,
              swipeStartingParentViewRect.origin.x + swipeStartingParentViewRect.size.width,
              swipeStartingParentViewRect.origin.y + swipeStartingParentViewRect.size.height);
          }
          break;
        default:
          break;
      }
    }
  }
  // start remote timeout
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (void)SiriSingleTapHandler:(UITapGestureRecognizer *)sender
{
  if (m_appAlive == YES)
  {
    switch (sender.state)
    {
      case UIGestureRecognizerStateEnded:
        CLog::Log(LOGDEBUG, "SiriSingleTapHandler:StateEnded");
        if (g_application.m_pPlayer->IsPlayingVideo() && !g_application.m_pPlayer->IsPaused())
        {
          //show (2.5sec auto hide)/hide normal progress bar
          if (g_infoManager.GetDisplayAfterSeek())
            g_infoManager.SetDisplayAfterSeek(0);
          else
            g_infoManager.SetDisplayAfterSeek(2500);
        }
        break;
      default:
        break;
    }
  }
  // start remote timeout
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (void)SiriDoubleTapHandler:(UITapGestureRecognizer *)sender
{
  if (m_appAlive == YES)
  {
    switch (sender.state)
    {
      case UIGestureRecognizerStateEnded:
        CLog::Log(LOGDEBUG, "SiriDoubleTapHandler:StateEnded");
        // placeholder to alter progress bar time display
        break;
     default:
        break;
    }
  }
  // start remote timeout
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (void)SiriTripleTapHandler:(UITapGestureRecognizer *)sender
{
  if (m_appAlive == YES)
  {
    switch (sender.state)
    {
      case UIGestureRecognizerStateEnded:
        CLog::Log(LOGDEBUG, "SiriTripleTapHandler:StateEnded");
        if (g_application.m_pPlayer->IsPlayingVideo() && !g_application.m_pPlayer->IsPaused())
        {
          KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(
            TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_SHOW_SUBTITLES)));
        }
        break;
     default:
        break;
    }
  }
  // start remote timeout
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (void)SiriMenuHandler:(UITapGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateEnded:
    {
      CLog::Log(LOGDEBUG, "SiriMenuHandler:StateEnded");
      CGUIWindow *focusWindow = CFocusEngineHandler::GetInstance().GetFocusWindow();
      if (focusWindow && focusWindow->GetID() == WINDOW_FULLSCREEN_VIDEO)
      {
        if ([self hasPlayerProgressScrubbing] && g_application.m_pPlayer->IsPaused())
        {
          // video playback, we are paused and progress bar scrubber is up
          [self sendButtonPressed:SiriRemote_PausePlayClick];
        }
        else
        {
          // normal video playback
          if (m_stopPlaybackOnMenu)
            CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_STOP);
          else
            [self sendButtonPressed:SiriRemote_MenuClickAtHome];
        }
      }
      else
      {
        [self sendButtonPressed:SiriRemote_MenuClick];
      }
      break;
    }
    default:
      break;
  }
  // start remote timeout
  [self startRemoteTimer];
}

typedef enum
{
  SELECT_NAVIGATION = 0,
  SELECT_VIDEOPLAY,
  SELECT_VIDEOPAUSED,
} SELECT_STATE;
SELECT_STATE selectState = SELECT_NAVIGATION;
CLICK_DIRECTION clickDirectionAtStateBegan = CLICK_SELECT_OR_RELEASED;
//--------------------------------------------------------------
- (void)SiriLongSelectHoldHandler
{
  self.m_holdCounter++;
  if (selectState == SELECT_VIDEOPLAY)
  {
    if (self.m_holdCounter == 1)
    {
      switch(clickDirectionAtStateBegan)
      {
        case CLICK_LEFT:
          // use 4X speed rewind.
          [self sendButtonPressed:SiriRemote_IR_Rewind];
          [self sendButtonPressed:SiriRemote_IR_Rewind];
          break;
        case CLICK_RIGHT:
          // use 4X speed forward.
          [self sendButtonPressed:SiriRemote_IR_FastForward];
          [self sendButtonPressed:SiriRemote_IR_FastForward];
          break;
        default:
          break;
      }
    }
  }
  else
  {
    [self.m_holdTimer invalidate];
    [self sendButtonPressed:SiriRemote_CenterHold];
  }
}
//--------------------------------------------------------------
- (void)SiriLongSelectHandler:(UITapGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      CLog::Log(LOGDEBUG, "SiriLongSelectHandler:StateBegan");
      self.m_holdCounter = 0;
      // assume we are navigating
      selectState = SELECT_NAVIGATION;
      if (g_application.m_pPlayer->IsPlayingVideo())
      {
        selectState = SELECT_VIDEOPLAY;
        if (g_application.m_pPlayer->IsPaused())
          selectState = SELECT_VIDEOPAUSED;
      }
      clickDirectionAtStateBegan = m_clickDirection;
      self.m_holdTimer = [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(SiriLongSelectHoldHandler) userInfo:nil repeats:YES];
      break;
    case UIGestureRecognizerStateChanged:
      CLog::Log(LOGDEBUG, "SiriLongSelectHandler:StateChanged");
      if (selectState == SELECT_NAVIGATION)
      {
        if (self.m_holdCounter > 1)
        {
          [self.m_holdTimer invalidate];
          [self sendButtonPressed:SiriRemote_CenterHold];
        }
      }
      break;
    case UIGestureRecognizerStateEnded:
      CLog::Log(LOGDEBUG, "SiriLongSelectHandler:StateEnded");
      [self.m_holdTimer invalidate];
      if (self.m_holdCounter < 1)
      {
        // hold timer never fired,
        // this is a normal press/release cycle
        switch(selectState)
        {
          case SELECT_NAVIGATION:
            // user was nav'ing around in skin and clicked
            [self sendButtonPressed:SiriRemote_CenterClick];
            break;
          case SELECT_VIDEOPLAY:
            // fullscreen video was playing but not paused
            switch(clickDirectionAtStateBegan)
            {
              case CLICK_UP:
                // big/chapter seek or channel change for pvr
                [self sendButtonPressed:SiriRemote_UpTap];
                break;
              case CLICK_DOWN:
                // big/chapter seek or channel change for pvr
                [self sendButtonPressed:SiriRemote_DownTap];
                break;
              case CLICK_LEFT:
                // seek backward
                [self sendButtonPressed:SiriRemote_LeftTap];
                break;
              case CLICK_RIGHT:
                // seek forward
                [self sendButtonPressed:SiriRemote_RightTap];
                break;
              case CLICK_SELECT_OR_RELEASED:
                // pause playback
                [self sendButtonPressed:SiriRemote_PausePlayClick];
                break;
            }
            break;
          case SELECT_VIDEOPAUSED:
            // idea here is that if user does not use ExpertMode, it shoud behave like "Netflix" in fullscreen
            // would have been easier to do this in keymap, but we could not make it backward compatible
            if ([_focusLayer.infocus.view isKindOfClass:[FocusLayerViewPlayerProgress class]] )
            {
              // progress bar with scrubber was up
              double appTotalTime = g_application.GetTotalTime();
              double appPercentage = g_application.GetPercentage();
              double appSeekTime = appPercentage * appTotalTime / 100;
              FocusLayerViewPlayerProgress *viewPlayerProgress = (FocusLayerViewPlayerProgress*)_focusLayer.infocus.view;
              double percentage = [viewPlayerProgress getSeekTimePercentage];
              double seekTime = percentage * appTotalTime / 100;
              // only seek if change is more than 500ms
              if (fabs(appSeekTime - seekTime) > 0.5)
              {
                g_application.SeekPercentage(percentage, true);
                // turn off display after seek.
                g_infoManager.SetDisplayAfterSeek(0);
              }
              else
              {
                // resume playback
                [self sendButtonPressed:SiriRemote_PausePlayClick];
              }
            }
            break;
        }
      }
      else
      {
        // hold timer fired,
        // this is a press/hold/release cycle
        switch(selectState)
        {
          case SELECT_NAVIGATION:
            // hold timer handled button press, do nothing
            break;
          case SELECT_VIDEOPLAY:
            // hold timer put us into ff/rw
            // restore to normal playback speed.
            if (g_application.m_pPlayer->IsPlaying() && !g_application.m_pPlayer->IsPaused())
              CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_PLAY)));
          case SELECT_VIDEOPAUSED:
            // do nothing
            break;
        }
      }
      selectState = SELECT_NAVIGATION;
      break;
    case UIGestureRecognizerStateCancelled:
      selectState = SELECT_NAVIGATION;
      CLog::Log(LOGDEBUG, "SiriLongSelectHandler:StateCancelled");
      break;
    default:
      break;
  }
  // start remote timeout
  [self startRemoteTimer];
}
//--------------------------------------------------------------
- (void)SiriPlayPauseHandler:(UITapGestureRecognizer *) sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateEnded:
      CLog::Log(LOGDEBUG, "SiriPlayPauseHandler:StateEnded");
      [self sendButtonPressed:SiriRemote_PausePlayClick];
      // start remote timeout
      [self startRemoteTimer];
      break;
    default:
      break;
  }
}

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - IR remote directional handlers
// only used during video playback, tvOS focus engine will
// automatically include IR directional navigation.
//--------------------------------------------------------------
//--------------------------------------------------------------
// start repeating after 0.25s
#define REPEATED_KEYPRESS_DELAY_S 0.25
// pause 0.05s (50ms) between keypresses
#define REPEATED_KEYPRESS_PAUSE_S 0.15
//--------------------------------------------------------------
static CFAbsoluteTime keyPressTimerStartSeconds;

- (void)startKeyPressTimer:(int)keyId
{
  [self startKeyPressTimer:keyId doBeforeDelay:true withDelay:REPEATED_KEYPRESS_DELAY_S];
}

- (void)startKeyPressTimer:(int)keyId doBeforeDelay:(bool)doBeforeDelay withDelay:(NSTimeInterval)delay
{
  [self startKeyPressTimer:keyId doBeforeDelay:doBeforeDelay withDelay:delay withInterval:REPEATED_KEYPRESS_PAUSE_S];
}

- (void)startKeyPressTimer:(int)keyId doBeforeDelay:(bool)doBeforeDelay withDelay:(NSTimeInterval)delay withInterval:(NSTimeInterval)interval
{
  if (self.pressAutoRepeatTimer != nil)
    [self stopKeyPressTimer];

  if (doBeforeDelay)
    [self sendButtonPressed:keyId];

  NSNumber *number = [NSNumber numberWithInt:keyId];
  NSDate *fireDate = [NSDate dateWithTimeIntervalSinceNow:delay];

  keyPressTimerStartSeconds = CFAbsoluteTimeGetCurrent() + delay;
  // schedule repeated timer which starts after REPEATED_KEYPRESS_DELAY_S
  // and fires every REPEATED_KEYPRESS_PAUSE_S
  NSTimer *timer = [[NSTimer alloc] initWithFireDate:fireDate
    interval:interval target:self selector:@selector(keyPressTimerCallback:) userInfo:number repeats:YES];

  // schedule the timer to the runloop
  [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
  self.pressAutoRepeatTimer = timer;
}
- (void)stopKeyPressTimer
{
  if (self.pressAutoRepeatTimer != nil)
  {
    [self.pressAutoRepeatTimer invalidate];
    self.pressAutoRepeatTimer = nil;
  }
}
- (void)keyPressTimerCallback:(NSTimer*)theTimer
{
  NSNumber *keyId = [theTimer userInfo];
  [self sendButtonPressed:[keyId intValue]];
}

- (IBAction)IRRemoteLeftArrowPressed:(UIGestureRecognizer *)sender
{
  // only used during video playback, tvOS focus engine will
  // include IR key pressing
  if (m_appAlive == YES)
  {
    switch (sender.state)
    {
      case UIGestureRecognizerStateBegan:
        CLog::Log(LOGDEBUG, "PlayerProgress::IRRemoteLeftArrowPressed");
        if (g_application.m_pPlayer->IsPaused())
          [self startKeyPressTimer:SiriRemote_LeftTap doBeforeDelay:true withDelay:REPEATED_KEYPRESS_DELAY_S];
        else
          [self sendButtonPressed:SiriRemote_LeftTap];
        break;
      case UIGestureRecognizerStateEnded:
      case UIGestureRecognizerStateChanged:
      case UIGestureRecognizerStateCancelled:
        [self stopKeyPressTimer];
        break;
      default:
        break;
    }
  }
  [self startRemoteTimer];
}
- (IBAction)IRRemoteRightArrowPressed:(UIGestureRecognizer *)sender
{
  // only used during video playback, tvOS focus engine will
  // include IR key pressing
  if ([self hasPlayerProgressScrubbing])
  {
    if (m_appAlive == YES)
    {
      switch (sender.state)
      {
        case UIGestureRecognizerStateBegan:
          CLog::Log(LOGDEBUG, "PlayerProgress::IRRemoteRightArrowPressed");
          if (g_application.m_pPlayer->IsPaused())
            [self startKeyPressTimer:SiriRemote_RightTap doBeforeDelay:true withDelay:REPEATED_KEYPRESS_DELAY_S];
          else
            [self sendButtonPressed:SiriRemote_RightTap];
          break;
        case UIGestureRecognizerStateEnded:
        case UIGestureRecognizerStateChanged:
        case UIGestureRecognizerStateCancelled:
          [self stopKeyPressTimer];
          break;
        default:
          break;
      }
    }
  }
  [self startRemoteTimer];
}
- (IBAction)IRRemoteUpArrowPressed:(UIGestureRecognizer *)sender
{
  // only used during video playback, tvOS focus engine will
  // include IR key pressing
  if ([self hasPlayerProgressScrubbing])
  {
    if (m_appAlive == YES)
    {
      switch (sender.state)
      {
        case UIGestureRecognizerStateBegan:
          CLog::Log(LOGDEBUG, "PlayerProgress::IRRemoteUpArrowPressed");
          if (g_application.m_pPlayer->IsPaused())
            [self startKeyPressTimer:SiriRemote_UpTap doBeforeDelay:true withDelay:REPEATED_KEYPRESS_DELAY_S];
          else
            [self sendButtonPressed:SiriRemote_UpTap];
          break;
        case UIGestureRecognizerStateEnded:
        case UIGestureRecognizerStateChanged:
        case UIGestureRecognizerStateCancelled:
          [self stopKeyPressTimer];
          break;
        default:
          break;
      }
    }
  }
  [self startRemoteTimer];
}
- (IBAction)IRRemoteDownArrowPressed:(UIGestureRecognizer *)sender
{
  // only used during video playback, tvOS focus engine will
  // include IR key pressing
  if ([self hasPlayerProgressScrubbing])
  {
    if (m_appAlive == YES)
    {
      switch (sender.state)
      {
        case UIGestureRecognizerStateBegan:
          CLog::Log(LOGDEBUG, "PlayerProgress::IRRemoteDownArrowPressed");
          if (g_application.m_pPlayer->IsPaused())
            [self startKeyPressTimer:SiriRemote_DownTap doBeforeDelay:true withDelay:REPEATED_KEYPRESS_DELAY_S];
          else
            [self sendButtonPressed:SiriRemote_DownTap];
          break;
        case UIGestureRecognizerStateEnded:
        case UIGestureRecognizerStateChanged:
        case UIGestureRecognizerStateCancelled:
          [self stopKeyPressTimer];
          break;
        default:
          break;
      }
    }
  }
  [self startRemoteTimer];
}

//--------------------------------------------------------------
//--------------------------------------------------------------
- (void)remoteControlReceivedWithEvent:(UIEvent*)receivedEvent
{
  CLog::Log(LOGDEBUG, "remoteControlReceivedWithEvent");
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

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - focus changed idle timer
//--------------------------------------------------------------
- (void)startFocusTimer
{
  m_focusIdleState = false;

  //PRINT_SIGNATURE();
  if (self.focusIdleTimer != nil)
    [self stopFocusTimer];

  NSDate *fireDate = [NSDate dateWithTimeIntervalSinceNow:0.5];
  NSTimer *timer = [[NSTimer alloc] initWithFireDate:fireDate
                                    interval:0.0
                                    target:self
                                    selector:@selector(setFocusIdleState)
                                    userInfo:nil
                                    repeats:NO];

  [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
  self.focusIdleTimer = timer;
}

- (void)stopFocusTimer
{
  //PRINT_SIGNATURE();
  if (self.focusIdleTimer != nil)
  {
    [self.focusIdleTimer invalidate];
    self.focusIdleTimer = nil;
  }
  m_focusIdleState = false;
}

- (void)setFocusIdleState
{
  //PRINT_SIGNATURE();
  m_focusIdleState = true;
}

//--------------------------------------------------------------
//--------------------------------------------------------------
#pragma mark - remote/siri focus engine routines
//--------------------------------------------------------------
- (UIFocusSoundIdentifier)soundIdentifierForFocusUpdateInContext:(UIFocusUpdateContext *)context
{
  // disable focus engine sound effect when playing video
  // it will mess up audio if doing passthrough.
  if ( g_application.m_pPlayer->IsPlayingVideo() )
  {
    if (@available(tvOS 11.0, *))
      return UIFocusSoundIdentifierNone;
    else
      return nil;
  }
  if (@available(tvOS 11.0, *))
  {
    if (CSettings::GetInstance().GetString(CSettings::SETTING_LOOKANDFEEL_SOUNDSKIN) == "resource.uisounds.tvos")
      return UIFocusSoundIdentifierDefault;
    else
      return UIFocusSoundIdentifierNone;
  }
  else
    return nil;
}

CGRect debugView1;
CGRect debugView2;
//--------------------------------------------------------------
- (NSArray<id<UIFocusEnvironment>> *)preferredFocusEnvironments
{
  // The order of the items in the preferredFocusEnvironments array is the
  // priority that the focus engine will use when picking the focused item

  // if native keyboard is up, we don't want to send any button presses to MrMC
  if (m_nativeKeyboardActive)
    return [super preferredFocusEnvironments];

  [self updateFocusLayerInFocus];
  FocusLayerView *parentView = [self findParentView:_focusLayer.infocus.view];
  if (parentView && _focusLayer.infocus.view)
  {
    CGRect parentViewRect = parentView.bounds;
    if (!CGRectEqualToRect(debugView1, parentViewRect))
    {
      debugView1 = parentViewRect;
      /*
      CLog::Log(LOGDEBUG, "preferredFocusEnvironments: parentViewRect %f, %f, %f, %f",
        parentViewRect.origin.x,  parentViewRect.origin.y,
        parentViewRect.origin.x + parentViewRect.size.width,
        parentViewRect.origin.y + parentViewRect.size.height);
      */
    }
    CGRect focusLayerViewRect = _focusLayer.infocus.view.bounds;
    if (!CGRectEqualToRect(debugView2, focusLayerViewRect))
    {
      debugView2 = focusLayerViewRect;
      /*
      CLog::Log(LOGDEBUG, "preferredFocusEnvironments: focusLayerViewRect %f, %f, %f, %f",
        focusLayerViewRect.origin.x,  focusLayerViewRect.origin.y,
        focusLayerViewRect.origin.x + focusLayerViewRect.size.width,
        focusLayerViewRect.origin.y + focusLayerViewRect.size.height);
      */
    }

    NSMutableArray *viewArray = [NSMutableArray array];
    [viewArray addObject:(UIView*)_focusLayer.infocus.view];
    for (size_t indx = 0; indx < _focusLayer.infocus.items.size(); ++indx)
    {
      if (_focusLayer.infocus.core != _focusLayer.infocus.items[indx].core)
        [viewArray addObject:(UIView*)_focusLayer.infocus.items[indx].view];
    }
    [viewArray addObject:(UIView*)self.focusViewTop];
    [viewArray addObject:(UIView*)self.focusViewLeft];
    [viewArray addObject:(UIView*)self.focusViewRight];
    [viewArray addObject:(UIView*)self.focusViewBottom];
    //[viewArray addObject:(UIView*)parentView];
    return viewArray;
  }
  else if (_focusLayer.infocus.view)
  {
    CGRect focusLayerViewRect = _focusLayer.infocus.view.bounds;
    if (!CGRectEqualToRect(debugView2, focusLayerViewRect))
    {
      debugView2 = focusLayerViewRect;
      CLog::Log(LOGDEBUG, "preferredFocusEnvironments: focusLayerViewRect %f, %f, %f, %f",
        focusLayerViewRect.origin.x,  focusLayerViewRect.origin.y,
        focusLayerViewRect.origin.x + focusLayerViewRect.size.width,
        focusLayerViewRect.origin.y + focusLayerViewRect.size.height);
    }
    // need a focusable view or risk bouncing out on menu presses
    CGUIWindow *focusWindow = CFocusEngineHandler::GetInstance().GetFocusWindow();
    if (focusWindow && focusWindow->GetID() == WINDOW_FULLSCREEN_VIDEO)
    {
      if ( [_focusLayer.infocus.view canBecomeFocused] == NO )
        [self.focusView setFocusable:true];
    }
    return @[(UIView*)_focusLayer.infocus.view];
  }
  else
  {
    CLog::Log(LOGDEBUG, "preferredFocusEnvironments");
    // need a focusable view or risk bouncing out on menu presses
    if ( [self.focusView canBecomeFocused] == NO )
      [self.focusView setFocusable:true];
    return @[(UIView*)self.focusView];
  }
}

//--------------------------------------------------------------
- (void)didUpdateFocusInContext:(UIFocusUpdateContext *)context
  withAnimationCoordinator:(UIFocusAnimationCoordinator *)coordinator
{
  if (context.focusHeading != UIFocusHeadingNone)
  {
    // track focus idle time, if focus was idled,
    // allow wrapping in lists, else no wrapping in lists
    g_windowManager.SetWrapOverride(!m_focusIdleState);
    [self startFocusTimer];
  }

  // if we had a focus change, send the heading down to core
  switch (context.focusHeading)
  {
    case UIFocusHeadingUp:
      if (focusActionType == FocusActionSwipe)
        [self sendButtonPressed:SiriRemote_UpSwipe];
      else
        [self sendButtonPressed:SiriRemote_UpTap];
      CLog::Log(LOGDEBUG, "didUpdateFocusInContext:UIFocusHeadingUp");
      break;
    case UIFocusHeadingDown:
      if (focusActionType == FocusActionSwipe)
        [self sendButtonPressed:SiriRemote_DownSwipe];
      else
        [self sendButtonPressed:SiriRemote_DownTap];
      CLog::Log(LOGDEBUG, "didUpdateFocusInContext:UIFocusHeadingDown");
      break;
    case UIFocusHeadingLeft:
      if (focusActionType == FocusActionSwipe)
        [self sendButtonPressed:SiriRemote_LeftSwipe];
      else
        [self sendButtonPressed:SiriRemote_LeftTap];
      CLog::Log(LOGDEBUG, "didUpdateFocusInContext:UIFocusHeadingLeft");
      break;
    case UIFocusHeadingRight:
      if (focusActionType == FocusActionSwipe)
        [self sendButtonPressed:SiriRemote_RightSwipe];
      else
        [self sendButtonPressed:SiriRemote_RightTap];
      CLog::Log(LOGDEBUG, "didUpdateFocusInContext:UIFocusHeadingRight");
      break;
    case UIFocusHeadingNone:
    case UIFocusHeadingNext:
    case UIFocusHeadingPrevious:
      break;
  }
}
//--------------------------------------------------------------
- (BOOL)shouldUpdateFocusInContext:(UIFocusUpdateContext *)context
{
  // Asks whether the system should allow a focus update to occur.

  // useful debugging help
  // po [UIFocusDebugger help]
  // po [UIFocusDebugger status]
  // po [UIFocusDebugger simulateFocusUpdateRequestFromEnvironment:self]
  // po [UIFocusDebugger checkFocusabilityForItem:(UIView *)0x155e2a040]
  // quicklook on passed context.

  // Once we get hit from control view, we might also get one regarding the parent view
  // The one exception to this possible recursion is if you return NO. This stops the recursion.
  // We can use this to handle slide out panels that are represented by hidden views
  // Above/Below/Right/Left (self.focusViewTop and friends) which are subviews the main focus View.
  // So detect the focus request, post direction message to core and cancel tvOS focus update.

  // check remote/siri idler
  // start/restart the idle timer
  // if was idle, ignore focus change till next time,
  // this is to trap out spurious taps/pans/swipes
  // when the siri remote is picked up.
  bool remoteIdleState = m_remoteIdleState;
  [self startRemoteTimer];
  if (remoteIdleState)
    return NO;

  CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext: focusActionType %s", focusActionTypeNames[focusActionType]);
  // do not allow focus changes when playing video
  // we handle those directly. Otherwise taps/swipes will cause wild seeks.
  if ([self hasPlayerProgressScrubbing])
    return NO;

  // previouslyFocusedItem may be nil if no item was focused.
  CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext: previous %p, next %p",
    context.previouslyFocusedItem, context.nextFocusedItem);

  if (focusActionType == FocusActionSwipe)
  {
    swipeCounter++;
    // swipes are the problem child :)
    if (swipeNoMore)
      return NO;

    CGRect previousItemRect = ((FocusLayerView*)context.previouslyFocusedItem).bounds;
    CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext: previousItemRect %f, %f, %f, %f",
      previousItemRect.origin.x, previousItemRect.origin.y,
      previousItemRect.origin.x + previousItemRect.size.width,
      previousItemRect.origin.y + previousItemRect.size.height);

    CGRect nextFocusedItemRect = ((FocusLayerView*)context.nextFocusedItem).bounds;
    CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext: nextFocusedItemRect %f, %f, %f, %f",
      nextFocusedItemRect.origin.x, nextFocusedItemRect.origin.y,
      nextFocusedItemRect.origin.x + nextFocusedItemRect.size.width,
      nextFocusedItemRect.origin.y + nextFocusedItemRect.size.height);
/*
    if (CGRectEqualToRect(nextFocusedItemRect, self.focusView.frame))
    {
      if (swipeCounter > 1)
      {
        swipeNoMore = true;
        //[self setNeedsFocusUpdate];
        return NO;
      }
    }
*/
    if (!CGRectIntersectsRect(swipeStartingParentViewRect, nextFocusedItemRect))
    {
      if (context.nextFocusedItem == self.focusViewTop ||
          context.nextFocusedItem == self.focusViewLeft ||
          context.nextFocusedItem == self.focusViewRight ||
          context.nextFocusedItem == self.focusViewBottom )
      {
        CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext: Hit in borderView");
        [self setNeedsFocusUpdate];
      }
      else
      {
        swipeNoMore = true;
        CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext: Not in same parent view");
        switch (context.focusHeading)
        {
          case UIFocusHeadingUp:
            [self sendButtonPressed:SiriRemote_UpTap];
            break;
          case UIFocusHeadingDown:
            [self sendButtonPressed:SiriRemote_DownTap];
            break;
          case UIFocusHeadingLeft:
            [self sendButtonPressed:SiriRemote_LeftTap];
            break;
          case UIFocusHeadingRight:
            [self sendButtonPressed:SiriRemote_RightTap];
            break;
        }
        [self setNeedsFocusUpdate];
        return NO;
      }
    }
  }
  
  switch (context.focusHeading)
  {
    case UIFocusHeadingUp:
      CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext:UIFocusHeadingUp");
      if (context.nextFocusedItem == self.focusViewTop)
      {
        [self sendButtonPressed:SiriRemote_UpTap];
        return NO;
      }
      break;
    case UIFocusHeadingDown:
      CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext:UIFocusHeadingDown");
      if (context.nextFocusedItem == self.focusViewBottom)
      {
        [self sendButtonPressed:SiriRemote_DownTap];
        return NO;
      }
      break;
    case UIFocusHeadingLeft:
      CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext:UIFocusHeadingLeft");
      if (context.nextFocusedItem == self.focusViewLeft)
      {
        [self sendButtonPressed:SiriRemote_LeftTap];
        return NO;
      }
      break;
    case UIFocusHeadingRight:
      CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext:UIFocusHeadingRight");
      if (context.nextFocusedItem == self.focusViewRight)
      {
        [self sendButtonPressed:SiriRemote_RightTap];
        return NO;
      }
      break;
    case UIFocusHeadingNone:
      CLog::Log(LOGDEBUG, "shouldUpdateFocusInContext:UIFocusHeadingNone");
      break;
    case UIFocusHeadingNext:
    case UIFocusHeadingPrevious:
      break;
  }
  return [super shouldUpdateFocusInContext:context];
}
//--------------------------------------------------------------
- (FocusLayerView*)findParentView:(FocusLayerView *)thisView
{
  FocusLayerView *parentView = nullptr;
  for (auto viewIt = _focusLayer.views.begin(); viewIt != _focusLayer.views.end(); ++viewIt)
  {
    auto &views = *viewIt;
    for (size_t bndx = 0; bndx < views.items.size(); ++bndx)
    {
      if (thisView->core == views.items[bndx].core)
      {
        parentView = views.view;
        break;
      }
    }
  }
  return parentView;
}
//--------------------------------------------------------------
- (void)clearSubViews
{
  NSArray *subviews = self.focusView.subviews;
  if (subviews && [subviews count])
  {
    for (UIView *view in subviews)
    {
      if (view == self.focusViewLeft)
        continue;
      if (view == self.focusViewRight)
        continue;
      if (view == self.focusViewTop)
        continue;
      if (view == self.focusViewBottom)
        continue;
      [view removeFromSuperview];
    }
  }
}
//--------------------------------------------------------------
- (void)debugSubViews
{
  NSArray *subviews = self.focusView.subviews;
  if (subviews && [subviews count])
  {
    for (UIView *view in subviews)
    {
      LOG(@"debugSubViews: %@", view);
    }
  }
}

//--------------------------------------------------------------
- (void) initFocusLayerViews:(std::vector<FocusLayerControl>&)focusViews
  withCoreViews:(std::vector<FocusEngineCoreViews>&)coreViews
{
  // build through our views in reverse order (so that last (window) is first)
  for (auto viewIt = coreViews.rbegin(); viewIt != coreViews.rend(); ++viewIt)
  {
    auto &viewItem = *viewIt;
    // m_glView.bounds does not have screen scaling
    CGRect rect = CGRectMake(
      viewItem.rect.x1/m_screenScale, viewItem.rect.y1/m_screenScale,
      viewItem.rect.Width()/m_screenScale, viewItem.rect.Height()/m_screenScale);

    FocusLayerControl focusView;
    focusView.rect = rect;
    focusView.type = viewItem.type;
    focusView.core = viewItem.control;
    focusView.view = nil;
    for (auto itemsIt = viewItem.items.begin(); itemsIt != viewItem.items.end(); ++itemsIt)
    {
      auto &item = *itemsIt;
      // m_glView.bounds does not have screen scaling
      CGRect rect = CGRectMake(
        item.rect.x1/m_screenScale, item.rect.y1/m_screenScale,
        item.rect.Width()/m_screenScale, item.rect.Height()/m_screenScale);

      FocusLayerItem focusItem;
      focusItem.rect = rect;
      focusItem.type = item.type;
      focusItem.core = item.control;
      focusItem.view = nil;
      focusView.items.push_back(focusItem);
    }
    focusViews.push_back(focusView);
  }
}

//--------------------------------------------------------------
- (void) loadFocusLayerViews:(std::vector<FocusLayerControl>&)focusViews
{
  // build up new focusLayer from core items.
  [self clearSubViews];

#if dumpviewsonload
  if (!focusViews.empty())
    CLog::Log(LOGDEBUG, "updateFocusLayer: begin");
#endif

  bool hasPlayerProgressScrubbing = [self hasPlayerProgressScrubbing] && g_application.m_pPlayer->IsPaused();
  int viewCount = 0;
  for (auto viewsIt = focusViews.begin(); viewsIt != focusViews.end(); ++viewsIt)
  {
    auto &view = *viewsIt;

    FocusLayerView *focusLayerView = nil;
    focusLayerView = [[FocusLayerView alloc] initWithFrame:view.rect];
    [focusLayerView setFocusable:false];
    if (view.type == "window" || view.type == "dialog")
    {
      [focusLayerView setFocusable:true];
      [focusLayerView setViewVisible:false];
    }
    focusLayerView->core = view.core;
    view.view = focusLayerView;
    [self.focusView addSubview:focusLayerView];
#if dumpviewsonload
    CLog::Log(LOGDEBUG, "updateFocusLayer: %d, %s, %f, %f, %f, %f",
      viewCount, view.type.c_str(),
      view.rect.origin.x, view.rect.origin.y,
      view.rect.origin.x + view.rect.size.width, view.rect.origin.y + view.rect.size.height);
#endif
    for (auto itemsIt = view.items.begin(); itemsIt != view.items.end(); ++itemsIt)
    {
      auto &item = *itemsIt;
      FocusLayerView *focusLayerItem = nil;
      if (hasPlayerProgressScrubbing && item.type == "progress")
        focusLayerItem = [[FocusLayerViewPlayerProgress alloc] initWithFrame:item.rect];
      else
        focusLayerItem = [[FocusLayerView alloc] initWithFrame:item.rect];
      [focusLayerItem setFocusable:true];
      focusLayerItem->core = item.core;
      item.view = focusLayerItem;
      [self.focusView addSubview:focusLayerItem];
#if dumpviewsonload
      CLog::Log(LOGDEBUG, "updateFocusLayer: %d, %s, %f, %f, %f, %f",
        viewCount, item.type.c_str(),
        item.rect.origin.x, item.rect.origin.y,
        item.rect.origin.x + item.rect.size.width, item.rect.origin.y + item.rect.size.height);
#endif
    }
    viewCount++;
  }
  _focusLayer.views = focusViews;
  [self updateFocusLayerInFocus];
}
//--------------------------------------------------------------
- (bool) updateFocusLayerInFocus
{
  FocusLayerControl oldItem = _focusLayer.infocus;
  FocusLayerControl preferredItem;
  // default to focusView and in focus control
  preferredItem.view = self.focusView;
  preferredItem.core = CFocusEngineHandler::GetInstance().GetFocusControl();
  if (preferredItem.core)
  {
    CGUIControl *guiControlWindow = (CGUIControl*)preferredItem.core;
    if (guiControlWindow->GetID() == WINDOW_FULLSCREEN_VIDEO)
    {
      if (g_windowManager.IsWindowVisible(WINDOW_DIALOG_SEEK_BAR))
      {
        for (size_t andx = 0; andx < _focusLayer.views.size(); ++andx)
        {
          for (size_t indx = 0; indx < _focusLayer.views[andx].items.size(); ++indx)
          {
            CGUIControl *guiControl = (CGUIControl*)_focusLayer.views[andx].items[indx].core;
            if (guiControl->GetControlType() == CGUIControl::GUICONTROL_PROGRESS)
            {
              preferredItem.type = _focusLayer.views[andx].items[indx].type;
              preferredItem.rect = _focusLayer.views[andx].items[indx].rect;
              preferredItem.view = _focusLayer.views[andx].items[indx].view;
              preferredItem.core = _focusLayer.views[andx].items[indx].core;
              _focusLayer.infocus = preferredItem;
              return (_focusLayer.infocus.view != oldItem.view);
            }
          }
        }
      }
    }
  }

  bool continueLooping = true;
  for (size_t andx = 0; andx < _focusLayer.views.size() && continueLooping; ++andx)
  {
    if (preferredItem.core == _focusLayer.views[andx].core)
    {
      preferredItem = _focusLayer.views[andx];
      break;
    }
    for (size_t bndx = 0; bndx < _focusLayer.views[andx].items.size(); ++bndx)
    {
      if (preferredItem.core == _focusLayer.views[andx].items[bndx].core)
      {
        preferredItem.type = _focusLayer.views[andx].items[bndx].type;
        preferredItem.rect = _focusLayer.views[andx].items[bndx].rect;
        preferredItem.view = _focusLayer.views[andx].items[bndx].view;
        // we don't really have to set core, but do it for completeness
        preferredItem.core = (CGUIControl*)_focusLayer.views[andx].items[bndx].core;
        preferredItem.items = _focusLayer.views[andx].items;
        continueLooping = false;
        break;
      }
    }
  }
  // setup the 'in focus' view
  _focusLayer.infocus = preferredItem;
  return (_focusLayer.infocus.view != oldItem.view);
}
//--------------------------------------------------------------
- (void) updateFocusLayerMainThread
{
  if (m_animating && !m_nativeKeyboardActive)
    [self performSelectorOnMainThread:@selector(updateFocusLayer) withObject:nil  waitUntilDone:NO];
}
//--------------------------------------------------------------
- (void) updateFocusLayer
{
  bool needUpdate = false;
  bool isBusy = CFocusEngineHandler::GetInstance().IsBusy();
  bool hideViews = CFocusEngineHandler::GetInstance().NeedToHideViews();
  std::vector<FocusEngineCoreViews> coreViews;
  CFocusEngineHandler::GetInstance().GetCoreViews(coreViews);
  if (isBusy || hideViews || coreViews.empty())
  {
    // if views are empty, we need a focusable focusView
    // or we unhook from the gestureRecognizer that traps
    // UIPressTypeMenu and we will bounce out to tvOS home.
    if (isBusy || coreViews.empty())
      [self.focusView setFocusable:true];
    _focusLayer.Reset();
    [self clearSubViews];
    [self updateFocusLayerInFocus];
    needUpdate = true;
  }
  else
  {
    // revert enable of focus for focusView (see above)
    // if we have built views, we need focusView set
    // to canBecomeFocused == NO
    if ( [self.focusView canBecomeFocused] == YES )
      [self.focusView setFocusable:false];
    // this is deep 'is equals' comparison
    // has to match in order and content.
    std::vector<FocusLayerControl> focusViews;
    [self initFocusLayerViews:focusViews withCoreViews:coreViews];
    if (FocusLayerViewsAreEqual(focusViews, _focusLayer.views))
    {
      needUpdate = [self updateFocusLayerInFocus];
    }
    else
    {
      [self loadFocusLayerViews:focusViews];
      needUpdate = true;
      //CLog::Log(LOGDEBUG, "updateFocusLayer:hideViews(%s), rebuild", hideViews ? "yes":"no");
    }
  }
  if (needUpdate)
  {
    [self.focusView setNeedsDisplay];
    // if the focus update is accepted by the focus engine,
    // focus is reset to the preferred focused view
    [self setNeedsFocusUpdate];
    // tells the focus engine to force a focus update immediately
    [self updateFocusIfNeeded];
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

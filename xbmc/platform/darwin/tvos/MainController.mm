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
#import "FileItem.h"
#import "MusicInfoTag.h"
#import "SpecialProtocol.h"
#import "PlayList.h"
#import "TextureCache.h"

#import "guilib/GUIWindowManager.h"
#import "input/Key.h"
#import "interfaces/AnnouncementManager.h"
#import "messaging/ApplicationMessenger.h"

#import "platform/darwin/AutoPool.h"
#import "platform/darwin/NSLogDebugHelpers.h"
#import "platform/darwin/tvos/MainEAGLView.h"
#import "platform/darwin/tvos/MainController.h"
#import "platform/darwin/tvos/MainApplication.h"
#import "platform/MCRuntimeLib.h"
#import "platform/MCRuntimeLibContext.h"
#import "utils/Variant.h"
#import "windowing/WindowingFactory.h"

#import <MediaPlayer/MPMediaItem.h>
#import <MediaPlayer/MPNowPlayingInfoCenter.h>

using namespace KODI::MESSAGING;

MainController *g_xbmcController;

// notification messages
extern NSString* kBRScreenSaverActivated;
extern NSString* kBRScreenSaverDismissed;

id objectFromVariant(const CVariant &data);

NSArray *arrayFromVariantArray(const CVariant &data)
{
  if (!data.isArray())
    return nil;
  NSMutableArray *array = [[[NSMutableArray alloc] initWithCapacity:data.size()] autorelease];
  for (CVariant::const_iterator_array itr = data.begin_array(); itr != data.end_array(); ++itr)
    [array addObject:objectFromVariant(*itr)];

  return array;
}

NSDictionary *dictionaryFromVariantMap(const CVariant &data)
{
  if (!data.isObject())
    return nil;
  NSMutableDictionary *dict = [[[NSMutableDictionary alloc] initWithCapacity:data.size()] autorelease];
  for (CVariant::const_iterator_map itr = data.begin_map(); itr != data.end_map(); ++itr)
    [dict setValue:objectFromVariant(itr->second) forKey:[NSString stringWithUTF8String:itr->first.c_str()]];

  return dict;
}

id objectFromVariant(const CVariant &data)
{
  if (data.isNull())
    return nil;
  if (data.isString())
    return [NSString stringWithUTF8String:data.asString().c_str()];
  if (data.isWideString())
    return [NSString stringWithCString:(const char *)data.asWideString().c_str() encoding:NSUnicodeStringEncoding];
  if (data.isInteger())
    return [NSNumber numberWithLongLong:data.asInteger()];
  if (data.isUnsignedInteger())
    return [NSNumber numberWithUnsignedLongLong:data.asUnsignedInteger()];
  if (data.isBoolean())
    return [NSNumber numberWithInt:data.asBoolean()?1:0];
  if (data.isDouble())
    return [NSNumber numberWithDouble:data.asDouble()];
  if (data.isArray())
    return arrayFromVariantArray(data);
  if (data.isObject())
    return dictionaryFromVariantMap(data);

  return nil;
}

void AnnounceBridge(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  //LOG(@"AnnounceBridge: [%s], [%s], [%s]", ANNOUNCEMENT::AnnouncementFlagToString(flag), sender, message);
  NSDictionary *dict = dictionaryFromVariantMap(data);
  //LOG(@"data: %@", dict.description);
  const std::string msg(message);
  if (msg == "OnPlay")
  {
    NSDictionary *item = [dict valueForKey:@"item"];
    NSDictionary *player = [dict valueForKey:@"player"];
    [item setValue:[player valueForKey:@"speed"] forKey:@"speed"];
    std::string thumb = g_application.CurrentFileItem().GetArt("thumb");
    if (!thumb.empty())
    {
      bool needsRecaching;
      std::string cachedThumb(CTextureCache::GetInstance().CheckCachedImage(thumb, false, needsRecaching));
      //LOG("thumb: %s, %s", thumb.c_str(), cachedThumb.c_str());
      if (!cachedThumb.empty())
      {
        std::string thumbRealPath = CSpecialProtocol::TranslatePath(cachedThumb);
        [item setValue:[NSString stringWithUTF8String:thumbRealPath.c_str()] forKey:@"thumb"];
      }
    }
    double duration = g_application.GetTotalTime();
    if (duration > 0)
      [item setValue:[NSNumber numberWithDouble:duration] forKey:@"duration"];
    [item setValue:[NSNumber numberWithDouble:g_application.GetTime()] forKey:@"elapsed"];
    int current = g_playlistPlayer.GetCurrentSong();
    if (current >= 0)
    {
      [item setValue:[NSNumber numberWithInt:current] forKey:@"current"];
      [item setValue:[NSNumber numberWithInt:g_playlistPlayer.GetPlaylist(g_playlistPlayer.GetCurrentPlaylist()).size()] forKey:@"total"];
    }
    if (g_application.CurrentFileItem().HasMusicInfoTag())
    {
      const std::vector<std::string> &genre = g_application.CurrentFileItem().GetMusicInfoTag()->GetGenre();
      if (!genre.empty())
      {
        NSMutableArray *genreArray = [[NSMutableArray alloc] initWithCapacity:genre.size()];
        for(std::vector<std::string>::const_iterator it = genre.begin(); it != genre.end(); ++it)
        {
          [genreArray addObject:[NSString stringWithUTF8String:it->c_str()]];
        }
        [item setValue:genreArray forKey:@"genre"];
      }
    }
    //LOG(@"item: %@", item.description);
    [g_xbmcController performSelectorOnMainThread:@selector(onPlay:) withObject:item  waitUntilDone:NO];
  }
  else if (msg == "OnSpeedChanged" || msg == "OnPause")
  {
    NSDictionary *item = [dict valueForKey:@"item"];
    NSDictionary *player = [dict valueForKey:@"player"];
    [item setValue:[player valueForKey:@"speed"] forKey:@"speed"];
    [item setValue:[NSNumber numberWithDouble:g_application.GetTime()] forKey:@"elapsed"];
    //LOG(@"item: %@", item.description);
    [g_xbmcController performSelectorOnMainThread:@selector(OnSpeedChanged:) withObject:item  waitUntilDone:NO];
    if (msg == "OnPause")
      [g_xbmcController performSelectorOnMainThread:@selector(onPause:) withObject:[dict valueForKey:@"item"]  waitUntilDone:NO];
  }
  else if (msg == "OnStop")
  {
    [g_xbmcController performSelectorOnMainThread:@selector(onStop:) withObject:[dict valueForKey:@"item"]  waitUntilDone:NO];
  }
}

class AnnounceReceiver : public ANNOUNCEMENT::IAnnouncer
{
public:
  virtual void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
  {
    // not all Announce called from xbmc main thread, we need an auto poll here.
    CCocoaAutoPool pool;
    AnnounceBridge(flag, sender, message, data);
  }
  virtual ~AnnounceReceiver() {}
  static void init()
  {
    if (NULL==g_announceReceiver) {
      g_announceReceiver = new AnnounceReceiver();
      ANNOUNCEMENT::CAnnouncementManager::GetInstance().AddAnnouncer(g_announceReceiver);
    }
  }
  static void dealloc()
  {
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().RemoveAnnouncer(g_announceReceiver);
    delete g_announceReceiver;
  }
private:
  AnnounceReceiver() {}
  static AnnounceReceiver *g_announceReceiver;
};

AnnounceReceiver *AnnounceReceiver::g_announceReceiver = NULL;

//--------------------------------------------------------------
//

#pragma mark - MainController interface
@interface MainController ()
@property (strong, nonatomic) NSTimer *pressAutoRepeatTimer;

- (void)rescheduleNetworkAutoSuspend;
@end

#pragma mark - MainController implementation
@implementation MainController

@synthesize m_lastGesturePoint;
@synthesize m_screenScale;
@synthesize m_touchBeginSignaled;
@synthesize m_touchDirection;
@synthesize m_currentKey;
@synthesize m_screenIdx;
@synthesize m_currentClick;
@synthesize m_screensize;
@synthesize m_networkAutoSuspendTimer;
@synthesize m_nowPlayingInfo;
@synthesize m_clickResetPan;

// set to 1 to enable new remote function, to be tested for 1.0.2
#define NEW_REMOTE_HANDLING 1

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
    [self.pressAutoRepeatTimer release];
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

- (XBMCKey)getPanDirection:(CGPoint)translation
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
// enable for new remote function
#if (NEW_REMOTE_HANDLING)
  if ([gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]] && ([otherGestureRecognizer isKindOfClass:[UISwipeGestureRecognizer class]] || [otherGestureRecognizer isKindOfClass:[UITapGestureRecognizer class]]))
  {
    return YES;
  }
#endif
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
      // menu is special.
      //  a) if at our home view, should return to atv home screen.
      //  b) if not, let it pass to us.
      if (g_windowManager.GetActiveWindow() == WINDOW_HOME && g_windowManager.GetFocusedWindow() != WINDOW_DIALOG_FAVOURITES)
        handled = NO;
      break;

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
      return NO;
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
  [swipeLeft release];

  //single finger swipe right
  UISwipeGestureRecognizer *swipeRight = [[UISwipeGestureRecognizer alloc]
                                          initWithTarget:self action:@selector(handleSwipe:)];

  swipeRight.delaysTouchesBegan = NO;
  swipeRight.direction = UISwipeGestureRecognizerDirectionRight;
  swipeRight.delegate = self;
  [m_glView addGestureRecognizer:swipeRight];
  [swipeRight release];

  //single finger swipe up
  UISwipeGestureRecognizer *swipeUp = [[UISwipeGestureRecognizer alloc]
                                       initWithTarget:self action:@selector(handleSwipe:)];

  swipeUp.delaysTouchesBegan = NO;
  swipeUp.direction = UISwipeGestureRecognizerDirectionUp;
  swipeUp.delegate = self;
  [m_glView addGestureRecognizer:swipeUp];
  [swipeUp release];

  //single finger swipe down
  UISwipeGestureRecognizer *swipeDown = [[UISwipeGestureRecognizer alloc]
                                         initWithTarget:self action:@selector(handleSwipe:)];

  swipeDown.delaysTouchesBegan = NO;
  swipeDown.direction = UISwipeGestureRecognizerDirectionDown;
  swipeDown.delegate = self;
  [m_glView addGestureRecognizer:swipeDown];
  [swipeDown release];
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
  [pan release];
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
  [upRecognizer release];
  
  auto downRecognizer = [[UITapGestureRecognizer alloc]
                         initWithTarget: self action: @selector(tapDownArrowPressed:)];
  downRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeDownArrow]];
  downRecognizer.delegate = self;
  [m_glView addGestureRecognizer: downRecognizer];
  [downRecognizer release];
  
  auto leftRecognizer = [[UITapGestureRecognizer alloc]
                         initWithTarget: self action: @selector(tapLeftArrowPressed:)];
  leftRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeLeftArrow]];
  leftRecognizer.delegate = self;
  [m_glView addGestureRecognizer: leftRecognizer];
  [leftRecognizer release];
  
  auto rightRecognizer = [[UITapGestureRecognizer alloc]
                          initWithTarget: self action: @selector(tapRightArrowPressed:)];
  rightRecognizer.allowedPressTypes  = @[[NSNumber numberWithInteger:UIPressTypeRightArrow]];
  rightRecognizer.delegate = self;
  [m_glView addGestureRecognizer: rightRecognizer];
  [rightRecognizer release];
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
  [upRecognizer release];

  auto downRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteDownArrowPressed:)];
  downRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeDownArrow]];
  downRecognizer.minimumPressDuration = 0.01;
  downRecognizer.delegate = self;
  [self.view addGestureRecognizer: downRecognizer];
  [downRecognizer release];

  auto leftRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteLeftArrowPressed:)];
  leftRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeLeftArrow]];
  leftRecognizer.minimumPressDuration = 0.01;
  leftRecognizer.delegate = self;
  [self.view addGestureRecognizer: leftRecognizer];
  [leftRecognizer release];

  auto rightRecognizer = [[UILongPressGestureRecognizer alloc]
    initWithTarget: self action: @selector(IRRemoteRightArrowPressed:)];
  rightRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeRightArrow]];
  rightRecognizer.minimumPressDuration = 0.01;
  rightRecognizer.delegate = self;
  [self.view addGestureRecognizer: rightRecognizer];
  [rightRecognizer release];
  
  // we always have these under tvos
  auto menuRecognizer = [[UITapGestureRecognizer alloc]
                         initWithTarget: self action: @selector(menuPressed:)];
  menuRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeMenu]];
  menuRecognizer.delegate  = self;
  [m_glView addGestureRecognizer: menuRecognizer];
  [menuRecognizer release];
  
  auto playPauseRecognizer = [[UITapGestureRecognizer alloc]
                              initWithTarget: self action: @selector(playPausePressed:)];
  playPauseRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypePlayPause]];
  playPauseRecognizer.delegate  = self;
  [m_glView addGestureRecognizer: playPauseRecognizer];
  [playPauseRecognizer release];
  
  auto selectRecognizer = [[UILongPressGestureRecognizer alloc]
                          initWithTarget: self action: @selector(selectPressed:)];
  selectRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeSelect]];
  selectRecognizer.minimumPressDuration = 0.01;
  selectRecognizer.delegate = self;
  [self.view addGestureRecognizer: selectRecognizer];
  [selectRecognizer release];
  
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
        CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_STOP);
      else
        [self sendKeyDownUp:XBMCK_BACKSPACE];
      break;
    default:
      break;
  }
}
//--------------------------------------------------------------
- (void)selectPressed:(UITapGestureRecognizer *)sender
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
      break;
    default:
      break;
  }
}

//--------------------------------------------------------------
- (IBAction)tapUpArrowPressed:(UIGestureRecognizer *)sender
{
  [self sendKeyDownUp:XBMCK_UP];
}
//--------------------------------------------------------------
- (IBAction)tapDownArrowPressed:(UIGestureRecognizer *)sender
{
  [self sendKeyDownUp:XBMCK_DOWN];
}
//--------------------------------------------------------------
- (IBAction)tapLeftArrowPressed:(UIGestureRecognizer *)sender
{
  [self sendKeyDownUp:XBMCK_LEFT];
}
//--------------------------------------------------------------
- (IBAction)tapRightArrowPressed:(UIGestureRecognizer *)sender
{
  [self sendKeyDownUp:XBMCK_RIGHT];
}

//--------------------------------------------------------------
- (IBAction)handlePan:(UIPanGestureRecognizer *)sender 
{
  if (m_appAlive == YES) //NO GESTURES BEFORE WE ARE UP AND RUNNING
  {
#if (NEW_REMOTE_HANDLING)
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
      
      key = [self getPanDirection:gesturePoint];
      
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
      break;
    default:
      break;
  }
}
#else
  XBMCKey key;
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
    {
      m_touchBeginSignaled = false;
      break;
    }
    case UIGestureRecognizerStateChanged:
    {
      if (!m_touchBeginSignaled && m_touchDirection)
      {
        switch (m_touchDirection)
        {
          case UISwipeGestureRecognizerDirectionRight:
            key = XBMCK_RIGHT;
            break;
          case UISwipeGestureRecognizerDirectionLeft:
            key = XBMCK_LEFT;
            break;
          case UISwipeGestureRecognizerDirectionUp:
            key = XBMCK_UP;
            break;
          case UISwipeGestureRecognizerDirectionDown:
            key = XBMCK_DOWN;
            break;
          default:
            break;
        }
        // ignore UP/DOWN swipes while in full screen playback
        if (g_windowManager.GetFocusedWindow() != WINDOW_FULLSCREEN_VIDEO ||
            key == XBMCK_LEFT ||
            key == XBMCK_RIGHT)
        {
          m_touchBeginSignaled = true;
          [self startKeyPressTimer:key];
        }
      }
      break;
    }
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
      if (m_touchBeginSignaled)
      {
        m_touchBeginSignaled = false;
        m_touchDirection = NULL;
        [self stopKeyPressTimer];
        [self sendKeyUp:key];
      }
      break;
    default:
      break;
  }
}
#endif
}

//--------------------------------------------------------------
- (IBAction)handleSwipe:(UISwipeGestureRecognizer *)sender
{
  if(m_appAlive == YES)//NO GESTURES BEFORE WE ARE UP AND RUNNING
  {
#if (NEW_REMOTE_HANDLING)
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
#else
    // remove if we go with new remote function
    m_touchDirection = [sender direction];
#endif
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
  //PRINT_SIGNATURE();
  m_screenIdx = 0;
  self = [super init];
  if (!self)
    return nil;

  m_pause = FALSE;
  m_appAlive = FALSE;
  m_animating = FALSE;
  m_readyToRun = FALSE;

  m_isPlayingBeforeInactive = NO;
  m_bgTask = UIBackgroundTaskInvalid;
  m_playbackState = TVOS_PLAYBACK_STOPPED;

  m_window = [[UIWindow alloc] initWithFrame:frame];
  [m_window setRootViewController:self];  
  m_window.screen = screen;
  m_window.backgroundColor = [UIColor blackColor];
  // Turn off autoresizing
  m_window.autoresizingMask = 0;
  m_window.autoresizesSubviews = NO;

  [self enableScreenSaver];

  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center addObserver: self
     selector: @selector(observeDefaultCenterStuff:) name: nil object: nil];

  [m_window makeKeyAndVisible];
  g_xbmcController = self;  
  
  AnnounceReceiver::init();

  return self;
}
//--------------------------------------------------------------
- (void)dealloc
{
  // stop background task
  [m_networkAutoSuspendTimer invalidate];
  [self enableNetworkAutoSuspend:nil];
  
  AnnounceReceiver::dealloc();
  [self stopAnimation];
  [m_glView release];
  [m_window release];
  
  NSNotificationCenter *center;
  // take us off the default center for our app
  center = [NSNotificationCenter defaultCenter];
  [center removeObserver: self];
  
  [super dealloc];
}
//--------------------------------------------------------------
- (void)loadView
{
  [super loadView];

  self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.view.autoresizesSubviews = YES;
  
  m_glView = [[MainEAGLView alloc] initWithFrame:self.view.bounds withScreen:[UIScreen mainScreen]];

  // Check if screen is Retina
  m_screenScale = [[UIScreen mainScreen] nativeScale];

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
}
//--------------------------------------------------------------
- (void)viewWillAppear:(BOOL)animated
{
  //PRINT_SIGNATURE();
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
  //PRINT_SIGNATURE();
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
  return [[[UIView alloc] initWithFrame:CGRectZero] autorelease];
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
  // Releases the view if it doesn't have a superview.
  [super didReceiveMemoryWarning];
  // Release any cached data, images, etc. that aren't in use.
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
}
//--------------------------------------------------------------
- (void)enableScreenSaver
{
  m_disableIdleTimer = NO;
  [[UIApplication sharedApplication] setIdleTimerDisabled:NO];
}

//--------------------------------------------------------------
- (void)resetSystemIdleTimer
{
  //PRINT_SIGNATURE();
  // this is silly :)
  // when system screen saver kicks off, we switch to UIApplicationStateInactive, the only way
  // to get out of the screensaver is to call ourself to open an custom URL that is registered
  // in our Info.plist. The openURL method of UIApplication must be supported but we can just
  // reply NO and we get restored to UIApplicationStateActive.
  if ([UIApplication sharedApplication].applicationState == UIApplicationStateInactive)
  {
    NSURL *url = [NSURL URLWithString:@"mrmc://wakeup"];
    [[UIApplication sharedApplication] openURL:url];
  }
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
  NSMutableArray *array = [[[NSMutableArray alloc] initWithCapacity:1] autorelease];
  [array addObject:[screen currentMode]];
  return array;
}

//--------------------------------------------------------------
- (bool)changeScreen:(unsigned int)screenIdx withMode:(UIScreenMode *)mode
{
  return true;
}
//--------------------------------------------------------------
- (void)enterBackground
{
  //PRINT_SIGNATURE();
  if (g_application.m_pPlayer->IsPlaying() && !g_application.m_pPlayer->IsPaused())
  {
    m_isPlayingBeforeInactive = YES;
    CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_PAUSE_IF_PLAYING);
  }
  g_Windowing.OnAppFocusChange(false);
}

- (void)enterForeground
{
  //PRINT_SIGNATURE();
  g_Windowing.OnAppFocusChange(true);
  // when we come back, restore playing if we were.
  if (m_isPlayingBeforeInactive)
  {
    CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_UNPAUSE);
    m_isPlayingBeforeInactive = NO;
  	[self disableScreenSaver];
  }
  else
  {
    [self enableScreenSaver];
  }
}

- (void)becomeInactive
{
  //PRINT_SIGNATURE();
  // if we were interrupted, already paused here
  // else if user background us or lock screen, only pause video here, audio keep playing.
  if (g_application.m_pPlayer->IsPlayingVideo() &&
     !g_application.m_pPlayer->IsPaused())
  {
    m_isPlayingBeforeInactive = YES;
    CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_PAUSE_IF_PLAYING);
  }
  // check whether we need disable network auto suspend.
  [self rescheduleNetworkAutoSuspend];
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
  if (m_animating == NO && [m_glView getContext])
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
  CCocoaAutoPool outerpool;
  [[NSThread currentThread] setName:@"MCRuntimeLib"];
  
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
  //PRINT_SIGNATURE();
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
  //PRINT_SIGNATURE();
  if (m_bgTask != UIBackgroundTaskInvalid)
  {
    [[UIApplication sharedApplication] endBackgroundTask: m_bgTask];
    m_bgTask = UIBackgroundTaskInvalid;
  }
}
//--------------------------------------------------------------
- (void)remoteControlReceivedWithEvent:(UIEvent*)receivedEvent
{
  //LOG(@"%s: type %ld, subtype: %d", __PRETTY_FUNCTION__, (long)receivedEvent.type, (int)receivedEvent.subtype);
  if (receivedEvent.type == UIEventTypeRemoteControl)
  {
    [self disableNetworkAutoSuspend];
    switch (receivedEvent.subtype)
    {
      case UIEventSubtypeRemoteControlTogglePlayPause:
        CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_PLAYPAUSE)));
        break;
      case UIEventSubtypeRemoteControlPlay:
        CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_PLAY)));
        break;
      case UIEventSubtypeRemoteControlPause:
        // ACTION_PAUSE sometimes cause unpause, use MediaPauseIfPlaying to make sure pause only
        //CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_PAUSE);
        // warning, something is wacky, in tvOS we only get this if play/pause button is pushed
        // the playPausePressed method should be getting called and it does, sometimes. WTF ?
        CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_PLAYPAUSE)));
        break;
      case UIEventSubtypeRemoteControlStop:
        CApplicationMessenger::GetInstance().SendMsg(TMSG_MEDIA_STOP);
        break;
      case UIEventSubtypeRemoteControlNextTrack:
        CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_NEXT_ITEM)));
        break;
      case UIEventSubtypeRemoteControlPreviousTrack:
        CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PREV_ITEM)));
        break;
      case UIEventSubtypeRemoteControlBeginSeekingForward:
        // use 4X speed forward.
        CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_FORWARD)));
        CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_FORWARD)));
        break;
      case UIEventSubtypeRemoteControlBeginSeekingBackward:
        // use 4X speed rewind.
        CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_REWIND)));
        CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_REWIND)));
        break;
      case UIEventSubtypeRemoteControlEndSeekingForward:
      case UIEventSubtypeRemoteControlEndSeekingBackward:
        // restore to normal playback speed.
        if (g_application.m_pPlayer->IsPlaying() && !g_application.m_pPlayer->IsPaused())
          CApplicationMessenger::GetInstance().SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PLAYER_PLAY)));
        break;
      default:
        //LOG(@"unhandled subtype: %d", (int)receivedEvent.subtype);
        break;
    }
    [self rescheduleNetworkAutoSuspend];
  }
}

#pragma mark - Now Playing routines
//--------------------------------------------------------------
- (void)setTVOSNowPlayingInfo:(NSDictionary *)info
{
  self.m_nowPlayingInfo = info;
  [[MPNowPlayingInfoCenter defaultCenter] setNowPlayingInfo:self.m_nowPlayingInfo];
}
//--------------------------------------------------------------
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
  NSArray *artists = [item objectForKey:@"artist"];
  if (artists && artists.count > 0)
    [dict setObject:[artists componentsJoinedByString:@" "] forKey:MPMediaItemPropertyArtist];
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
    NSString *thumb = [item objectForKey:@"thumb"];
    if (thumb && thumb.length > 0)
    {
      UIImage *image = [UIImage imageWithContentsOfFile:thumb];
      if (image)
      {
        /*
        MPMediaItemArtwork *mArt = [[MPMediaItemArtwork alloc] initWithImage:image];
        if (mArt)
        {
          [dict setObject:mArt forKey:MPMediaItemPropertyArtwork];
          [mArt release];
        }
        */
      }
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

  [self setTVOSNowPlayingInfo:dict];
  [dict release];

  m_playbackState = TVOS_PLAYBACK_PLAYING;
  [self disableNetworkAutoSuspend];
}
//--------------------------------------------------------------
- (void)OnSpeedChanged:(NSDictionary *)item
{
  //PRINT_SIGNATURE();
  if (NSClassFromString(@"MPNowPlayingInfoCenter"))
  {
    NSMutableDictionary *info = [self.m_nowPlayingInfo mutableCopy];
    NSNumber *elapsed = [item objectForKey:@"elapsed"];
    if (elapsed)
      [info setObject:elapsed forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    NSNumber *speed = [item objectForKey:@"speed"];
    if (speed)
      [info setObject:speed forKey:MPNowPlayingInfoPropertyPlaybackRate];

    [self setTVOSNowPlayingInfo:info];
  }
}
//--------------------------------------------------------------
- (void)onPause:(NSDictionary *)item
{
  //PRINT_SIGNATURE();
  m_playbackState = TVOS_PLAYBACK_PAUSED;
  // schedule set network auto suspend state for save power if idle.
  [self rescheduleNetworkAutoSuspend];
}
//--------------------------------------------------------------
- (void)onStop:(NSDictionary *)item
{
  //PRINT_SIGNATURE();
  [self setTVOSNowPlayingInfo:nil];

  m_playbackState = TVOS_PLAYBACK_STOPPED;
  // delay set network auto suspend state in case we are switching playing item.
  [self rescheduleNetworkAutoSuspend];
}
//--------------------------------------------------------------
- (void)rescheduleNetworkAutoSuspend
{
  //LOG(@"%s: playback state: %d", __PRETTY_FUNCTION__,  m_playbackState);
  if (m_playbackState == TVOS_PLAYBACK_PLAYING)
  {
    [self disableNetworkAutoSuspend];
    return;
  }
  if (m_networkAutoSuspendTimer)
    [m_networkAutoSuspendTimer invalidate];

  int delay = m_playbackState == TVOS_PLAYBACK_PAUSED ? 60 : 30;  // wait longer if paused than stopped
  self.m_networkAutoSuspendTimer = [NSTimer scheduledTimerWithTimeInterval:delay target:self selector:@selector(enableNetworkAutoSuspend:) userInfo:nil repeats:NO];
}

#pragma mark - private helper methods
- (void)observeDefaultCenterStuff:(NSNotification *)notification
{
//  LOG(@"default: %@", [notification name]);
//  LOG(@"userInfo: %@", [notification userInfo]);
}

@end

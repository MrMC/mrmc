/*
 *      Copyright (C) 2018 Team MrMC
 *      https://github.com/MrMC
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

#import "platform/darwin/tvos/FocusLayerViewPlayerProgress.h"

#import "Application.h"
#import "FileItem.h"
#import "messaging/ApplicationMessenger.h"
#import "platform/darwin/NSLogDebugHelpers.h"
#import "platform/darwin/tvos/ProgressThumbNailer.h"
#import "platform/darwin/tvos/FocusLayerViewPlayerProgressSettings.h"
#import "guilib/GUISliderControl.h"
#import "guilib/GUIWindowManager.h"
#import "video/VideoInfoTag.h"
#import "utils/MathUtils.h"
#import "utils/StringUtils.h"
#import "utils/log.h"

@interface FocusLayerViewPlayerProgress ()
@property (strong, nonatomic) NSTimer *pressAutoRepeatTimer;
@property (strong, nonatomic) NSTimer *remoteIdleTimer;
@end

#pragma mark - FocusLayerViewPlayerProgress implementation
@implementation FocusLayerViewPlayerProgress

- (id)initWithFrame:(CGRect)frame
{
  barRect = frame;
  // standard 16:9 video rect
  videoRect = CGRectMake(0, 0, 400, 225);

  CGRect screenRect = [UIScreen mainScreen].bounds;
  // see if we are in upper or lower screen area
  // we need to expand the view to include
  // drawing room for line going from thumb position
  // to thumbnail pict and thumbnail pict area
  if (barRect.origin.y > screenRect.size.height/2)
  {
    videoRectIsAboveBar = true;
    // if in lower area, expand up
    frame.origin.y -= videoRect.size.height + 10;
    frame.size.height += videoRect.size.height + 10;
  }
  else
  {
    videoRectIsAboveBar = false;
    // if in upper area, expand down
    frame.size.height += videoRect.size.height + 10;
  }
  // allow video thumb image to extend within 50 of left/right sides
  frame.origin.x -= videoRect.size.width/2;
  frame.size.width += videoRect.size.width;
  screenRect = CGRectInset(screenRect, 50, 0);
  frame = CGRectIntersection(frame, screenRect);

	self = [super initWithFrame:frame];
	if (self)
	{
    self._value = 0.0;

    self->min = 0.0;
    self->max = 100.0;
    self->distance = 100;
    self->thumb = 0.0;
    self->thumbConstant = 0.0;
    // controls the rate of deceleration,
    // ie. how fast the auto pan slows down
    self->decelerationRate = 0.50;
    self->decelerationMaxVelocity = 1000;
    self->totalTimeSeconds = -1;
    self->seekTimeSeconds = 0.0;
    self->thumbNailer = nullptr;
    self->slideDownView = nil;
    float percentage = 0.0;
    if (g_application.m_pPlayer->IsPlayingVideo())
    {
      CFileItem &fileitem = g_application.CurrentFileItem();
      if (fileitem.HasVideoInfoTag())
        self->totalTimeSeconds = fileitem.GetVideoInfoTag()->m_streamDetails.GetVideoDuration();
      if (self->totalTimeSeconds <= 0)
        self->totalTimeSeconds = g_application.GetTotalTime();
      // get percentage from application, includes stacks
      self->seekTimeSeconds = g_application.GetTime();
      percentage = self->seekTimeSeconds / self->totalTimeSeconds;
      self->thumbNailer = new CProgressThumbNailer(fileitem, 400, self);
      //TODO: grab initial thumb from renderer.
    }
    // initial slider position and kick off a thumb image gen
    [self setPercentage:percentage];
    thumbConstant = thumb;

    auto pan = [[UIPanGestureRecognizer alloc]
      initWithTarget:self action:@selector(handlePanGesture:)];
    pan.delegate = self;
    [self addGestureRecognizer:pan];

    auto leftRecognizer = [[UILongPressGestureRecognizer alloc]
      initWithTarget: self action: @selector(IRRemoteLeftArrowPressed:)];
    leftRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeLeftArrow]];
    leftRecognizer.minimumPressDuration = 0.01;
    leftRecognizer.delegate = self;
    [self addGestureRecognizer: leftRecognizer];

    auto rightRecognizer = [[UILongPressGestureRecognizer alloc]
      initWithTarget: self action: @selector(IRRemoteRightArrowPressed:)];
    rightRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeRightArrow]];
    rightRecognizer.minimumPressDuration = 0.01;
    rightRecognizer.delegate = self;
    [self addGestureRecognizer: rightRecognizer];

    auto downRecognizer = [[UILongPressGestureRecognizer alloc]
      initWithTarget: self action: @selector(IRRemoteDownArrowPressed:)];
    downRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeDownArrow]];
    downRecognizer.minimumPressDuration = 0.01;
    downRecognizer.delegate = self;
    [self addGestureRecognizer: downRecognizer];

    auto *swipeUp = [[UISwipeGestureRecognizer alloc]
    initWithTarget:self action:@selector(handleUpSwipeGesture:)];
    swipeUp.delaysTouchesBegan = NO;
    swipeUp.direction = UISwipeGestureRecognizerDirectionUp;
    swipeUp.delegate = self;
    [self  addGestureRecognizer:swipeUp];

    auto *swipeDown = [[UISwipeGestureRecognizer alloc]
    initWithTarget:self action:@selector(handleDownSwipeGesture:)];
    swipeDown.delaysTouchesBegan = NO;
    swipeDown.direction = UISwipeGestureRecognizerDirectionDown;
    swipeDown.delegate = self;
    [self  addGestureRecognizer:swipeDown];

  }
	return self;
}

- (void)removeFromSuperview
{
  [self->deceleratingTimer invalidate];
  SAFE_DELETE(self->thumbNailer);
  CGImageRelease(self->thumbImage.image);
  self->thumbImage.image = nil;
  [super removeFromSuperview];
}

- (double) value
{
  return self._value;
}

- (void) setValue:(double)newValue
{
  self._value = newValue;
  [self updateView];
}

- (void) setPercentage:(double)percentage
{
  if (percentage < 0)
    percentage = 0;
  if (percentage > 1)
    percentage = 1;
  self.value = (distance * percentage) + min;
  CLog::Log(LOGDEBUG, "PlayerProgress::set percentage(%f), value(%f)", percentage, self.value);
  if (self->thumbNailer)
  {
    self->seekTimeSeconds = percentage * self->totalTimeSeconds;
    self->thumbNailer->RequestThumbAsPercentage(100.0 * percentage);
  }
}

- (double) getSeekTimePercentage
{
  if (self->thumbNailer)
  {
    // take the seek time (ms) of the displayed thumb
    int seekTimeMilliSeconds = self->seekTimeSeconds * 1000;
    if (self->thumbImage.image)
      seekTimeMilliSeconds = self->thumbImage.time;
    if (seekTimeMilliSeconds < 0)
      seekTimeMilliSeconds = 0;
    int totalTimeMilliSeconds = self->totalTimeSeconds * 1000;
    if (seekTimeMilliSeconds > totalTimeMilliSeconds)
      seekTimeMilliSeconds = totalTimeMilliSeconds;
    double percentage = (double)seekTimeMilliSeconds / totalTimeMilliSeconds;
    CLog::Log(LOGDEBUG, "PlayerProgress::getSeekTimePercentage(%f), value(%f)", percentage, self.value);
    return 100.0 * percentage;
  }
  return -1;
}

//--------------------------------------------------------------
- (BOOL)canBecomeFocused
{
  if (self->slideDownView)
    return NO;

  return YES;
}
//--------------------------------------------------------------
- (void) drawRect:(CGRect)rect
{
  [super drawRect:rect];
  CGContextRef ctx = UIGraphicsGetCurrentContext();
#if 0
  CGContextSetLineWidth(ctx, 1.0);
  CGContextSetStrokeColorWithColor(ctx, [[UIColor whiteColor] CGColor]);
  CGContextStrokeRect(ctx, self.bounds);

  CGContextSetLineWidth(ctx, 1.0);
  CGContextSetStrokeColorWithColor(ctx, [[UIColor whiteColor] CGColor]);
  CGContextStrokeRect(ctx, barRect);

  CGContextSetLineWidth(ctx, 1.0);
  CGContextSetStrokeColorWithColor(ctx, [[UIColor orangeColor] CGColor]);
  CGContextStrokeRect(ctx, thumbRect);
#endif

  // draw the vertical tick mark in the bar to show current position
  CGContextSetStrokeColorWithColor(ctx, [[UIColor whiteColor] CGColor]);
  CGContextSetLineWidth(ctx, 2.0);
  CGPoint thumbPointerBGN = CGPointMake(CGRectGetMidX(thumbRect), CGRectGetMinY(thumbRect));
  CGPoint thumbPointerEND = CGPointMake(thumbPointerBGN.x, CGRectGetMaxY(thumbRect));
  CGContextMoveToPoint(ctx, thumbPointerBGN.x, thumbPointerBGN.y);
  CGContextAddLineToPoint(ctx, thumbPointerEND.x, thumbPointerEND.y);
  CGContextStrokePath(ctx);

  videoRect = CGRectMake(0, 0, 400, 225);
  videoRect.origin.x = CGRectGetMidX(thumbRect) - videoRect.size.width/2;
  if (videoRectIsAboveBar)
    videoRect.origin.y = thumbRect.origin.y - (videoRect.size.height + 10);
  else
    videoRect.origin.y = thumbRect.origin.y + (thumbRect.size.height + 10);
  // clamp left/right sides to left/right sides of bar
  if (CGRectGetMinX(videoRect) < CGRectGetMinX(self.bounds))
    videoRect.origin.x = self.bounds.origin.x;
  if (CGRectGetMaxX(videoRect) > CGRectGetMaxX(self.bounds))
    videoRect.origin.x = CGRectGetMaxX(self.bounds) - videoRect.size.width;

  if (self->thumbNailer)
  {
    ThumbNailerImage newThumbImage = self->thumbNailer->GetThumb();
    if (newThumbImage.image)
    {
      CGImageRelease(self->thumbImage.image);
      self->thumbImage = newThumbImage;
      CLog::Log(LOGDEBUG, "PlayerProgress::drawRect:got newThumbImage at %d", newThumbImage.time);
    }
  }

  CGRect videoBounds = videoRect;
  bool haveThumbImage = self->thumbImage.image != nil;
  if (haveThumbImage)
  {
    // image will be scaled, if necessary, to fit into rect
    // but we need to keep the correct aspect ration
    size_t width = CGImageGetWidth(self->thumbImage.image);
    size_t height = CGImageGetHeight(self->thumbImage.image);
    float aspect = (float)width / height;
    videoBounds.size.height = videoRect.size.width / aspect;
    if (videoRectIsAboveBar)
      videoBounds.origin.y += videoRect.size.height - videoBounds.size.height;
    else
      videoBounds.origin.y -= videoRect.size.height - videoBounds.size.height;

    // clear to black the under video area, might not need this
    CGContextSetFillColorWithColor(ctx, [[UIColor blackColor] CGColor]);
    CGContextFillRect(ctx, videoBounds);
    // now we can draw the video thumb image
    CGContextDrawImage(ctx, videoBounds, self->thumbImage.image);

    // draw a thin white frame around the video thumb image
    CGContextSetStrokeColorWithColor(ctx, [[UIColor whiteColor] CGColor]);
    CGContextSetLineWidth(ctx, 0.5);
    CGContextStrokeRect(ctx, videoBounds);
  }

  // always show time text (H:M:S)
  int imageTimeSeconds = self->seekTimeSeconds;
  /*
  if (haveThumbImage)
  {
    // prefer matching time with thumb image
    imageTimeSeconds = self->thumbImage.time / 1000;
  }
  */
  std::string timeString = StringUtils::SecondsToTimeString(imageTimeSeconds, TIME_FORMAT_HH_MM_SS);
  [self drawString:ctx withCString:timeString inRect:videoBounds drawFrame:!haveThumbImage];
}

- (void) drawString:(CGContextRef) ctx withCString:(const std::string&)cstring inRect:(CGRect)videoRect drawFrame:(bool)drawFrame
{
  NSString *string = [NSString stringWithUTF8String:cstring.c_str()];

  CGRect contextRect = videoRect;
  contextRect.origin.y = CGRectGetMaxY(videoRect) - videoRect.size.height / 4;
  contextRect.size.height = videoRect.size.height / 4;

  /// Make a copy of the default paragraph style
  NSMutableParagraphStyle *paragraphStyle = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  /// Set line break mode
  paragraphStyle.lineBreakMode = NSLineBreakByTruncatingTail;
  /// Set text alignment
  paragraphStyle.alignment = NSTextAlignmentCenter;

  NSDictionary *attributes = @{ NSFontAttributeName: [UIFont systemFontOfSize:32],
    NSForegroundColorAttributeName: [UIColor whiteColor],
    NSParagraphStyleAttributeName: paragraphStyle };

  CGSize size = [string sizeWithAttributes:attributes];

  CGRect textRect = CGRectMake(contextRect.origin.x + floorf((contextRect.size.width - size.width) / 2),
    contextRect.origin.y + floorf((contextRect.size.height - size.height) / 2),
    size.width, size.height);

  textRect.origin.y = CGRectGetMaxY(videoRect) - textRect.size.height;

  CGRect underRect = CGRectInset(textRect, -4, 0);
  CGContextSetFillColorWithColor(ctx, [[UIColor blackColor] CGColor]);
  CGContextFillRect(ctx, underRect);
  if (drawFrame)
  {
    CGContextSetStrokeColorWithColor(ctx, [[UIColor whiteColor] CGColor]);
    CGContextSetLineWidth(ctx, 0.5);
    CGContextStrokeRect(ctx, underRect);
  }
  [string drawInRect:textRect withAttributes:attributes];
}

- (void) updateView
{
  if (distance == 0.0)
    return;
  thumb = barRect.size.width * (CGFloat)((self.value - min) / distance);
  CGPoint thumbPoint = CGPointMake(barRect.origin.x + thumb, barRect.origin.y);
  thumbRect = CGRectMake(thumbPoint.x - barRect.size.height/2, thumbPoint.y, barRect.size.height, barRect.size.height);

  dispatch_async(dispatch_get_main_queue(),^{
    [self setNeedsDisplay];
  });
}

//--------------------------------------------------------------
- (void) updateViewMainThread
{
  [self performSelectorOnMainThread:@selector(updateView) withObject:nil  waitUntilDone:NO];
}

#pragma mark - tvOS focus engine routines
//--------------------------------------------------------------
//--------------------------------------------------------------
- (NSArray<id<UIFocusEnvironment>> *)preferredFocusEnvironments
{
  if (self->slideDownView)
    return @[self->slideDownView, (UIView*)self];

  return @[(UIView*)self];
}
//--------------------------------------------------------------
- (BOOL) shouldUpdateFocusInContext:(UIFocusUpdateContext *)context
{
  if (self->slideDownView)
    return NO;
  return YES;
}

- (void) didUpdateFocusInContext:(UIFocusUpdateContext *)context
    withAnimationCoordinator:(UIFocusAnimationCoordinator *)coordinator
{
  CLog::Log(LOGDEBUG, "PlayerProgress::didUpdateFocusInContext");
}

#pragma mark - touch/gesture handlers
//--------------------------------------------------------------
//--------------------------------------------------------------
- (BOOL) gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
  if (self->slideDownView)
    return NO;
  CLog::Log(LOGDEBUG, "PlayerProgress::gestureRecognizer:shouldReceiveTouch");
  return YES;
}
//--------------------------------------------------------------
- (BOOL) gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceivePress:(UIPress *)press
{
  if (self->slideDownView)
    return NO;
  CLog::Log(LOGDEBUG, "PlayerProgress::gestureRecognizer:shouldReceivePress");
  return YES;
}
//--------------------------------------------------------------
- (BOOL) gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
  CLog::Log(LOGDEBUG, "PlayerProgress::gestureRecognizerShouldBegin");
  if ([gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]])
  {
    UIPanGestureRecognizer *panGestureRecognizer = (UIPanGestureRecognizer*)gestureRecognizer;
    CGPoint translation = [panGestureRecognizer translationInView:self];
    CLog::Log(LOGDEBUG, "PlayerProgress::gestureRecognizerShouldBegin x(%f), y(%f)", translation.x, translation.y);
    if (fabs(translation.x) > fabs(translation.y))
      return [self isFocused];
  }
  else if ([gestureRecognizer isKindOfClass:[UITapGestureRecognizer class]])
  {
    return [self isFocused];
  }
  else if ([gestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]])
  {
    return [self isFocused];
  }
  else if ([gestureRecognizer isKindOfClass:[UISwipeGestureRecognizer class]])
  {
    return [self isFocused];
  }
  return NO;
}
//--------------------------------------------------------------
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
  return YES;
}
//--------------------------------------------------------------
- (IBAction) handleUpSwipeGesture:(UISwipeGestureRecognizer *)sender
{
  CLog::Log(LOGDEBUG, "PlayerProgress::handleDownSwipeGesture");
  if (self->deceleratingTimer)
    [self stopDeceleratingTimer];
  if (self->slideDownView)
  {
    [UIView animateWithDuration:0.5
      animations:^{
        CGRect frame = CGRectOffset([self->slideDownView frame], 0.0, -100.0);
        [self->slideDownView setFrame:frame];
        [self->slideDownView layoutIfNeeded];
      }
      completion:^(BOOL finished){
        [self->slideDownView removeFromSuperview];
        self->slideDownView = nil;
      }];
  }
}
//--------------------------------------------------------------
- (IBAction) handleDownSwipeGesture:(UISwipeGestureRecognizer *)sender
{
  CLog::Log(LOGDEBUG, "PlayerProgress::handleDownSwipeGesture");
  if (self->deceleratingTimer)
    [self stopDeceleratingTimer];
  if (self->slideDownView)
    [self->slideDownView removeFromSuperview];
#if 0
  CGRect frameRect = [UIScreen mainScreen].bounds;
  frameRect.size.height = 100.0;
  frameRect.origin.y = -100.0;
  self->slideDownView = [[FocusLayerViewPlayerProgressSettings alloc] initWithFrame:frameRect];
  [self addSubview:self->slideDownView];
  [UIView animateWithDuration:0.5
    animations:^{
      CGRect frame = CGRectOffset([self->slideDownView frame], 0.0, 100.0);
      [self->slideDownView setFrame:frame];
      [self->slideDownView layoutIfNeeded];
    }
    completion:^(BOOL finished){
      [self setNeedsFocusUpdate];
    }];
#else
  g_windowManager.ActivateWindow(11200);
#endif
}
//--------------------------------------------------------------
- (IBAction) handlePanGesture:(UIPanGestureRecognizer *)sender
{
  CLog::Log(LOGDEBUG, "PlayerProgress::handlePanGesture");
  CGPoint translation = [sender translationInView:self];
  CGPoint velocity =  [sender velocityInView:self];
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
        [self stopDeceleratingTimer];
        thumbConstant = thumb;
      break;
    case UIGestureRecognizerStateChanged:
      {
        double swipesForFullRange = 9.0;
        double leading = thumbConstant + translation.x / swipesForFullRange;
        [self setPercentage:leading / barRect.size.width];
      }
      break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
      {
        thumbConstant = thumb;
        int direction = velocity.x > 0 ? 1 : -1;
        deceleratingVelocity = fabs(velocity.x) > decelerationMaxVelocity ? decelerationMaxVelocity * direction : velocity.x;
        self->deceleratingTimer = [NSTimer scheduledTimerWithTimeInterval:0.01 target:self selector:@selector(handleDeceleratingTimer:) userInfo:nil repeats:YES];
      }
      break;
    default:
      break;
  }
}
//--------------------------------------------------------------
- (void) handleDeceleratingTimer:(id)obj
{
  // invalidate is a request to stop timer,
  // we want updates to stop immediatly
  // when stopDeceleratingTimer is called.
  if (self->deceleratingTimer)
  {
    double leading = thumbConstant + deceleratingVelocity * 0.01;
    [self setPercentage:(double)leading / barRect.size.width];
    thumbConstant = thumb;

    deceleratingVelocity *= decelerationRate;
    if (![self isFocused] || fabs(deceleratingVelocity) < 1.0)
      [self stopDeceleratingTimer];
  }
}
//--------------------------------------------------------------
- (void) stopDeceleratingTimer
{
  [self->deceleratingTimer invalidate];
  self->deceleratingTimer = nil;
  self->deceleratingVelocity = 0.0;
}

#pragma mark - IR remote handlers
//--------------------------------------------------------------
//--------------------------------------------------------------
typedef enum IRRemoteTypes
{
  IR_Left = 1,
  IR_Right = 2,
  IR_LeftFast = 3,
  IR_RightFast = 4,
} IRRemoteTypes;
//--------------------------------------------------------------
- (void)sendButtonPressed:(int)buttonId
{
  if (self->thumbNailer)
  {
    int seekTimeMilliSeconds = self->seekTimeSeconds * 1000;
    if (seekTimeMilliSeconds == -1)
    {
      thumbConstant = thumb;
      [self setPercentage:thumbConstant / barRect.size.width];
      return;
    }
    if (buttonId == IR_Left)
      seekTimeMilliSeconds -= 10000;
    else if (buttonId == IR_LeftFast)
      seekTimeMilliSeconds -= 60000;
    else if (buttonId == IR_Right)
      seekTimeMilliSeconds += 10000;
    else if (buttonId == IR_RightFast)
      seekTimeMilliSeconds += 60000;
    if (seekTimeMilliSeconds < 0)
      seekTimeMilliSeconds = 0;
    int totalTimeMilliSeconds = self->totalTimeSeconds * 1000;
    if (seekTimeMilliSeconds > totalTimeMilliSeconds)
      seekTimeMilliSeconds = totalTimeMilliSeconds;
    CLog::Log(LOGDEBUG, "PlayerProgress::sendButtonPressed:seekTime(%d)", seekTimeMilliSeconds);
    double percentage = (double)seekTimeMilliSeconds / totalTimeMilliSeconds;
    [self setPercentage:percentage];
    thumbConstant = thumb;
  }
}
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
  CFAbsoluteTime secondsFromStart = CFAbsoluteTimeGetCurrent() - keyPressTimerStartSeconds;
  if (secondsFromStart > 1.5f)
  {
    int keyvalue = [keyId intValue];
    if (keyvalue == IR_Left)
      keyvalue = IR_LeftFast;
    if (keyvalue == IR_Right)
      keyvalue = IR_RightFast;
    [self sendButtonPressed:keyvalue];
  }
  else
    [self sendButtonPressed:[keyId intValue]];
}

- (IBAction)IRRemoteLeftArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      CLog::Log(LOGDEBUG, "PlayerProgress::IRRemoteLeftArrowPressed");
      [self startKeyPressTimer:IR_Left doBeforeDelay:true withDelay:REPEATED_KEYPRESS_DELAY_S];
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
- (IBAction)IRRemoteRightArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      CLog::Log(LOGDEBUG, "PlayerProgress::IRRemoteRightArrowPressed");
      [self startKeyPressTimer:IR_Right doBeforeDelay:true withDelay:REPEATED_KEYPRESS_DELAY_S];
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

- (IBAction)IRRemoteDownArrowPressed:(UIGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      CLog::Log(LOGDEBUG, "PlayerProgress::IRRemoteDownArrowPressed");
      KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(
        TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_SHOW_OSD)));
      break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateChanged:
    case UIGestureRecognizerStateCancelled:
      break;
    default:
      break;
  }
}

@end

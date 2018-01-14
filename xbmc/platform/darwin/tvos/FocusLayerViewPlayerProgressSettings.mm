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

#import "platform/darwin/tvos/FocusLayerViewPlayerProgressSettings.h"

#import "Application.h"
#import "guilib/GUIColorManager.h"
#import "messaging/ApplicationMessenger.h"
#import "platform/darwin/NSLogDebugHelpers.h"
#import "utils/StringUtils.h"
#import "utils/log.h"

@interface FocusLayerViewPlayerProgressSettings ()
@end

#pragma mark -
@implementation FocusLayerViewPlayerProgressSettings

- (id)initWithFrame:(CGRect)frame
{
  frame = CGRectInset(frame, -400, 0);
  self = [super initWithFrame:frame];
  if (self)
  {
    self.backgroundColor = [[UIColor blackColor] colorWithAlphaComponent:0.6f];

    UIButton *subtilesButton = [UIButton buttonWithType:UIButtonTypeSystem];
    CGRect subtilesFrameRect = CGRectMake(0, 0, 260.0, 60.0);
    subtilesFrameRect.origin.x = CGRectGetMidX(self.bounds) - (subtilesFrameRect.size.width + 50.0);
    subtilesFrameRect.origin.y = CGRectGetMidY(self.bounds) - (subtilesFrameRect.size.height / 2.0);
    subtilesButton.frame = subtilesFrameRect;
    [subtilesButton addTarget:self action:@selector(subtitleButtonMethod:)
     forControlEvents:UIControlEventPrimaryActionTriggered];
    [subtilesButton.titleLabel setFont:[UIFont systemFontOfSize:32]];
    [subtilesButton setTitle:@"Subtitles" forState:UIControlStateNormal];
    subtilesButton.userInteractionEnabled = YES;
    self->subtilesView = subtilesButton;
    [self addSubview:self->subtilesView];

    UIButton *settingsButton = [UIButton buttonWithType:UIButtonTypeSystem];
    CGRect settingsFrameRect = CGRectMake(0, 0, 260.0, 60.0);
    settingsFrameRect.origin.x = CGRectGetMidX(self.bounds) + 50.0;
    settingsFrameRect.origin.y = CGRectGetMidY(self.bounds) - (settingsFrameRect.size.height / 2.0);
    settingsButton.frame = settingsFrameRect;
    [settingsButton addTarget:self action:@selector(settingsButtonMethod:)
     forControlEvents:UIControlEventPrimaryActionTriggered];
    [settingsButton.titleLabel setFont:[UIFont systemFontOfSize:32]];
    [settingsButton setTitle:@"Settings" forState:UIControlStateNormal];
    settingsButton.userInteractionEnabled = YES;
    self->settingsView = settingsButton;
    [self addSubview:self->settingsView];

    preferredFocus = self->subtilesView;

/*
    CGRect buttonRect = CGRectMake(0, 0, 360.0, 60.0);
    CGRect subtilesFrameRect = buttonRect;
    self->subtilesView = [[UIView alloc] initWithFrame:subtilesFrameRect];
    self->settingsView.backgroundColor = [UIColor clearColor];
    self->subtilesView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:self->subtilesView];

    CGRect settingsFrameRect = buttonRect;
    settingsFrameRect.origin.x = CGRectGetMidX(self.bounds) + 50.0;
    settingsFrameRect.origin.y = CGRectGetMidY(self.bounds) - (settingsFrameRect.size.height / 2.0);
    self->settingsView = [[UIView alloc] initWithFrame:settingsFrameRect];
    self->settingsView.backgroundColor = [UIColor clearColor];
    self->settingsView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:self->settingsView];
*/
    [self setNeedsLayout];
/*
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

    auto *swipeUp = [[UISwipeGestureRecognizer alloc]
    initWithTarget:self action:@selector(handleUpSwipeGesture:)];
    swipeUp.delaysTouchesBegan = NO;
    swipeUp.direction = UISwipeGestureRecognizerDirectionUp;
    swipeUp.delegate = self;
    [self  addGestureRecognizer:swipeUp];
*/
    auto pan = [[UIPanGestureRecognizer alloc]
      initWithTarget:self action:@selector(handlePanGesture:)];
    pan.delegate = self;
    [self addGestureRecognizer:pan];


    auto *swipeLeft = [[UISwipeGestureRecognizer alloc]
    initWithTarget:self action:@selector(handleLeftSwipeGesture:)];
    swipeLeft.delaysTouchesBegan = NO;
    swipeLeft.direction = UISwipeGestureRecognizerDirectionLeft;
    swipeLeft.delegate = self;
    [self addGestureRecognizer:swipeLeft];

    auto *swipeRight = [[UISwipeGestureRecognizer alloc]
    initWithTarget:self action:@selector(handleRightSwipeGesture:)];
    swipeRight.delaysTouchesBegan = NO;
    swipeRight.direction = UISwipeGestureRecognizerDirectionRight;
    swipeRight.delegate = self;
    [self addGestureRecognizer:swipeRight];

    auto longSelectRecognizer = [[UILongPressGestureRecognizer alloc]
      initWithTarget: self action: @selector(handleLongSelect:)];
    longSelectRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeSelect]];
    longSelectRecognizer.minimumPressDuration = 0.001;
    longSelectRecognizer.delegate = self;
    [self addGestureRecognizer: longSelectRecognizer];
  }
	return self;
}

- (void)removeFromSuperview
{
  [self->subtilesView removeFromSuperview];
  [self->settingsView removeFromSuperview];
  [super removeFromSuperview];
}

//--------------------------------------------------------------
- (BOOL)canBecomeFocused
{
  return YES;
}

- (void)drawRect:(CGRect)rect
{
/*
  CGContextRef ctx = UIGraphicsGetCurrentContext();
  CGContextSetBlendMode(ctx, kCGBlendModeCopy);

  color_t m_selectedColor = g_colorManager.GetColor("red");
  UIColor *selectedColor = [UIColor
    colorWithRed:(float)GET_R(m_selectedColor)/255
   green:(float)GET_G(m_selectedColor)/255
   blue:(float)GET_B(m_selectedColor)/255
   alpha:1.0];
  //CGContextSetFillColorWithColor(ctx, [[UIColor blackColor] CGColor]);
  //CGContextFillRect(ctx, self->subtilesView.frame);
  CGContextSetFillColorWithColor(ctx, [selectedColor CGColor]);
  CGContextFillRect(ctx, self->subtilesView.frame);

  std::string stringSubtitles = "Subtitles";
  [self drawString:ctx withCString:stringSubtitles inRect:self->subtilesView.frame];

  std::string sringSettings = "Settings";
  [self drawString:ctx withCString:sringSettings inRect:self->settingsView.frame];
*/
}

- (void) drawString:(CGContextRef) ctx withCString:(const std::string&)cstring inRect:(CGRect)contextRect
{
  NSString *string = [NSString stringWithUTF8String:cstring.c_str()];

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

  [string drawInRect:textRect withAttributes:attributes];
}

- (BOOL) gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
  CLog::Log(LOGDEBUG, "PlayerProgressSettings::gestureRecognizer:shouldReceiveTouch");
  return YES;
}
- (BOOL) gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceivePress:(UIPress *)press
{
  CLog::Log(LOGDEBUG, "PlayerProgressSettings::gestureRecognizer:shouldReceivePress");
  return YES;
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
  return YES;
}

//--------------------------------------------------------------
- (NSArray<id<UIFocusEnvironment>> *)preferredFocusEnvironments
{
  return @[preferredFocus];
}
//--------------------------------------------------------------
- (BOOL) shouldUpdateFocusInContext:(UIFocusUpdateContext *)context
{
  if ( [context.nextFocusedItem isKindOfClass:[UIButton class]] == NO)
    return NO;
  return YES;
}

- (void) didUpdateFocusInContext:(UIFocusUpdateContext *)context
    withAnimationCoordinator:(UIFocusAnimationCoordinator *)coordinator
{
  CLog::Log(LOGDEBUG, "PlayerProgressSettings::didUpdateFocusInContext");
}

- (void)subtitleButtonMethod:(UIButton*)button
{
  NSLog(@"PlayerProgressSettings::subtitleButton clicked.");
}

- (void)settingsButtonMethod:(UIButton*)button
{
  NSLog(@"PlayerProgressSettings::settingsButton clicked.");
}

- (IBAction) handlePanGesture:(UIPanGestureRecognizer *)sender
{
  CLog::Log(LOGDEBUG, "PlayerProgressSettings::handlePanGesture");
}
//--------------------------------------------------------------
- (IBAction) handleLeftSwipeGesture:(UISwipeGestureRecognizer *)sender
{
  CLog::Log(LOGDEBUG, "PlayerProgressSettings::handleLeftSwipeGesture");
  if (preferredFocus == self->subtilesView)
    preferredFocus = self->settingsView;
  else if (preferredFocus == self->settingsView)
    preferredFocus = self->subtilesView;
}
//--------------------------------------------------------------
- (IBAction) handleRightSwipeGesture:(UISwipeGestureRecognizer *)sender
{
  CLog::Log(LOGDEBUG, "PlayerProgressSettings::handleRightSwipeGesture");
  if (preferredFocus == self->subtilesView)
    preferredFocus = self->settingsView;
  else if (preferredFocus == self->settingsView)
    preferredFocus = self->subtilesView;
}
//--------------------------------------------------------------
- (IBAction) handleUpSwipeGesture:(UISwipeGestureRecognizer *)sender
{
  CLog::Log(LOGDEBUG, "PlayerProgressSettings::handleUpSwipeGesture");
}
//--------------------------------------------------------------
- (void)handleLongSelect:(UITapGestureRecognizer *)sender
{
  switch (sender.state)
  {
    case UIGestureRecognizerStateBegan:
      CLog::Log(LOGDEBUG, "handleLongSelect:StateBegan");
      break;
    case UIGestureRecognizerStateEnded:
      CLog::Log(LOGDEBUG, "handleLongSelect:StateEnded");
      break;
    case UIGestureRecognizerStateCancelled:
      CLog::Log(LOGDEBUG, "handleLongSelect:StateCancelled");
      break;
    default:
      break;
  }
}

@end
  

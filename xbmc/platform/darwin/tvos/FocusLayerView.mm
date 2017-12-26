/*
 *      Copyright (C) 2015 Team MrMC
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

#import <string>
#import "platform/darwin/tvos/FocusLayerView.h"
#import "platform/darwin/NSLogDebugHelpers.h"
#import "platform/darwin/tvos/MainController.h"

#import <AVFoundation/AVFoundation.h>

#pragma mark -
@interface UIAppleTvMotionEffectGroup : UIMotionEffectGroup
@end
@implementation UIAppleTvMotionEffectGroup
- (id)init
{
  if ((self = [super init]) != nil)
  {
    // Size of shift movements
    CGFloat const shiftDistance = 10.0f;

    // Make horizontal movements shift the centre left and right
    UIInterpolatingMotionEffect *xShift = [[UIInterpolatingMotionEffect alloc]
                                          initWithKeyPath:@"center.x"
                                          type:UIInterpolatingMotionEffectTypeTiltAlongHorizontalAxis];
    xShift.minimumRelativeValue = [NSNumber numberWithFloat: shiftDistance * -1.0f];
    xShift.maximumRelativeValue = [NSNumber numberWithFloat: shiftDistance];

    // Make vertical movements shift the centre up and down
    UIInterpolatingMotionEffect *yShift = [[UIInterpolatingMotionEffect alloc]
                                          initWithKeyPath:@"center.y"
                                          type:UIInterpolatingMotionEffectTypeTiltAlongVerticalAxis];
    yShift.minimumRelativeValue = [NSNumber numberWithFloat: shiftDistance * -1.0f];
    yShift.maximumRelativeValue = [NSNumber numberWithFloat: shiftDistance];

    // Size of tilt movements
    CGFloat const tiltAngle = M_PI_4 * 0.125;

    // Now make horizontal movements effect a rotation about the Y axis for side-to-side rotation.
    UIInterpolatingMotionEffect *xTilt = [[UIInterpolatingMotionEffect alloc] initWithKeyPath:@"layer.transform" type:UIInterpolatingMotionEffectTypeTiltAlongHorizontalAxis];

    // CATransform3D value for minimumRelativeValue
    CATransform3D transMinimumTiltAboutY = CATransform3DIdentity;
    transMinimumTiltAboutY.m34 = 1.0 / 500;
    transMinimumTiltAboutY = CATransform3DRotate(transMinimumTiltAboutY, tiltAngle * -1.0, 0, 1, 0);

    // CATransform3D value for maximumRelativeValue
    CATransform3D transMaximumTiltAboutY = CATransform3DIdentity;
    transMaximumTiltAboutY.m34 = 1.0 / 500;
    transMaximumTiltAboutY = CATransform3DRotate(transMaximumTiltAboutY, tiltAngle, 0, 1, 0);

    // Set the transform property boundaries for the interpolation
    xTilt.minimumRelativeValue = [NSValue valueWithCATransform3D: transMinimumTiltAboutY];
    xTilt.maximumRelativeValue = [NSValue valueWithCATransform3D: transMaximumTiltAboutY];

    // Now make vertical movements effect a rotation about the X axis for up and down rotation.
    UIInterpolatingMotionEffect *yTilt = [[UIInterpolatingMotionEffect alloc] initWithKeyPath:@"layer.transform" type:UIInterpolatingMotionEffectTypeTiltAlongVerticalAxis];

    // CATransform3D value for minimumRelativeValue
    CATransform3D transMinimumTiltAboutX = CATransform3DIdentity;
    transMinimumTiltAboutX.m34 = 1.0 / 500;
    transMinimumTiltAboutX = CATransform3DRotate(transMinimumTiltAboutX, tiltAngle * -1.0, 1, 0, 0);

    // CATransform3D value for maximumRelativeValue
    CATransform3D transMaximumTiltAboutX = CATransform3DIdentity;
    transMaximumTiltAboutX.m34 = 1.0 / 500;
    transMaximumTiltAboutX = CATransform3DRotate(transMaximumTiltAboutX, tiltAngle, 1, 0, 0);

    // Set the transform property boundaries for the interpolation
    yTilt.minimumRelativeValue = [NSValue valueWithCATransform3D: transMinimumTiltAboutX];
    yTilt.maximumRelativeValue = [NSValue valueWithCATransform3D: transMaximumTiltAboutX];

    // Add all of the motion effects to this group
    self.motionEffects = @[xShift, yShift, xTilt, yTilt];
  }
  return self;
}
@end

#pragma mark -
@implementation FocusLayerView

- (id)initWithFrame:(CGRect)frame
{
  PRINT_SIGNATURE();
	self = [super initWithFrame:frame];
	if (self)
	{
    self.opaque = NO;
    self.userInteractionEnabled = YES;
    self.layer.backgroundColor = [[UIColor clearColor] CGColor];
    [self setNeedsLayout];
    [self layoutIfNeeded];
  }

	return self;
}

- (void) updateItems:(std::vector<CGRect> &)items
{
  m_items = items;
}

- (void)drawRect:(CGRect)rect
{
  //PRINT_SIGNATURE();
  CGContextRef context = UIGraphicsGetCurrentContext();
  // make the window transparent
  CGContextSetBlendMode(context, kCGBlendModeClear);
  CGContextFillRect(context, rect);
  CGContextSetBlendMode(context, kCGBlendModeCopy);
  CGContextSetStrokeColorWithColor(context, [[UIColor whiteColor] CGColor]);

  CGContextSetLineWidth(context, 2);
  if (m_items.size() > 0)
  {
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
      CGContextStrokeRect(context, *it);
  }
  //else
/*
  CGContextSetLineWidth(context, 10);
  CGContextStrokeRect(context, rect);
*/
}

- (void)didUpdateFocusInContext:(UIFocusUpdateContext *)context withAnimationCoordinator:(UIFocusAnimationCoordinator *)coordinator
{
  PRINT_SIGNATURE();
#ifdef false
  // Create a static instance of the motion effect group
  // (could do this anywhere, really, maybe init would be better - we only need one of them.)
  static UIAppleTvMotionEffectGroup *s_atvMotionEffect = nil;
  if (s_atvMotionEffect == nil)
    s_atvMotionEffect = [[UIAppleTvMotionEffectGroup alloc] init];

  [coordinator addCoordinatedAnimations: ^{
    if (self.focused)
    {
      [self addMotionEffect: s_atvMotionEffect];
      [self setNeedsDisplay];
    }
    else
    {
      [self removeMotionEffect: s_atvMotionEffect];
      [self setNeedsDisplay];
    }
  } completion:nil];
#else
  if (self.focused)
  {
    // Apply focused appearence,
    // e.g scale both of them using transform or apply background color
    [g_xbmcController performSelectorOnMainThread:@selector(changeFocus:) withObject:self  waitUntilDone:NO];
  }
  else
  {
    // Apply normal appearance
  }
  [self setNeedsDisplay];
#endif
}

//--------------------------------------------------------------
- (BOOL)canBecomeFocused
{
  //PRINT_SIGNATURE();
  // need this or we do not get GestureRecognizers under tvos.
  //return YES;
  return NO;
}


- (void)dealloc
{
  PRINT_SIGNATURE();
}

- (void)layoutSubviews
{
/*
  Subclasses can override this method as needed to perform more precise layout of their subviews. You should override this method only if the autoresizing and constraint-based behaviors of the subviews do not offer the behavior you want. You can use your implementation to set the frame rectangles of your subviews directly.
*/
  PRINT_SIGNATURE();
  [self setNeedsDisplay];
}

- (void)viewDidLayoutSubviews
{
  PRINT_SIGNATURE();
}

@end

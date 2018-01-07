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

#import "platform/darwin/tvos/FocusLayerView.h"
#import "platform/darwin/NSLogDebugHelpers.h"

#pragma mark -
@implementation FocusLayerView

- (id)initWithFrame:(CGRect)frame
{
	self = [super initWithFrame:frame];
	if (self)
	{
    self.opaque = NO;
    self.userInteractionEnabled = YES;
    self.bounds = frame;
    self.layer.backgroundColor = [[UIColor clearColor] CGColor];

    // set to false to hide frame drawing (used for debugging)
    self->viewVisable = false;
    self->focusable = false;
    self->viewBounds = frame;
    self->frameColor = [UIColor whiteColor];
  }
	return self;
}

- (void)didUpdateFocusInContext:(UIFocusUpdateContext *)context
    withAnimationCoordinator:(UIFocusAnimationCoordinator *)coordinator
{
  // if some focus changed, we need to update ourselves
  // to show correct frame color
  if (self->viewVisable)
    [self setNeedsDisplay];
}

//--------------------------------------------------------------
- (BOOL)canBecomeFocused
{
  if (self->focusable)
    return YES;
  return NO;
}

- (void)drawRect:(CGRect)rect
{
  if (self->viewVisable)
  {
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    if (self.focused)
    {
      CGContextSetLineWidth(context, 8.0);
      CGContextSetStrokeColorWithColor(context, [[UIColor orangeColor] CGColor]);
    }
    else
    {
      CGContextSetLineWidth(context, 4.0);
      CGContextSetStrokeColorWithColor(context, [self->frameColor CGColor]);
    }
    CGContextStrokeRect(context, rect);
  }
}

- (void) setFocusable:(bool)focusable
{
  self->focusable = focusable;
  if (self->focusable)
    self->frameColor = [UIColor greenColor];
  else
    self->frameColor = [UIColor whiteColor];
}

- (void) setViewVisable:(bool)viewVisable
{
  self->viewVisable = viewVisable;
}

@end

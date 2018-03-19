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

bool FocusLayerViewsAreEqual(std::vector<FocusLayerControl> &views1, std::vector<FocusLayerControl> &views2)
{
  if (views1.size() != views2.size())
    return false;
  for (size_t indx = 0; indx < views1.size(); ++indx)
  {
    // sizes are the same, so we have to compare views
    if (!views1[indx].IsEqual(views2[indx]))
      return false;
  }
  return true;
}

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

    // set to false to hide frame drawing (used for debugging)
    self->viewVisible = false;
    self->focusable = false;
    self->viewBounds = frame;
    self->frameColor = [UIColor whiteColor];
  }
	return self;
}

-(void) layerWillDraw:(CALayer *)layer
{
  // http://www.openradar.me/32702889
  // an empty implementation of layerWillDraw will
  // receive an 8 bit backed context on Wide Color devices
  // we also add the convert for when apple fixes this bug.
  NSString* format = layer.contentsFormat;
  if( ![format isEqualToString:kCAContentsFormatRGBA8Uint]  )
      layer.contentsFormat = kCAContentsFormatRGBA8Uint;
}

- (void)didUpdateFocusInContext:(UIFocusUpdateContext *)context
    withAnimationCoordinator:(UIFocusAnimationCoordinator *)coordinator
{
  // if some focus changed, we need to update ourselves
  // to show correct frame color
  if (self->viewVisible)
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
  if (self->viewVisible)
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

- (void) setViewVisible:(bool)viewVisible
{
  self->viewVisible = viewVisible;
}

@end

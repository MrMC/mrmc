#pragma once
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

#import <UIKit/UIKit.h>
#import "platform/darwin/tvos/FocusLayerView.h"
#import "platform/darwin/tvos/ProgressThumbNailer.h"

class CProgressThumbNailer;

@interface FocusLayerViewPlayerProgress  : FocusLayerView  <UIGestureRecognizerDelegate>
{
@public
  double max;
  double min;

@private
  double thumb;
  double thumbConstant;
  double distance;
  CGFloat decelerationRate;
  CGFloat deceleratingVelocity;
  CGFloat decelerationMaxVelocity;
  NSTimer *deceleratingTimer;

  CGRect barRect;
  CGRect thumbRect;
  CGRect videoRect;
  bool   videoRectIsAboveBar;
  double seekTimeSeconds;
  double totalTimeSeconds;
  ThumbNailerImage thumbImage;
  CProgressThumbNailer *thumbNailer;
  UIView *slideDownView;
}
@property (nonatomic) double _value;

- (double) getSeekTimePercentage;
- (void)   updateViewMainThread;

@end

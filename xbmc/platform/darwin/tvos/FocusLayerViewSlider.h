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

@class FocusLayerViewSlider;

@protocol FocusLayerViewSliderDelegate
  - (void)slider:(FocusLayerViewSlider*)slider textWithValue:(double)value;

  - (void)sliderDidTap:(FocusLayerViewSlider*)slider;
  - (void)slider:(FocusLayerViewSlider*)slider didChangeValue:(double)value;
  - (void)slider:(FocusLayerViewSlider*)slider didUpdateFocusInContext:(UIFocusUpdateContext*)context withAnimationCoordinator:   (UIFocusAnimationCoordinator*)coordinator;
@end

@interface FocusLayerViewSlider  : FocusLayerView  <UIGestureRecognizerDelegate>
{
@public
  double max;
  double min;
  double value;

@private
  double  distance;
  CGFloat deceleratingVelocity;
  double seekerViewLeadingConstraintConstant;
  NSTimer *deceleratingTimer;
  NSLayoutConstraint *seekerViewLeadingConstraint;

}
@property (nonatomic, weak) id<FocusLayerViewSliderDelegate> delegate;

@end

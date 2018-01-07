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
#import <string>
#import <vector>

@class FocusLayerView;

typedef struct FocusLayerItem
{
  void *core;
  CGRect rect;
  std::string type;
  FocusLayerView *view;
} FocusLayerItem;

typedef struct FocusLayerControl
{
  void *core;
  CGRect rect;
  std::string type;
  FocusLayerView *view;
  std::vector<FocusLayerItem> items;
} FocusLayerControl;

typedef struct FocusLayer
{
  void Reset()
  {
    infocus.type = "";
    infocus.core = nullptr;
    infocus.view = nullptr;
    infocus.items.clear();
    views.clear();
  }
  // core and view that are 'in focus'
  // this could be enclosing view or individual item
  FocusLayerControl infocus;
  // array of all views attached to focus layer
  std::vector<FocusLayerControl> views;
} FocusLayer;

@interface FocusLayerView : UIView
{
@public
  void *core;

@private
  bool focusable;
  bool viewVisable;
  CGRect viewBounds;
  UIColor *frameColor;
}

- (void) setFocusable:(bool)focusable;
- (void) setViewVisable:(bool)viewVisable;

@end

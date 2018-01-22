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
  bool IsEqual(FocusLayerItem &item)
  {
    if (!IsEqualRect(item))
      return false;
    if (!IsEqualCore(item))
      return false;
    return true;
  }
  bool IsEqualRect(FocusLayerItem &item)
  {
    if (!CGRectEqualToRect(rect, item.rect))
      return false;
    return true;
  }
  bool IsEqualCore(FocusLayerItem &item)
  {
    if (core != item.core)
      return false;
    return true;
  }
  void *core;
  CGRect rect;
  std::string type;
  FocusLayerView *view;
} FocusLayerItem;

typedef struct FocusLayerControl
{
  bool IsEqual(FocusLayerControl &view)
  {
    if (items.size() != view.items.size())
      return false;
    if (!IsEqualRect(view))
      return false;
    if (!IsEqualCore(view))
      return false;
    return true;
  }
  bool IsEqualRect(FocusLayerControl &view)
  {
    if (!CGRectEqualToRect(rect, view.rect))
      return false;
    for (size_t indx = 0; indx < items.size(); ++indx)
    {
      if (!CGRectEqualToRect(items[indx].rect, view.items[indx].rect))
        return false;
    }
    return true;
  }
  bool IsEqualCore(FocusLayerControl &view)
  {
    if (core != view.core)
      return false;
    for (size_t indx = 0; indx < items.size(); ++indx)
    {
      if (items[indx].core != view.items[indx].core)
        return false;
    }
    return true;
  }
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
    infocus.rect = CGRectZero;
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

bool FocusLayerViewsAreEqual(std::vector<FocusLayerControl> &views1, std::vector<FocusLayerControl> &views2);

@interface FocusLayerView : UIView
{
@public
  void *core;

@private
  bool focusable;
  bool viewVisible;
  CGRect viewBounds;
  UIColor *frameColor;
}

- (void) setFocusable:(bool)focusable;
- (void) setViewVisible:(bool)viewVisible;

@end

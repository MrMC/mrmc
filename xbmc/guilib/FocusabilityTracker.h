#pragma once

/*
 *      Copyright (C) 2017-2018 Team MrMC
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

#include "Geometry.h"
#include "guilib/GUIControl.h"

#include <vector>

typedef struct
{
  int renderOrder = 0;             // render order
  CRect renderRect;                 // in display coordinates
  CGUIControl *control = nullptr;   // control item reference
  // views are what enclose focusable controls,
  // they can be containers or groups (which include window/dialog).
  // for example, a button is enclosed by a group (for a group of controls)
  // or a window/dialog when the button not enclosed with others controls.
  int viewOrder = 0;                // process order
  CRect viewRenderRect;             // in display coordinates
  CGUIControl *parentView = nullptr;
} GUIFocusabilityItem;

class CFocusabilityTracker
{
public:
  CFocusabilityTracker();
 ~CFocusabilityTracker();

  void Clear();

  bool IsEnabled();
  void SetEnabled(bool enable);
  void Append(CGUIControl *control, CGUIControl *view = nullptr);
  void BeginRender();
  void AfterRender();
  const std::vector<GUIFocusabilityItem>& GetItems() const;

private:
  bool m_enable = true;
  int m_viewOrder = 0;
  int m_renderOrder = 0;
  std::vector<GUIFocusabilityItem> m_items;
};

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

#include "FocusabilityTracker.h"
#include "utils/log.h"

CFocusabilityTracker::CFocusabilityTracker()
{
}

CFocusabilityTracker::~CFocusabilityTracker()
{
}

void CFocusabilityTracker::Clear()
{
  m_viewOrder = 0;
  m_renderOrder = 0;
  m_items.clear();
}

bool CFocusabilityTracker::IsEnabled()
{
  return m_enable;
}

void CFocusabilityTracker::SetEnabled(bool enable)
{
  m_enable = enable;
}

void CFocusabilityTracker::Append(CGUIControl *control, CGUIControl *view)
{
  if (m_enable)
  {
    GUIFocusabilityItem item;
    item.control = control;
    item.renderOrder = ++m_renderOrder;
    item.viewOrder = m_viewOrder;
    item.parentView = view;
    if (item.parentView)
      m_viewOrder++;
    m_items.push_back(item);
  }
}

void CFocusabilityTracker::BeginRender()
{
}

void CFocusabilityTracker::AfterRender()
{
}

const std::vector<GUIFocusabilityItem>& CFocusabilityTracker::GetItems() const
{
  return m_items;
}

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
  //CLog::Log(LOGDEBUG, "CFocusabilityTracker::BeginRender");
  m_controlOrder = 0;
  for (const auto &control : m_controls)
    control->SetRenderTrackingOrder(-1);
}

void CFocusabilityTracker::AfterRender()
{
  //CLog::Log(LOGDEBUG, "CFocusabilityTracker::AfterRender");
  std::vector<CGUIControl *> renderedControls;
  for (const auto &control : m_controls)
  {
    if (control->GetRenderTrackingOrder() > -1)
      renderedControls.push_back(control);
  }
  //CLog::Log(LOGDEBUG, "CFocusabilityTracker::AfterRender rendered %lu controls", renderedControls.size());
}

void CFocusabilityTracker::UpdateRender(CGUIControl *control, bool remove)
{
  if (remove)
  {
    auto foundControl = std::find(m_controls.begin(), m_controls.end(), control);
    if (foundControl != m_controls.end())
      m_controls.erase(foundControl);
  }
  else
  {
    auto foundControl = std::find(m_controls.begin(), m_controls.end(), control);
    if (foundControl == m_controls.end())
      m_controls.push_back(control);
    else
      control->SetRenderTrackingOrder(++m_controlOrder);
  }
}

const std::vector<GUIFocusabilityItem>& CFocusabilityTracker::GetItems() const
{
  return m_items;
}

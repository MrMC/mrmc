/*
 *      Copyright (C) 2017 Team MrMC
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

 // https://developer.apple.com/library/content/documentation/General/Conceptual/AppleTV_PG/WorkingwiththeAppleTVRemote.html#//apple_ref/doc/uid/TP40015241-CH5-SW5

#include "FocusEngineHandler.h"

#include "guilib/GUIBaseContainer.h"
#include "guilib/GUIControl.h"
#include "guilib/GUIListItem.h"
#include "guilib/GUIScrollBarControl.h"
#include "guilib/GUIControlGroupList.h"
#include "guilib/GUIMultiSelectText.h"
#include "guilib/GUIListLabel.h"
#include "guilib/GUIWindowManager.h"
#include "guiinfo/GUIInfoLabels.h"

#include "threads/Atomics.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

static std::atomic<long> sg_focusenginehandler_lock {0};
CFocusEngineHandler* CFocusEngineHandler::m_instance = nullptr;

CFocusEngineHandler&
CFocusEngineHandler::GetInstance()
{
  CAtomicSpinLock lock(sg_focusenginehandler_lock);
  if (!m_instance)
    m_instance = new CFocusEngineHandler();

  return *m_instance;
}

CFocusEngineHandler::CFocusEngineHandler()
{
}

const CRect
CFocusEngineHandler::GetFocusedItemRect()
{
  CGUIWindow* pWindow = g_windowManager.GetWindow(g_windowManager.GetFocusedWindow());
  if (!pWindow)
    return CRect();

  CGUIControl *focusedControl = pWindow->GetFocusedControl();
  if (!focusedControl)
    return CRect();

  CRect focusedItem;
  switch(focusedControl->GetControlType())
  {
    case CGUIControl::GUICONTROL_UNKNOWN:
      CLog::Log(LOGDEBUG, "GetFocusedItem: GUICONTROL_UNKNOWN");
      break;
    case CGUIControl::GUICONTROL_BUTTON:
    case CGUIControl::GUICONTROL_CHECKMARK:
    case CGUIControl::GUICONTROL_FADELABEL:
    case CGUIControl::GUICONTROL_IMAGE:
    case CGUIControl::GUICONTROL_BORDEREDIMAGE:
    case CGUIControl::GUICONTROL_LARGE_IMAGE:
    case CGUIControl::GUICONTROL_LABEL:
    case CGUIControl::GUICONTROL_PROGRESS:
    case CGUIControl::GUICONTROL_RADIO:
    case CGUIControl::GUICONTROL_RSS:
    case CGUIControl::GUICONTROL_SELECTBUTTON:
    case CGUIControl::GUICONTROL_SPIN:
    case CGUIControl::GUICONTROL_SPINEX:
    case CGUIControl::GUICONTROL_TEXTBOX:
    case CGUIControl::GUICONTROL_TOGGLEBUTTON:
    case CGUIControl::GUICONTROL_VIDEO:
    case CGUIControl::GUICONTROL_SLIDER:
    case CGUIControl::GUICONTROL_SETTINGS_SLIDER:
    case CGUIControl::GUICONTROL_MOVER:
    case CGUIControl::GUICONTROL_RESIZE:
    case CGUIControl::GUICONTROL_EDIT:
    case CGUIControl::GUICONTROL_VISUALISATION:
    case CGUIControl::GUICONTROL_RENDERADDON:
    case CGUIControl::GUICONTROL_MULTI_IMAGE:
    case CGUIControl::GUICONTROL_LISTGROUP:
    case CGUIControl::GUICONTROL_GROUPLIST:
    case CGUIControl::GUICONTROL_LISTLABEL:
    case CGUIControl::GUICONTAINER_LIST:
    case CGUIControl::GUICONTAINER_WRAPLIST:
    case CGUIControl::GUICONTAINER_FIXEDLIST:
    case CGUIControl::GUICONTAINER_EPGGRID:
    case CGUIControl::GUICONTAINER_PANEL:
    case CGUIControl::GUICONTROL_GROUP:
    case CGUIControl::GUICONTROL_SCROLLBAR:
    case CGUIControl::GUICONTROL_MULTISELECT:
      {
        // returned rect is in screen coordinates.
        focusedItem = focusedControl->GetSelectionRenderRect();
        if (focusedItem != m_focusedItem)
        {
          m_focusedItem = focusedItem;
          //CLog::Log(LOGDEBUG, "GetFocusedItem: itemRect, t(%f) l(%f) w(%f) h(%f)",
          //  focusedItem.x1, focusedItem.y1, focusedItem.Width(), focusedItem.Height());
        }
      }
      break;
  }

  return m_focusedItem;
}

const CPoint
CFocusEngineHandler::GetFocusedItemCenter()
{
  return GetFocusedItemRect().Center();
}

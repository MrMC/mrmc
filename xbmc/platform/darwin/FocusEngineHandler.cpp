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

#include "guilib/GUIControl.h"
#include "guilib/GUIListGroup.h"
#include "guilib/GUIBaseContainer.h"
#include "guilib/GUIWindowManager.h"
#include "windows/GUIMediaWindow.h"
#include "threads/Atomics.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/XBMCTinyXML.h"


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
: m_focusZoom(true)
, m_focusSlide(true)
, m_showFocusRect(false)
, m_showVisibleRects(true)
, m_state(FocusEngineState::Idle)
, m_focusedOrientation(UNDEFINED)
{
}

void CFocusEngineHandler::Process()
{
  FocusEngineFocus focus;
  UpdateFocus(focus);
  if (m_focus.itemFocus != focus.itemFocus ||
      m_focus.window != focus.window       ||
      m_focus.windowID != focus.windowID)
  {
    if (m_focus.itemFocus)
    {
      m_focus.itemFocus->ResetAnimation(ANIM_TYPE_DYNAMIC);
      m_focus.itemFocus->ClearDynamicAnimations();
    }
    CSingleLock lock(m_focusLock);
    // itemsVisible will start cleared
    m_focus = focus;
  }
  UpdateVisible(m_focus);

  if (m_focus.itemFocus)
  {
    CSingleLock lock(m_stateLock);
    switch(m_state)
    {
      case FocusEngineState::Idle:
        break;
      case FocusEngineState::Clear:
        m_focus.itemFocus->ResetAnimation(ANIM_TYPE_DYNAMIC);
        m_focus.itemFocus->ClearDynamicAnimations();
        m_focusAnimate = FocusEngineAnimate();
        m_state = FocusEngineState::Idle;
        break;
      case FocusEngineState::Update:
        {
          CRect rect = focus.itemFocus->GetSelectionRenderRect();
          if (!rect.IsEmpty())
          {
            FocusEngineAnimate focusAnimate = m_focusAnimate;
            std::vector<CAnimation> animations;
            // handle control slide
            if (m_focusSlide && (fabs(focusAnimate.slideX) > 0.0f || fabs(focusAnimate.slideY) > 0.0f))
            {
              float screenDX =   focusAnimate.slideX  * focusAnimate.maxScreenSlideX;
              float screenDY = (-focusAnimate.slideY) * focusAnimate.maxScreenSlideY;
              TiXmlElement node("animation");
              node.SetAttribute("reversible", "false");
              node.SetAttribute("effect", "slide");
              node.SetAttribute("start", "0, 0");
              std::string temp = StringUtils::Format("%f, %f", screenDX, screenDY);
              node.SetAttribute("end", temp);
              //node.SetAttribute("time", "10");
              node.SetAttribute("condition", "true");
              TiXmlText text("dynamic");
              node.InsertEndChild(text);

              CAnimation anim;
              anim.Create(&node, rect, 0);
              animations.push_back(anim);
            }
            // handle control zoom
            if (m_focusZoom && (focusAnimate.zoomX > 0.0f && focusAnimate.zoomY > 0.0f))
            {
              TiXmlElement node("animation");
              node.SetAttribute("reversible", "false");
              node.SetAttribute("effect", "zoom");
              node.SetAttribute("start", "100, 100");
              float aspectRatio = rect.Width()/ rect.Height();
              //CLog::Log(LOGDEBUG, "FocusEngineState::Update: aspectRatio(%f)", aspectRatio);
              if (aspectRatio > 2.5f)
              {
                CRect newRect(rect);
                newRect.x1 -= 2;
                newRect.y1 -= 2;
                newRect.x2 += 8;
                newRect.y2 += 8;
                // format is end="x,y,width,height"
                std::string temp = StringUtils::Format("%f, %f, %f, %f",
                  newRect.x1, newRect.y1, newRect.Width(), newRect.Height());
                node.SetAttribute("end", temp);
              }
              else
              {
                std::string temp = StringUtils::Format("%f, %f", focusAnimate.zoomX, focusAnimate.zoomY);
                node.SetAttribute("end", temp);
              }
              node.SetAttribute("center", "auto");
              node.SetAttribute("condition", "true");
              TiXmlText text("dynamic");
              node.InsertEndChild(text);

              CAnimation anim;
              anim.Create(&node, rect, 0);
              animations.push_back(anim);
            }
            m_focus.itemFocus->ResetAnimation(ANIM_TYPE_DYNAMIC);
            m_focus.itemFocus->SetDynamicAnimations(animations);
          }
          m_state = FocusEngineState::Idle;
        }
        break;
    }
  }
}

void CFocusEngineHandler::ClearAnimation()
{
  CSingleLock lock(m_stateLock);
  m_state = FocusEngineState::Clear;
}

void CFocusEngineHandler::UpdateAnimation(FocusEngineAnimate &focusAnimate)
{
  CSingleLock lock(m_stateLock);
  m_focusAnimate = focusAnimate;
  m_state = FocusEngineState::Update;
}

void CFocusEngineHandler::EnableFocusZoom(bool enable)
{
  CSingleLock lock(m_stateLock);
  m_focusZoom = enable;
}

void CFocusEngineHandler::EnableFocusSlide(bool enable)
{
  CSingleLock lock(m_stateLock);
  m_focusSlide = enable;
}

void CFocusEngineHandler::InvalidateFocus(CGUIControl *control)
{
  CSingleLock lock(m_focusLock);
  if (m_focus.rootFocus == control || m_focus.itemFocus == control)
    m_focus = FocusEngineFocus();

}

const int
CFocusEngineHandler::GetFocusWindowID()
{
  CSingleLock lock(m_focusLock);
  return m_focus.windowID;
}

const CRect
CFocusEngineHandler::GetFocusRect()
{
  FocusEngineFocus focus;
  // skip finding focused window, use current
  CSingleLock lock(m_focusLock);
  focus.window = m_focus.window;
  focus.windowID = m_focus.windowID;
  lock.Leave();
  if (focus.window && focus.windowID != 0 && focus.windowID != WINDOW_INVALID)
  {
    UpdateFocus(focus);
    if (focus.itemFocus)
    {
      CRect focusedRenderRect = focus.itemFocus->GetSelectionRenderRect();
      return focusedRenderRect;
    }
  }
  return CRect();
}

bool CFocusEngineHandler::ShowFocusRect()
{
  return m_showFocusRect;
}

bool CFocusEngineHandler::ShowVisibleRects()
{
  return m_showVisibleRects;
}


ORIENTATION CFocusEngineHandler::GetFocusOrientation()
{
  FocusEngineFocus focus;
  // skip finding focused window, use current
  CSingleLock lock(m_focusLock);
  focus.window = m_focus.window;
  focus.windowID = m_focus.windowID;
  lock.Leave();
  if (focus.window && focus.windowID != 0 && focus.windowID != WINDOW_INVALID)
  {
    UpdateFocus(focus);
    if (focus.itemFocus)
    {
      switch(focus.itemFocus->GetControlType())
      {
        case CGUIControl::GUICONTROL_LISTGROUP:
          return focus.rootFocus->GetOrientation();
          break;
        case CGUIControl::GUICONTROL_BUTTON:
        case CGUIControl::GUICONTROL_IMAGE:
          {
            CGUIControl *parentFocusedControl = focus.itemFocus->GetParentControl();
            if (parentFocusedControl)
              return parentFocusedControl->GetOrientation();
          }
          break;
        default:
          break;
      }
      return focus.itemFocus->GetOrientation();
    }
  }
  return UNDEFINED;
}

void CFocusEngineHandler::UpdateFocus(FocusEngineFocus &focus)
{
  // if focus.window is valid, use it and
  // skip finding focused window else invalidate focus
  if (!focus.window || focus.windowID == 0 || focus.windowID == WINDOW_INVALID)
  {
    focus.windowID = g_windowManager.GetActiveWindowID();
    focus.window = g_windowManager.GetWindow(focus.windowID);
    if (!focus.window)
      return;
    if(focus.windowID == 0 || focus.windowID == WINDOW_INVALID)
    {
      focus = FocusEngineFocus();
      return;
    }
  }

  focus.rootFocus = focus.window->GetFocusedControl();
  if (!focus.rootFocus)
    return;

  if (focus.rootFocus->GetControlType() == CGUIControl::GUICONTROL_UNKNOWN)
    return;

  if (!focus.rootFocus->HasFocus())
    return;

  switch(focus.rootFocus->GetControlType())
  {
    // include all known types of controls
    // we do not really need to do this but compiler
    // will generate a warning if a new one is added.
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
    case CGUIControl::GUICONTROL_GROUP:
    case CGUIControl::GUICONTROL_SCROLLBAR:
    case CGUIControl::GUICONTROL_MULTISELECT:
    case CGUIControl::GUICONTAINER_LIST:
    case CGUIControl::GUICONTAINER_WRAPLIST:
    case CGUIControl::GUICONTAINER_EPGGRID:
    case CGUIControl::GUICONTAINER_PANEL:
      {
        focus.itemFocus = focus.rootFocus->GetSelectionControl();
      }
      break;
    case CGUIControl::GUICONTAINER_FIXEDLIST:
      {
        focus.itemFocus = focus.rootFocus->GetSelectionControl();
      }
      break;
  }
}

std::vector<FocusEngineItem> * CFocusEngineHandler::GetVisible()
{
  // skip finding focused window, use current
  CSingleLock lock(m_focusLock);
  if (m_focus.window && m_focus.windowID != 0 && m_focus.windowID != WINDOW_INVALID)
    return &m_focus.items;
  return nullptr;
}

void CFocusEngineHandler::UpdateVisible(FocusEngineFocus &focus)
{
  size_t visibilityCount = focus.items.size();

  std::vector<CGUIControl *> controls;
  focus.window->GetControlsFromLookUpMap(controls);

  // add in missing containers
  for (auto it = controls.begin(); it != controls.end(); ++it)
  {
    bool  addControl = false;
    CGUIControl *control = *it;
    switch(control->GetControlType())
    {
      // include all known types of controls
      // we do not really need to do this but compiler
      // will generate a warning if a new one is added.
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
      case CGUIControl::GUICONTROL_LISTLABEL:
      case CGUIControl::GUICONTROL_SCROLLBAR:
      case CGUIControl::GUICONTROL_MULTISELECT:
        {
          addControl = control->CanFocus() && control->IsVisibleFromSkin();
        }
        break;
      case CGUIControl::GUICONTROL_GROUP:
       {
          addControl = control->CanFocus() && control->IsVisibleFromSkin();
          if (addControl)
          {
            if (focus.window->IsMediaWindow())
            {
              int data1 = ((CGUIMediaWindow*)(focus.window))->GetViewContainerID();
              const CGUIControl *control1 = focus.window->GetControl(data1);
              if (control1 && control1->IsContainer())
              {
                std::vector<CGUIControl *> mediaControls;
                //((IGUIContainer *)control1)->GetContainers(mediaControls);
              }

            }

            CGUIControlGroup *groupControl = (CGUIControlGroup*)control;
            std::vector<CGUIControl *> groupControls;
            groupControl->GetContainers(groupControls);
            for (auto groupIt = groupControls.begin(); groupIt != groupControls.end(); ++groupIt)
            {
              if ((*groupIt)->CanFocus() && (*groupIt)->IsVisibleFromSkin())
              {
                AddVisible(focus, *groupIt);
                addControl = false;
              }
            }
          }
        }
        break;
      case CGUIControl::GUICONTROL_GROUPLIST:
      case CGUIControl::GUICONTROL_LISTGROUP:
       {
          addControl = control->CanFocus() && control->IsVisibleFromSkin();
          if (addControl)
          {
            CGUIControlGroup *groupControl = (CGUIControlGroup*)control;
            std::vector<CGUIControl *> groupControls;
            groupControl->GetContainers(groupControls);
            for (auto groupIt = groupControls.begin(); groupIt != groupControls.end(); ++groupIt)
            {
              if ((*groupIt)->CanFocus() && (*groupIt)->IsVisibleFromSkin())
              {
                AddVisible(focus, *groupIt);
                addControl = false;
              }
            }
          }
        }
        break;
      case CGUIControl::GUICONTAINER_FIXEDLIST:
      case CGUIControl::GUICONTAINER_WRAPLIST:
      case CGUIControl::GUICONTAINER_EPGGRID:
      case CGUIControl::GUICONTAINER_PANEL:
        {
          //CGUIControl *parent = control->GetParentControl();
          //if (parent)
          //  control = parent;
          addControl = control->CanFocus() && control->IsVisibleFromSkin();
          if (addControl)
          {
            CGUIBaseContainer *baseContainer = (CGUIBaseContainer*)control;
            CGUIListItemLayout *layout = baseContainer->GetFocusedLayout();
            if (layout)
            {
              CGUIListGroup *group =  layout->GetGroup();
              std::vector<CGUIControl *> groupControls;
              group->GetContainers(groupControls);
              for (auto groupIt = groupControls.begin(); groupIt != groupControls.end(); ++groupIt)
              {
                if ((*groupIt)->CanFocus() && (*groupIt)->IsVisibleFromSkin())
                {
                  AddVisible(focus, *groupIt);
                  addControl = false;
                }
              }
            }
          }
        }
        break;
     case CGUIControl::GUICONTAINER_LIST:
        {
          CGUIControl *parent = control->GetParentControl();
          if (parent)
            control = parent;
          addControl = control->CanFocus() && control->IsVisibleFromSkin();
        }
        break;
    }
    if (addControl)
    {
      auto foundControl = std::find_if(focus.items.begin(), focus.items.end(),
          [&](FocusEngineItem item)
          { return item.control == control;
      });
      // missing from our list, add it in
      if (foundControl == focus.items.end())
      {
        CRect renderRect = control->GetRenderRect();
        //CLog::Log(LOGDEBUG, "%s: add renderRect - %f,%f %f x %f", __FUNCTION__,
        //  renderRect.x1, renderRect.y1, renderRect.Width(), renderRect.Height());
        if (!renderRect.IsEmpty())
        {
          FocusEngineItem item;
          item.control = control;
          focus.items.push_back(item);
        }
        //CLog::Log(LOGDEBUG, "%s: add %p %lu", __FUNCTION__, control, focus.itemsVisible.size());
      }
    }
  }

  bool visibleChanged = false;
  visibleChanged = visibilityCount != focus.items.size();
  if (visibleChanged)
  {
    for (auto it = focus.items.begin(); it != focus.items.end(); ++it)
    {
      CRect renderRect = (*it).control->GetRenderRect();
      CLog::Log(LOGDEBUG, "%s: add renderRect - %f,%f %f x %f", __FUNCTION__,
        renderRect.x1, renderRect.y1, renderRect.Width(), renderRect.Height());
      CLog::Log(LOGDEBUG, "%s: add %p %lu", __FUNCTION__, (*it).control, focus.items.size());
    }
  }
}

void CFocusEngineHandler::AddVisible(FocusEngineFocus &focus, CGUIControl *control)
{
    auto foundControl = std::find_if(focus.items.begin(), focus.items.end(),
        [&](FocusEngineItem item)
        { return item.control == control;
    });
  // missing from our list, add it in
  if (foundControl == focus.items.end())
  {
    CRect renderRect = control->GetRenderRect();
    CLog::Log(LOGDEBUG, "%s: add renderRect - %f,%f %f x %f", __FUNCTION__,
      renderRect.x1, renderRect.y1, renderRect.Width(), renderRect.Height());
    FocusEngineItem item;
    item.control = control;
    focus.items.push_back(item);
    CLog::Log(LOGDEBUG, "%s: add %p %lu", __FUNCTION__, control, focus.items.size());
  }
}

void CFocusEngineHandler::RemoveVisible(CGUIControl *visible)
{
}

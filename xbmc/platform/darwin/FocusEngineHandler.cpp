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

 // https://developer.apple.com/library/content/documentation/General/Conceptual/AppleTV_PG/WorkingwiththeAppleTVRemote.html#//apple_ref/doc/uid/TP40015241-CH5-SW5

#include "FocusEngineHandler.h"

#include "guilib/GUIControl.h"
#include "guilib/GUIWindow.h"
#include "guilib/GUIDialog.h"
#include "guilib/GUIListGroup.h"
#include "guilib/GUIListLabel.h"
#include "guilib/GUIBaseContainer.h"
#include "guilib/GUIControlGroupList.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/GUIControlFactory.h"
#include "threads/Atomics.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/XBMCTinyXML.h"
#include "platform/darwin/DarwinUtils.h"

#define VALID_FOCUS_WINDOW(focus) (focus.window && focus.windowID != 0 && focus.windowID != WINDOW_INVALID)
#define NOT_VALID_FOCUS_WINDOW(focus) (!focus.window || focus.windowID == 0 && focus.windowID == WINDOW_INVALID)

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
, m_showVisibleRects(false)
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
  m_focus.busy = focus.busy;
  m_focus.hideViews = focus.hideViews;

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

  auto foundControl = std::find_if(m_focus.items.begin(), m_focus.items.end(),
      [&](GUIFocusabilityItem item)
      { return item.control == control;
  });
  if (foundControl != m_focus.items.end())
    m_focus.items.erase(foundControl);
}

const int
CFocusEngineHandler::GetFocusWindowID()
{
  CSingleLock lock(m_focusLock);
  return m_focus.windowID;
}

const bool
CFocusEngineHandler::IsBusy()
{
  CSingleLock lock(m_focusLock);
  return m_focus.busy;
}

const bool
CFocusEngineHandler::NeedToHideViews()
{
  CSingleLock lock(m_focusLock);
  return m_focus.hideViews;
}

const CRect
CFocusEngineHandler::GetFocusRect()
{
  FocusEngineFocus focus;
  // skip finding focused window, use current
  CSingleLock lock(m_focusLock);
  focus.window = m_focus.window;
  focus.windowID = m_focus.windowID;
  focus.busy = m_focus.busy;
  focus.hideViews = m_focus.hideViews;
  lock.Leave();
  if (VALID_FOCUS_WINDOW(focus))
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

CGUIControl*
CFocusEngineHandler::GetFocusControl()
{
  CSingleLock lock(m_focusLock);
  if (m_focus.itemFocus)
    return m_focus.itemFocus;
  // if we do not have a focused control
  // kick back to the window control
  if (m_focus.window)
    return m_focus.window;
  return nullptr;
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
  focus.busy = m_focus.busy;
  focus.hideViews = m_focus.hideViews;
  lock.Leave();
  if (VALID_FOCUS_WINDOW(focus))
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
  if (NOT_VALID_FOCUS_WINDOW(focus))
  {
    focus.busy = false;
    focus.hideViews = false;
    focus.windowID = g_windowManager.GetActiveWindowID();
    // handle window id aliases but we need to keep orginal
    // window id as keymaps depend on them
    if (focus.windowID == WINDOW_FULLSCREEN_LIVETV || focus.windowID == WINDOW_VIDEO_MENU)
      focus.window = g_windowManager.GetWindow(WINDOW_FULLSCREEN_VIDEO);
    else if (focus.windowID == WINDOW_FULLSCREEN_RADIO)
      focus.window = g_windowManager.GetWindow(WINDOW_VISUALISATION);
    else
      focus.window = g_windowManager.GetWindow(focus.windowID);

    if (!focus.window)
      return;
    if(focus.windowID == 0 || focus.windowID == WINDOW_INVALID)
    {
      focus = FocusEngineFocus();
      return;
    }
  }

  // window exceptions, we hide views for these windows
  if (focus.windowID == WINDOW_DIALOG_BUSY)
    focus.busy = true;

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
    case CGUIControl::GUICONTROL_LISTLABEL:
    case CGUIControl::GUICONTROL_GROUP:
    case CGUIControl::GUICONTROL_SCROLLBAR:
    case CGUIControl::GUICONTROL_MULTISELECT:
    case CGUIControl::GUICONTROL_GROUPLIST:
    case CGUIControl::GUICONTAINER_LIST:
    case CGUIControl::GUICONTAINER_WRAPLIST:
    case CGUIControl::GUICONTAINER_EPGGRID:
    case CGUIControl::GUICONTAINER_PANEL:
    case CGUIControl::GUICONTAINER_FIXEDLIST:
      {
        focus.itemFocus = focus.rootFocus->GetSelectionControl();
      }
      break;
  }
}

void CFocusEngineHandler::GetCoreViews(std::vector<FocusEngineCoreViews> &views)
{
  // skip finding focused window, use current
  CSingleLock lock(m_focusLock);
  if (VALID_FOCUS_WINDOW(m_focus))
    views = m_focus.views;
}

void CFocusEngineHandler::GetGUIFocusabilityItems(std::vector<GUIFocusabilityItem> &items)
{
  // skip finding focused window, use current
  CSingleLock lock(m_focusLock);
  if (VALID_FOCUS_WINDOW(m_focus))
  {
    if (m_focus.rootFocus)
      items = m_focus.items;
    else
      items.clear();
  }
}

void CFocusEngineHandler::SetGUIFocusabilityItems(const CFocusabilityTracker &focusabilityTracker)
{
  CSingleLock lock(m_focusLock);
  if (VALID_FOCUS_WINDOW(m_focus))
  {
    auto items = focusabilityTracker.GetItems();
    // there should always something that has focus. if incoming focusabilityTracker is empty,
    // it is a transition effect or nothing reported a dirty region and rendering was skipped.
    if (!items.empty())
    {
      std::vector<GUIFocusabilityItem> verifiedItems;
      for (auto it = items.begin(); it != items.end(); ++it)
      {
        if ((*it).control->HasProcessed() && (*it).control->IsVisible())
          verifiedItems.push_back(*it);
      }

      if (verifiedItems.size() != m_focus.items.size())
      {
        m_focus.items = verifiedItems;
        std::sort(m_focus.items.begin(), m_focus.items.end(),
          [] (GUIFocusabilityItem const& a, GUIFocusabilityItem const& b)
        {
            return a.renderOrder < b.renderOrder;
        });
      }
      UpdateFocusability();
      UpdateNeedToHideViews();
      //if (m_focus.hideViews)
      //  CLog::Log(LOGDEBUG, "Control is animating");
    }
  }

  // trigger tvOS main thread to rebuild views and focus
  CDarwinUtils::UpdateFocusLayerMainThread();
}

void CFocusEngineHandler::UpdateFocusability()
{
  // use current focused window
  CSingleLock lock(m_focusLock);
  if (VALID_FOCUS_WINDOW(m_focus))
  {
    CRect boundsRect = CRect(0, 0, (float)g_graphicsContext.GetWidth(), (float)g_graphicsContext.GetHeight());
    // update all renderRects 1st, we depend on them being correct in next step.
    for (auto it = m_focus.items.begin(); it != m_focus.items.end(); ++it)
    {
      auto &item = *it;
      item.renderRect = item.control->GetRenderRect();
      if (item.parentView)
        item.viewRenderRect = item.parentView->GetRenderRect();
    }
    // qualify items and rather them into views. The enclosing
    // view will be last in draw order.
    std::vector<FocusEngineCoreItem> items;
    std::vector<FocusEngineCoreViews> views;
    for (auto it = m_focus.items.begin(); it != m_focus.items.end(); ++it)
    {
      auto &focusabilityItem = *it;
      // should never be an empty render rect :)
      if (focusabilityItem.renderRect.IsEmpty())
        continue;

#if true
      // clip all render rects to screen bounds
      if (focusabilityItem.renderRect.x1 < boundsRect.x1)
        focusabilityItem.renderRect.x1 = boundsRect.x1;
      if (focusabilityItem.renderRect.y1 < boundsRect.y1)
        focusabilityItem.renderRect.y1 = boundsRect.y1;
      if (focusabilityItem.renderRect.x2 > boundsRect.x2)
        focusabilityItem.renderRect.x2 = boundsRect.x2;
      if (focusabilityItem.renderRect.y2 > boundsRect.y2)
        focusabilityItem.renderRect.y2 = boundsRect.y2;

      //  remove zero width or zero height rects
      if (focusabilityItem.renderRect.x1 == focusabilityItem.renderRect.x2)
        continue;
      if (focusabilityItem.renderRect.y1 == focusabilityItem.renderRect.y2)
        continue;

      // remove rects that might have a negative width/height
      // with respect to bounds rect
      if (focusabilityItem.renderRect.x2 < boundsRect.x1)
        continue;
      if (focusabilityItem.renderRect.y2 < boundsRect.y1)
        continue;

      // remove rects that might have a negative width/height
      if (focusabilityItem.renderRect.x2 < focusabilityItem.renderRect.x1)
        continue;
      if (focusabilityItem.renderRect.y2 < focusabilityItem.renderRect.y1)
        continue;
#endif
      if ((*it).control == (*it).parentView)
      {
        FocusEngineCoreViews view;
        view.rect = (*it).renderRect;
        view.type = TranslateControlType((*it).control, (*it).parentView);
        if (items.empty() && (view.type != "dialog" && view.type != "window"))
          continue;
#if false
        for (auto &item : items)
        {
          // clip all item rects to enclosing view rect
          if (item.rect.x1 < view.rect.x1)
            item.rect.x1 = view.rect.x1;
          if (item.rect.y1 < view.rect.y1)
            item.rect.y1 = view.rect.y1;
          if (item.rect.x2 > view.rect.x2)
            item.rect.x2 = view.rect.x2;
          if (item.rect.y2 > view.rect.y2)
            item.rect.y2 = view.rect.y2;
        }
#endif
        view.items = items;
        view.control = (*it).control;
        views.push_back(view);
        items.clear();
      }
      else
      {
        FocusEngineCoreItem item;
        item.rect = (*it).renderRect;
        item.type = TranslateControlType((*it).control, (*it).parentView);
        item.control = (*it).control;
        items.push_back(item);
      }
    }
#if false
    // the window
    for (auto it = views.begin(); it != views.end(); )
    {
      auto &view = *it;
      if (view.items.empty())
        it = views.erase(it);
      else
        ++it;
    }
#endif
#if false
    // run through our views in reverse order (so that last is checked first)
    //for (auto it = views.begin(); it != views.rend(); )
    for (auto it = views.rbegin(); it != views.rend(); )
    {
      auto &view = *it;
      // ignore a view that is obscured by the higher views
      auto obscuredIt = it;
      ++obscuredIt;
      bool IsObscured = false;
      std::vector<CRect> partialOverlaps;
      //for (;obscuredIt != views.end(); ++obscuredIt)
      for (;obscuredIt != views.rend(); ++obscuredIt)
      {
        auto &testViewItem = *obscuredIt;

        // should never be an empty rect :)
        if (testViewItem.rect.IsEmpty())
          continue;
        // ignore rects that are the same size as display bounds
        if (testViewItem.rect == boundsRect)
          continue;
        CRect testRect = boundsRect;
        testRect.Intersect(testViewItem.rect);
        if (testRect == boundsRect)
          continue;

        if (testViewItem.rect.RectInRect(view.rect))
        {
          IsObscured = true;
          break;
        }

        // collect intersections that overlap into obscuringRect.
        CRect intersection = testViewItem.rect;
        intersection.Intersect(view.rect);
        if (!intersection.IsEmpty())
          partialOverlaps.push_back(intersection);
      }
      if (!IsObscured && partialOverlaps.size() > 0)
      {
        std::vector<CRect> rects = view.rect.SubtractRects(partialOverlaps);
        if (rects.size() == 0)
          IsObscured = true;
      }
      if (IsObscured)
        it = views.erase(it);
      else
        ++it;
    }
#endif
    m_focus.views = views;
  }
}

void CFocusEngineHandler::UpdateNeedToHideViews()
{
  bool debug = false;
  // we hide views when certain controls are animating
  // such as slide out effects. Check for two cases.
  // Sliding and Scrolling. This speeds up tvOS focus engine
  // handling in that we do not keep rebuilding views while
  // the animation effect is ocurring. Once views are stable
  // again, then we rebuild and setup who has focus.
  m_focus.hideViews = false;

  CGUIBaseContainer *baseContainer = dynamic_cast<CGUIBaseContainer*>(m_focus.rootFocus);
  if (baseContainer)
  {
    if (baseContainer->IsSliding())
    {
      if (debug)
        CLog::Log(LOGDEBUG, "UpdateNeedToHideViews:CGUIBaseContainer");
      m_focus.hideViews = true;
      return;
    }
    if (baseContainer->IsScrolling())
    {
      if (debug)
        CLog::Log(LOGDEBUG, "UpdateNeedToHideViews:CGUIBaseContainer");
      m_focus.hideViews = true;
      return;
    }
  }

  CGUIControlGroupList *controlGroupList = dynamic_cast<CGUIControlGroupList*>(m_focus.rootFocus);
  if (controlGroupList)
  {
    if (controlGroupList->IsSliding())
    {
      if (debug)
        CLog::Log(LOGDEBUG, "UpdateNeedToHideViews:CGUIControlGroupList");
      m_focus.hideViews = true;
      return;
    }
    if (controlGroupList->IsScrolling())
    {
      if (debug)
        CLog::Log(LOGDEBUG, "UpdateNeedToHideViews:CGUIControlGroupList");
      m_focus.hideViews = true;
      return;
    }
  }

  for (auto viewIt = m_focus.views.begin(); viewIt != m_focus.views.end(); ++viewIt)
  {
    if ((*viewIt).control->IsSliding())
    {
      if (debug)
        CLog::Log(LOGDEBUG, "UpdateNeedToHideViews:CGUIControl");
      m_focus.hideViews = true;
      return;
    }
    if ((*viewIt).control->IsScrolling())
    {
      if (debug)
        CLog::Log(LOGDEBUG, "UpdateNeedToHideViews:CGUIControl");
      m_focus.hideViews = true;
      return;
    }
    for (auto itemIt = (*viewIt).items.begin(); itemIt != (*viewIt).items.end(); ++itemIt)
    {
      if ((*itemIt).control->IsSliding())
      {
        if (debug)
          CLog::Log(LOGDEBUG, "UpdateNeedToHideViews:CGUIControl");
        m_focus.hideViews = true;
        return;
      }
      if ((*itemIt).control->IsScrolling())
      {
        if (debug)
          CLog::Log(LOGDEBUG, "UpdateNeedToHideViews:CGUIControl");
        m_focus.hideViews = true;
        return;
      }
    }
  }
}

std::string CFocusEngineHandler::TranslateControlType(CGUIControl *control, CGUIControl *parent)
{
  if (parent)
  {
    CGUIDialog *dialog = dynamic_cast<CGUIDialog*>(parent);
    if (dialog)
      return "dialog";
    CGUIWindow *window = dynamic_cast<CGUIWindow*>(parent);
    if (window)
      return "window";
  }
  // TranslateControlType returns 'group' for
  // both CGUIListGroup and CGUIControlGroup
  CGUIListGroup *listgroup = dynamic_cast<CGUIListGroup*>(control);
  if (listgroup)
    return "listgroup";

  return CGUIControlFactory::TranslateControlType(control->GetControlType());
}

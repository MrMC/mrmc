#pragma once

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

#include "guilib/GUIControl.h"
#include "threads/CriticalSection.h"

class CAnimation;
class CGUIWindow;

typedef enum FocusEngineState
{
  Idle,
  Clear,
  Update,
} FocusEngineState;

typedef struct
{
  CGUIControl *control;
  CRect renderRect;
} FocusEngineItem;
typedef struct
{
  int windowID = 0;
  CGUIWindow  *window = nullptr;
  CGUIControl *rootFocus = nullptr;
  CGUIControl *itemFocus = nullptr;
  std::vector<FocusEngineItem> items;
} FocusEngineFocus;

typedef struct
{
  float zoomX = 0.0f;
  float zoomY = 0.0f;
  float slideX = 0.0f;
  float slideY = 0.0f;
  // amx amount (screen pixels) that control slides
  float maxScreenSlideX = 15.0f;
  float maxScreenSlideY = 15.0f;
} FocusEngineAnimate;

class CFocusEngineHandler
{
 public:
  static CFocusEngineHandler& GetInstance();

  void          Process();
  void          ClearAnimation();
  void          UpdateAnimation(FocusEngineAnimate &focusAnimate);
  void          EnableFocusZoom(bool enable);
  void          EnableFocusSlide(bool enable);
  void          InvalidateFocus(CGUIControl *control);
  const int     GetFocusWindowID();
  const CRect   GetFocusRect();
  bool          ShowFocusRect();
  bool          ShowVisibleRects();
  ORIENTATION   GetFocusOrientation();
  std::vector<FocusEngineItem> *GetVisible();

private:
  void          UpdateFocus(FocusEngineFocus &focus);
  void          UpdateVisible(FocusEngineFocus &focus);
  void          AddVisible(FocusEngineFocus &focus, CGUIControl *visible);
  void          RemoveVisible(CGUIControl *visible);

  CFocusEngineHandler();
  CFocusEngineHandler(CFocusEngineHandler const&);
  CFocusEngineHandler& operator=(CFocusEngineHandler const&);

  bool m_focusZoom;
  bool m_focusSlide;
  bool m_showFocusRect;
  bool m_showVisibleRects;
  FocusEngineState m_state;
  CCriticalSection m_stateLock;
  FocusEngineFocus m_focus;
  CCriticalSection m_focusLock;
  ORIENTATION m_focusedOrientation;
  FocusEngineAnimate m_focusAnimate;
  static CFocusEngineHandler* m_instance;
};

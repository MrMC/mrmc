#pragma once

/*
 *      Copyright (C) 2011-2018 Team MrMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "rendering/gles/RenderSystemGLES.h"
#include "utils/GlobalsHandling.h"
#include <EGL/egl.h>
#include "windowing/WinSystem.h"

class IDispResource;

class CWinSystemAndroidEGL : public CWinSystemBase, public CRenderSystemGLES
{
public:
  CWinSystemAndroidEGL();
  virtual ~CWinSystemAndroidEGL();

  virtual bool  InitWindowSystem();
  virtual bool  DestroyWindowSystem();
  virtual bool  CreateNewWindow(const std::string& name, bool fullScreen, RESOLUTION_INFO& res, PHANDLE_EVENT_FUNC userFunction);
  virtual bool  DestroyWindow();
  virtual bool  ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop);
  virtual bool  SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays);
  virtual void  UpdateResolutions();
  virtual bool  IsExtSupported(const char* extension);
  virtual bool  CanDoWindowed() { return false; }

  virtual void  ShowOSMouse(bool show);
  virtual bool  HasCursor();

  virtual void  NotifyAppActiveChange(bool bActivated);

  virtual bool  Minimize();
  virtual bool  Restore() ;
  virtual bool  Hide();
  virtual bool  Show(bool raise = true);
  virtual bool  BringToFront();

  virtual void  Register(IDispResource *resource);
  virtual void  Unregister(IDispResource *resource);
          void  OnLostDevice();
          void  OnResetDevice();
          void  OnAppFocusChange(bool focus);

  virtual bool  Support3D(int width, int height, uint32_t mode)     const;
  virtual bool  ClampToGUIDisplayLimits(int &width, int &height);

  EGLConfig     GetEGLConfig();
  EGLDisplay    GetEGLDisplay();
  EGLContext    GetEGLContext();

protected:
  virtual bool  PresentRenderImpl(const CDirtyRegionList &dirty);
  virtual void  SetVSyncImpl(bool enable);

  bool          CreateWindow(RESOLUTION_INFO &res);
  bool          GetNativeResolution(RESOLUTION_INFO *res);
  bool          SetNativeResolution(const RESOLUTION_INFO &res);
  bool          ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions);

  int                   m_displayWidth;
  int                   m_displayHeight;
  EGLint                m_result;

  EGLDisplay            m_display;
  EGLSurface            m_surface;
  EGLContext            m_context;
  EGLConfig             m_config;

  std::string           m_extensions;
  bool                  m_resourceLost;
  CCriticalSection      m_resourceSection;
  std::vector<IDispResource*> m_resources;
};

XBMC_GLOBAL_REF(CWinSystemAndroidEGL,g_Windowing);
#define g_Windowing XBMC_GLOBAL_USE(CWinSystemAndroidEGL)


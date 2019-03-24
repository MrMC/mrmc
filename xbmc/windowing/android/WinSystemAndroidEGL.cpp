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
#include "system.h"

#ifdef HAS_EGL

#include "WinSystemAndroidEGL.h"
#include "filesystem/SpecialProtocol.h"
#include "guilib/GraphicContext.h"
#include "settings/DisplaySettings.h"
#include "guilib/IDirtyRegionSolver.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/DisplaySettings.h"
#include "guilib/DispResource.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "utils/Stopwatch.h"
#include <vector>
#include <float.h>

#include <androidjni/SystemProperties.h>
#include <androidjni/Display.h>
#include <androidjni/View.h>
#include <androidjni/Window.h>
#include <androidjni/WindowManager.h>
#include <androidjni/Build.h>
#include <androidjni/System.h>

#include <EGL/egl.h>
#include "guilib/gui3d.h"
#include "utils/log.h"
#include "system.h"
#include "settings/Settings.h"
#include "utils/StringUtils.h"
#include "utils/SysfsUtils.h"
#include "platform/android/activity/XBMCApp.h"

#define CheckError() m_result = eglGetError(); if(m_result != EGL_SUCCESS) CLog::Log(LOGERROR, "EGL error in %s: %x",__FUNCTION__, m_result);

static bool s_hasModeApi = false;
static std::vector<RESOLUTION_INFO> s_res_displayModes;
static RESOLUTION_INFO s_res_cur_displayMode;

static float currentRefreshRate()
{
  if (s_hasModeApi)
    return s_res_cur_displayMode.fRefreshRate;

  CJNIWindow window = CXBMCApp::get()->getWindow();
  if (window)
  {
    float preferredRate = window.getAttributes().getpreferredRefreshRate();
    if (preferredRate > 20.0 && preferredRate < 70.0)
    {
      CLog::Log(LOGINFO, "CWinSystemAndroidEGL: Preferred refresh rate: %f", preferredRate);
      return preferredRate;
    }
    CJNIView view(window.getDecorView());
    if (view) {
      CJNIDisplay display(view.getDisplay());
      if (display)
      {
        float reportedRate = display.getRefreshRate();
        if (reportedRate > 20.0 && reportedRate < 70.0)
        {
          CLog::Log(LOGINFO, "CWinSystemAndroidEGL: Current display refresh rate: %f", reportedRate);
          return reportedRate;
        }
      }
    }
  }
  CLog::Log(LOGDEBUG, "found no refresh rate");
  return 60.0;
}

static void fetchDisplayModes()
{
  s_hasModeApi = false;
  s_res_displayModes.clear();

  CJNIDisplay display = CXBMCApp::get()->getWindow().getDecorView().getDisplay();

  if (display)
  {
    CJNIDisplayMode m = display.getMode();
    if (m)
    {
      if (m.getPhysicalWidth() > m.getPhysicalHeight())   // Assume unusable if portrait is returned
      {
        s_res_cur_displayMode.strId = StringUtils::Format("%d", m.getModeId());
        s_res_cur_displayMode.iWidth = s_res_cur_displayMode.iScreenWidth = m.getPhysicalWidth();
        s_res_cur_displayMode.iHeight = s_res_cur_displayMode.iScreenHeight = m.getPhysicalHeight();
        s_res_cur_displayMode.fRefreshRate = m.getRefreshRate();
        s_res_cur_displayMode.dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
        s_res_cur_displayMode.iScreen       = 0;
        s_res_cur_displayMode.bFullScreen   = true;
        s_res_cur_displayMode.iSubtitles    = (int)(0.965 * s_res_cur_displayMode.iHeight);
        s_res_cur_displayMode.fPixelRatio   = 1.0f;
        s_res_cur_displayMode.strMode       = StringUtils::Format("%dx%d @ %.6f%s - Full Screen", s_res_cur_displayMode.iScreenWidth, s_res_cur_displayMode.iScreenHeight, s_res_cur_displayMode.fRefreshRate,
                                                                  s_res_cur_displayMode.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");

        std::vector<CJNIDisplayMode> modes = display.getSupportedModes();
        for (auto m : modes)
        {
          RESOLUTION_INFO res;
          res.strId = StringUtils::Format("%d", m.getModeId());
          res.iWidth = res.iScreenWidth = m.getPhysicalWidth();
          res.iHeight = res.iScreenHeight = m.getPhysicalHeight();
          res.fRefreshRate = m.getRefreshRate();
          res.dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
          res.iScreen       = 0;
          res.bFullScreen   = true;
          res.iSubtitles    = (int)(0.965 * res.iHeight);
          res.fPixelRatio   = 1.0f;
          res.strMode       = StringUtils::Format("%dx%d @ %.6f%s - Full Screen", res.iScreenWidth, res.iScreenHeight, res.fRefreshRate,
                                                  res.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");

          s_res_displayModes.push_back(res);
        }
        if (s_res_displayModes.size() == 0)
          CLog::Log(LOGWARNING, "CWinSystemAndroidEGL : fetchDisplayModes : no mode list");
      }
    }
    else
      CLog::Log(LOGWARNING, "CWinSystemAndroidEGL : fetchDisplayModes : no current mode");
  }
  else
    CLog::Log(LOGWARNING, "CWinSystemAndroidEGL : fetchDisplayModes : no display");
  // Enable if mode api is borked
//  if (s_res_displayModes.size() > 1)
  if (s_res_displayModes.size() > 0)
  {
    s_hasModeApi = true;
    CLog::Log(LOGINFO, "CWinSystemAndroidEGL : fetchDisplayModes : found %d modes", s_res_displayModes.size());
  }
  else
    CLog::Log(LOGWARNING, "CWinSystemAndroidEGL : fetchDisplayModes : no modes");
}

////////////////////////////////////////////////////////////////////////////////////////////
CWinSystemAndroidEGL::CWinSystemAndroidEGL() : CWinSystemBase()
{
  m_eWindowSystem = WINDOW_SYSTEM_EGL;

  m_displayWidth      = 0;
  m_displayHeight     = 0;

  m_display           = EGL_NO_DISPLAY;
  m_surface           = EGL_NO_SURFACE;
  m_context           = EGL_NO_CONTEXT;
  m_config            = NULL;

  m_iVSyncMode        = 0;
  // by default, resources are always lost on starup
  m_resourceLost      = true;
}

CWinSystemAndroidEGL::~CWinSystemAndroidEGL()
{
  DestroyWindowSystem();
}

bool CWinSystemAndroidEGL::InitWindowSystem()
{
  RESOLUTION_INFO preferred_resolution;

  std::string displaySize;
  m_width = m_height = 0;

  if (!*CXBMCApp::GetNativeWindow(30000))
    return false;

  if (CJNIBuild::DEVICE != "foster" || CJNIBase::GetSDKVersion() >= 24)   // Buggy implementation of DisplayMode API on SATV
  {
    fetchDisplayModes();
    for (auto res : s_res_displayModes)
    {
      if (res.iWidth > m_width || res.iHeight > m_height)
      {
        m_width = res.iWidth;
        m_height = res.iHeight;
      }
    }
  }

  if (!m_width || !m_height)
  {
    // Property available on some devices
    displaySize = CJNISystemProperties::get("sys.display-size", "");
    if (!displaySize.empty())
    {
      std::vector<std::string> aSize = StringUtils::Split(displaySize, "x");
      if (aSize.size() == 2)
      {
        m_width = StringUtils::IsInteger(aSize[0]) ? atoi(aSize[0].c_str()) : 0;
        m_height = StringUtils::IsInteger(aSize[1]) ? atoi(aSize[1].c_str()) : 0;
      }
      CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL: display-size: %s(%dx%d)", displaySize.c_str(), m_width, m_height);
    }
  }

  CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL: maximum/current resolution: %dx%d", m_width, m_height);
  int limit = CSettings::GetInstance().GetInt("videoscreen.limitgui");
  switch (limit)
  {
    case 0: // auto
      m_width = 0;
      m_height = 0;
      break;

    case 9999:  // unlimited
      break;

    case 720:
      if (m_height > 720)
      {
        m_width = 1280;
        m_height = 720;
      }
      break;

    case 1080:
      if (m_height > 1080)
      {
        m_width = 1920;
        m_height = 1080;
      }
      break;
  }
  CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL: selected resolution: %dx%d", m_width, m_height);

  //nativeDisplay can be (and usually is) NULL. Don't use if(nativeDisplay) as a test!
  EGLint status;
  EGLNativeDisplayType nativeDisplay = EGL_DEFAULT_DISPLAY;
  m_display = eglGetDisplay(nativeDisplay);
  CheckError();
  if (m_display == EGL_NO_DISPLAY)
  {
    CLog::Log(LOGERROR, "EGL failed to obtain display");
    return false;
  }

  status = eglInitialize(m_display, 0, 0);
  CheckError();

  EGLint surface_type = EGL_WINDOW_BIT;
  // for the non-trivial dirty region modes, we need the EGL buffer to be preserved across updates
  if (g_advancedSettings.m_guiAlgorithmDirtyRegions == DIRTYREGION_SOLVER_COST_REDUCTION ||
      g_advancedSettings.m_guiAlgorithmDirtyRegions == DIRTYREGION_SOLVER_UNION)
    surface_type |= EGL_SWAP_BEHAVIOR_PRESERVED_BIT;

  EGLint configAttrs [] = {
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,     16,
        EGL_STENCIL_SIZE,    0,
        EGL_SAMPLE_BUFFERS,  0,
        EGL_SAMPLES,         0,
        EGL_SURFACE_TYPE,    surface_type,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
  };

  EGLint     configCount = 0;
  EGLConfig* configList = NULL;

  // Find out how many configurations suit our needs
  EGLBoolean eglStatus = eglChooseConfig(m_display, configAttrs, NULL, 0, &configCount);
  CheckError();

  if (!eglStatus || !configCount)
  {
    CLog::Log(LOGERROR, "EGL failed to return any matching configurations: %i", configCount);
    return false;
  }

  // Allocate room for the list of matching configurations
  configList = (EGLConfig*)malloc(configCount * sizeof(EGLConfig));
  if (!configList)
  {
    CLog::Log(LOGERROR, "EGL failure obtaining configuration list");
    return false;
  }

  // Obtain the configuration list from EGL
  eglStatus = eglChooseConfig(m_display, configAttrs, configList, configCount, &configCount);
  CheckError();
  if (!eglStatus || !configCount)
  {
    CLog::Log(LOGERROR, "EGL failed to populate configuration list: %d", eglStatus);
    return false;
  }

  // Select an EGL configuration that matches the native window
  m_config = configList[0];

  free(configList);

  std::string extensions = eglQueryString(m_display, EGL_EXTENSIONS);
  CheckError();
  m_extensions = " " + extensions + " ";

  return CWinSystemBase::InitWindowSystem();
}

bool CWinSystemAndroidEGL::CreateWindow(RESOLUTION_INFO &res)
{
  SetNativeResolution(res);

  EGLNativeWindowType* nativeWindow = (EGLNativeWindowType*) CXBMCApp::GetNativeWindow(30000);
  SetNativeResolution(res);

  m_surface = eglCreateWindowSurface(m_display, m_config, *nativeWindow, NULL);
  CheckError();

  int width = 0, height = 0;
  const bool failedToQuerySurfaceSize =
    !eglQuerySurface(m_display, m_surface, EGL_WIDTH, &width) ||
    !eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &height);

  if (width <= 0 || height <= 0)
  {
    CLog::Log(LOGERROR, "%s: Surface is invalid",__FUNCTION__);
    return false;
  }
  CLog::Log(LOGDEBUG, "%s: Created surface of size %ix%i",__FUNCTION__, width, height);

  EGLBoolean status;
  status = eglBindAPI(EGL_OPENGL_ES_API);
  CheckError();

  if (!(status && m_result == EGL_SUCCESS))
  {
    CLog::Log(LOGERROR, "%s: Could not bind %i api",__FUNCTION__, EGL_OPENGL_ES_API);
    return false;
  }

  if (m_context == EGL_NO_CONTEXT)
  {
    const EGLint contextAttrs[] =
    {
      EGL_CONTEXT_CLIENT_VERSION, 3,
      EGL_NONE
    };
    m_context = eglCreateContext(m_display, m_config, NULL, contextAttrs);
    if (m_context == EGL_NO_CONTEXT)
    {
      CLog::Log(LOGWARNING, "%s: EGL3 not supported; Falling back to EGL2",__FUNCTION__);
      m_RenderVersionMajor = 2;
      const EGLint contextAttrsFallback[] =
      {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
      };
      m_context = eglCreateContext(m_display, m_config, NULL, contextAttrsFallback);
    }
    else
    {
      if (!m_RenderVersionMajor)
        m_RenderVersionMajor = 3;
    }
    if (m_context == EGL_NO_CONTEXT)
    {
      CLog::Log(LOGERROR, "%s: Could not create context",__FUNCTION__);
      return false;
    }
  }

  status = eglMakeCurrent(m_display, m_surface, m_surface, m_context);
  CheckError();
  if (!status)
  {
    CLog::Log(LOGERROR, "%s: Could not bind to context",__FUNCTION__);
    return false;
  }


  // for the non-trivial dirty region modes, we need the EGL buffer to be preserved across updates
  if (g_advancedSettings.m_guiAlgorithmDirtyRegions == DIRTYREGION_SOLVER_COST_REDUCTION ||
      g_advancedSettings.m_guiAlgorithmDirtyRegions == DIRTYREGION_SOLVER_UNION)
  {
    if (!eglSurfaceAttrib(m_display, m_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED))
      CLog::Log(LOGDEBUG, "%s: Could not set EGL_SWAP_BEHAVIOR",__FUNCTION__);
  }

  m_bWindowCreated = true;

  return true;
}

bool CWinSystemAndroidEGL::DestroyWindowSystem()
{
  DestroyWindow();

  if (m_context != EGL_NO_CONTEXT && m_display != EGL_NO_DISPLAY)
  {
    eglDestroyContext(m_display, m_context);
    CheckError();
  }
  m_context = EGL_NO_CONTEXT;

  if (m_display != EGL_NO_DISPLAY)
  {
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(m_display);
    CheckError();
  }
  m_display = EGL_NO_DISPLAY;

  CWinSystemBase::DestroyWindowSystem();
  return true;
}

bool CWinSystemAndroidEGL::GetNativeResolution(RESOLUTION_INFO *res)
{
  EGLNativeWindowType *nativeWindow = (EGLNativeWindowType*)CXBMCApp::GetNativeWindow(30000);
  if (!*nativeWindow)
    return false;

  bool iswindowed = false;
  if (CJNIBase::GetSDKVersion() >= 24)
    iswindowed = CXBMCApp::get()->isInMultiWindowMode();

  if ((!m_width || !m_height) && !iswindowed)
  {
    ANativeWindow_acquire(*nativeWindow);
    m_width = ANativeWindow_getWidth(*nativeWindow);
    m_height= ANativeWindow_getHeight(*nativeWindow);
    ANativeWindow_release(*nativeWindow);
    CLog::Log(LOGNOTICE,"CWinSystemAndroidEGL: window resolution: %dx%d", m_width, m_height);
  }

  if (s_hasModeApi)
  {
    *res = s_res_cur_displayMode;
    if (!m_width || !m_height)
    {
      m_width = res->iWidth;
      m_height = res->iHeight;
    }
    else
    {
      res->iWidth = m_width;
      res->iHeight = m_height;
    }
  }
  else
  {
    res->strId = "-1";
    res->fRefreshRate = currentRefreshRate();
    res->dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
    res->iScreen       = 0;
    res->bFullScreen   = true;
    res->iWidth = m_width;
    res->iHeight = m_height;
    res->fPixelRatio   = 1.0f;
    res->iScreenWidth  = res->iWidth;
    res->iScreenHeight = res->iHeight;
  }
  res->iSubtitles    = (int)(0.965 * res->iHeight);
  res->strMode       = StringUtils::Format("%dx%d @ %.6f%s - Full Screen", res->iScreenWidth, res->iScreenHeight, res->fRefreshRate,
                                           res->dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
  CLog::Log(LOGNOTICE,"CWinSystemAndroidEGL: Current resolution: %dx%d %s\n", res->iWidth, res->iHeight, res->strMode.c_str());
  return true;
}

bool CWinSystemAndroidEGL::SetNativeResolution(const RESOLUTION_INFO &res)
{
  CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL: SetNativeResolution: %s: %dx%d %dx%d@%f", res.strId.c_str(), res.iWidth, res.iHeight, res.iScreenWidth, res.iScreenHeight, res.fRefreshRate);

  if (s_hasModeApi && !(res.iScreenWidth == s_res_cur_displayMode.iScreenWidth && res.iScreenHeight == s_res_cur_displayMode.iScreenHeight && res.fRefreshRate == s_res_cur_displayMode.fRefreshRate))
  {
    int modeid = -1;
    fetchDisplayModes();
    for (auto moderes : s_res_displayModes)
    {
      if (res.iScreenWidth == moderes.iScreenWidth && res.iScreenHeight == moderes.iScreenHeight && res.fRefreshRate == moderes.fRefreshRate)
      {
        modeid = atoi(moderes.strId.c_str());
        break;
      }
    }

    if (modeid == -1)
    {
      CLog::Log(LOGERROR, "CWinSystemAndroidEGL : Cannot find resolution %s", res.strMode.c_str());
      return false;
    }
    CXBMCApp::get()->SetDisplayModeId(modeid, res.fRefreshRate);
    s_res_cur_displayMode = res;

  }
  else if (abs(currentRefreshRate() - res.fRefreshRate) > 0.0001)
    CXBMCApp::get()->SetRefreshRate(res.fRefreshRate);

  if (*CXBMCApp::GetNativeWindow(30000))
    CXBMCApp::SetBuffersGeometry(res.iWidth, res.iHeight, 0);

  return true;
}

bool CWinSystemAndroidEGL::ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions)
{
  RESOLUTION_INFO cur_res;
  bool ret = GetNativeResolution(&cur_res);

  CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL: ProbeResolutions: %dx%d", m_width, m_height);

  if (s_hasModeApi)
  {
    for(RESOLUTION_INFO res : s_res_displayModes)
    {
      if (m_width && m_height)
      {
        res.iWidth = std::min(res.iWidth, m_width);
        res.iHeight = std::min(res.iHeight, m_height);
      }
      resolutions.push_back(res);
    }
    return true;
  }

  if (ret && cur_res.iWidth > 1 && cur_res.iHeight > 1)
  {
    std::vector<float> refreshRates;
    CJNIWindow window = CXBMCApp::get()->getWindow();
    if (window)
    {
      CJNIView view = window.getDecorView();
      if (view)
      {
        CJNIDisplay display = view.getDisplay();
        if (display)
        {
          refreshRates = display.getSupportedRefreshRates();
        }
      }

      if (!refreshRates.empty())
      {
        for (unsigned int i = 0; i < refreshRates.size(); i++)
        {
          if (refreshRates[i] < 20.0 || refreshRates[i] > 70.0)
            continue;
          cur_res.fRefreshRate = refreshRates[i];
          cur_res.strMode      = StringUtils::Format("%dx%d @ %.6f%s - Full Screen", cur_res.iScreenWidth, cur_res.iScreenHeight, cur_res.fRefreshRate,
                                                 cur_res.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
          CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL: found refresh rate: %f", cur_res.fRefreshRate);
          resolutions.push_back(cur_res);
        }
      }
    }
    if (resolutions.empty())
    {
      /* No valid refresh rates available, just provide the current one */
      CLog::Log(LOGINFO, "CWinSystemAndroidEGL: no refresh rate found: using current");
      resolutions.push_back(cur_res);
    }
    return true;
  }
  return false;
}

bool CWinSystemAndroidEGL::CreateNewWindow(const std::string& name, bool fullScreen, RESOLUTION_INFO& res, PHANDLE_EVENT_FUNC userFunction)
{
  RESOLUTION_INFO current_resolution;
  current_resolution.iWidth = current_resolution.iHeight = 0;

  m_nWidth        = res.iWidth;
  m_nHeight       = res.iHeight;
  m_displayWidth  = res.iScreenWidth;
  m_displayHeight = res.iScreenHeight;
  m_fRefreshRate  = res.fRefreshRate;

  if ((m_bWindowCreated && GetNativeResolution(&current_resolution)) &&
    current_resolution.iWidth == res.iWidth && current_resolution.iHeight == res.iHeight &&
    current_resolution.iScreenWidth == res.iScreenWidth && current_resolution.iScreenHeight == res.iScreenHeight &&
    m_bFullScreen == fullScreen && current_resolution.fRefreshRate == res.fRefreshRate &&
    (current_resolution.dwFlags & D3DPRESENTFLAG_MODEMASK) == (res.dwFlags & D3DPRESENTFLAG_MODEMASK))
  {
    CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL::CreateNewWindow: No need to create a new window");
    return true;
  }

  OnLostDevice();

  m_bFullScreen = fullScreen;
  // Destroy any existing window
  if (m_surface != EGL_NO_SURFACE)
    DestroyWindow();

  // If we previously destroyed an existing window we need to create a new one
  // (otherwise this is taken care of by InitWindowSystem())
  if (!CreateWindow(res))
  {
    CLog::Log(LOGERROR, "%s: Could not create new window",__FUNCTION__);
    return false;
  }

  OnResetDevice();

  Show();

  return true;
}

bool CWinSystemAndroidEGL::DestroyWindow()
{
  EGLBoolean status;
  if (m_display != EGL_NO_DISPLAY)
  {
    status = eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    CheckError();
  }

  if (m_surface != EGL_NO_SURFACE)
  {
    status = eglDestroySurface(m_display, m_surface);
    CheckError();
  }

  m_surface = EGL_NO_SURFACE;
  m_bWindowCreated = false;
  return true;
}

bool CWinSystemAndroidEGL::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
  CRenderSystemGLES::ResetRenderSystem(newWidth, newHeight, true, 0);
  int vsync_mode = CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOSCREEN_VSYNC);
  if (vsync_mode != VSYNC_DRIVER)
    SetVSyncImpl(m_iVSyncMode);
  return true;
}

bool CWinSystemAndroidEGL::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{
  CreateNewWindow("", fullScreen, res, NULL);
  CRenderSystemGLES::ResetRenderSystem(res.iWidth, res.iHeight, fullScreen, res.fRefreshRate);
  int vsync_mode = CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOSCREEN_VSYNC);
  if (vsync_mode != VSYNC_DRIVER)
    SetVSyncImpl(m_iVSyncMode);
  return true;
}

void CWinSystemAndroidEGL::UpdateResolutions()
{
  CWinSystemBase::UpdateResolutions();

  RESOLUTION_INFO resDesktop, curDisplay;
  std::vector<RESOLUTION_INFO> resolutions;

  if (!ProbeResolutions(resolutions) || resolutions.empty())
  {
    CLog::Log(LOGERROR, "%s: Fatal Error, ProbeResolutions failed",__FUNCTION__);
    return;
  }

  /* ProbeResolutions includes already all resolutions.
   * Only get desktop resolution so we can replace xbmc's desktop res
   */
  if (GetNativeResolution(&curDisplay))
    resDesktop = curDisplay;


  RESOLUTION ResDesktop = RES_INVALID;
  RESOLUTION res_index  = RES_DESKTOP;

  for (size_t i = 0; i < resolutions.size(); i++)
  {
    // if this is a new setting,
    // create a new empty setting to fill in.
    if ((int)CDisplaySettings::GetInstance().ResolutionInfoSize() <= res_index)
    {
      RESOLUTION_INFO res;
      CDisplaySettings::GetInstance().AddResolutionInfo(res);
    }

    g_graphicsContext.ResetOverscan(resolutions[i]);
    CDisplaySettings::GetInstance().GetResolutionInfo(res_index) = resolutions[i];

    CLog::Log(LOGNOTICE, "Found resolution %d x %d for display %d with %d x %d%s @ %f Hz\n",
      resolutions[i].iWidth,
      resolutions[i].iHeight,
      resolutions[i].iScreen,
      resolutions[i].iScreenWidth,
      resolutions[i].iScreenHeight,
      resolutions[i].dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "",
      resolutions[i].fRefreshRate);

    if(resDesktop.iWidth == resolutions[i].iWidth &&
       resDesktop.iHeight == resolutions[i].iHeight &&
       resDesktop.iScreenWidth == resolutions[i].iScreenWidth &&
       resDesktop.iScreenHeight == resolutions[i].iScreenHeight &&
       (resDesktop.dwFlags & D3DPRESENTFLAG_MODEMASK) == (resolutions[i].dwFlags & D3DPRESENTFLAG_MODEMASK) &&
       fabs(resDesktop.fRefreshRate - resolutions[i].fRefreshRate) < FLT_EPSILON)
    {
      ResDesktop = res_index;
    }

    res_index = (RESOLUTION)((int)res_index + 1);
  }

  // swap desktop index for desktop res if available
  if (ResDesktop != RES_INVALID)
  {
    CLog::Log(LOGNOTICE, "Found (%dx%d%s@%f) at %d, setting to RES_DESKTOP at %d",
      resDesktop.iWidth, resDesktop.iHeight,
      resDesktop.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "",
      resDesktop.fRefreshRate,
      (int)ResDesktop, (int)RES_DESKTOP);

    RESOLUTION_INFO desktop = CDisplaySettings::GetInstance().GetResolutionInfo(RES_DESKTOP);
    CDisplaySettings::GetInstance().GetResolutionInfo(RES_DESKTOP) = CDisplaySettings::GetInstance().GetResolutionInfo(ResDesktop);
    CDisplaySettings::GetInstance().GetResolutionInfo(ResDesktop) = desktop;
  }
}

bool CWinSystemAndroidEGL::IsExtSupported(const char* extension)
{
  std::string name;

  name  = " ";
  name += extension;
  name += " ";

  return (m_extensions.find(name) != std::string::npos || CRenderSystemGLES::IsExtSupported(extension));
}

bool CWinSystemAndroidEGL::PresentRenderImpl(const CDirtyRegionList &dirty)
{
  if ((m_display == EGL_NO_DISPLAY) || (m_surface == EGL_NO_SURFACE))
    return false;
  eglSwapBuffers(m_display, m_surface);

  return true;
}

void CWinSystemAndroidEGL::SetVSyncImpl(bool enable)
{
  m_iVSyncMode = enable ? 10:0;
  EGLBoolean status;
  // depending how buffers are setup, eglSwapInterval
  // might fail so let caller decide if this is an error.
  status = eglSwapInterval(m_display, enable ? 1 : 0);
  CheckError();

  if (!status)
  {
    m_iVSyncMode = 0;
    CLog::Log(LOGERROR, "%s,Could not set egl vsync", __FUNCTION__);
  }
}

void CWinSystemAndroidEGL::ShowOSMouse(bool show)
{
}

bool CWinSystemAndroidEGL::HasCursor()
{
  return false;
}

void CWinSystemAndroidEGL::NotifyAppActiveChange(bool bActivated)
{
}

bool CWinSystemAndroidEGL::Minimize()
{
  CXBMCApp::get()->Minimize();
  return true;
}

bool CWinSystemAndroidEGL::Restore()
{
  Show(true);
  return false;
}

bool CWinSystemAndroidEGL::Hide()
{
  return false;
}

bool CWinSystemAndroidEGL::Show(bool raise)
{
  return false;
}

bool CWinSystemAndroidEGL::BringToFront()
{
  CXBMCApp::get()->BringToFront();
  return true;
}

void CWinSystemAndroidEGL::Register(IDispResource *resource)
{
  CSingleLock lock(m_resourceSection);
  m_resources.push_back(resource);
}

void CWinSystemAndroidEGL::Unregister(IDispResource* resource)
{
  CSingleLock lock(m_resourceSection);
  std::vector<IDispResource*>::iterator i = find(m_resources.begin(), m_resources.end(), resource);
  if (i != m_resources.end())
    m_resources.erase(i);
}

void CWinSystemAndroidEGL::OnLostDevice()
{
  CSingleLock lock(m_resourceSection);
  if (!m_resourceLost)
  {
    CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL::OnLostDevice");
    m_resourceLost = true;
    for (std::vector<IDispResource *>::iterator i = m_resources.begin(); i != m_resources.end(); ++i)
      (*i)->OnLostDevice();
  }
}

void CWinSystemAndroidEGL::OnResetDevice()
{
  CSingleLock lock(m_resourceSection);
  if (m_resourceLost)
  {
    m_resourceLost = false;
    CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL::OnResetDevice");
    for (std::vector<IDispResource *>::iterator i = m_resources.begin(); i != m_resources.end(); ++i)
      (*i)->OnResetDevice();
  }
}

void CWinSystemAndroidEGL::OnAppFocusChange(bool focus)
{
  CLog::Log(LOGDEBUG, "CWinSystemAndroidEGL::OnAppFocusChange");
  CSingleLock lock(m_resourceSection);
  for (std::vector<IDispResource *>::iterator i = m_resources.begin(); i != m_resources.end(); i++)
    (*i)->OnAppFocusChange(focus);
}

EGLDisplay CWinSystemAndroidEGL::GetEGLDisplay()
{
  return m_display;
}

EGLContext CWinSystemAndroidEGL::GetEGLContext()
{
  return m_context;
}

EGLConfig CWinSystemAndroidEGL::GetEGLConfig()
{
  return m_config;
}

// the logic in this function should match whether CBaseRenderer::FindClosestResolution picks a 3D mode
bool CWinSystemAndroidEGL::Support3D(int width, int height, uint32_t mode) const
{
  RESOLUTION_INFO &curr = CDisplaySettings::GetInstance().GetResolutionInfo(g_graphicsContext.GetVideoResolution());

  // if we are using automatic hdmi mode switching
  if (CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOPLAYER_ADJUSTREFRESHRATE) != ADJUST_REFRESHRATE_OFF)
  {
    int searchWidth = curr.iScreenWidth;
    int searchHeight = curr.iScreenHeight;

    // only search the custom resolutions
    for (unsigned int i = (int)RES_DESKTOP; i < CDisplaySettings::GetInstance().ResolutionInfoSize(); i++)
    {
      RESOLUTION_INFO res = CDisplaySettings::GetInstance().GetResolutionInfo(i);
      if(res.iScreenWidth == searchWidth && res.iScreenHeight == searchHeight && (res.dwFlags & mode))
        return true;
    }
  }
  // otherwise just consider current mode
  else
  {
     if (curr.dwFlags & mode)
       return true;
  }

  return false;
}

bool CWinSystemAndroidEGL::ClampToGUIDisplayLimits(int &width, int &height)
{
  width = width > m_nWidth ? m_nWidth : width;
  height = height > m_nHeight ? m_nHeight : height;
  return true;
}

#endif

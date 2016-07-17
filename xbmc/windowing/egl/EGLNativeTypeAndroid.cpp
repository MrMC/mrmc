/*
 *      Copyright (C) 2011-2013 Team XBMC
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
#include <stdlib.h>

#include "system.h"

#include <EGL/egl.h>
#include "EGLNativeTypeAndroid.h"

#include "guilib/gui3d.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/SysfsUtils.h"

#include "platform/android/activity/XBMCApp.h"
#include "platform/android/activity/AndroidFeatures.h"
#include "platform/android/jni/SystemProperties.h"
#include "platform/android/jni/View.h"
#include "platform/android/jni/Display.h"
#include "platform/android/jni/Window.h"
#include "platform/android/jni/WindowManager.h"
#include "platform/android/jni/Build.h"
#include "platform/android/jni/System.h"


CEGLNativeTypeAndroid::CEGLNativeTypeAndroid()
: m_width(0)
, m_height(0)
, m_hasDisplayMode(false)
{
}

CEGLNativeTypeAndroid::~CEGLNativeTypeAndroid()
{
}

bool CEGLNativeTypeAndroid::CheckCompatibility()
{
  return true;
}

void CEGLNativeTypeAndroid::Initialize()
{
  std::string displaySize;
  m_width = m_height = 0;

  CJNIWindow window = CXBMCApp::getWindow();
  if (window)
  {
    CJNIView view(window.getDecorView());
    if (view)
    {
      CJNIDisplay display = view.getDisplay();
      if (display)
      {
        m_hasDisplayMode = display.getMode() != NULL;
        CLog::Log(LOGDEBUG, "CEGLNativeTypeAndroid: hasDisplayMode = %d", m_hasDisplayMode);
        if (m_hasDisplayMode)
        {
          CJNIDisplayMode mode = display.getMode();
          m_width = mode.getPhysicalWidth();
          m_height = mode.getPhysicalHeight();
          CLog::Log(LOGDEBUG, "CEGLNativeTypeAndroid: current display mode: %dx%d@%f",
            m_width, m_height, mode.getRefreshRate());
        }
        else
        {
          m_width = display.getWidth();
          m_height = display.getHeight();
          CLog::Log(LOGDEBUG, "CEGLNativeTypeAndroid: current display: %dx%d@%f",
            m_width, m_height, display.getRefreshRate());
        }
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
      CLog::Log(LOGDEBUG, "CEGLNativeTypeAndroid: sys.display-size reports: %s(%dx%d)", displaySize.c_str(), m_width, m_height);
    }
  }

  CLog::Log(LOGDEBUG, "CEGLNativeTypeAndroid: maximum/current resolution: %dx%d", m_width, m_height);
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
  CLog::Log(LOGDEBUG, "CEGLNativeTypeAndroid: selected limitgui resolution: %dx%d", m_width, m_height);
}

void CEGLNativeTypeAndroid::Destroy()
{
  return;
}

bool CEGLNativeTypeAndroid::CreateNativeDisplay()
{
  m_nativeDisplay = EGL_DEFAULT_DISPLAY;
  return true;
}

bool CEGLNativeTypeAndroid::CreateNativeWindow()
{
  // Android hands us a window, we don't have to create it
  return true;
}

bool CEGLNativeTypeAndroid::GetNativeDisplay(XBNativeDisplayType **nativeDisplay) const
{
  if (!nativeDisplay)
    return false;
  *nativeDisplay = (XBNativeDisplayType*) &m_nativeDisplay;
  return true;
}

bool CEGLNativeTypeAndroid::GetNativeWindow(XBNativeWindowType **nativeWindow) const
{
  if (!nativeWindow)
    return false;
  *nativeWindow = (XBNativeWindowType*) CXBMCApp::GetNativeWindow(2000);
  return (*nativeWindow != NULL && **nativeWindow != NULL);
}

bool CEGLNativeTypeAndroid::DestroyNativeDisplay()
{
  return true;
}

bool CEGLNativeTypeAndroid::DestroyNativeWindow()
{
  return true;
}

static float CurrentRefreshRate()
{
  CJNIWindow window = CXBMCApp::getWindow();
  if (window)
  {
    float preferredRate = window.getAttributes().getpreferredRefreshRate();
    if (preferredRate > 20.0 && preferredRate < 70.0)
    {
      CLog::Log(LOGINFO, "CEGLNativeTypeAndroid:CurrentRefreshRate Preferred refresh rate: %f", preferredRate);
      return preferredRate;
    }
    CJNIView view(window.getDecorView());
    if (view) {
      CJNIDisplay display(view.getDisplay());
      if (display)
      {
        float reportedRate;
        if (display.getMode() != NULL)
          reportedRate = display.getMode().getRefreshRate();
        else
          reportedRate = display.getRefreshRate();

        if (reportedRate > 20.0 && reportedRate < 70.0)
        {
          CLog::Log(LOGINFO, "CEGLNativeTypeAndroid:CurrentRefreshRate Current display refresh rate: %f", reportedRate);
          return reportedRate;
        }
      }
    }
  }

  CLog::Log(LOGDEBUG, "found no refresh rate");
  return 60.0;
}

bool CEGLNativeTypeAndroid::GetNativeResolution(RESOLUTION_INFO *res) const
{
  EGLNativeWindowType *nativeWindow = (EGLNativeWindowType*)CXBMCApp::GetNativeWindow(30000);
  if (!nativeWindow)
    return false;

  if (!m_width || !m_height)
  {
    ANativeWindow_acquire(*nativeWindow);
    res->iWidth = ANativeWindow_getWidth(*nativeWindow);
    res->iHeight= ANativeWindow_getHeight(*nativeWindow);
    ANativeWindow_release(*nativeWindow);
  }
  else
  {
    res->iWidth = m_width;
    res->iHeight = m_height;
  }

  res->fRefreshRate = CurrentRefreshRate();
  res->dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
  res->iScreen       = 0;
  res->bFullScreen   = true;
  res->iSubtitles    = (int)(0.965 * res->iHeight);
  res->fPixelRatio   = 1.0f;
  res->iScreenWidth  = res->iWidth;
  res->iScreenHeight = res->iHeight;
  res->strMode       = StringUtils::Format("%dx%d @ %.2f%s - Full Screen", res->iScreenWidth, res->iScreenHeight, res->fRefreshRate,
                                           res->dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
  CLog::Log(LOGNOTICE,"CEGLNativeTypeAndroid: GetNativeResolution: %s\n",res->strMode.c_str());
  return true;
}

bool CEGLNativeTypeAndroid::SetNativeResolution(const RESOLUTION_INFO &res)
{
  CJNIWindow window = CXBMCApp::getWindow();
  if (window)
  {
    CJNIView view(window.getDecorView());
    if (view)
    {
      CJNIDisplay display = view.getDisplay();
      if (display.getMode() != NULL)
      {
        int curModeId = display.getMode().getModeId();
        std::vector<CJNIDisplayMode> modes = display.getSupportedModes();
        for (auto m : modes)
        {
          int width = m.getPhysicalWidth();
          int height = m.getPhysicalHeight();
          float rate = m.getRefreshRate();
          int modeId = m.getModeId();
          if (width  == res.iWidth &&
              height == res.iHeight &&
              rate   == res.fRefreshRate)
          {
            m_width = res.iWidth;
            m_height = res.iHeight;
            if (modeId != curModeId)
              CXBMCApp::SetDisplayModeId(modeId);
            CLog::Log(LOGDEBUG, "CEGLNativeTypeAndroid: SetNativeResolution: %dx%d@%f %i", res.iWidth, res.iHeight, res.fRefreshRate, modeId);
            break;
          }
        }
      }
      else
      {
        m_width = display.getWidth();
        m_height = display.getHeight();
      }
      CXBMCApp::SetBuffersGeometry(m_width, m_height, 0);
    }
  }

  return true;
}

bool CEGLNativeTypeAndroid::ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions)
{
  resolutions.clear();

  CJNIWindow window = CXBMCApp::getWindow();
  if (window)
  {
    CJNIView view(window.getDecorView());
    if (view)
    {
      CJNIDisplay display = view.getDisplay();
      if (display)
      {
        if (display.getMode() != NULL)
        {
          std::vector<CJNIDisplayMode> modes = display.getSupportedModes();
          for (auto m : modes)
          {
            RESOLUTION_INFO found_res(m.getPhysicalWidth(), m.getPhysicalHeight());
            found_res.iScreenWidth = found_res.iWidth;
            found_res.iScreenHeight = found_res.iHeight;
            found_res.iSubtitles = (int)(0.965 * found_res.iHeight);
            found_res.fRefreshRate = m.getRefreshRate();
            found_res.strMode = StringUtils::Format("%dx%d @ %.2f%s - Full Screen",
              found_res.iScreenWidth, found_res.iScreenHeight, found_res.fRefreshRate,
              found_res.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
            CLog::Log(LOGDEBUG, "CEGLNativeTypeAndroid:ProbeResolutions: %s", found_res.strMode.c_str());
            resolutions.push_back(found_res);
          }
        }
      }
    }
  }

  if (resolutions.empty())
  {
    RESOLUTION_INFO res;
    GetNativeResolution(&res);
    resolutions.push_back(res);
  }

  return true;
}

bool CEGLNativeTypeAndroid::GetPreferredResolution(RESOLUTION_INFO *res) const
{
  return false;
}

bool CEGLNativeTypeAndroid::ShowWindow(bool show)
{
  return false;
}

bool CEGLNativeTypeAndroid::BringToFront()
{
  CXBMCApp::BringToFront();
  return true;
}

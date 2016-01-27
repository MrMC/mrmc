/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#include "platform/MCRuntimeLibStartupLogger.h"

#include "Application.h"
#include "settings/AdvancedSettings.h"
#include "utils/log.h"

#ifdef TARGET_RASPBERRY_PI
#include "linux/RBP.h"
#endif


extern "C" void MCRuntimeLib_Preflight()
{
}

extern "C" void MCRuntimeLib_Postflight()
{
}

extern "C" void MCRuntimeLib_SetRenderGUI(bool renderGUI)
{
  g_application.SetRenderGUI(renderGUI);
}

extern "C" bool MCRuntimeLib_Initialized()
{
  return g_application.IsAppInitialized();
}

extern "C" bool MCRuntimeLib_Running()
{
  return !g_application.m_bStop;
}

extern "C" int MCRuntimeLib_Run(bool renderGUI)
{
  int status = -1;

  //this can't be set from CAdvancedSettings::Initialize()
  //because it will overwrite the loglevel set with the --debug flag
#ifdef _DEBUG
  g_advancedSettings.m_logLevel     = LOG_LEVEL_DEBUG;
  g_advancedSettings.m_logLevelHint = LOG_LEVEL_DEBUG;
#else
  g_advancedSettings.m_logLevel     = LOG_LEVEL_NORMAL;
  g_advancedSettings.m_logLevelHint = LOG_LEVEL_NORMAL;
#endif
  CLog::SetLogLevel(g_advancedSettings.m_logLevel);

  // not a failure if returns false, just means someone
  // did the init before us.
  if (!g_advancedSettings.Initialized())
    g_advancedSettings.Initialize();

  if (!g_application.Create())
  {
    CMCRuntimeLibStartupLogger::DisplayError("ERROR: Unable to create application. Exiting");
    return status;
  }

#ifdef TARGET_RASPBERRY_PI
  if(!g_RBP.Initialize())
    return false;
  g_RBP.LogFirmwareVerison();
#endif

  if (renderGUI && !g_application.CreateGUI())
  {
    CMCRuntimeLibStartupLogger::DisplayError("ERROR: Unable to create GUI. Exiting");
    return status;
  }
  if (!g_application.Initialize())
  {
    CMCRuntimeLibStartupLogger::DisplayError("ERROR: Unable to Initialize. Exiting");
    return status;
  }

  try
  {
    status = g_application.Run();
  }
  catch(...)
  {
    CMCRuntimeLibStartupLogger::DisplayError("ERROR: Exception caught on main loop. Exiting");
    status = -1;
  }

#ifdef TARGET_RASPBERRY_PI
  g_RBP.Deinitialize();
#endif

  return status;
}

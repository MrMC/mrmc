/*
*      Copyright (C) 2005-2015 Team XBMC
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
#include "CompileInfo.h"

#ifdef TARGET_ANDROID
#include "platform/android/service/XBMCService.h"
#endif

#include <stdio.h>

void CMCRuntimeLibStartupLogger::DisplayMessage(const std::string& message)
{
#ifdef TARGET_ANDROID
  CXBMCService::android_printf("info: %s\n", message.c_str());
#else
  fprintf(stdout, "%s\n", message.c_str());
#endif
}

void CMCRuntimeLibStartupLogger::DisplayWarning(const std::string& warning)
{
#ifdef TARGET_ANDROID
  CXBMCService::android_printf("warning: %s\n", warning.c_str());
#else
  fprintf(stderr, "%s\n", warning.c_str());
#endif
}

void CMCRuntimeLibStartupLogger::DisplayError(const std::string& error)
{
#ifdef TARGET_ANDROID
  CXBMCService::android_printf("ERROR: %s\n", error.c_str());
#else
  fprintf(stderr,"%s\n", error.c_str());
#endif
}

void CMCRuntimeLibStartupLogger::DisplayHelpMessage(const std::vector<std::pair<std::string, std::string>>& help)
{
  //very crude implementation, pretty it up when possible
  std::string message;
  for (const auto& line : help)
  {
    message.append(line.first + "\t" + line.second + "\n");
  }

  fprintf(stdout, "%s\n", message.c_str());
}

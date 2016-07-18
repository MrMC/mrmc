/*
 *      Copyright (C) 2010-2013 Team XBMC
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
#ifndef _DARWIN_UTILS_H_
#define _DARWIN_UTILS_H_

#include <string>

// We forward declare CFStringRef in order to avoid
// pulling in tons of Objective-C headers.
struct __CFString;
typedef const struct __CFString * CFStringRef;

class CDarwinUtils
{
public:
  static const char *getIosPlatformString(void);
  static bool        IsMavericks(void);
  static bool        IsLion(void);
  static bool        IsSnowLeopard(void);
  static bool        DeviceHasRetina(double &scale);
  static bool        DeviceHasLeakyVDA(void);
  static bool        DeviceHas10BitH264(void);
  static const char *GetOSReleaseString(void);
  static const char *GetOSVersionString(void);
  static float       GetIOSVersion(void);
  static const char *GetIOSVersionString(void);
  static const char *GetOSXVersionString(void);
  static int         GetFrameworkPath(char* path, size_t *pathsize);
  static int         GetExecutablePath(char* path, size_t *pathsize);
  static const char *GetUserLogDirectory(void);
  static const char *GetUserTempDirectory(void);
  static const char *GetUserHomeDirectory(void);
  static const char *GetOSAppRootFolder(void);
  static const char *GetOSCachesDirectory();
  static const char *GetOSTemporaryDirectory();
  static int         BatteryLevel(void);
  static void        EnableOSScreenSaver(bool enable);
  static bool        ResetSystemIdleTimer();
  static void        SetScheduling(int message);
  static void        PrintDebugString(std::string debugString);
  static bool        CFStringRefToString(CFStringRef source, std::string& destination);
  static bool        CFStringRefToUTF8String(CFStringRef source, std::string& destination);
  static const std::string&  GetManufacturer(void);
  static bool        IsAliasShortcut(const std::string& path, bool isdirectory);
  static void        TranslateAliasShortcut(std::string& path);
  static bool        CreateAliasShortcut(const std::string& fromPath, const std::string& toPath);
  static bool        OpenAppWithOpenURL(const std::string& path);
  static std::string GetAudioRoute();
  static void        DumpAudioDescriptions(const std::string& why);

};

#endif

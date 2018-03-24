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

#include "system.h"
#include "Application.h"
#include "DllPaths.h"
#include "URL.h"
#include "GUIUserMessages.h"
#include "filesystem/File.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "utils/StringUtils.h"

#include "CompileInfo.h"

#if defined(TARGET_DARWIN)

#if defined(TARGET_DARWIN_IOS)
  #include "utils/StringHasher.h"
  #import <Foundation/Foundation.h>
  #import <AVFoundation/AVFoundation.h>
  #import <UIKit/UIKit.h>
  #import <mach/mach_host.h>
  #import <sys/sysctl.h>
  #if defined(TARGET_DARWIN_TVOS)
    #import "platform/darwin/tvos/MainController.h"
  #endif
#else
  #import <Cocoa/Cocoa.h>
  #import <CoreFoundation/CoreFoundation.h>
  #import <IOKit/IOKitLib.h>
  #import <IOKit/ps/IOPowerSources.h>
  #import <IOKit/ps/IOPSKeys.h>
#endif

#import "DarwinUtils.h"

#ifndef NSAppKitVersionNumber10_5
#define NSAppKitVersionNumber10_5 949
#endif

#ifndef NSAppKitVersionNumber10_6
#define NSAppKitVersionNumber10_6 1038
#endif

#ifndef NSAppKitVersionNumber10_9
#define NSAppKitVersionNumber10_9 1265
#endif


enum iosPlatform
{
  iDeviceUnknown = -1,
  iPhone2G,
  iPhone3G,
  iPhone3GS,
  iPodTouch1G,
  iPodTouch2G,
  iPodTouch3G,
  iPad,
  iPad3G,
  iPad2WIFI,
  iPad2CDMA,
  iPad2,
  iPadMini,
  iPadMiniGSMCDMA,
  iPadMiniWIFI,
  AppleTV2,
  AppleTV4,
  AppleTV4K,
  iPhone4,            //from here on list devices with retina support (e.x. mainscreen scale == 2.0)
  iPhone4CDMA,
  iPhone4S,
  iPhone5,
  iPhone5GSMCDMA, 
  iPhone5CGSM,
  iPhone5CGlobal,
  iPhone5SGSM,
  iPhone5SGlobal,
  iPodTouch4G,
  iPodTouch5G,
  iPodTouch6G,
  iPad3WIFI,
  iPad3GSMCDMA,
  iPad3,
  iPad4WIFI,
  iPad4,
  iPad4GSMCDMA,
  iPad5Wifi,
  iPad5Cellular,
  iPadAirWifi,
  iPadAirCellular,
  iPadAirTDLTE,
  iPadMini2Wifi,
  iPadMini2Cellular,
  iPhone6,
  iPhone6s,
  iPhoneSE,
  iPhone7,
  iPhone8,
  iPadAir2Wifi,
  iPadAir2Cellular,
  iPadPro9_7InchWifi,
  iPadPro9_7InchCellular,
  iPadPro12_9InchWifi,
  iPadPro12_9InchCellular,
  iPadPro2_12_9InchWifi,
  iPadPro2_12_9InchCellular,
  iPadPro_10_5InchWifi,
  iPadPro_10_5InchCellular,
  iPadMini3Wifi,
  iPadMini3Cellular,
  iPadMini4Wifi,
  iPadMini4Cellular,
  iPhone6Plus,        //from here on list devices with retina support which have scale == 3.0
  iPhone6sPlus,
  iPhone7Plus,
  iPhone8Plus,
  iPhoneX,
};

// platform strings are based on http://theiphonewiki.com/wiki/Models
const char* CDarwinUtils::getIosPlatformString(void)
{
  static std::string iOSPlatformString;
  if (iOSPlatformString.empty())
  {
#if defined(TARGET_DARWIN_IOS)
    // Gets a string with the device model
    size_t size;  
    sysctlbyname("hw.machine", NULL, &size, NULL, 0);  
    char *machine = new char[size];  
    if (sysctlbyname("hw.machine", machine, &size, NULL, 0) == 0 && machine[0])
      iOSPlatformString.assign(machine, size -1);
   else
#endif
      iOSPlatformString = "unknown0,0";

#if defined(TARGET_DARWIN_IOS)
    delete [] machine;
#endif
  }

  return iOSPlatformString.c_str();
}

enum iosPlatform getIosPlatform()
{
  static enum iosPlatform eDev = iDeviceUnknown;
#if defined(TARGET_DARWIN_IOS)
  if (eDev == iDeviceUnknown)
  {
    using namespace StringHasher;
    switch(mkhash(CDarwinUtils::getIosPlatformString()))
    {
      case "iPhone1,1"_mkhash: eDev = iPhone2G; break;
      case "iPhone1,2"_mkhash: eDev = iPhone3G; break;
      case "iPhone2,1"_mkhash: eDev = iPhone3GS; break;
      case "iPhone3,1"_mkhash: eDev = iPhone4; break;
      case "iPhone3,2"_mkhash: eDev = iPhone4; break;
      case "iPhone3,3"_mkhash: eDev = iPhone4CDMA; break;
      case "iPhone4,1"_mkhash: eDev = iPhone4S; break;
      case "iPhone5,1"_mkhash: eDev = iPhone5; break;
      case "iPhone5,2"_mkhash: eDev = iPhone5GSMCDMA; break;
      case "iPhone5,3"_mkhash: eDev = iPhone5GSMCDMA; break;
      case "iPhone5,4"_mkhash: eDev = iPhone5CGSM; break;
      case "iPhone6,1"_mkhash: eDev = iPhone5CGlobal; break;
      case "iPhone6,2"_mkhash: eDev = iPhone5SGlobal; break;
      case "iPhone7,1"_mkhash: eDev = iPhone6Plus; break;
      case "iPhone7,2"_mkhash: eDev = iPhone6; break;
      case "iPhone8,1"_mkhash: eDev = iPhone6s; break;
      case "iPhone8,2"_mkhash: eDev = iPhone6sPlus; break;
      case "iPhone8,4"_mkhash: eDev = iPhoneSE; break;
      case "iPhone9,1"_mkhash: eDev = iPhone7; break;
      case "iPhone9,2"_mkhash: eDev = iPhone7Plus; break;
      case "iPhone9,3"_mkhash: eDev = iPhone6Plus; break;
      case "iPhone9,4"_mkhash: eDev = iPhone7Plus; break;
      case "iPhone10,1"_mkhash: eDev = iPhone8; break;
      case "iPhone10,2"_mkhash: eDev = iPhone8Plus; break;
      case "iPhone10,3"_mkhash: eDev = iPhoneX; break;
      case "iPhone10,4"_mkhash: eDev = iPhone8; break;
      case "iPhone10,5"_mkhash: eDev = iPhone8Plus; break;
      case "iPhone10,6"_mkhash: eDev = iPhoneX; break;
      case "iPod1,1"_mkhash: eDev = iPodTouch1G; break;
      case "iPod2,1"_mkhash: eDev = iPodTouch2G; break;
      case "iPod3,1"_mkhash: eDev = iPodTouch3G; break;
      case "iPod4,1"_mkhash: eDev = iPodTouch4G; break;
      case "iPod5,1"_mkhash: eDev = iPodTouch5G; break;
      case "iPod7,1"_mkhash: eDev = iPodTouch6G; break;
      case "iPad1,1"_mkhash: eDev = iPad; break;
      case "iPad1,2"_mkhash: eDev = iPad; break;
      case "iPad2,1"_mkhash: eDev = iPad2WIFI; break;
      case "iPad2,2"_mkhash: eDev = iPad2; break;
      case "iPad2,3"_mkhash: eDev = iPad2CDMA; break;
      case "iPad2,4"_mkhash: eDev = iPad2; break;
      case "iPad2,5"_mkhash: eDev = iPadMiniWIFI; break;
      case "iPad2,6"_mkhash: eDev = iPadMini; break;
      case "iPad2,7"_mkhash: eDev = iPadMiniGSMCDMA; break;
      case "iPad3,1"_mkhash: eDev = iPad3WIFI; break;
      case "iPad3,2"_mkhash: eDev = iPad3GSMCDMA; break;
      case "iPad3,3"_mkhash: eDev = iPad3; break;
      case "iPad3,4"_mkhash: eDev = iPad4WIFI; break;
      case "iPad3,5"_mkhash: eDev = iPad4; break;
      case "iPad3,6"_mkhash: eDev = iPad4GSMCDMA; break;
      case "iPad4,1"_mkhash: eDev = iPadAirWifi; break;
      case "iPad4,2"_mkhash: eDev = iPadAirCellular; break;
      case "iPad4,3"_mkhash: eDev = iPadAirTDLTE; break;
      case "iPad4,4"_mkhash: eDev = iPadMini2Wifi; break;
      case "iPad4,5"_mkhash: eDev = iPadMini2Cellular; break;
      case "iPad4,6"_mkhash: eDev = iPadMini2Cellular; break;
      case "iPad4,7"_mkhash: eDev = iPadMini3Wifi; break;
      case "iPad4,8"_mkhash: eDev = iPadMini3Cellular; break;
      case "iPad4,9"_mkhash: eDev = iPadMini3Cellular; break;
      case "iPad5,1"_mkhash: eDev = iPadMini4Wifi; break;
      case "iPad5,2"_mkhash: eDev = iPadMini4Cellular; break;
      case "iPad5,3"_mkhash: eDev = iPadAir2Wifi; break;
      case "iPad5,4"_mkhash: eDev = iPadAir2Cellular; break;
      case "iPad6,3"_mkhash: eDev = iPadPro9_7InchWifi; break;
      case "iPad6,4"_mkhash: eDev = iPadPro9_7InchCellular; break;
      case "iPad6,7"_mkhash: eDev = iPadPro12_9InchWifi; break;
      case "iPad6,8"_mkhash: eDev = iPadPro12_9InchCellular; break;
      case "iPad6,11"_mkhash: eDev = iPad5Wifi; break;
      case "iPad6,12"_mkhash: eDev = iPad5Cellular; break;
      case "iPad7,1"_mkhash: eDev = iPadPro2_12_9InchWifi; break;
      case "iPad7,2"_mkhash: eDev = iPadPro2_12_9InchCellular; break;
      case "iPad7,3"_mkhash: eDev = iPadPro_10_5InchWifi; break;
      case "iPad7,4"_mkhash: eDev = iPadPro_10_5InchCellular; break;
      case "AppleTV2,1"_mkhash: eDev = AppleTV2; break;
      case "AppleTV5,3"_mkhash: eDev = AppleTV4; break;
      case "AppleTV6,2"_mkhash: eDev = AppleTV4K; break;
    }
  }
#endif
  return eDev;
}

bool CDarwinUtils::IsMavericks(void)
{
  static int isMavericks = -1;
#if defined(TARGET_DARWIN_OSX)
  // there is no NSAppKitVersionNumber10_9 out there anywhere
  // so we detect mavericks by one of these newly added app nap
  // methods - and fix the ugly mouse rect problem which was hitting
  // us when mavericks came out
  if (isMavericks == -1)
  {
    isMavericks = [NSProcessInfo instancesRespondToSelector:@selector(beginActivityWithOptions:reason:)] == TRUE ? 1 : 0;
  }
#endif
  return isMavericks == 1;
}

bool CDarwinUtils::IsLion(void)
{
  static int isLion = -1;
#if defined(TARGET_DARWIN_OSX)
  if (isLion == -1)
  {
    double appKitVersion = floor(NSAppKitVersionNumber);
    // everything lower 10.8 is 10.7.x because 10.7 is deployment target...
    isLion = (appKitVersion < NSAppKitVersionNumber10_8) ? 1 : 0;
  }
#endif
  return isLion == 1;
}

bool CDarwinUtils::IsSnowLeopard(void)
{
  static int isSnowLeopard = -1;
#if defined(TARGET_DARWIN_OSX)
  if (isSnowLeopard == -1)
  {
    double appKitVersion = floor(NSAppKitVersionNumber);
    isSnowLeopard = (appKitVersion <= NSAppKitVersionNumber10_6 && appKitVersion > NSAppKitVersionNumber10_5) ? 1 : 0;
  }
#endif
  return isSnowLeopard == 1;
}

bool CDarwinUtils::IsAppleTV4KOrAbove(void)
{
  static int isAppleTV4KOrAbove = -1;
  if (isAppleTV4KOrAbove == -1)
  {
    isAppleTV4KOrAbove = 0;
#if defined(TARGET_DARWIN_TVOS)
    if (std::string(CDarwinUtils::getIosPlatformString()) != "AppleTV5,3")
      isAppleTV4KOrAbove = 1;
#endif
  }
  return isAppleTV4KOrAbove == 1;
}

bool CDarwinUtils::HasDisplayRateSwitching(void)
{
  static int hasDisplayRateSwitching = -1;
  if (hasDisplayRateSwitching == -1)
  {
    hasDisplayRateSwitching = 0;
#if defined(TARGET_DARWIN_TVOS)
    std::string avDisplayCriteria = "AVDisplayCriteria";
    Class AVDisplayCriteriaClass = NSClassFromString([NSString stringWithUTF8String: avDisplayCriteria.c_str()]);
    if (AVDisplayCriteriaClass)
      hasDisplayRateSwitching = 1;
#elif defined(TARGET_DARWIN_OSX)
    hasDisplayRateSwitching = 1;
#endif
  }
  return hasDisplayRateSwitching == 1;
}

bool CDarwinUtils::DeviceHas10BitH264(void)
{
  static int has10BitH264 = -1;
#if defined(TARGET_DARWIN_IOS)
  if (has10BitH264 == -1)
  {
    cpu_type_t type;
    size_t size = sizeof(type);
    sysctlbyname("hw.cputype", &type, &size, NULL, 0);

    // 10bit H264 decoding was introduced with the 64bit ARM CPUs, the A7/A8
    if (type == CPU_TYPE_ARM64)
      has10BitH264 = 1;
    else
      has10BitH264 = 0;
  }
#endif
  return has10BitH264 == 1;
}

bool CDarwinUtils::DeviceHasRetina(double &scale)
{
  static enum iosPlatform platform = iDeviceUnknown;

#if defined(TARGET_DARWIN_IOS)
  if( platform == iDeviceUnknown )
  {
    platform = getIosPlatform();
  }
#endif
  scale = 1.0; //no retina

  // see http://www.paintcodeapp.com/news/iphone-6-screens-demystified
  if (platform >= iPhone4 && platform < iPhone6Plus)
  {
    scale = 2.0; // 2x render retina
  }

  if (platform >= iPhone6Plus)
  {
    scale = 3.0; //3x render retina + downscale
  }

  return (platform >= iPhone4);
}

bool CDarwinUtils::DeviceHasLeakyVDA(void)
{
#if defined(TARGET_DARWIN_OSX)
  static int hasLeakyVDA = -1;
  if (hasLeakyVDA == -1)
    hasLeakyVDA = NSAppKitVersionNumber <= NSAppKitVersionNumber10_9 ? 1 : 0;
  return hasLeakyVDA == 1;
#else
  return false;
#endif
}

const char *CDarwinUtils::GetOSReleaseString(void)
{
  static std::string osreleaseStr;
  if (osreleaseStr.empty())
  {
    size_t size;
    sysctlbyname("kern.osrelease", NULL, &size, NULL, 0);
    char *osrelease = new char[size];
    sysctlbyname("kern.osrelease", osrelease, &size, NULL, 0);
    osreleaseStr = osrelease;
    delete [] osrelease;
  }
  return osreleaseStr.c_str();
}

const char *CDarwinUtils::GetOSVersionString(void)
{
  return [[[NSProcessInfo processInfo] operatingSystemVersionString] UTF8String];
}

float CDarwinUtils::GetIOSVersion(void)
{
  float version;
#if defined(TARGET_DARWIN_IOS)
  version = [[[UIDevice currentDevice] systemVersion] floatValue];
#else
  version = 0.0f;
#endif

  return(version);
}

const char *CDarwinUtils::GetIOSVersionString(void)
{
#if defined(TARGET_DARWIN_IOS)
  static std::string iOSVersionString;
  if (iOSVersionString.empty())
  {
    iOSVersionString.assign((const char*)[[[UIDevice currentDevice] systemVersion] UTF8String]);
  }
  return iOSVersionString.c_str();
#else
  return "0.0";
#endif
}

const char *CDarwinUtils::GetOSXVersionString(void)
{
#if defined(TARGET_DARWIN_OSX)
  static std::string OSXVersionString;
  if (OSXVersionString.empty())
  {
    OSXVersionString.assign((const char*)[[[NSDictionary dictionaryWithContentsOfFile:
                         @"/System/Library/CoreServices/SystemVersion.plist"] objectForKey:@"ProductVersion"] UTF8String]);
  }
  
  return OSXVersionString.c_str();
#else
  return "0.0";
#endif
}

int  CDarwinUtils::GetFrameworkPath(bool forPython, char* path, size_t *pathsize)
{
  // see if we can figure out who we are
  NSString *pathname;

  path[0] = 0;
  *pathsize = 0;

  // 1) Kodi application running under IOS
  pathname = [[NSBundle mainBundle] executablePath];
  std::string appName = std::string(CCompileInfo::GetAppName()) + ".app/" + std::string(CCompileInfo::GetAppName());
  if (pathname && strstr([pathname UTF8String], appName.c_str()))
  {
    strcpy(path, [pathname UTF8String]);
    // Move backwards to last "/"
    for (size_t n=strlen(path)-1; path[n] != '/'; n--)
      path[n] = '\0';
    strcat(path, "Frameworks");
    *pathsize = strlen(path);
    //CLog::Log(LOGDEBUG, "DarwinFrameworkPath(c) -> %s", path);
    return 0;
  }

  // 2) Kodi application running under OSX
  pathname = [[NSBundle mainBundle] executablePath];
  if (pathname && strstr([pathname UTF8String], "Contents"))
  {
    strcpy(path, [pathname UTF8String]);
    // ExectuablePath is <product>.app/Contents/MacOS/<executable>
    char *lastSlash = strrchr(path, '/');
    if (lastSlash)
    {
      *lastSlash = '\0';//remove /<executable>  
      lastSlash = strrchr(path, '/');
      if (lastSlash)
        *lastSlash = '\0';//remove /MacOS
    }
    strcat(path, "/Libraries");//add /Libraries
    //we should have <product>.app/Contents/Libraries now
    *pathsize = strlen(path);
    //CLog::Log(LOGDEBUG, "DarwinFrameworkPath(d) -> %s", path);
    return 0;
  }

  // e) Kodi OSX binary running under xcode or command-line
  // but only if it's not for python. In this case, let python
  // use it's internal compiled paths.
  if (!forPython)
  {
    strcpy(path, PREFIX_USR_PATH);
    strcat(path, "/lib");
    *pathsize = strlen(path);
    //CLog::Log(LOGDEBUG, "DarwinFrameworkPath(e) -> %s", path);
    return 0;
  }

  return -1;
}

int  CDarwinUtils::GetExecutablePath(char* path, size_t *pathsize)
{
  // see if we can figure out who we are
  NSString *pathname;

  // 1) Kodi application running under IOS
  // 2) Kodi application running under OSX
  pathname = [[NSBundle mainBundle] executablePath];
  strcpy(path, [pathname UTF8String]);
  *pathsize = strlen(path);
  //CLog::Log(LOGDEBUG, "DarwinExecutablePath(b/c) -> %s", path);

  return 0;
}

const char* CDarwinUtils::GetUserLogDirectory(void)
{
  static std::string appLogFolder;
  if (appLogFolder.empty())
  {
    // log file location
    #if defined(TARGET_DARWIN_TVOS)
      appLogFolder = CDarwinUtils::GetOSCachesDirectory();
    #elif defined(TARGET_DARWIN_IOS)
      appLogFolder = URIUtils::AddFileToFolder(CDarwinUtils::GetOSAppRootFolder(), CCompileInfo::GetAppName());
    #else
      appLogFolder = URIUtils::AddFileToFolder(getenv("HOME"), "Library");
    #endif
    appLogFolder = URIUtils::AddFileToFolder(appLogFolder, "logs");
    // stupid log directory wants a ending slash
    URIUtils::AddSlashAtEnd(appLogFolder);
  }

  return appLogFolder.c_str();
}

const char* CDarwinUtils::GetUserTempDirectory(void)
{
  static std::string appTempFolder;
  if (appTempFolder.empty())
  {
    // location for temp files
    #if defined(TARGET_DARWIN_TVOS)
      appTempFolder = CDarwinUtils::GetOSTemporaryDirectory();
    #elif defined(TARGET_DARWIN_IOS)
      appTempFolder = URIUtils::AddFileToFolder(CDarwinUtils::GetOSAppRootFolder(),  CCompileInfo::GetAppName());
    #else
      std::string dotLowerAppName = StringUtils::Format(".%s", CCompileInfo::GetAppName());
      StringUtils::ToLower(dotLowerAppName);
      appTempFolder = URIUtils::AddFileToFolder(getenv("HOME"), dotLowerAppName);
      mkdir(appTempFolder.c_str(), 0755);
    #endif
    appTempFolder = URIUtils::AddFileToFolder(appTempFolder, "temp");
  }

  return appTempFolder.c_str();
}

const char* CDarwinUtils::GetUserHomeDirectory(void)
{
  static std::string appHomeFolder;
  if (appHomeFolder.empty())
  {
    #if defined(TARGET_DARWIN_TVOS)
      appHomeFolder = URIUtils::AddFileToFolder(CDarwinUtils::GetOSCachesDirectory(), "home");
    #elif defined(TARGET_DARWIN_IOS)
      appHomeFolder = URIUtils::AddFileToFolder(CDarwinUtils::GetOSAppRootFolder(), CCompileInfo::GetAppName());
    #else
      appHomeFolder = URIUtils::AddFileToFolder(getenv("HOME"), "Library/Application Support");
      appHomeFolder = URIUtils::AddFileToFolder(appHomeFolder, CCompileInfo::GetAppName());
    #endif
  }

  return appHomeFolder.c_str();
}

const char* CDarwinUtils::GetOSAppRootFolder(void)
{
  NSArray *writablePaths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *appTempFolder = [writablePaths lastObject];
  return [appTempFolder UTF8String];
}

const char* CDarwinUtils::GetOSCachesDirectory()
{
  static std::string cacheFolder;
  if (cacheFolder.empty())
  {
    NSString *cachePath = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) lastObject];
    cacheFolder = [cachePath UTF8String];
    URIUtils::RemoveSlashAtEnd(cacheFolder);
  }
  return cacheFolder.c_str();
}

const char* CDarwinUtils::GetOSTemporaryDirectory()
{
  static std::string tmpFolder;
  if (tmpFolder.empty())
  {
    tmpFolder = [NSTemporaryDirectory() UTF8String];
    URIUtils::RemoveSlashAtEnd(tmpFolder);
  }

  return tmpFolder.c_str();
}

int CDarwinUtils::BatteryLevel(void)
{
  float batteryLevel = 0;
#if !defined(TARGET_DARWIN_TVOS)
#if defined(TARGET_DARWIN_IOS)
  batteryLevel = [[UIDevice currentDevice] batteryLevel];
#else
  CFTypeRef powerSourceInfo = IOPSCopyPowerSourcesInfo();
  CFArrayRef powerSources = IOPSCopyPowerSourcesList(powerSourceInfo);

  CFDictionaryRef powerSource = NULL;
  const void *powerSourceVal;

  for (int i = 0 ; i < CFArrayGetCount(powerSources) ; i++)
  {
    powerSource = IOPSGetPowerSourceDescription(powerSourceInfo, CFArrayGetValueAtIndex(powerSources, i));
    if (!powerSource) break;

    powerSourceVal = (CFStringRef)CFDictionaryGetValue(powerSource, CFSTR(kIOPSNameKey));

    int curLevel = 0;
    int maxLevel = 0;

    powerSourceVal = CFDictionaryGetValue(powerSource, CFSTR(kIOPSCurrentCapacityKey));
    CFNumberGetValue((CFNumberRef)powerSourceVal, kCFNumberSInt32Type, &curLevel);

    powerSourceVal = CFDictionaryGetValue(powerSource, CFSTR(kIOPSMaxCapacityKey));
    CFNumberGetValue((CFNumberRef)powerSourceVal, kCFNumberSInt32Type, &maxLevel);

    batteryLevel = (double)curLevel/(double)maxLevel;
  }
  CFRelease(powerSources);
  CFRelease(powerSourceInfo);
#endif
#endif
  return batteryLevel * 100;  
}

void CDarwinUtils::EnableOSScreenSaver(bool enable)
{
#if defined(TARGET_DARWIN_TVOS)
  //CLog::Log(LOGDEBUG, "CDarwinUtils::EnableOSScreenSaver(%d)", enable);
  if (enable)
    [g_xbmcController enableScreenSaver];
  else
    [g_xbmcController disableScreenSaver];
#endif
}

bool CDarwinUtils::ResetSystemIdleTimer()
{
#if defined(TARGET_DARWIN_TVOS)
  //CLog::Log(LOGDEBUG, "CDarwinUtils::ResetSystemIdleTimer");
  if ([NSThread currentThread] != [NSThread mainThread])
  {
    __block bool rtn;
    dispatch_async(dispatch_get_main_queue(),^{
      rtn = [g_xbmcController resetSystemIdleTimer];
    });
    return rtn;
  }
  else
  {
    return [g_xbmcController resetSystemIdleTimer];
  }
#else
  return false;
#endif
}

void CDarwinUtils::UpdateFocusLayerMainThread()
{
#if defined(TARGET_DARWIN_TVOS)
  [g_xbmcController updateFocusLayerMainThread];
#endif
}

void CDarwinUtils::SetScheduling(int message)
{
  int policy;
  struct sched_param param;
  pthread_t this_pthread_self = pthread_self();

  pthread_getschedparam(this_pthread_self, &policy, &param );

  policy = SCHED_OTHER;
  thread_extended_policy_data_t theFixedPolicy={true};

  if (message == GUI_MSG_PLAYBACK_STARTED && g_application.m_pPlayer->IsPlayingVideo())
  {
    policy = SCHED_RR;
    theFixedPolicy.timeshare = false;
  }

  thread_policy_set(pthread_mach_thread_np(this_pthread_self),
    THREAD_EXTENDED_POLICY, 
    (thread_policy_t)&theFixedPolicy,
    THREAD_EXTENDED_POLICY_COUNT);

  pthread_setschedparam(this_pthread_self, policy, &param );
}

bool CFStringRefToStringWithEncoding(CFStringRef source, std::string &destination, CFStringEncoding encoding)
{
  const char *cstr = CFStringGetCStringPtr(source, encoding);
  if (!cstr)
  {
    CFIndex strLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(source) + 1,
                                                       encoding);
    char *allocStr = (char*)malloc(strLen);

    if(!allocStr)
      return false;

    if(!CFStringGetCString(source, allocStr, strLen, encoding))
    {
      free((void*)allocStr);
      return false;
    }

    destination = allocStr;
    free((void*)allocStr);

    return true;
  }

  destination = cstr;
  return true;
}

void CDarwinUtils::PrintDebugString(std::string debugString)
{
  NSLog(@"Debug Print: %s", debugString.c_str());
}


bool CDarwinUtils::CFStringRefToString(CFStringRef source, std::string &destination)
{
  return CFStringRefToStringWithEncoding(source, destination, CFStringGetSystemEncoding());
}

bool CDarwinUtils::CFStringRefToUTF8String(CFStringRef source, std::string &destination)
{
  return CFStringRefToStringWithEncoding(source, destination, kCFStringEncodingUTF8);
}

const std::string& CDarwinUtils::GetManufacturer(void)
{
  static std::string manufName;
  if (manufName.empty())
  {
#ifdef TARGET_DARWIN_IOS
    // to avoid dlloading of IOIKit, hardcode return value
	// until other than Apple devices with iOS will be released
    manufName = "Apple Inc.";
#elif defined(TARGET_DARWIN_OSX)
    const CFMutableDictionaryRef matchExpDev = IOServiceMatching("IOPlatformExpertDevice");
    if (matchExpDev)
    {
      const io_service_t servExpDev = IOServiceGetMatchingService(kIOMasterPortDefault, matchExpDev);
      if (servExpDev)
      {
        CFTypeRef manufacturer = IORegistryEntryCreateCFProperty(servExpDev, CFSTR("manufacturer"), kCFAllocatorDefault, 0);
        if (manufacturer)
        {
          if (CFGetTypeID(manufacturer) == CFStringGetTypeID())
            manufName = (const char*)[[NSString stringWithString:(__bridge NSString*)manufacturer] UTF8String];
          else if (CFGetTypeID(manufacturer) == CFDataGetTypeID())
          {
            manufName.assign((const char*)CFDataGetBytePtr((CFDataRef)manufacturer), CFDataGetLength((CFDataRef)manufacturer));
            if (!manufName.empty() && manufName[manufName.length() - 1] == 0)
              manufName.erase(manufName.length() - 1); // remove extra null at the end if any
          }
          CFRelease(manufacturer);
        }
      }
      IOObjectRelease(servExpDev);
    }
#endif // TARGET_DARWIN_OSX
  }
  return manufName;
}

bool CDarwinUtils::IsAliasShortcut(const std::string& path, bool isdirectory)
{
  bool ret = false;
#if defined(TARGET_DARWIN_OSX)
  NSURL *nsUrl;
  if (isdirectory)
  {
    std::string cleanpath = path;
    URIUtils::RemoveSlashAtEnd(cleanpath);
    NSString *nsPath = [NSString stringWithUTF8String:cleanpath.c_str()];
    nsUrl = [NSURL fileURLWithPath:nsPath isDirectory:TRUE];
  }
  else
  {
    NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
    nsUrl = [NSURL fileURLWithPath:nsPath isDirectory:FALSE];
  }

  NSNumber* wasAliased = nil;
  if (nsUrl != nil)
  {
    NSError *error = nil;

    if ([nsUrl getResourceValue:&wasAliased forKey:NSURLIsAliasFileKey error:&error])
    {
      ret = [wasAliased boolValue];
    }
  }
#endif
  return ret;
}

void CDarwinUtils::TranslateAliasShortcut(std::string& path)
{
#if defined(TARGET_DARWIN_OSX)
  NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
  NSURL *nsUrl = [NSURL fileURLWithPath:nsPath];
  
  if (nsUrl != nil)
  {
    NSError *error = nil;
    NSData * bookmarkData = [NSURL bookmarkDataWithContentsOfURL:nsUrl error:&error];
    if (bookmarkData)
    {
      BOOL isStale = NO;
      NSURLBookmarkResolutionOptions options = NSURLBookmarkResolutionWithoutUI |
                                               NSURLBookmarkResolutionWithoutMounting;

      NSURL* resolvedURL = [NSURL URLByResolvingBookmarkData:bookmarkData
                                                     options:options
                                               relativeToURL:nil
                                         bookmarkDataIsStale:&isStale
                                                       error:&error];
      if (resolvedURL)
      {
        // [resolvedURL path] returns a path as /dir/dir/file ...
        path = (const char*)[[resolvedURL path] UTF8String];
      }
    }
  }
#endif
}

bool CDarwinUtils::CreateAliasShortcut(const std::string& fromPath, const std::string& toPath)
{
  bool ret = false;
#if defined(TARGET_DARWIN_OSX)
  NSString *nsToPath = [NSString stringWithUTF8String:toPath.c_str()];
  NSURL *toUrl = [NSURL fileURLWithPath:nsToPath];
  NSString *nsFromPath = [NSString stringWithUTF8String:fromPath.c_str()];
  NSURL *fromUrl = [NSURL fileURLWithPath:nsFromPath];
  NSError *error = nil;
  NSData *bookmarkData = [toUrl bookmarkDataWithOptions: NSURLBookmarkCreationSuitableForBookmarkFile includingResourceValuesForKeys:nil relativeToURL:nil error:&error];

  if(bookmarkData != nil && fromUrl != nil && toUrl != nil) 
  {
    if([NSURL writeBookmarkData:bookmarkData toURL:fromUrl options:NSURLBookmarkCreationSuitableForBookmarkFile error:&error])
    {
      ret = true;
    }
  }
#endif
  return ret;
}

bool CDarwinUtils::OpenAppWithOpenURL(const std::string& path)
{
  bool ret = false;
#if defined(TARGET_DARWIN_IOS)
  NSString *ns_path = [NSString stringWithUTF8String:path.c_str()];
  NSCharacterSet *set = [NSCharacterSet URLQueryAllowedCharacterSet];
  NSString *ns_encoded_path = [ns_path stringByAddingPercentEncodingWithAllowedCharacters:set];
  NSURL *ns_url = [NSURL URLWithString:ns_encoded_path];
  
  if ([[UIApplication sharedApplication] canOpenURL:ns_url])
  {
    // Can open the youtube app URL so launch the youTube app with this URL
    [[UIApplication sharedApplication] openURL:ns_url options:@{} completionHandler:nil];
    return true;
  }
#endif

  return ret;
}

std::string CDarwinUtils::GetAudioRoute()
{
  std::string route;
#if defined(TARGET_DARWIN_IOS)
  AVAudioSession *myAudioSession = [AVAudioSession sharedInstance];
  AVAudioSessionRouteDescription *currentRoute = [myAudioSession currentRoute];
  NSString *output = [[currentRoute.outputs objectAtIndex:0] portType];
  if (output)
    route = [output UTF8String];
#endif
  return route;
}

void CDarwinUtils::DumpAudioDescriptions(const std::string& why)
{
#if defined(TARGET_DARWIN_IOS)
  if (!why.empty())
    CLog::Log(LOGDEBUG, "DumpAudioDescriptions: %s", why.c_str());

  AVAudioSession *myAudioSession = [AVAudioSession sharedInstance];

  NSArray *currentInputs = myAudioSession.currentRoute.inputs;
  int count_in = [currentInputs count];
  CLog::Log(LOGDEBUG, "DumpAudioDescriptions: input count = %d", count_in);
  for (int k = 0; k < count_in; ++k)
  {
    AVAudioSessionPortDescription *portDesc = [currentInputs objectAtIndex:k];
    CLog::Log(LOGDEBUG, "DumpAudioDescriptions: portName, %s", [portDesc.portName UTF8String]);
    for (AVAudioSessionChannelDescription *channel in portDesc.channels)
    {
      CLog::Log(LOGDEBUG, "DumpAudioDescriptions: channelLabel, %d", channel.channelLabel);
      CLog::Log(LOGDEBUG, "DumpAudioDescriptions: channelName , %s", [channel.channelName UTF8String]);
    }
  }

  NSArray *currentOutputs = myAudioSession.currentRoute.outputs;
  int count_out = [currentOutputs count];
  CLog::Log(LOGDEBUG, "DumpAudioDescriptions: output count = %d", count_out);
  for (int k = 0; k < count_out; ++k)
  {
    AVAudioSessionPortDescription *portDesc = [currentOutputs objectAtIndex:k];
    CLog::Log(LOGDEBUG, "DumpAudioDescriptions : portName, %s", [portDesc.portName UTF8String]);
    for (AVAudioSessionChannelDescription *channel in portDesc.channels)
    {
      CLog::Log(LOGDEBUG, "DumpAudioDescriptions: channelLabel, %d", channel.channelLabel);
      CLog::Log(LOGDEBUG, "DumpAudioDescriptions: channelName , %s", [channel.channelName UTF8String]);
    }
  }
#endif
}

std::string CDarwinUtils::GetHardwareUUID()
{
  static std::string uuid = "NOUUID";
  if (uuid == "NOUUID")
  {
#if defined(TARGET_DARWIN_OSX)
    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
    CFStringRef uuidCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
    IOObjectRelease(ioRegistryRoot);
    CFStringRefToString(uuidCf, uuid);
    CFRelease(uuidCf);
#elif defined(TARGET_DARWIN_IOS)
    NSString *nsuuid = nullptr;
    // all info about identifiers can be found here:
    // http://nshipster.com/uuid-udid-unique-identifier/
    // apple doesn't want us to call uniqueIdentifier and deprecated it
    if([[UIDevice currentDevice] respondsToSelector:@selector(identifierForVendor)])
      nsuuid = [[UIDevice currentDevice] identifierForVendor].UUIDString;
    
    if (nsuuid != nullptr)
      uuid = [nsuuid UTF8String];
#endif
  }

  return uuid;
}

void CDarwinUtils::GetAppMemory(int64_t &free, int64_t &delta)
{
  mach_task_basic_info_data_t taskinfo = {0};
  mach_msg_type_number_t outCount = MACH_TASK_BASIC_INFO_COUNT;
  kern_return_t error = task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&taskinfo, &outCount);
  if (error == KERN_SUCCESS)
  {
    // resident_size includes ALL app memory impact, kernel, drivers and app.
    // it will be higher than what Activity Monitor or Xcode shows.
    static mach_vm_size_t old_resident_size = 0;
    free = taskinfo.resident_size;
    delta = taskinfo.resident_size - old_resident_size;
    old_resident_size = taskinfo.resident_size;
  }
}

typedef struct FontHeader {
  int32_t version;
  uint16_t numTables;
  uint16_t searchRange;
  uint16_t entrySelector;
  uint16_t rangeShift;
} FontHeader;

typedef struct TableEntry {
  uint32_t tag;
  uint32_t checkSum;
  uint32_t offset;
  uint32_t length;
} TableEntry;

static uint32_t calcTableCheckSum(const uint32_t* table, uint32_t numberOfBytesInTable)
{
  uint32_t sum = 0;
  uint32_t nLongs = (numberOfBytesInTable + 3) / 4;
  while (nLongs-- > 0) {
    sum += CFSwapInt32HostToBig(*table++);
  }
  return sum;
}

static void fontDataForCGFont(CGFontRef cgFont, unsigned char **data, size_t &size)
{
    if (!cgFont)
      return;

    CFRetain(cgFont);

    CFArrayRef tags = CGFontCopyTableTags(cgFont);
    int tableCount = CFArrayGetCount(tags);

    std::vector<size_t> tableSizes;
    tableSizes.resize(tableCount);
    std::vector<CFDataRef> dataRefs;
    dataRefs.resize(tableCount);

    BOOL containsCFFTable = NO;

    size_t totalSize = sizeof(FontHeader) + sizeof(TableEntry) * tableCount;

    for (int index = 0; index < tableCount; ++index) {
        size_t tableSize = 0;
        intptr_t aTag = (intptr_t)CFArrayGetValueAtIndex(tags, index);

        if (aTag == 'CFF ' && !containsCFFTable) {
            containsCFFTable = YES;
        }

        dataRefs[index] = CGFontCopyTableForTag(cgFont, aTag);

        if (dataRefs[index] != NULL) {
            tableSize = CFDataGetLength(dataRefs[index]);
        }

        totalSize += (tableSize + 3) & ~3;

        tableSizes[index] = tableSize;
    }

    unsigned char *stream = (unsigned char*)malloc(totalSize);

    char* dataStart = (char*)stream;
    char* dataPtr = dataStart;

    // Write font header (also called sfnt header, offset subtable)
    FontHeader* offsetTable = (FontHeader*)dataPtr;

    // Compute font header entries
    // c.f: Organization of an OpenType Font in:
    // https://www.microsoft.com/typography/otspec/otff.htm
    {
        // (Maximum power of 2 <= numTables) x 16
        uint16_t entrySelector = 0;
        // Log2(maximum power of 2 <= numTables).
        uint16_t searchRange = 1;

        while (searchRange < tableCount >> 1) {
            entrySelector++;
            searchRange <<= 1;
        }
        searchRange <<= 4;

        // NumTables x 16-searchRange.
        uint16_t rangeShift = (tableCount << 4) - searchRange;

        // OpenType Font contains CFF Table use 'OTTO' as version, and with .otf extension
        // otherwise 0001 0000
        offsetTable->version = containsCFFTable ? 'OTTO' : CFSwapInt16HostToBig(1);
        offsetTable->numTables = CFSwapInt16HostToBig((uint16_t)tableCount);
        offsetTable->searchRange = CFSwapInt16HostToBig((uint16_t)searchRange);
        offsetTable->entrySelector = CFSwapInt16HostToBig((uint16_t)entrySelector);
        offsetTable->rangeShift = CFSwapInt16HostToBig((uint16_t)rangeShift);
    }

    dataPtr += sizeof(FontHeader);

    // Write tables
    TableEntry* entry = (TableEntry*)dataPtr;
    dataPtr += sizeof(TableEntry) * tableCount;

    for (int index = 0; index < tableCount; ++index) {

        intptr_t aTag = (intptr_t)CFArrayGetValueAtIndex(tags, index);
        CFDataRef tableDataRef = dataRefs[index];

        if (tableDataRef == NULL) { continue; }

        size_t tableSize = CFDataGetLength(tableDataRef);

        memcpy(dataPtr, CFDataGetBytePtr(tableDataRef), tableSize);

        entry->tag = CFSwapInt32HostToBig((uint32_t)aTag);
        entry->checkSum = CFSwapInt32HostToBig(calcTableCheckSum((uint32_t *)dataPtr, tableSize));

        uint32_t offset = dataPtr - dataStart;
        entry->offset = CFSwapInt32HostToBig((uint32_t)offset);
        entry->length = CFSwapInt32HostToBig((uint32_t)tableSize);
        dataPtr += (tableSize + 3) & ~3;
        ++entry;
        CFRelease(tableDataRef);
    }

    CFRelease(cgFont);

    *data = stream;
    size = totalSize;
}

void CDarwinUtils::CloneSystemFonts(std::string strPath)
{
  // only clone for iOS/tvOS, we are sandboxed in them
#if defined(TARGET_DARWIN_IOS)
  NSArray* fallbacks = [UIFont familyNames];
  for (id fallback in fallbacks)
  {
    UIFont *font = [UIFont fontWithName:fallback size:1.0];
    CGFontRef fontRef = CGFontCreateWithFontName((CFStringRef)font.fontName);
    if (!fontRef)
      continue;

    size_t size = 0;
    unsigned char *data = nullptr;
    fontDataForCGFont(fontRef, &data, size);
    CGFontRelease(fontRef);
    if (size > 0)
    {
      std::string filepath = strPath + [font.fontName UTF8String] + ".ttf";
      const CURL dtsUrl(filepath);
      XFILE::CFile dstfile;
      if (!dstfile.Exists(dtsUrl) && dstfile.OpenForWrite(dtsUrl, true))
      {
        ssize_t iread = size;
        ssize_t iwrite = 0;
        while (iwrite < iread)
        {
          ssize_t iwrite2 = dstfile.Write(data + iwrite, iread - iwrite);
          if (iwrite2 <= 0)
            break;
          iwrite += iwrite2;
        }
      }
      dstfile.Close();
    }

    free(data);
  }
#endif
}


#endif

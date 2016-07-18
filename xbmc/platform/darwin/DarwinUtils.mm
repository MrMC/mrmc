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
#include "GUIUserMessages.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "utils/StringUtils.h"

#include "CompileInfo.h"

#if defined(TARGET_DARWIN)

#if defined(TARGET_DARWIN_IOS)
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

#import "AutoPool.h"
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
  iPad3WIFI,
  iPad3GSMCDMA,
  iPad3,
  iPad4WIFI,
  iPad4,
  iPad4GSMCDMA,
  iPadAirWifi,
  iPadAirCellular,
  iPadMini2Wifi,
  iPadMini2Cellular,
  iPhone6,
  iPadAir2Wifi,
  iPadAir2Cellular,
  iPadMini3Wifi,
  iPadMini3Cellular,
  iPhone6Plus,        //from here on list devices with retina support which have scale == 3.0
  AppleTV4,
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
    std::string devStr(CDarwinUtils::getIosPlatformString());
    
    if (devStr == "iPhone1,1") eDev = iPhone2G;
    else if (devStr == "iPhone1,2") eDev = iPhone3G;
    else if (devStr == "iPhone2,1") eDev = iPhone3GS;
    else if (devStr == "iPhone3,1") eDev = iPhone4;
    else if (devStr == "iPhone3,2") eDev = iPhone4;
    else if (devStr == "iPhone3,3") eDev = iPhone4CDMA;    
    else if (devStr == "iPhone4,1") eDev = iPhone4S;
    else if (devStr == "iPhone5,1") eDev = iPhone5;
    else if (devStr == "iPhone5,2") eDev = iPhone5GSMCDMA;
    else if (devStr == "iPhone5,3") eDev = iPhone5CGSM;
    else if (devStr == "iPhone5,4") eDev = iPhone5CGlobal;
    else if (devStr == "iPhone6,1") eDev = iPhone5SGSM;
    else if (devStr == "iPhone6,2") eDev = iPhone5SGlobal;
    else if (devStr == "iPhone7,1") eDev = iPhone6Plus;
    else if (devStr == "iPhone7,2") eDev = iPhone6;
    else if (devStr == "iPod1,1") eDev = iPodTouch1G;
    else if (devStr == "iPod2,1") eDev = iPodTouch2G;
    else if (devStr == "iPod3,1") eDev = iPodTouch3G;
    else if (devStr == "iPod4,1") eDev = iPodTouch4G;
    else if (devStr == "iPod5,1") eDev = iPodTouch5G;
    else if (devStr == "iPad1,1") eDev = iPad;
    else if (devStr == "iPad1,2") eDev = iPad;
    else if (devStr == "iPad2,1") eDev = iPad2WIFI;
    else if (devStr == "iPad2,2") eDev = iPad2;
    else if (devStr == "iPad2,3") eDev = iPad2CDMA;
    else if (devStr == "iPad2,4") eDev = iPad2;
    else if (devStr == "iPad2,5") eDev = iPadMiniWIFI;
    else if (devStr == "iPad2,6") eDev = iPadMini;
    else if (devStr == "iPad2,7") eDev = iPadMiniGSMCDMA;
    else if (devStr == "iPad3,1") eDev = iPad3WIFI;
    else if (devStr == "iPad3,2") eDev = iPad3GSMCDMA;
    else if (devStr == "iPad3,3") eDev = iPad3;
    else if (devStr == "iPad3,4") eDev = iPad4WIFI;
    else if (devStr == "iPad3,5") eDev = iPad4;
    else if (devStr == "iPad3,6") eDev = iPad4GSMCDMA;
    else if (devStr == "iPad4,1") eDev = iPadAirWifi;
    else if (devStr == "iPad4,2") eDev = iPadAirCellular;
    else if (devStr == "iPad4,4") eDev = iPadMini2Wifi;
    else if (devStr == "iPad4,5") eDev = iPadMini2Cellular;
    else if (devStr == "iPad4,7") eDev = iPadMini3Wifi;
    else if (devStr == "iPad4,8") eDev = iPadMini3Cellular;
    else if (devStr == "iPad4,9") eDev = iPadMini3Cellular;
    else if (devStr == "iPad5,3") eDev = iPadAir2Wifi;
    else if (devStr == "iPad5,4") eDev = iPadAir2Cellular;
    else if (devStr == "AppleTV2,1") eDev = AppleTV2;
    else if (devStr == "AppleTV5,3") eDev = AppleTV4;
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
  CCocoaAutoPool pool;
  return [[[NSProcessInfo processInfo] operatingSystemVersionString] UTF8String];
}

float CDarwinUtils::GetIOSVersion(void)
{
  CCocoaAutoPool pool;
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
    CCocoaAutoPool pool;
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
    CCocoaAutoPool pool;
    OSXVersionString.assign((const char*)[[[NSDictionary dictionaryWithContentsOfFile:
                         @"/System/Library/CoreServices/SystemVersion.plist"] objectForKey:@"ProductVersion"] UTF8String]);
  }
  
  return OSXVersionString.c_str();
#else
  return "0.0";
#endif
}

int  CDarwinUtils::GetFrameworkPath(char* path, size_t *pathsize)
{
  CCocoaAutoPool pool;
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

  strcpy(path, PREFIX_USR_PATH);
  strcat(path, "/lib");
  *pathsize = strlen(path);
  //CLog::Log(LOGDEBUG, "DarwinFrameworkPath(e) -> %s", path);
  return 0;

  return -1;
}

int  CDarwinUtils::GetExecutablePath(char* path, size_t *pathsize)
{
  CCocoaAutoPool pool;
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
  CCocoaAutoPool pool;
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
  return [g_xbmcController resetSystemIdleTimer];
#else
  return false;
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
  CCocoaAutoPool pool;
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
  CCocoaAutoPool pool;
  return CFStringRefToStringWithEncoding(source, destination, CFStringGetSystemEncoding());
}

bool CDarwinUtils::CFStringRefToUTF8String(CFStringRef source, std::string &destination)
{
  CCocoaAutoPool pool;
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
    CCocoaAutoPool pool;

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
            manufName = (const char*)[[NSString stringWithString:(NSString *)manufacturer] UTF8String];
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
  CCocoaAutoPool pool;

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
  CCocoaAutoPool pool;

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
  CCocoaAutoPool pool;

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
#if defined(TARGET_DARWIN_TVOS)
  NSString *ns_path = [NSString stringWithUTF8String:path.c_str()];
  NSCharacterSet *set = [NSCharacterSet URLQueryAllowedCharacterSet];
  NSString *ns_encoded_path = [ns_path stringByAddingPercentEncodingWithAllowedCharacters:set];
  NSURL *ns_url = [NSURL URLWithString:ns_encoded_path];
  
  if ([[UIApplication sharedApplication] canOpenURL:ns_url])
  {
    // Can open the youtube app URL so launch the youTube app with this URL
    [[UIApplication sharedApplication] openURL:ns_url];
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

#endif

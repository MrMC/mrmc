/*
 *      Copyright (C) 2012-2013 Team XBMC
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

#include "AndroidFeatures.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

#include <cpu-features.h>
#include "platform/android/jni/Build.h"
#include "platform/android/jni/Context.h"
#include "platform/android/jni/JNIThreading.h"
#include "platform/android/jni/PackageManager.h"

bool CAndroidFeatures::HasNeon()
{
  if (android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM)
    return ((android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0);
  return false;
}

int CAndroidFeatures::GetVersion()
{
  static int version = -1;
  if (version == -1)
  {
    version = 0;

    JNIEnv *jenv = xbmc_jnienv();
    jclass jcOsBuild = jenv->FindClass("android/os/Build$VERSION");
    if (jcOsBuild == NULL)
    {
      CLog::Log(LOGERROR, "%s: Error getting class android.os.Build.VERSION", __PRETTY_FUNCTION__);
      return version;
    }

    jint iSdkVersion = jenv->GetStaticIntField(jcOsBuild, jenv->GetStaticFieldID(jcOsBuild, "SDK_INT", "I"));
    CLog::Log(LOGDEBUG, "%s: android.os.Build.VERSION %d", __PRETTY_FUNCTION__, (int)iSdkVersion);

    // <= 10 Gingerbread
    // <= 13 Honeycomb
    // <= 15 IceCreamSandwich
    //       JellyBean
    // <= 19 KitKat
    version = iSdkVersion;

    jenv->DeleteLocalRef(jcOsBuild);
  }
  return version;
}

int CAndroidFeatures::GetCPUCount()
{
  static int count = -1;
  if (count == -1)
  {
    count = android_getCpuCount();
  }
  return count;
}

int CAndroidFeatures::GetActualCPUCount()
{
  static int count = -1;
  if (count == -1)
  {
    count = android_getCpuCount();
    if (StringUtils::StartsWith(CJNIBuild::MODEL, "AFTS") &&
        StringUtils::StartsWithNoCase(CJNIBuild::MANUFACTURER, "Amazon"))
    {
      CLog::Log(LOGDEBUG, "CAndroidFeatures::GetActualCPUCount firetv2 detected cpu = 2 for 32 bit");
      // MediaTek 8173C, 2xA72 + 2xA53, returns 4 cores
      // but there are really only two A7s when built as 32 bit
      count = 2;
    }
  }
  return count;
}

bool CAndroidFeatures::HasTouchScreen()
{
  static int hastouchscreen = -1;
  if (hastouchscreen == -1)
  {
    if (CJNIContext::GetPackageManager().hasSystemFeature("android.hardware.touchscreen"))
      hastouchscreen = 1;
    else
      hastouchscreen = 0;
  }
  return (hastouchscreen == 1);
}

bool CAndroidFeatures::IsAmazonDevice()
{
  static int isamazon = -1;
  if (isamazon == -1)
  {
    if (StringUtils::StartsWithNoCase(CJNIBuild::MANUFACTURER, "Amazon"))
    {
      CLog::Log(LOGDEBUG, "CAndroidFeatures::IsAmazonDevice = yes");
      isamazon = 1;
    }
    else
      isamazon = 0;
  }
  return (isamazon == 1);
}

bool CAndroidFeatures::IsFireTVDevice()
{
  static int isfiretv = -1;
  if (isfiretv == -1)
  {
    if (StringUtils::StartsWith(CJNIBuild::MODEL, "AFT") &&
        StringUtils::StartsWithNoCase(CJNIBuild::MANUFACTURER, "Amazon"))
    {
      CLog::Log(LOGDEBUG, "CAndroidFeatures::IsFireTVDevice = yes");
      isfiretv = 1;
    }
    else
      isfiretv = 0;
  }
  return (isfiretv == 1);
}

bool CAndroidFeatures::IsShieldTVDevice()
{
  static int isshieldtv = -1;
  if (isshieldtv == -1)
  {
    if (StringUtils::StartsWith(CJNIBuild::MODEL, "SHIELD Android TV") &&
        StringUtils::StartsWithNoCase(CJNIBuild::MANUFACTURER, "NVIDIA"))
    {
      CLog::Log(LOGDEBUG, "CAndroidFeatures::IsShieldTVDevice = yes");
      isshieldtv = 1;
    }
    else
      isshieldtv = 0;
  }
  return (isshieldtv == 1);
}

/*
 *      Copyright (C) 2011-2016 Team XBMC
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

#include "SysfsUtils.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

int SysfsUtils::SetString(const std::string& path, const std::string &valstr)
{
  int fd = open(path.c_str(), O_RDWR, 0644);
  int ret = -1;
  if (fd >= 0)
  {
    if (write(fd, valstr.c_str(), valstr.size()) > 0)
      ret = 0;
    close(fd);
  }
  if (ret)
    CLog::Log(LOGDEBUG, "%s: error writing %s", __FUNCTION__, path.c_str());

  return ret;
}

int SysfsUtils::GetString(const std::string& path, std::string &valstr)
{
  int len;
  char buf[256] = {0};

  int fd = open(path.c_str(), O_RDONLY);
  if (fd >= 0)
  {
    valstr.clear();
    while ((len = read(fd, buf, 256)) > 0)
      valstr.append(buf, len);
    close(fd);
 
    StringUtils::Trim(valstr);
    
    return 0;
  }

  CLog::Log(LOGDEBUG, "%s: error reading %s", __FUNCTION__, path.c_str());
  valstr = "fail";
  return -1;
}

int SysfsUtils::SetInt(const std::string& path, const int val)
{
  int ret = -1;
  int fd = open(path.c_str(), O_RDWR, 0644);
  if (fd >= 0)
  {
    char bcmd[16] = {0};
    sprintf(bcmd, "%d", val);
    if (write(fd, bcmd, strlen(bcmd)) > 0)
      ret = 0;
    close(fd);
  }
  if (ret)
    CLog::Log(LOGDEBUG, "%s: error writing %s", __FUNCTION__, path.c_str());

  return ret;
}

int SysfsUtils::GetInt(const std::string& path, int &val)
{
  int ret = -1;
  int fd = open(path.c_str(), O_RDONLY);
  if (fd >= 0)
  {
    char bcmd[16] = {0};
    if (read(fd, bcmd, sizeof(bcmd)) > 0)
    {
      val = strtol(bcmd, NULL, 10);
      ret = 0;
    }
    close(fd);
  }
  if (ret)
    CLog::Log(LOGDEBUG, "%s: error reading %s", __FUNCTION__, path.c_str());

  return ret;
}

bool SysfsUtils::Has(const std::string &path)
{
  int fd = open(path.c_str(), O_RDONLY);
  if (fd >= 0)
  {
    close(fd);
    return true;
  }
  return false;
}

bool SysfsUtils::HasRW(const std::string &path)
{
  int fd = open(path.c_str(), O_RDWR);
  if (fd >= 0)
  {
    close(fd);
    return true;
  }
  return false;
}

RESOLUTION_INFO SysfsUtils::CEAtoRES(int CEA)
{
  RESOLUTION_INFO res = {0};
  switch (CEA)
  {
    case 16: // 1080p60
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 60.0 * 1000.0 / 1001.0;
      break;
    case 4:  // 720p60
      res.iWidth = res.iScreenWidth = 1280;
      res.iHeight = res.iScreenHeight = 720;
      res.fRefreshRate = 60.0 * 1000.0 / 1001.0;
      break;
    case 31: // 1080p50
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 50.0;
      break;
    case 19: // 720p50
      res.iWidth = res.iScreenWidth = 1280;
      res.iHeight = res.iScreenHeight = 720;
      res.fRefreshRate = 50.0;
      break;
    default:
      break;
  }

  return res;
}

RESOLUTION_INFO SysfsUtils::FireOS_ConvertResolution(int res_hdmi)
{
  typedef enum
  {
    HDMI_VIDEO_DISCONNECTED = 0,
    // below are internal kernel values, we want +1
    // as zero is HDMI display off or disconnected
    HDMI_VIDEO_720x480p_60Hz,     /* 0 */
    HDMI_VIDEO_720x576p_50Hz,     /* 1 */
    HDMI_VIDEO_1280x720p_60Hz,    /* 2 */
    HDMI_VIDEO_1280x720p_50Hz,    /* 3 */
    HDMI_VIDEO_1920x1080i_60Hz,   /* 4 */
    HDMI_VIDEO_1920x1080i_50Hz,   /* 5 */
    HDMI_VIDEO_1920x1080p_30Hz,   /* 6 */
    HDMI_VIDEO_1920x1080p_25Hz,   /* 7 */
    HDMI_VIDEO_1920x1080p_24Hz,   /* 8 */
    HDMI_VIDEO_1920x1080p_23Hz,   /* 9 */
    HDMI_VIDEO_1920x1080p_29Hz,   /* a */
    HDMI_VIDEO_1920x1080p_60Hz,   /* b */
    HDMI_VIDEO_1920x1080p_50Hz,   /* c */

    HDMI_VIDEO_1280x720p3d_60Hz,	/* d */
    HDMI_VIDEO_1280x720p3d_50Hz,	/* e */
    HDMI_VIDEO_1920x1080i3d_60Hz,	/* f */
    HDMI_VIDEO_1920x1080i3d_50Hz,	/* 10 */
    HDMI_VIDEO_1920x1080p3d_24Hz,	/* 11 */
    HDMI_VIDEO_1920x1080p3d_23Hz,	/* 12 */

    /* 2160 means 3840x2160 */
    HDMI_VIDEO_2160P_23_976HZ,    /* 13 */
    HDMI_VIDEO_2160P_24HZ,        /* 14 */
    HDMI_VIDEO_2160P_25HZ,        /* 15 */
    HDMI_VIDEO_2160P_29_97HZ,     /* 16 */
    HDMI_VIDEO_2160P_30HZ,        /* 17 */
    /* 2161 means 4096x2160 */
    HDMI_VIDEO_2161P_24HZ,        /* 18 */
  } HDMI_VIDEO_RESOLUTION;

  RESOLUTION_INFO res = {0};
  switch(res_hdmi)
  {
    case HDMI_VIDEO_720x480p_60Hz:
      res.iWidth = res.iScreenWidth = 720;
      res.iHeight = res.iScreenHeight = 480;
      res.fRefreshRate = 60.0 * 1000.0 / 1001.0;
      break;
    case HDMI_VIDEO_720x576p_50Hz:
      res.iWidth = res.iScreenWidth = 720;
      res.iHeight = res.iScreenHeight = 576;
      res.fRefreshRate = 50.0;
      break;
    case HDMI_VIDEO_1280x720p_60Hz:
      res.iWidth = res.iScreenWidth = 1280;
      res.iHeight = res.iScreenHeight = 720;
      res.fRefreshRate = 60.0 * 1000.0 / 1001.0;
      break;
    case HDMI_VIDEO_1280x720p_50Hz:
      res.iWidth = res.iScreenWidth = 1280;
      res.iHeight = res.iScreenHeight = 720;
      res.fRefreshRate = 50.0;
      break;
    case HDMI_VIDEO_1920x1080i_60Hz:
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 60.0 * 1000.0 / 1001.0;
      break;
    case HDMI_VIDEO_1920x1080i_50Hz:
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 50.0;
      break;
    case HDMI_VIDEO_1920x1080p_30Hz:
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 30.0;
      break;
    case HDMI_VIDEO_1920x1080p_25Hz:
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 25.0;
      break;
    case HDMI_VIDEO_1920x1080p_24Hz:
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 24.0;
      break;
    case HDMI_VIDEO_1920x1080p_23Hz:
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 24.0 * 1000.0 / 1001.0;
      break;
    case HDMI_VIDEO_1920x1080p_29Hz:
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 30.0 * 1000.0 / 1001.0;
      break;
    case HDMI_VIDEO_1920x1080p_60Hz:
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 60.0 * 1000.0 / 1001.0;
      break;
    case HDMI_VIDEO_1920x1080p_50Hz:
      res.iWidth = res.iScreenWidth = 1920;
      res.iHeight = res.iScreenHeight = 1080;
      res.fRefreshRate = 50.0;
      break;
    case HDMI_VIDEO_2160P_23_976HZ:
      res.iWidth = res.iScreenWidth = 3840;
      res.iHeight = res.iScreenHeight = 2160;
      res.fRefreshRate = 24.0 * 1000.0 / 1001.0;
      break;
    case HDMI_VIDEO_2160P_24HZ:
      res.iWidth = res.iScreenWidth = 3840;
      res.iHeight = res.iScreenHeight = 2160;
      res.fRefreshRate = 24.0;
      break;
    case HDMI_VIDEO_2160P_25HZ:
      res.iWidth = res.iScreenWidth = 3840;
      res.iHeight = res.iScreenHeight = 2160;
      res.fRefreshRate = 25.0;
      break;
    case HDMI_VIDEO_2160P_29_97HZ:
      res.iWidth = res.iScreenWidth = 3840;
      res.iHeight = res.iScreenHeight = 2160;
      res.fRefreshRate = 30.0 * 1000.0 / 1001.0;
      break;
    case HDMI_VIDEO_2161P_24HZ:
      res.iWidth = res.iScreenWidth = 4096;
      res.iHeight = res.iScreenHeight = 2160;
      res.fRefreshRate = 24.0;
      break;
    // figure these out later
    case HDMI_VIDEO_1280x720p3d_60Hz:
    case HDMI_VIDEO_1280x720p3d_50Hz:
    case HDMI_VIDEO_1920x1080i3d_60Hz:
    case HDMI_VIDEO_1920x1080i3d_50Hz:
    case HDMI_VIDEO_1920x1080p3d_24Hz:
    case HDMI_VIDEO_1920x1080p3d_23Hz:
    default:
      break;
  }

  return res;
}

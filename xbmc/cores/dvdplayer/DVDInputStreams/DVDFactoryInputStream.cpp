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

#include "system.h"
#include "DVDFactoryInputStream.h"
#include "DVDInputStream.h"
#include "DVDInputStreamFile.h"
#include "DVDInputStreamFFmpeg.h"
#include "DVDInputStreamPVRManager.h"
#include "DVDInputStreamRTMP.h"
#include "FileItem.h"
#include "storage/MediaManager.h"
#include "URL.h"
#include "filesystem/File.h"
#include "utils/URIUtils.h"


CDVDInputStream* CDVDFactoryInputStream::CreateInputStream(IDVDPlayer* pPlayer, const std::string& file, const std::string& content, bool contentlookup)
{
  CFileItem item(file.c_str(), false);

  item.SetMimeType(content);

  if(file.substr(0, 6) == "pvr://")
    return new CDVDInputStreamPVRManager(pPlayer);
  else if(file.substr(0, 6) == "rtp://"
       || file.substr(0, 7) == "rtsp://"
       || file.substr(0, 6) == "sdp://"
       || file.substr(0, 6) == "udp://"
       || file.substr(0, 6) == "tcp://"
       || file.substr(0, 6) == "mms://"
       || file.substr(0, 7) == "mmst://"
       || file.substr(0, 7) == "mmsh://")
    return new CDVDInputStreamFFmpeg();
#ifdef HAS_LIBRTMP
  else if(file.substr(0, 7) == "rtmp://"
       || file.substr(0, 8) == "rtmpt://"
       || file.substr(0, 8) == "rtmpe://"
       || file.substr(0, 9) == "rtmpte://"
       || file.substr(0, 8) == "rtmps://")
    return new CDVDInputStreamRTMP();
#endif
  else if (item.IsInternetStream())
  {
    if (item.IsType(".m3u8"))
      return new CDVDInputStreamFFmpeg();

    if (contentlookup)
    {
      // request header
      item.SetMimeType("");
      item.FillInMimeType();
    }

    if (item.GetMimeType() == "application/vnd.apple.mpegurl")
      return new CDVDInputStreamFFmpeg();
  }

  // our file interface handles all these types of streams
  return (new CDVDInputStreamFile());
}

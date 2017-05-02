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

#include <limits.h>

#include "DVDInputStreamFFmpeg.h"
#include "playlists/PlayListM3U.h"
#include "filesystem/File.h"
#include "settings/Settings.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"
#include "URL.h"

extern "C" {
#include "libavformat/avformat.h"
}
using namespace XFILE;

std::vector<XFILE::CFile*> g_cached_hls_files;
CCriticalSection g_cached_hls_files_lock;

static int hls_file_read(void *h, uint8_t* buf, int size)
{
  CSingleLock lock(g_cached_hls_files_lock);
  //CLog::Log(LOGDEBUG, "%s - hls_file_read", __FUNCTION__);
  //if(hls_interrupt_cb(h))
  //  return AVERROR_EXIT;

  XFILE::CFile *cfile = static_cast<XFILE::CFile*>(h);
  return cfile->Read(buf, size);
}
static int64_t hls_file_seek(void *h, int64_t pos, int whence)
{
  CSingleLock lock(g_cached_hls_files_lock);
  //CLog::Log(LOGDEBUG, "%s - hls_file_seek", __FUNCTION__);
  //if (hls_interrupt_cb(h))
  //  return AVERROR_EXIT;

  XFILE::CFile *cfile = static_cast<XFILE::CFile*>(h);
  if(whence == AVSEEK_SIZE)
    return cfile->GetLength();
  else
    return cfile->Seek(pos, whence & ~AVSEEK_FORCE);
}
static void hls_file_close(struct AVFormatContext *s, AVIOContext *pb)
{
  CSingleLock lock(g_cached_hls_files_lock);
  if (pb && pb->opaque)
  {
    //CLog::Log(LOGDEBUG, "%s - hls_file_close", __FUNCTION__);
    XFILE::CFile *cfile = static_cast<XFILE::CFile*>(pb->opaque);
    auto file = std::find(g_cached_hls_files.begin(), g_cached_hls_files.end(), cfile);
    if (file != g_cached_hls_files.end())
      g_cached_hls_files.erase(file);
    delete cfile;
    pb->opaque = nullptr;
    av_freep(&pb->buffer);
    av_freep(&pb);
  }
}
static int hls_file_open(struct AVFormatContext *s,
  AVIOContext **pb, const char *url, int flags, AVDictionary **options)
{
  CURL curl(url);
  AVDictionaryEntry *entry = NULL;
  while ((entry = av_dict_get(*options, "", entry, AV_DICT_IGNORE_SUFFIX)))
  {
    //CLog::Log(LOGDEBUG, "%s - hls_file_open options, key %s, value %s", __FUNCTION__, entry->key, entry->value);
    // copy our options over into AVFormatContext, FFMpeg will not do that for us.
    av_dict_set(&s->metadata, entry->key, entry->value, 0);
  }

  entry = NULL;
  while ((entry = av_dict_get(s->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)))
  {
    //CLog::Log(LOGDEBUG, "%s - hls_file_open metadata, key %s, value %s", __FUNCTION__, entry->key, entry->value);
    // copy our options over into AVFormatContext, FFMpeg will not do that for us.
    std::string key = entry->key;
    if (key == "headers")
    {
      // copy the headers form options into our CURL so they propogate too.
      // this must match how GetFFMpegOptionsFromURL works.
      std::vector<std::string> values = StringUtils::Split(entry->value, "\r\n");
      for (auto &value : values)
      {
        // stupid FFMpeg has a dangling space, strip it.
        StringUtils::Replace(value, ": ",":");
        std::vector<std::string> header = StringUtils::Split(value, ":");
        if (header.size() == 2)
          curl.SetProtocolOption(header[0], header[1]);
      }
    }
  }
  curl.SetOption("waitForSegments", "1");

  //CLog::Log(LOGDEBUG, "%s - hls_file_open, curl %s", __FUNCTION__, curl.Get().c_str());
  int cfileflags = 0;
    cfileflags |= READ_BITRATE | READ_CHUNKED | READ_CACHED;
  XFILE::CFile *cfile = new XFILE::CFile();
  if (cfile->Open(curl, cfileflags))
  {
    int blocksize = 8 * 32768;
    // large blocksize buffer transfers
    unsigned char* buffer = (unsigned char*)av_malloc(blocksize);
    *pb = avio_alloc_context(buffer, blocksize, 0, cfile, hls_file_read, NULL, hls_file_seek);
    (*pb)->max_packet_size = blocksize;

    if (StringUtils::EqualsNoCase(URIUtils::GetExtension(url), ".ts"))
    {
      CSingleLock lock(g_cached_hls_files_lock);
      g_cached_hls_files.push_back(cfile);
    }
    return 0;
  }

  return AVERROR(EINVAL);
}

static int hls_interrupt_cb(void* ctx)
{
  CDVDInputStreamFFmpeg *inputStream = static_cast<CDVDInputStreamFFmpeg*>(ctx);
  if(inputStream && inputStream->Aborted())
    return 1;
  return 0;
}

CDVDInputStreamFFmpeg::CDVDInputStreamFFmpeg(CFileItem& fileitem)
  : CDVDInputStream(DVDSTREAM_TYPE_FFMPEG, fileitem)
  , m_can_pause(false)
  , m_can_seek(false)
  , m_aborted(false)
  , m_usingHLSCustomIO(false)
{

}

CDVDInputStreamFFmpeg::~CDVDInputStreamFFmpeg()
{
  Close();
}

bool CDVDInputStreamFFmpeg::IsEOF()
{
  if(m_aborted)
    return true;
  else
    return false;
}

bool CDVDInputStreamFFmpeg::Open()
{
  if (!CDVDInputStream::Open())
    return false;

  m_can_pause = true;
  m_can_seek  = true;
  m_aborted   = false;

  if(strnicmp(m_item.GetPath().c_str(), "udp://", 6) == 0
  || strnicmp(m_item.GetPath().c_str(), "rtp://", 6) == 0)
  {
    m_can_pause = false;
    m_can_seek = false;
    m_realtime = true;
  }

  if(strnicmp(m_item.GetPath().c_str(), "tcp://", 6) == 0)
  {
    m_can_pause = true;
    m_can_seek  = false;
  }
  return true;
}

// close file and reset everyting
void CDVDInputStreamFFmpeg::Close()
{
  CDVDInputStream::Close();
}

int CDVDInputStreamFFmpeg::Read(uint8_t* buf, int buf_size)
{
  return -1;
}

int64_t CDVDInputStreamFFmpeg::GetLength()
{
  return 0;
}

int64_t CDVDInputStreamFFmpeg::Seek(int64_t offset, int whence)
{
  return -1;
}

bool CDVDInputStreamFFmpeg::GetCacheStatus(XFILE::SCacheStatus *status)
{
  if (m_usingHLSCustomIO)
  {
    CSingleLock lock(g_cached_hls_files_lock);
    if (!g_cached_hls_files.empty())
    {
      XFILE::SCacheStatus fileStatus;
      for (auto &file : g_cached_hls_files)
      {
        if (file->IoControl(IOCTRL_CACHE_STATUS, &fileStatus) >= 0)
        {
          *status = fileStatus;
        }
      }
    }
  }
  return false;
}

bool CDVDInputStreamFFmpeg::UseHLSCustomIO(AVFormatContext *formatContext)
{
  if (m_item.IsInternetStream() && (m_item.IsType(".m3u8") || m_item.GetMimeType() == "application/vnd.apple.mpegurl"))
  {
    const AVIOInterruptCB interruptCallback = { hls_interrupt_cb, this };
    // have to explicity set the AVFMT_FLAG_CUSTOM_IO flag or die when closing.
    formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;
    formatContext->io_open = hls_file_open;
    formatContext->io_close = hls_file_close;
    formatContext->interrupt_callback = interruptCallback;
    m_usingHLSCustomIO = true;
    return true;
  }

  return false;
}

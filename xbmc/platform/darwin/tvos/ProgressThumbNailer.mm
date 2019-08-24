/*
 *      Copyright (C) 2018 Team MrMC
 *      https://github.com/MrMC
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
 *  along with MrMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

//#define enableDebugLogging 1

#import "ProgressThumbNailer.h"

#import <UIKit/UIKit.h>

#import "DVDFileInfo.h"
#import "DVDStreamInfo.h"
#import "FileItem.h"
#import "cores/FFmpeg.h"
#import "cores/dvdplayer/DVDInputStreams/DVDInputStream.h"
#import "cores/dvdplayer/DVDInputStreams/DVDInputStreamBluray.h"
#import "cores/dvdplayer/DVDInputStreams/DVDFactoryInputStream.h"
#import "cores/dvdplayer/DVDDemuxers/DVDDemux.h"
#import "cores/dvdplayer/DVDDemuxers/DVDDemuxUtils.h"
#import "cores/dvdplayer/DVDDemuxers/DVDFactoryDemuxer.h"
#import "cores/dvdplayer/DVDDemuxers/DVDDemuxFFmpeg.h"
#import "cores/dvdplayer/DVDCodecs/DVDCodecs.h"
#import "cores/dvdplayer/DVDCodecs/DVDCodecUtils.h"
#import "cores/dvdplayer/DVDCodecs/DVDFactoryCodec.h"
#import "cores/dvdplayer/DVDCodecs/Video/DVDVideoCodec.h"
#import "cores/dvdplayer/DVDCodecs/Video/DVDVideoCodecFFmpeg.h"
#import "cores/dvdplayer/DVDCodecs/Video/DVDVideoCodecVideoToolBox.h"
#import "cores/dvdplayer/DVDClock.h"
#import "cores/VideoRenderers/RenderManager.h"
#import "cores/VideoRenderers/RenderCapture.h"
#import "filesystem/StackDirectory.h"
#import "platform/darwin/tvos/FocusLayerViewPlayerProgress.h"
#import "threads/SystemClock.h"
#import "video/VideoInfoTag.h"
#import "utils/log.h"

void flip_vertically(unsigned char *dst, unsigned char *src, const size_t height, const size_t stride)
{
    unsigned char *low = src;
    unsigned char *high = dst;
    high += (height - 1) * stride;

    for (size_t line = 0; line < height; line++, low += stride, high -= stride)
        memcpy(high, low, stride);
}

CProgressThumbNailer::CProgressThumbNailer(const CFileItem& item, int width, id obj)
: CThread("ProgressThumbNailer")
{
  m_obj = obj;
  m_width = width;
  m_path = item.GetPath();
  m_redactPath = CURL::GetRedacted(m_path);

  if (item.IsVideoDb() && item.HasVideoInfoTag())
    m_path = item.GetVideoInfoTag()->m_strFileNameAndPath;

  if (item.IsStack())
    m_path = XFILE::CStackDirectory::GetFirstStackedFile(item.GetPath());

  if (item.HasVideoInfoTag())
    m_totalTimeMilliSeconds = 1000.0 * item.GetVideoInfoTag()->m_streamDetails.GetVideoDuration();

  CThread::Create();
}

CProgressThumbNailer::~CProgressThumbNailer()
{
  if (m_videoDemuxer)
    m_videoDemuxer->Abort();
  if (m_inputStream)
    m_inputStream->Abort();

  if (IsRunning())
  {
    m_bStop = true;
    m_processSleep.Set();
    StopThread();
  }
  // dispose of codec after process thread stops
  // it might be in use
  if (m_videoCodec)
    m_videoCodec->Dispose();
  SAFE_DELETE(m_videoCodec);
  SAFE_DELETE(m_videoDemuxer);
  SAFE_DELETE(m_inputStream);
}

void CProgressThumbNailer::RequestThumb(double seconds)
{
  m_seekQueue.push(seconds);
  m_processSleep.Set();
}

void CProgressThumbNailer::RequestThumbAsScreenCapture(double seconds)
{
  if (!g_renderManager.CanRenderCapture())
  {
    RequestThumb(seconds);
    return;
  }

  float aspectRatio = g_renderManager.GetAspectRatio();

  int width = m_width;
  int height = (int)(m_width / aspectRatio);
  if (height > m_width)
  {
    height = m_width;
    width = (int)(m_width * aspectRatio);
  }

#if enableDebugLogging
  unsigned int nTime = XbmcThreads::SystemClockMillis();
#endif
  ThumbNailerImage thumbNailerImage;
  CRenderCapture* thumbnail = g_renderManager.AllocRenderCapture();
  if (thumbnail)
  {
    g_renderManager.Capture(thumbnail, width, height, CAPTUREFLAG_IMMEDIATELY);

    thumbnail->GetEvent().WaitMSec(1000);
    if (thumbnail->GetUserState() == CAPTURESTATE_DONE)
    {
      // XOR Swap BGRA -> RGBA
      unsigned char* pixels = (unsigned char*)thumbnail->GetPixels();
      for (unsigned int i = 0; i < thumbnail->GetWidth() * thumbnail->GetHeight(); i++, pixels+=4)
        std::swap(pixels[0], pixels[2]);

      int scaledLineSize = width * 4;
      uint8_t *scaledData = (uint8_t*)malloc(scaledLineSize * height);
      flip_vertically(scaledData, thumbnail->GetPixels(), height, scaledLineSize);
      //memcpy(scaledData, thumbnail->GetPixels(), height * scaledLineSize);

      CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault;
      CFDataRef data = CFDataCreate(kCFAllocatorDefault, scaledData, scaledLineSize * height);
      CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
      CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
      CGImageRef cgImageRef = CGImageCreate(width,
                         height,
                         8,
                         32,
                         scaledLineSize,
                         colorSpace,
                         bitmapInfo,
                         provider,
                         NULL,
                         NO,
                         kCGRenderingIntentDefault);
      CGColorSpaceRelease(colorSpace);
      CGDataProviderRelease(provider);
      CFRelease(data);

      if (cgImageRef)
      {
        if (seconds < m_totalTimeMilliSeconds)
          m_seekTimeMilliSeconds = seconds * 1000.0;
        else
          m_seekTimeMilliSeconds = m_totalTimeMilliSeconds;
#if enableDebugLogging
        CLog::Log(LOGDEBUG, "RequestThumbAsScreenCapture - requested(%d)", m_seekTimeMilliSeconds);
#endif
        thumbNailerImage.isCapture = true;
        thumbNailerImage.time = m_seekTimeMilliSeconds;
        thumbNailerImage.image = cgImageRef;
        CSingleLock lock(m_thumbImagesCritical);
        m_thumbImages.push(thumbNailerImage);
        lock.Leave();
        if ([m_obj isKindOfClass:[FocusLayerViewPlayerProgress class]] )
        {
          FocusLayerViewPlayerProgress *viewPlayerProgress = (FocusLayerViewPlayerProgress*)m_obj;
          [viewPlayerProgress updateViewMainThread];
        }
      }

      free(scaledData);
    }
    else
      CLog::Log(LOGERROR,"RequestThumbAsScreenCapture: failed to create thumbnail");

    g_renderManager.ReleaseRenderCapture(thumbnail);
  }
#if enableDebugLogging
  unsigned int nTotalTime = XbmcThreads::SystemClockMillis() - nTime;
  CLog::Log(LOGDEBUG,"%s - measured %u ms to capture thumb at %d.", __FUNCTION__, nTotalTime, thumbNailerImage.time);
#endif
}

ThumbNailerImage CProgressThumbNailer::GetThumb()
{
  ThumbNailerImage thumbImage;
  if (!m_thumbImages.empty())
  {
    CSingleLock lock(m_thumbImagesCritical);
    // grab latest generated thumb
    thumbImage = m_thumbImages.back();
    while (!m_thumbImages.empty())
      m_thumbImages.pop();
  }
  return thumbImage;
}

void CProgressThumbNailer::Process()
{
  CFileItem item(m_path, false);
  m_inputStream = CDVDFactoryInputStream::CreateInputStream(NULL, item);
  if (!m_inputStream)
  {
    CLog::Log(LOGERROR, "CProgressThumbNailer::ExtractThumb: Error creating stream for %s", m_redactPath.c_str());
    return;
  }

  if (m_inputStream->IsStreamType(DVDSTREAM_TYPE_DVD)
   || m_inputStream->IsStreamType(DVDSTREAM_TYPE_BLURAY))
  {
    CLog::Log(LOGDEBUG, "CProgressThumbNailer::ExtractThumb: disc streams not supported for thumb extraction, file: %s", m_redactPath.c_str());
    SAFE_DELETE(m_inputStream);
    return;
  }

  if (m_inputStream->IsStreamType(DVDSTREAM_TYPE_TV))
  {
    SAFE_DELETE(m_inputStream);
    return;
  }

  if (m_bStop)
    return;

  m_inputStream->SetNoCaching();
  if (!m_inputStream->Open())
  {
    CLog::Log(LOGERROR, "InputStream: Error opening, %s", m_redactPath.c_str());
    SAFE_DELETE(m_inputStream);
    return;
  }

  if (m_bStop)
    return;

  try
  {
    m_videoDemuxer = CDVDFactoryDemuxer::CreateDemuxer(m_inputStream, false);
    if(!m_videoDemuxer)
    {
      SAFE_DELETE(m_inputStream);
      CLog::Log(LOGERROR, "%s - Error creating demuxer", __FUNCTION__);
      return;
    }
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "%s - Exception thrown when opening demuxer", __FUNCTION__);
    SAFE_DELETE(m_videoDemuxer);
    SAFE_DELETE(m_inputStream);
    return;
  }

  if (m_bStop)
    return;

  for (int i = 0; i < m_videoDemuxer->GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = m_videoDemuxer->GetStream(i);
    if (pStream)
    {
      // ignore if it's a picture attachment (e.g. jpeg artwork)
      if(pStream->type == STREAM_VIDEO && !(pStream->flags & AV_DISPOSITION_ATTACHED_PIC))
        m_videoStream = i;
      else
        pStream->SetDiscard(AVDISCARD_ALL);
    }
  }

  if (m_videoStream != -1)
  {
    CDVDStreamInfo hints(*m_videoDemuxer->GetStream(m_videoStream), true);
    hints.software = true;
    CDVDCodecOptions dvdOptions;
    dvdOptions.m_keys.push_back(CDVDCodecOption("skip-deinterlacing", "1"));
    m_videoCodec = CDVDFactoryCodec::OpenCodec(new CDVDVideoCodecFFmpeg(), hints, dvdOptions);
    //m_videoCodec = CDVDFactoryCodec::OpenCodec(new CDVDVideoCodecVideoToolBox(), hints, dvdOptions);
    if (!m_videoCodec)
    {
      CLog::Log(LOGERROR, "%s - Error creating codec", __FUNCTION__);
      return;
    }
    m_aspect = hints.aspect;
    m_forced_aspect = hints.forced_aspect;
  }

  if (m_totalTimeMilliSeconds <= 0)
    m_totalTimeMilliSeconds = m_videoDemuxer->GetStreamLength();
  while (!m_bStop)
  {
    if (!m_seekQueue.empty())
    {
      CSingleLock lock(m_seekQueueCritical);
      double seconds = m_seekQueue.back();
      // grab last submitted seek percent
      while (!m_seekQueue.empty())
        m_seekQueue.pop();
      lock.Leave();

      if (seconds < m_totalTimeMilliSeconds)
        m_seekTimeMilliSeconds = seconds * 1000.0;
      else
        m_seekTimeMilliSeconds = m_totalTimeMilliSeconds;
#if enableDebugLogging
      CLog::Log(LOGDEBUG, "QueueExtractThumb - requested(%d)", m_seekTimeMilliSeconds);
#endif
      QueueExtractThumb(m_seekTimeMilliSeconds);
    }
    m_processSleep.WaitMSec(10);
    m_processSleep.Reset();
  }
}

void CProgressThumbNailer::QueueExtractThumb(int seekTime)
{
  if (!m_videoDemuxer || !m_videoCodec)
    return;

#if enableDebugLogging
  unsigned int nTime = XbmcThreads::SystemClockMillis();
#endif

  // reset codec on entry, we have no
  // clue about previous state.
  m_videoCodec->Reset();

  int packetsTried = 0;
  ThumbNailerImage thumbNailerImage;
  // timebase is ms
  if (m_videoDemuxer->SeekTime(seekTime, true))
  {
    int iDecoderState = VC_ERROR;
    DVDVideoPicture picture {};

    if (m_bStop)
      return;
    // num streams * 160 frames, should get a valid frame, if not abort.
    int abort_index = m_videoDemuxer->GetNrOfStreams() * 160;
    do
    {
      packetsTried++;
      DemuxPacket* pPacket = m_videoDemuxer->Read();
      if (!pPacket)
        break;

      if (pPacket->iStreamId != m_videoStream)
      {
        CDVDDemuxUtils::FreeDemuxPacket(pPacket);
        continue;
      }

      iDecoderState = m_videoCodec->Decode(pPacket->pData, pPacket->iSize, pPacket->dts, pPacket->pts);
      CDVDDemuxUtils::FreeDemuxPacket(pPacket);
      if (m_bStop)
        return;

      if (iDecoderState & VC_ERROR)
        break;

      if (iDecoderState & VC_PICTURE)
      {
        memset(&picture, 0, sizeof(DVDVideoPicture));
        if (m_videoCodec->GetPicture(&picture))
        {
          if (!(picture.iFlags & DVP_FLAG_DROPPED))
            break;
        }
      }

    } while (!m_bStop && abort_index--);
    if (m_bStop)
      return;

    if (iDecoderState & VC_PICTURE && !(picture.iFlags & DVP_FLAG_DROPPED))
    {
      if (picture.format == RENDER_FMT_NONE)
        return;

      unsigned int nWidth = m_width;
      double aspect = (double)picture.iDisplayWidth / (double)picture.iDisplayHeight;
      if(m_forced_aspect && m_aspect != 0)
        aspect = m_aspect;
      unsigned int nHeight = (unsigned int)((double)m_width / aspect);

      int scaledLineSize = nWidth * 3;
      uint8_t *scaledData = (uint8_t*)av_malloc(scaledLineSize * nHeight);

      struct SwsContext *context = sws_getContext(
        picture.iWidth, picture.iHeight, (AVPixelFormat)CDVDCodecUtils::PixfmtFromEFormat(picture.format),
        nWidth, nHeight, AV_PIX_FMT_RGB24,
        SWS_FAST_BILINEAR, NULL, NULL, NULL);
      if (context)
      {
        // CGImages have flipped in y-axis coordinated, we can do the flip as we convert/scale
        uint8_t * flipData = scaledData + (scaledLineSize * (nHeight -1));
        int flipLineSize = -scaledLineSize;

        uint8_t *src[] = { picture.data[0], picture.data[1], picture.data[2], 0 };
        int     srcStride[] = { picture.iLineSize[0], picture.iLineSize[1], picture.iLineSize[2], 0 };
        uint8_t *dst[] = { flipData, 0, 0, 0 };
        int     dstStride[] = { flipLineSize, 0, 0, 0 };
        sws_scale(context, src, srcStride, 0, picture.iHeight, dst, dstStride);
        sws_freeContext(context);

        if (m_bStop)
          return;

        CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault;
        CFDataRef data = CFDataCreate(kCFAllocatorDefault, scaledData, scaledLineSize * nHeight);
        CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGImageRef cgImageRef = CGImageCreate(nWidth,
                           nHeight,
                           8,
                           24,
                           scaledLineSize,
                           colorSpace,
                           bitmapInfo,
                           provider,
                           NULL,
                           NO,
                           kCGRenderingIntentDefault);
        CGColorSpaceRelease(colorSpace);
        CGDataProviderRelease(provider);
        CFRelease(data);

        if (cgImageRef)
        {
          if (picture.pts != DVD_NOPTS_VALUE)
            thumbNailerImage.time = DVD_TIME_TO_MSEC(picture.pts);
          else if (picture.dts != DVD_NOPTS_VALUE)
            thumbNailerImage.time = DVD_TIME_TO_MSEC(picture.dts);
          else
            thumbNailerImage.time = seekTime;
          thumbNailerImage.image = cgImageRef;
          CSingleLock lock(m_thumbImagesCritical);
          m_thumbImages.push(thumbNailerImage);
          lock.Leave();
          if (!m_bStop && [m_obj isKindOfClass:[FocusLayerViewPlayerProgress class]] )
          {
            FocusLayerViewPlayerProgress *viewPlayerProgress = (FocusLayerViewPlayerProgress*)m_obj;
            [viewPlayerProgress updateViewMainThread];
          }
        }
      }
      av_free(scaledData);
    }
    else
    {
      CLog::Log(LOGDEBUG,"%s - decode failed in %s after %d packets.", __FUNCTION__, m_redactPath.c_str(), packetsTried);
    }
  }

#if enableDebugLogging
  unsigned int nTotalTime = XbmcThreads::SystemClockMillis() - nTime;
  CLog::Log(LOGDEBUG,"%s - measured %u ms to extract thumb at %d in %d packets. ", __FUNCTION__, nTotalTime, thumbNailerImage.time, packetsTried);
#endif
  return;
}

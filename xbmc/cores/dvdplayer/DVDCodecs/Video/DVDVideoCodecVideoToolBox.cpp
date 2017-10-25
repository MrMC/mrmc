/*
 *      Copyright (C) 2010-2017 Team XBMC
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

#include "DVDVideoCodecVideoToolBox.h"

#include "cores/dvdplayer/DVDClock.h"
#include "platform/darwin/DictionaryUtils.h"
#include "platform/darwin/DarwinUtils.h"
#include "settings/Settings.h"
#include "settings/AdvancedSettings.h"
#include "utils/BitstreamConverter.h"
#include "utils/log.h"

extern "C" {
#include "libavformat/avformat.h"
}

#if __IPHONE_OS_VERSION_MAX_ALLOWED < 110000 || __TV_OS_VERSION_MAX_ALLOWED < 110000
extern OSStatus CMVideoFormatDescriptionCreateFromHEVCParameterSets(CFAllocatorRef allocator, size_t parameterSetCount, const uint8_t *const  _Nonnull *parameterSetPointers, const size_t *parameterSetSizes, int NALUnitHeaderLength, CFDictionaryRef extensions, CMFormatDescriptionRef  _Nullable *formatDescriptionOut) __attribute__((weak_import));
#endif


const uint8_t AVC_NAL_SPS = 7;
const uint8_t AVC_NAL_PPS = 8;
const uint8_t HEVC_NAL_VPS = 32;
const uint8_t HEVC_NAL_SPS = 33;
const uint8_t HEVC_NAL_PPS = 34;

#if !defined(TARGET_DARWIN_OSX)
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
static int CheckNP2( unsigned x )
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}
#endif

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
// helper function to create a CMSampleBufferRef from demuxer data
static CMSampleBufferRef
CreateSampleBufferFrom(CMFormatDescriptionRef fmt_desc,
  CMSampleTimingInfo &timingInfo, void *demux_buff, size_t demux_size)
{
  // need to retain the demux data until decoder is done with it.
  // the best way to do this is malloc/memcpy and use a kCFAllocatorMalloc.
  size_t demuxSize = demux_size;
  uint8_t *demuxData = (uint8_t*)malloc(demuxSize);
  memcpy(demuxData, demux_buff, demuxSize);

  CMBlockBufferRef videoBlock = nullptr;
  CMBlockBufferFlags flags = 0;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
    kCFAllocatorDefault,  // CFAllocatorRef structureAllocator
    demuxData,            // void *memoryBlock
    demuxSize,            // size_t blockLength
    kCFAllocatorMalloc,   // CFAllocatorRef blockAllocator
    nullptr,              // const CMBlockBufferCustomBlockSource *customBlockSource
    0,                    // size_t offsetToData
    demux_size,           // size_t dataLength
    flags,                // CMBlockBufferFlags flags
    &videoBlock);         // CMBlockBufferRef

  CMSampleBufferRef sBufOut = nullptr;
  const size_t sampleSizeArray[] = {demuxSize};
  const CMSampleTimingInfo timingInfoArray[] = {timingInfo};

  if (status == noErr)
  {
    status = CMSampleBufferCreate(
      kCFAllocatorDefault,// CFAllocatorRef allocator
      videoBlock,         // CMBlockBufferRef dataBuffer
      true,               // Boolean dataReady
      nullptr,            // CMSampleBufferMakeDataReadyCallback makeDataReadyCallback
      nullptr,            // void *makeDataReadyRefcon
      fmt_desc,           // CMFormatDescriptionRef formatDescription
      1,                  // CMItemCount numSamples
      1,                  // CMItemCount numSampleTimingEntries
      timingInfoArray,    // const CMSampleTimingInfo *sampleTimingArray
      1,                  // CMItemCount numSampleSizeEntries
      sampleSizeArray,    // const size_t *sampleSizeArray
      &sBufOut);          // CMSampleBufferRef *sBufOut
  }
  CFRelease(videoBlock);

  return sBufOut;
}

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
CDVDVideoCodecVideoToolBox::CDVDVideoCodecVideoToolBox() : CDVDVideoCodec()
{
  m_fmt_desc    = nullptr;
  m_bitstream   = nullptr;
  m_vt_session  = nullptr;
  m_pFormatName = "vtb";

  m_queue_depth = 0;
  m_display_queue = nullptr;
  m_max_ref_frames = 4;
  pthread_mutex_init(&m_queue_mutex, NULL);

  memset(&m_videobuffer, 0, sizeof(DVDVideoPicture));
  m_DropPictures = false;
  m_codecControlFlags = 0;
  m_started = false;
  m_lastKeyframe = 0;
  m_sessionRestart = false;
  m_sessionRestartPTS = DVD_NOPTS_VALUE;
  m_enable_temporal_processing = false;
}

CDVDVideoCodecVideoToolBox::~CDVDVideoCodecVideoToolBox()
{
  Dispose();
  pthread_mutex_destroy(&m_queue_mutex);
}

bool CDVDVideoCodecVideoToolBox::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEVIDEOTOOLBOX) && !hints.software)
  {
    if (hints.maybe_interlaced)
    {
      CLog::Log(LOGNOTICE, "%s - interlaced content.", __FUNCTION__);
      return false;
    }

    m_hints = hints;
    m_options = options;
 
    switch(m_hints.profile)
    {
      //case FF_PROFILE_H264_HIGH_10:
      case FF_PROFILE_H264_HIGH_10_INTRA:
      case FF_PROFILE_H264_HIGH_422:
      case FF_PROFILE_H264_HIGH_422_INTRA:
      case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
      case FF_PROFILE_H264_HIGH_444_INTRA:
      case FF_PROFILE_H264_CAVLC_444:
        CLog::Log(LOGNOTICE, "%s - unsupported h264 profile(%d)", __FUNCTION__, m_hints.profile);
        return false;
        break;
    }

    if (m_hints.width <= 0 || m_hints.height <= 0)
    {
      CLog::Log(LOGNOTICE, "%s - bailing with bogus hints, width(%d), height(%d)",
        __FUNCTION__, m_hints.width, m_hints.height);
      return false;
    }
    
    switch (m_hints.codec)
    {
      default:
      case AV_CODEC_ID_MPEG4:
      case AV_CODEC_ID_MPEG2VIDEO:
        return false;
      break;

      case AV_CODEC_ID_H264:
        if (m_hints.extrasize < 7 || m_hints.extradata == NULL)
        {
          CLog::Log(LOGNOTICE, "%s - avcC atom too data small or missing", __FUNCTION__);
          return false;
        }
        else
        {
          // use a bitstream converter for all flavors, that way
          // even avcC with silly 3-byte nals are covered.
          m_bitstream = new CBitstreamConverter;
          if (!m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, false))
          {
            SAFE_DELETE(m_bitstream);
            return false;
          }

          if (m_bitstream->GetExtraSize() < 8)
          {
            SAFE_DELETE(m_bitstream);
            return false;
          }
          else
          {
            // check the avcC atom's sps for number of reference frames
            // ignore if interlaced, it's handled in hints check above (until we get it working :)
            bool interlaced = true;
            uint8_t *spc = m_bitstream->GetExtraData() + 6;
            uint32_t sps_size = BS_RB16(spc);
            if (sps_size)
              m_bitstream->parseh264_sps(spc+3, sps_size-1, &interlaced, &m_max_ref_frames);
          }
          if (m_max_ref_frames == 0)
            m_max_ref_frames = 2;

          if (m_hints.profile == FF_PROFILE_H264_MAIN && m_hints.level == 32 && m_max_ref_frames > 4)
          {
            // Main@L3.2, VTB cannot handle greater than 4 reference frames
            CLog::Log(LOGNOTICE, "%s - Main@L3.2 detected, VTB cannot decode.", __FUNCTION__);
            SAFE_DELETE(m_bitstream);
            return false;
          }
        }
        m_pFormatName = "vtb-h264";
      break;

      case AV_CODEC_ID_HEVC:
        if (!CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEVIDEOTOOLBOX_HEVC_HDR) &&
            (hints.profile == FF_PROFILE_HEVC_MAIN_10 || hints.profile == FF_PROFILE_HEVC_REXT))
        {
          return false;
        }
         if (m_hints.extrasize < 23 || m_hints.extradata == NULL)
        {
          CLog::Log(LOGNOTICE, "%s - hvcC atom too data small or missing", __FUNCTION__);
          return false;
        }
        else
        {
         // use a bitstream converter for all flavors
          m_bitstream = new CBitstreamConverter;
          if (!m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, false))
          {
            SAFE_DELETE(m_bitstream);
            return false;
          }
          if (m_bitstream->GetExtraSize() < 23)
          {
            SAFE_DELETE(m_bitstream);
            return false;
          }
          m_pFormatName = "vtb-h265";
        }
      break;
    }

    if (!CreateParameterSetArraysFromExtraData())
    {
      Dispose();
      return false;
    }

    if (!CreateFormatDescriptorFromParameterSetArrays())
    {
      Dispose();
      return false;
    }

    if (!CreateVTSessionAndInitPictureFrame())
    {
      Dispose();
      return false;
    }

    m_DropPictures = false;
    // default to 5 min, this helps us feed correct pts to the player.
    m_max_ref_frames = std::max(m_max_ref_frames + 1, 5);
    // some VUI bitstream restrictions lie (GoPro mp4)
    m_max_ref_frames += 4;
    // don't grow past 16
    m_max_ref_frames = m_max_ref_frames > 16 ? 16 : m_max_ref_frames;

    CLog::Log(LOGDEBUG,"VideoToolBox: opened width(%d), height(%d)", m_hints.width, m_hints.height);

    return true;
  }

  return false;
}

void CDVDVideoCodecVideoToolBox::Dispose()
{
  DestroyVTSession();
  if (m_fmt_desc)
    CFRelease(m_fmt_desc), m_fmt_desc = nullptr;
  SAFE_DELETE(m_bitstream);

  if (m_parameterSetCount)
  {
    free(m_parameterSetSizes), m_parameterSetSizes = nullptr;
    free(m_parameterSetTypes), m_parameterSetTypes = nullptr;
    free(m_SavedParameterSets), m_SavedParameterSets = nullptr;
    free(m_parameterSetPointers), m_parameterSetPointers = nullptr;
  }

  if (m_videobuffer.iFlags & DVP_FLAG_ALLOCATED)
  {
    // release any previous retained cvbuffer reference
    if (m_videobuffer.cvBufferRef)
      CVBufferRelease(m_videobuffer.cvBufferRef);
    m_videobuffer.cvBufferRef = nullptr;
    m_videobuffer.iFlags = 0;
  }
  
  while (m_queue_depth)
    DisplayQueuePop();
}

void CDVDVideoCodecVideoToolBox::Reopen()
{
  m_started = false;
  Dispose();
  Open(m_hints, m_options);
}

void CDVDVideoCodecVideoToolBox::SetDropState(bool bDrop)
{
  // more a message to decoder to hurry up.
  // VideoToolBox has no such ability so ignore it.
  m_DropPictures = bDrop;
}

int CDVDVideoCodecVideoToolBox::Decode(uint8_t* pData, int iSize, double dts, double pts)
{
  if (m_codecControlFlags & DVD_CODEC_CTRL_DRAIN)
  {
    if (m_queue_depth > 0)
      return VC_PICTURE;
    else
      return VC_BUFFER;
  }

  if (pData)
  {
    if (m_bitstream)
    {
      m_bitstream->Convert(pData, iSize);
      iSize = m_bitstream->GetConvertSize();
      pData = m_bitstream->GetConvertBuffer();
    }
    ValidateVTSessionParameterSetsForRestart(pData, iSize);

    // exclude AV_CODEC_ID_H265 until we update CBitstreamParser::HasKeyframe
    if (CBitstreamParser::HasKeyframe(m_hints.codec, pData, iSize, false))
    {
      // VideoToolBox is picky about starting up with a keyframe
      // Check and skip until we hit one. m_lastKeyframe tracks how many frames back
      // was the last one. It is used during reset and reopen.
      //CLog::Log(LOGDEBUG, "%s - Keyframe found, m_lastKeyframe %d", __FUNCTION__, m_lastKeyframe);
      m_started = true;
      m_lastKeyframe = 0;
    }
    m_lastKeyframe++;

    if (!m_started && m_lastKeyframe < 64)
      return VC_BUFFER;

    CMSampleTimingInfo sampleTimingInfo = kCMTimingInfoInvalid;
    if (dts != DVD_NOPTS_VALUE)
      sampleTimingInfo.decodeTimeStamp = CMTimeMake(dts, DVD_TIME_BASE);
    if (pts != DVD_NOPTS_VALUE)
      sampleTimingInfo.presentationTimeStamp = CMTimeMake(pts, DVD_TIME_BASE);

    CMSampleBufferRef sampleBuff = CreateSampleBufferFrom(m_fmt_desc, sampleTimingInfo, pData, iSize);
    if (!sampleBuff)
    {
      CLog::Log(LOGNOTICE, "%s - CreateSampleBufferFrom failed", __FUNCTION__);
      return VC_ERROR;
    }
    
    VTDecodeFrameFlags decoderFlags = 0;
    if (m_enable_temporal_processing)
      decoderFlags |= kVTDecodeFrame_EnableTemporalProcessing;

    OSStatus status = VTDecompressionSessionDecodeFrame((VTDecompressionSessionRef)m_vt_session, sampleBuff, decoderFlags, nullptr, nullptr);
    if (status != noErr)
    {
      CFRelease(sampleBuff);
      // might not really be an error (could have been force inactive) so do not log it.
      if (status == kVTInvalidSessionErr)
      {
        m_sessionRestart = true;
        m_sessionRestartPTS = pts;
        if (m_display_queue)
        {
          m_sessionRestartPTS = m_display_queue->pts;
        }
        return VC_REOPEN;
      }

      if (status == kVTVideoDecoderMalfunctionErr)
      {
        CLog::Log(LOGNOTICE, "%s - VTDecompressionSessionDecodeFrame returned kVTVideoDecoderMalfunctionErr", __FUNCTION__);
        return VC_SWFALLBACK;
      }
      else
      {
        // VTDecompressionSessionDecodeFrame returned 8969 (codecBadDataErr)
        // VTDecompressionSessionDecodeFrame returned -12350
        // VTDecompressionSessionDecodeFrame returned -12902 (kVTParameterErr)
        // VTDecompressionSessionDecodeFrame returned -12903 (kVTInvalidSessionErr)
        // VTDecompressionSessionDecodeFrame returned -12909 (kVTVideoDecoderBadDataErr)
        // VTDecompressionSessionDecodeFrame returned -12911 (kVTVideoDecoderMalfunctionErr)
        CLog::Log(LOGNOTICE, "%s - VTDecompressionSessionDecodeFrame returned(%d)", __FUNCTION__, (int)status);
        return VC_ERROR;
      }
    }

/*
    // wait for decoding to finish
    status = VTDecompressionSessionWaitForAsynchronousFrames((VTDecompressionSessionRef)m_vt_session);
    if (status != kVTDecoderNoErr)
    {
      CLog::Log(LOGNOTICE, "%s - VTDecompressionSessionWaitForAsynchronousFrames returned(%d)",
        __FUNCTION__, (int)status);
      CFRelease(sampleBuff);
      return VC_ERROR;
    }
*/
    CFRelease(sampleBuff);

    // put a limit on convergence count to avoid
    // huge mem usage on streams without keyframes
    if (m_lastKeyframe > 300)
    {
      CLog::Log(LOGNOTICE, "%s - m_lastKeyframe (%i) clamped ", __FUNCTION__, m_lastKeyframe);
      m_lastKeyframe = 300;
    }
  }

  if (m_queue_depth < (2 * m_max_ref_frames))
    return VC_BUFFER;

  return VC_PICTURE;
}

void CDVDVideoCodecVideoToolBox::Reset(void)
{
  // flush decoder
  VTDecompressionSessionWaitForAsynchronousFrames((VTDecompressionSessionRef)m_vt_session);

  while (m_queue_depth)
    DisplayQueuePop();
  
  m_codecControlFlags = 0;
}

unsigned CDVDVideoCodecVideoToolBox::GetConvergeCount()
{
  return m_lastKeyframe;
}

unsigned CDVDVideoCodecVideoToolBox::GetAllowedReferences()
{
  return 5;
}

void CDVDVideoCodecVideoToolBox::SetCodecControl(int flags)
{
  m_codecControlFlags = flags;
}

bool CDVDVideoCodecVideoToolBox::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
  // clone the video picture buffer settings.
  *pDvdVideoPicture = m_videobuffer;

  // get the top picture frame, we risk getting the wrong frame if the frame queue
  // depth is less than the number of encoded reference frames. If queue depth
  // is greater than the number of encoded reference frames, then the top frame
  // will never change and we can just grab a ref to the top frame. This way
  // we don't lockout the vdadecoder while doing color format convert.
  pthread_mutex_lock(&m_queue_mutex);
  pDvdVideoPicture->dts             = DVD_NOPTS_VALUE;
  pDvdVideoPicture->pts             = m_display_queue->pts;
  pDvdVideoPicture->iFlags          = DVP_FLAG_ALLOCATED;
  pDvdVideoPicture->iWidth          = (unsigned int)m_display_queue->width;
  pDvdVideoPicture->iHeight         = (unsigned int)m_display_queue->height;
  pDvdVideoPicture->iDisplayWidth   = (unsigned int)m_display_queue->width;
  pDvdVideoPicture->iDisplayHeight  = (unsigned int)m_display_queue->height;
  pDvdVideoPicture->cvBufferRef     = m_display_queue->pixel_buffer_ref;
  m_display_queue->pixel_buffer_ref = nullptr;
  pthread_mutex_unlock(&m_queue_mutex);

  // now we can pop the top frame
  DisplayQueuePop();

  static double old_pts;
  if (g_advancedSettings.CanLogComponent(LOGVIDEO) && pDvdVideoPicture->pts < old_pts)
    CLog::Log(LOGDEBUG, "%s - VTBDecoderDecode dts(%f), pts(%f), old_pts(%f)", __FUNCTION__,
      pDvdVideoPicture->dts, pDvdVideoPicture->pts, old_pts);
  old_pts = pDvdVideoPicture->pts;
  
//  CLog::Log(LOGDEBUG, "%s - VTBDecoderDecode dts(%f), pts(%f), cvBufferRef(%p)", __FUNCTION__,
//    pDvdVideoPicture->dts, pDvdVideoPicture->pts, pDvdVideoPicture->cvBufferRef);

  if (m_codecControlFlags & DVD_CODEC_CTRL_DROP)
    pDvdVideoPicture->iFlags |= DVP_FLAG_DROPPED;

  // if vtb session restarts, we start decoding at last IDR frame
  // but dvdplay/renderer will show frames in fast forward style
  // until we hit sync point. Visually anoying so we force those
  // frames to get dropped and not shown.
  if (m_sessionRestart)
  {
    if (m_sessionRestartPTS == pDvdVideoPicture->pts)
      m_sessionRestart = false;
    else
      pDvdVideoPicture->iFlags |= DVP_FLAG_DROPPED;
  }

  return true;
}

bool CDVDVideoCodecVideoToolBox::ClearPicture(DVDVideoPicture* pDvdVideoPicture)
{
  // release any previous retained image buffer ref that
  // has not been passed up to renderer (ie. dropped frames, etc).
  if (pDvdVideoPicture->cvBufferRef)
    CVBufferRelease(pDvdVideoPicture->cvBufferRef);

  return CDVDVideoCodec::ClearPicture(pDvdVideoPicture);
}

void CDVDVideoCodecVideoToolBox::DisplayQueuePop(void)
{
  if (!m_display_queue || m_queue_depth == 0)
    return;

  // pop the top frame off the queue
  pthread_mutex_lock(&m_queue_mutex);
  frame_queue *top_frame = m_display_queue;
  m_display_queue = m_display_queue->nextframe;
  m_queue_depth--;
  pthread_mutex_unlock(&m_queue_mutex);

  // and release it
  if (top_frame->pixel_buffer_ref)
    CVBufferRelease(top_frame->pixel_buffer_ref);
  free(top_frame);
}

bool
CDVDVideoCodecVideoToolBox::CreateParameterSetArraysFromExtraData()
{
  bool rtn = false;
  switch (m_hints.codec)
  {
    default:
    break;
    case AV_CODEC_ID_H264:
    {
      uint8_t *ps_ptr = m_bitstream->GetExtraData();
      ps_ptr += 5; // skip over fixed length header
      size_t numberOfSPSs = 0x001F & *ps_ptr++; // sps count is lower five bits;

      size_t parameterSetIndex = 0;

      m_parameterSetCount = numberOfSPSs;
      m_parameterSetSizes = (size_t*)malloc(sizeof(size_t*) * m_parameterSetCount);
      m_parameterSetTypes = (uint8_t*)malloc(sizeof(uint8_t*) * m_parameterSetCount);
      m_parameterSetPointers = (uint8_t**)malloc(sizeof(uint8_t*) * m_parameterSetCount);
      // handle sps's
      for (size_t i = 0; i < numberOfSPSs; i++)
      {
        uint32_t ps_size = BS_RB16(ps_ptr);
        ps_ptr += 2;
        m_parameterSetSizes[parameterSetIndex] = ps_size;
        m_parameterSetTypes[parameterSetIndex] = AVC_NAL_SPS;
        m_parameterSetPointers[parameterSetIndex++] = ps_ptr;
        ps_ptr += ps_size;
      }

      size_t numberOfPPSs = *ps_ptr++;
      m_parameterSetCount += numberOfPPSs;
      m_parameterSetSizes = (size_t*)realloc(m_parameterSetSizes, sizeof(size_t*) * m_parameterSetCount);
      m_parameterSetTypes = (uint8_t*)realloc(m_parameterSetTypes, sizeof(uint8_t*) * m_parameterSetCount);
      m_parameterSetPointers = (uint8_t**)realloc(m_parameterSetPointers, sizeof(uint8_t*) * m_parameterSetCount);
      for (size_t i = 0; i < numberOfPPSs; i++)
      {
        uint32_t ps_size = BS_RB16(ps_ptr);
        ps_ptr += 2;
        m_parameterSetSizes[parameterSetIndex] = ps_size;
        m_parameterSetTypes[parameterSetIndex] = AVC_NAL_PPS;
        m_parameterSetPointers[parameterSetIndex++] = ps_ptr;
        ps_ptr += ps_size;
      }
      // h264 only requires sps
      if (m_parameterSetCount >= 1)
        rtn = true;
    }
    break;
    case AV_CODEC_ID_HEVC:
    {
      uint8_t *ps_ptr = m_bitstream->GetExtraData();
      ps_ptr += 22; // skip over fixed length header

      // number of arrays
      size_t numberOfArrays = *ps_ptr++;
      m_parameterSetCount = 0;
      m_parameterSetSizes = (size_t*)malloc(sizeof(size_t*) * m_parameterSetCount);
      m_parameterSetTypes = (uint8_t*)malloc(sizeof(uint8_t*) * m_parameterSetCount);
      m_parameterSetPointers = (uint8_t**)malloc(sizeof(uint8_t*) * m_parameterSetCount);

      size_t NALIndex = 0;
      for (size_t i = 0; i < numberOfArrays; i++)
      {
        // bit(1) array_completeness;
        // bit(1) reserved = 0;
        // bit(6) NAL_unit_type;
        int nal_type = 0x3F & *ps_ptr++;
        size_t numberOfNALs = BS_RB16(ps_ptr);
        ps_ptr += 2;
        m_parameterSetCount += numberOfNALs;
        m_parameterSetSizes = (size_t*)realloc(m_parameterSetSizes, sizeof(size_t*) * m_parameterSetCount);
        m_parameterSetTypes = (uint8_t*)realloc(m_parameterSetTypes, sizeof(uint8_t*) * m_parameterSetCount);
        m_parameterSetPointers = (uint8_t**)realloc(m_parameterSetPointers, sizeof(uint8_t*) * m_parameterSetCount);
        for (size_t i = 0; i < numberOfNALs; i++)
        {
          uint32_t ps_size = BS_RB16(ps_ptr);
          ps_ptr += 2;
          m_parameterSetSizes[NALIndex] = ps_size;
          m_parameterSetTypes[NALIndex] = nal_type;
          m_parameterSetPointers[NALIndex++] = ps_ptr;
          ps_ptr += ps_size;
        }
      }
      // h265 requires sps, pps and vps
      if (m_parameterSetCount >= 3)
        rtn = true;
    }
    break;
  }

  if (rtn)
  {
    // copy over parameter sets and setup pointers to them
    int totalsize = 0;
    for (size_t i = 0; i < m_parameterSetCount; i++)
      totalsize += m_parameterSetSizes[i];
    m_SavedParameterSets = (uint8_t*)malloc(totalsize);
    uint8_t *savedParameterSets = m_SavedParameterSets;
    for (size_t i = 0; i < m_parameterSetCount; i++)
    {
      memcpy((void*)savedParameterSets, (void*)m_parameterSetPointers[i], m_parameterSetSizes[i]);
      // change ps pointer from pointing to extradata to our saved location
      m_parameterSetPointers[i] = savedParameterSets;
      savedParameterSets += m_parameterSetSizes[i];
    }
  }

  return rtn;
}

bool
CDVDVideoCodecVideoToolBox::CreateFormatDescriptorFromParameterSetArrays()
{
  bool rtn = false;
  OSStatus status = -1;
  switch (m_hints.codec)
  {
    default:
    break;
    case AV_CODEC_ID_H264:
    {
      CLog::Log(LOGNOTICE, "Constructing new format description");
      status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
        m_parameterSetCount, m_parameterSetPointers, m_parameterSetSizes, 4, &m_fmt_desc);
      if (status != noErr)
        CLog::Log(LOGERROR, "%s - CMVideoFormatDescriptionCreateFromH264ParameterSets failed status(%d)", __FUNCTION__, status);
    }
    break;
    case AV_CODEC_ID_HEVC:
    {
      CLog::Log(LOGNOTICE, "Constructing new format description");
      // check availability at runtime
      if (__builtin_available(macOS 10.13, ios 11, tvos 11, *))
      {
        status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(kCFAllocatorDefault,
          m_parameterSetCount, m_parameterSetPointers, m_parameterSetSizes, (size_t)4, nullptr, &m_fmt_desc);
      }
      if (status != noErr)
        CLog::Log(LOGERROR, "%s - CMVideoFormatDescriptionCreateFromHEVCParameterSets failed status(%d)", __FUNCTION__, status);
    }
    break;
  }

  if (status == noErr && m_fmt_desc != nullptr)
  {
    const Boolean useCleanAperture = true;
    const Boolean usePixelAspectRatio = false;
    auto videoSize = CMVideoFormatDescriptionGetPresentationDimensions(m_fmt_desc, usePixelAspectRatio, useCleanAperture);

    m_hints.width = videoSize.width;
    m_hints.height = videoSize.height;
    rtn = true;
  }

  return rtn;
}

bool
CDVDVideoCodecVideoToolBox::ValidateVTSessionParameterSetsForRestart(uint8_t *pData, int iSize)
{
  // temp bypass of hevc
  if (m_hints.codec == AV_CODEC_ID_HEVC)
    return true;

  bool rtn = true;

  // pData is in bit stream format, 32 size (big endian), followed by NAL
  size_t parameterSetCount = 0;
  uint8_t *data = pData;
  uint8_t *dataEnd = data + iSize;
  // do a quick search for parameter sets in this frame
  while (data < dataEnd)
  {
    int nal_size = BS_RB32(data);
    data += 4;
    if (m_hints.codec == AV_CODEC_ID_H264)
    {
      int nal_type = data[0] & 0x1f;
      switch(nal_type)
      {
        case AVC_NAL_SPS:
          parameterSetCount++;
          //CLog::Log(LOGDEBUG, "%s - found sps of size(%d)", __FUNCTION__, nal_size);
        break;
        case AVC_NAL_PPS:
          parameterSetCount++;
          //CLog::Log(LOGDEBUG, "%s - found pps of size(%d)", __FUNCTION__, nal_size);
        break;
      }
    }
    else if (m_hints.codec == AV_CODEC_ID_HEVC)
    {
      int nal_type = (data[0] >> 1) & 0x3f;
      switch(nal_type)
      {
        case HEVC_NAL_SPS:
          parameterSetCount++;
          //CLog::Log(LOGDEBUG, "%s - found sps of size(%d)", __FUNCTION__, nal_size);
        break;
        case HEVC_NAL_PPS:
          parameterSetCount++;
          //CLog::Log(LOGDEBUG, "%s - found pps of size(%d)", __FUNCTION__, nal_size);
        break;
        case HEVC_NAL_VPS:
          parameterSetCount++;
          //CLog::Log(LOGDEBUG, "%s - found vps of size(%d)", __FUNCTION__, nal_size);
        break;
      }
    }
    data += nal_size;
  }
  // found some parameter sets, now we can compare them to the original parameter sets
  // we do it this way to avoid churning memory with malloc/reallocs for each frame
  if (parameterSetCount)
  {
    size_t parameterSetIndex = 0;
    size_t parameterSetSizes[parameterSetCount];
    uint8_t parameterSetTypes[parameterSetCount];
    uint8_t *parameterSetPointers[parameterSetCount];
    data = pData;
    while (data < dataEnd)
    {
      int nal_size = BS_RB32(data);
      data += 4;
      if (m_hints.codec == AV_CODEC_ID_H264)
      {
        int nal_type = data[0] & 0x1f;
        switch(nal_type)
        {
          case AVC_NAL_SPS:
          case AVC_NAL_PPS:
            parameterSetSizes[parameterSetIndex] = nal_size;
            parameterSetTypes[parameterSetIndex] = nal_type;
            parameterSetPointers[parameterSetIndex++] = data;
        }
      }
      else if (m_hints.codec == AV_CODEC_ID_HEVC)
      {
        int nal_type = (data[0] >> 1) & 0x3f;
        switch(nal_type)
        {
          case HEVC_NAL_SPS:
          case HEVC_NAL_PPS:
          case HEVC_NAL_VPS:
            parameterSetSizes[parameterSetIndex] = nal_size;
            parameterSetTypes[parameterSetIndex] = nal_type;
            parameterSetPointers[parameterSetIndex++] = data;
        }
      }
      data += nal_size;
    }

    // quick test of parameter set count
    if (parameterSetCount != m_parameterSetCount)
    {
      rtn = ResetVTSession(parameterSetCount, parameterSetSizes, parameterSetTypes, parameterSetPointers);
    }
    else
    {
      for (size_t i = 0; i < parameterSetCount; i++)
      {
        if (parameterSetTypes[i] == AVC_NAL_PPS)
          continue;

        if (parameterSetTypes[i] == HEVC_NAL_PPS)
          continue;

        if (parameterSetSizes[i] != m_parameterSetSizes[i])
        {
          // some parameter size changed, recreate the vtsession
          CLog::Log(LOGDEBUG, "%s - ps type(%d), changed size(%zu), orignal size(%zu)", __FUNCTION__, parameterSetTypes[i], parameterSetSizes[i], m_parameterSetSizes[i]);
          rtn = ResetVTSession(parameterSetCount, parameterSetSizes, parameterSetTypes, parameterSetPointers);
          break;
        }
        if (parameterSetTypes[i] == parameterSetTypes[i])
        {
          if (0 != memcmp(parameterSetPointers[i], m_parameterSetPointers[i], parameterSetSizes[i]))
          {
            // some parameter size changed, recreate the vtsession
            CLog::Log(LOGDEBUG, "%s - ps type(%d), contents differ size(%zu), orignal size(%zu)", __FUNCTION__, parameterSetTypes[i], parameterSetSizes[i], m_parameterSetSizes[i]);
            CLog::MemDump((char*)parameterSetPointers[i], parameterSetSizes[i]);
            CLog::MemDump((char*)m_parameterSetPointers[i], parameterSetSizes[i]);
            rtn = ResetVTSession(parameterSetCount, parameterSetSizes, parameterSetTypes, parameterSetPointers);
            break;
          }
        }
      }
    }
  }

  return rtn;
}

bool
CDVDVideoCodecVideoToolBox::ResetVTSession(size_t count, size_t *sizes, uint8_t *types, uint8_t **pointers)
{
  m_parameterSetCount = 0;
  free(m_parameterSetSizes), m_parameterSetSizes = nullptr;
  free(m_parameterSetTypes), m_parameterSetTypes = nullptr;
  free(m_SavedParameterSets), m_SavedParameterSets = nullptr;
  free(m_parameterSetPointers), m_parameterSetPointers = nullptr;

  // parameter set changed, recreate the vtsession
  m_parameterSetCount = count;
  m_parameterSetSizes = (size_t*)malloc(sizeof(size_t*) * m_parameterSetCount);
  m_parameterSetTypes = (uint8_t*)malloc(sizeof(uint8_t*) * m_parameterSetCount);
  m_parameterSetPointers = (uint8_t**)malloc(sizeof(uint8_t*) * m_parameterSetCount);
  int totalsize = 0;
  for (size_t i = 0; i < m_parameterSetCount; i++)
  {
    totalsize += sizes[i];
    m_parameterSetSizes[i] = sizes[i];
    m_parameterSetTypes[i] = types[i];
    m_parameterSetPointers[i] = pointers[i];
  }
  m_SavedParameterSets = (uint8_t*)malloc(totalsize);
  // copy over parameter sets and setup pointers to them
  uint8_t *savedParameterSets = m_SavedParameterSets;
  for (size_t i = 0; i < count; i++)
  {
    memcpy((void*)savedParameterSets, (void*)pointers[i], sizes[i]);
    // change ps pointer from pointing to ps in frame to our saved location
    m_parameterSetPointers[i] = savedParameterSets;
    savedParameterSets += m_parameterSetSizes[i];
  }

  if (m_fmt_desc)
    CFRelease(m_fmt_desc), m_fmt_desc = nullptr;
  if (!CreateFormatDescriptorFromParameterSetArrays())
    return false;

  DestroyVTSession();
  if (!CreateVTSessionAndInitPictureFrame())
    return false;

  CLog::Log(LOGDEBUG, "%s - width(%d), height(%d)", __FUNCTION__, m_hints.width, m_hints.height);
  return true;
}

bool
CDVDVideoCodecVideoToolBox::CreateVTSessionAndInitPictureFrame()
{
  int width = m_hints.width;
  int height = m_hints.height;
#if defined(TARGET_DARWIN_TVOS)
  if (std::string(CDarwinUtils::getIosPlatformString()) == "AppleTV5,3")
  {
    // decoding, scaling and rendering 4k h264 runs into
    // some bandwidth limit. detect and scale down to reduce
    // the bandwidth requirements.
    int width_clamp = 1920;
    int new_width = CheckNP2(width);
    if (width != new_width)
    {
      // force picture width to power of two and scale up height
      // we do this because no GL_UNPACK_ROW_LENGTH in OpenGLES
      // and the CVPixelBufferPixel gets created using some
      // strange alignment when width is non-standard.
      double w_scaler = (double)new_width / width;
      width = new_width;
      height = height * w_scaler;
    }
    // scale output pictures down to 1080p size for display
    if (width > width_clamp)
    {
      double w_scaler = (float)width_clamp / width;
      width = width_clamp;
      height = height * w_scaler;
    }
  }
#elif defined(TARGET_DARWIN_IOS)
  double scale = 0.0;

  // decoding, scaling and rendering above 1920 x 800 runs into
  // some bandwidth limit. detect and scale down to reduce
  // the bandwidth requirements.
  int width_clamp = 1280;
  if ((width * height) > (1920 * 800))
    width_clamp = 960;

  // for retina devices it should be safe [tm] to
  // loosen the clamp a bit to 1280 pixels width
  if (CDarwinUtils::DeviceHasRetina(scale))
    width_clamp = 1280;

  int new_width = CheckNP2(width);
  if (width != new_width)
  {
    // force picture width to power of two and scale up height
    // we do this because no GL_UNPACK_ROW_LENGTH in OpenGLES
    // and the CVPixelBufferPixel gets created using some
    // strange alignment when width is non-standard.
    double w_scaler = (double)new_width / width;
    width = new_width;
    height = height * w_scaler;
  }
#if !defined(__LP64__)
  // scale output pictures down to 720p size for display
  if (width > width_clamp)
  {
    double w_scaler = (float)width_clamp / width;
    width = width_clamp;
    height = height * w_scaler;
  }
#endif
#endif
  CFMutableDictionaryRef destinationPixelBufferAttributes = CFDictionaryCreateMutable(
    nullptr, // CFAllocatorRef allocator
    0,    // CFIndex capacity
    &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks);

#if defined(TARGET_DARWIN_OSX)
  CFDictionarySetSInt32(destinationPixelBufferAttributes,
    kCVPixelBufferPixelFormatTypeKey, kCVPixelFormatType_422YpCbCr8);
#else
  CFDictionarySetSInt32(destinationPixelBufferAttributes,
    kCVPixelBufferPixelFormatTypeKey, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
#endif

  CFDictionarySetSInt32(destinationPixelBufferAttributes,
    kCVPixelBufferWidthKey, width);
  CFDictionarySetSInt32(destinationPixelBufferAttributes,
    kCVPixelBufferHeightKey, height);
  CFDictionarySetValue(destinationPixelBufferAttributes,
    kCVPixelBufferIOSurfacePropertiesKey, kCFBooleanTrue);
#if defined(TARGET_DARWIN_OSX)
  CFDictionarySetValue(destinationPixelBufferAttributes,
    kCVPixelBufferOpenGLTextureCacheCompatibilityKey, kCFBooleanTrue);
#endif
#if defined(TARGET_DARWIN_IOS)
  CFDictionarySetValue(destinationPixelBufferAttributes,
    kCVPixelBufferOpenGLESCompatibilityKey, kCFBooleanTrue);
  CFDictionarySetValue(destinationPixelBufferAttributes,
    kCVPixelBufferOpenGLESTextureCacheCompatibilityKey, kCFBooleanTrue);
#endif
  CFMutableDictionaryRef io_surface_properties = CFDictionaryCreateMutable(kCFAllocatorDefault,
    0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(destinationPixelBufferAttributes, kCVPixelBufferIOSurfacePropertiesKey, io_surface_properties);

  VTDecompressionOutputCallbackRecord outputCallback;
  outputCallback.decompressionOutputCallback = VTDecoderCallback;
  outputCallback.decompressionOutputRefCon = this;

  VTDecompressionSessionRef vt_session = nullptr;
  OSStatus status = VTDecompressionSessionCreate(
    nullptr, // CFAllocatorRef allocator
    m_fmt_desc,
    nullptr, // CFTypeRef sessionOptions
    destinationPixelBufferAttributes,
    &outputCallback,
    &vt_session);
  if (status != noErr)
  {
    m_vt_session = nullptr;
    CLog::Log(LOGERROR, "%s - failed with status = (%d)", __FUNCTION__, (int)status);
    // -12906, kVTCouldNotFindVideoDecoderErr
  }
  else
  {
    m_vt_session = (void*)vt_session;
  }

  CFRelease(io_surface_properties);
  CFRelease(destinationPixelBufferAttributes);

  // setup a DVDVideoPicture buffer.
  // first make sure all properties are reset.
  memset(&m_videobuffer, 0, sizeof(DVDVideoPicture));

  m_videobuffer.dts = DVD_NOPTS_VALUE;
  m_videobuffer.pts = DVD_NOPTS_VALUE;
  m_videobuffer.format = RENDER_FMT_CVBREF;
  m_videobuffer.color_range  = 0;
  m_videobuffer.color_matrix = 4;
  m_videobuffer.iFlags  = DVP_FLAG_ALLOCATED;
  m_videobuffer.iWidth  = m_hints.width;
  m_videobuffer.iHeight = m_hints.height;
  m_videobuffer.iDisplayWidth  = m_hints.width;
  m_videobuffer.iDisplayHeight = m_hints.height;

  return m_vt_session != nullptr;
}

void
CDVDVideoCodecVideoToolBox::DestroyVTSession()
{
  if (m_vt_session)
  {
    // Prevent deadlocks in VTDecompressionSessionInvalidate by waiting for the frames to complete manually.
    // Seems to have appeared in iOS11
    VTDecompressionSessionWaitForAsynchronousFrames((VTDecompressionSessionRef)m_vt_session);
    VTDecompressionSessionInvalidate((VTDecompressionSessionRef)m_vt_session);
    CFRelease((VTDecompressionSessionRef)m_vt_session);
    m_vt_session = nullptr;
  }
}

typedef void (*VTDecompressionOutputCallback)(
		void * CM_NULLABLE decompressionOutputRefCon,
		void * CM_NULLABLE sourceFrameRefCon,
		OSStatus status, 
		VTDecodeInfoFlags infoFlags,
		CM_NULLABLE CVImageBufferRef imageBuffer,
		CMTime presentationTimeStamp, 
		CMTime presentationDuration );

void
CDVDVideoCodecVideoToolBox::VTDecoderCallback(
  void              *refcon,
  void              *frameInfo,
  OSStatus           status,
  VTDecodeInfoFlags  infoFlags,
  CVBufferRef        imageBuffer,
  CMTime             pts,
  CMTime             duration)
{
  // This is an sync callback due to VTDecompressionSessionWaitForAsynchronousFrames
  CDVDVideoCodecVideoToolBox *ctx = (CDVDVideoCodecVideoToolBox*)refcon;
  if (status != noErr)
  {
    //CLog::Log(LOGDEBUG, "%s - status error (%d)", __FUNCTION__, (int)status);
    return;
  }
  if (imageBuffer == nullptr)
  {
    //CLog::Log(LOGDEBUG, "%s - imageBuffer is nullptr", __FUNCTION__);
    return;
  }

  /*
  // https://bugzilla.gnome.org/show_bug.cgi?id=728435
  // ignore the dropped flag if buffer was received
  if (kVTDecodeInfo_FrameDropped & infoFlags)
  {
    if (g_advancedSettings.CanLogComponent(LOGVIDEO))
      CLog::Log(LOGDEBUG, "%s - frame dropped", __FUNCTION__);
    return;
  }
  */

  // allocate a new frame and populate it with some information.
  // this pointer to a frame_queue type keeps track of the newest decompressed frame
  // and is then inserted into a linked list of frame pointers depending on the display time (pts)
  // parsed out of the bitstream and stored in the frameInfo dictionary by the client
  frame_queue *newFrame = (frame_queue*)calloc(sizeof(frame_queue), 1);
  newFrame->nextframe = nullptr;
  if (CVPixelBufferIsPlanar(imageBuffer) )
  {
    newFrame->width  = CVPixelBufferGetWidthOfPlane(imageBuffer, 0);
    newFrame->height = CVPixelBufferGetHeightOfPlane(imageBuffer, 0);
  }
  else
  {
    newFrame->width  = CVPixelBufferGetWidth(imageBuffer);
    newFrame->height = CVPixelBufferGetHeight(imageBuffer);
  }  
  newFrame->pixel_buffer_format = CVPixelBufferGetPixelFormatType(imageBuffer);;
  newFrame->pixel_buffer_ref = CVBufferRetain(imageBuffer);
  newFrame->pts = (double)pts.value;

  // since the frames we get may be in decode order rather than presentation order
  // our hypothetical callback places them in a queue of frames which will
  // hold them in display order for display on another thread
  pthread_mutex_lock(&ctx->m_queue_mutex);

  frame_queue base;
  base.nextframe = ctx->m_display_queue;
  frame_queue *ptr = &base;
  for(; ptr->nextframe; ptr = ptr->nextframe)
  {
    if (ptr->nextframe->pts == DVD_NOPTS_VALUE || newFrame->pts == DVD_NOPTS_VALUE)
      continue;
    if (ptr->nextframe->pts > newFrame->pts)
      break;
  }
  // insert after ptr
  newFrame->nextframe = ptr->nextframe;
  ptr->nextframe = newFrame;

  // update anchor if needed
  if (newFrame->nextframe == ctx->m_display_queue)
    ctx->m_display_queue = newFrame;

  ctx->m_queue_depth++;
  pthread_mutex_unlock(&ctx->m_queue_mutex);
}

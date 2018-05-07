/*
 *      Copyright (C) 2015 Team MrMC
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

#import "config.h"

#if defined(TARGET_DARWIN_IOS)
#import "cores/dvdplayer/DVDCodecs/Video/DVDVideoCodecAVFoundation.h"

#import "cores/dvdplayer/DVDClock.h"
#import "cores/dvdplayer/DVDStreamInfo.h"
#import "cores/VideoRenderers/RenderManager.h"
#import "platform/darwin/DarwinUtils.h"
#import "platform/darwin/ios-common/VideoLayerView.h"
#if defined(TARGET_DARWIN_TVOS)
#import "platform/darwin/tvos/MainController.h"
#else
#import "platform/darwin/ios/XBMCController.h"
#endif
#import "settings/Settings.h"
#import "settings/MediaSettings.h"
#import "utils/BitstreamConverter.h"
#import "utils/log.h"

// tracks pts in and output queue in display order
typedef struct pktTracker {
  double dts;
  double pts;
  size_t size;
} pktTracker;

static bool pktTrackerSortPredicate(const pktTracker* lhs, const pktTracker* rhs)
{
  if (lhs->dts != DVD_NOPTS_VALUE && rhs->dts != DVD_NOPTS_VALUE)
    return lhs->dts < rhs->dts;
  else if (lhs->pts != DVD_NOPTS_VALUE && rhs->pts != DVD_NOPTS_VALUE)
    return lhs->pts < rhs->pts;
  else
    return false;
}

// helper function to create a CMSampleBufferRef from demuxer data.
// the demuxer data is in accV format, length byte is already present.
static CMSampleBufferRef
CreateSampleBufferFrom(CMFormatDescriptionRef fmt_desc,
  CMSampleTimingInfo *timingInfo, void *demux_buff, size_t demux_size)
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
      timingInfo,         // const CMSampleTimingInfo *sampleTimingArray
      1,                  // CMItemCount numSampleSizeEntries
      sampleSizeArray,    // const size_t *sampleSizeArray
      &sBufOut);          // CMSampleBufferRef *sBufOut
  }
  CFRelease(videoBlock);

  return sBufOut;
}

enum AVMESSAGE
{
  ERROR = 0,
  NONE,
  RESET,
  START,
  PAUSE,
  PLAY,
};
class CAVFCodecMessage
{
public:
  CAVFCodecMessage()
  {
    pthread_mutex_init(&m_mutex, nullptr);
  }

 ~CAVFCodecMessage()
  {
    pthread_mutex_destroy(&m_mutex);
  }
  
  void enqueue(AVMESSAGE msg)
  {
    pthread_mutex_lock(&m_mutex);
    m_messages.push(msg);
    pthread_mutex_unlock(&m_mutex);
  }

  AVMESSAGE dequeue()
  {
    pthread_mutex_lock(&m_mutex);
    AVMESSAGE msg = m_messages.front();
    m_messages.pop();
    pthread_mutex_unlock(&m_mutex);
    return msg;
  }

  size_t size()
  {
    return m_messages.size();
  }
protected:
  pthread_mutex_t       m_mutex;
  std::queue<AVMESSAGE> m_messages;
};

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// This codec renders direct to a UIView/CALayer via AVSampleBufferLayer.
// DVDPlayer/VideoRenderer runs in bypass mode as we totally bypass them.
CDVDVideoCodecAVFoundation::CDVDVideoCodecAVFoundation()
: CDVDVideoCodec()
, CThread("CDVDVideoCodecAVFoundation")
, m_decoder(nullptr)
, m_pFormatName("avf-")
, m_speed(DVD_PLAYSPEED_NORMAL)
, m_bitstream(nullptr)
, m_withBlockRunning(false)
, m_messages(nullptr)
, m_framecount(0)
, m_fps(24000.0/1001.0)
, m_lastTrackingTS(DVD_NOPTS_VALUE)
{
  m_messages = new CAVFCodecMessage();
  memset(&m_videobuffer, 0, sizeof(DVDVideoPicture));
  pthread_mutex_init(&m_trackerQueueMutex, nullptr);
  pthread_mutex_init(&m_sampleBuffersMutex, nullptr);
  // the best way to feed the video layer.
  m_providerQueue = dispatch_queue_create("com.mrmc.avsm_providercallback", DISPATCH_QUEUE_SERIAL);
  dispatch_set_target_queue( m_providerQueue, dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_HIGH, 0 ) );
}

CDVDVideoCodecAVFoundation::~CDVDVideoCodecAVFoundation()
{
  Dispose();
}

bool CDVDVideoCodecAVFoundation::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEAVF) && !hints.software)
  {
    switch(hints.profile)
    {
      //case FF_PROFILE_H264_HIGH_10:
      case FF_PROFILE_H264_HIGH_10_INTRA:
      case FF_PROFILE_H264_HIGH_422:
      case FF_PROFILE_H264_HIGH_422_INTRA:
      case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
      case FF_PROFILE_H264_HIGH_444_INTRA:
      case FF_PROFILE_H264_CAVLC_444:
        CLog::Log(LOGNOTICE, "%s - unsupported h264 profile(%d)", __FUNCTION__, hints.profile);
        return false;
        break;
    }

    // some pvr livetv hole is bypassing normal hints.maybe_interlaced setup
    if (hints.codec == AV_CODEC_ID_H264)
    {
      CBitstreamConverter bs;
      if (!bs.Open(hints.codec, (uint8_t*)hints.extradata, hints.extrasize, false))
        return false;

      CFDataRef avcCData = CFDataCreate(kCFAllocatorDefault,
        (const uint8_t*)bs.GetExtraData(), bs.GetExtraSize());
      bool interlaced = true;
      int max_ref_frames;
      uint8_t *spc = (uint8_t*)CFDataGetBytePtr(avcCData) + 6;
      uint32_t sps_size = BS_RB16(spc);
      if (sps_size)
        bs.parseh264_sps(spc+3, sps_size-1, &interlaced, &max_ref_frames);
      CFRelease(avcCData);

      if (interlaced)
        hints.maybe_interlaced = true;
    }

    if (hints.maybe_interlaced)
    {
      CLog::Log(LOGNOTICE, "%s - interlaced content.", __FUNCTION__);
      return false;
    }

    if (hints.width <= 0 || hints.height <= 0)
    {
      CLog::Log(LOGNOTICE, "%s - bailing with bogus hints, width(%d), height(%d)",
        __FUNCTION__, hints.width, hints.height);
      return false;
    }

    m_width = hints.width;
    m_height = hints.height;
    m_codec = hints.codec;
    m_profile = hints.profile;
    if (hints.colorrange == AVCOL_RANGE_JPEG)
      m_colorrange = 1;
    else
      m_colorrange = 0;
    m_colorspace = hints.colorspace;
    m_colortransfer = hints.colortransfer;

    if (hints.fpsscale > 0 && hints.fpsrate > 0)
      m_fps = (double)hints.fpsrate / (double)hints.fpsscale;

    switch (hints.codec)
    {
      case AV_CODEC_ID_H264:
      {
        // we want avcC, not annex-b. use a bitstream converter for all flavors,
        // that way even avcC with silly 3-byte nals are covered.
        m_bitstream = new CBitstreamConverter;
        if (!m_bitstream->Open(hints.codec, (uint8_t*)hints.extradata, hints.extrasize, false))
          return false;

        // create a CMVideoFormatDescription from avcC extradata.
        // skip over avcC header (six bytes)
        uint8_t *spc_ptr = m_bitstream->GetExtraData() + 6;
        // length of sequence parameter set data
        uint32_t sps_size = BS_RB16(spc_ptr); spc_ptr += 2;
        // pointer to sequence parameter set data
        uint8_t *sps_ptr = spc_ptr; spc_ptr += sps_size;
        // number of picture parameter sets
        //uint32_t pps_cnt = *spc_ptr++;
        spc_ptr++;
        // length of picture parameter set data
        uint32_t pps_size = BS_RB16(spc_ptr); spc_ptr += 2;
        // pointer to picture parameter set data
        uint8_t *pps_ptr = spc_ptr;

        // check the avcC atom's sps for number of reference frames and
        // ignore if interlaced, it's handled in hints check above (until we get it working :)
        bool interlaced = true;
        int max_ref_frames = 0;
        if (sps_size)
          m_bitstream->parseh264_sps(sps_ptr+1, sps_size-1, &interlaced, &max_ref_frames);
        // default to 5 min, this helps us feed correct pts to the player.
        m_max_ref_frames = std::max(max_ref_frames + 1, 5);

        // bitstream converter avcC's always have 4 byte NALs.
        int nalUnitHeaderLength  = 4;
        size_t parameterSetCount = 2;
        const uint8_t* const parameterSetPointers[2] = {
          (const uint8_t*)sps_ptr, (const uint8_t*)pps_ptr };
        const size_t parameterSetSizes[2] = {
          sps_size, pps_size };
        OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
         parameterSetCount, parameterSetPointers, parameterSetSizes, nalUnitHeaderLength, &m_fmt_desc);
        if (status != noErr)
        {
          CLog::Log(LOGERROR, "%s - CMVideoFormatDescriptionCreateFromH264ParameterSets failed status(%d)", __FUNCTION__, status);
          SAFE_DELETE(m_bitstream);
          return false;
        }

        const Boolean useCleanAperture = true;
        const Boolean usePixelAspectRatio = false;
        auto videoSize = CMVideoFormatDescriptionGetPresentationDimensions(m_fmt_desc, usePixelAspectRatio, useCleanAperture);

        m_width = hints.width = videoSize.width;
        m_height = hints.height = videoSize.height;

        m_format = 'avc1';
        m_pFormatName = "avf-h264";
        m_dynamicrange = DVP_DYNAMIC_RANGE_SDR;
      }
      break;
      case AV_CODEC_ID_H265:
      {
        if (hints.extrasize < 23 || hints.extradata == NULL)
        {
          CLog::Log(LOGNOTICE, "%s - hvcC atom too data small or missing", __FUNCTION__);
          return false;
        }
        else
        {
         // use a bitstream converter for all flavors
          m_bitstream = new CBitstreamConverter;
          if (!m_bitstream->Open(hints.codec, (uint8_t*)hints.extradata, hints.extrasize, false))
          {
            SAFE_DELETE(m_bitstream);
            return false;
          }
          if (m_bitstream->GetExtraSize() < 23)
          {
            SAFE_DELETE(m_bitstream);
            return false;
          }

          uint8_t *ps_ptr = m_bitstream->GetExtraData();
          ps_ptr += 22; // skip over fixed length header

          // number of arrays
          size_t numberOfArrays = *ps_ptr++;
          size_t parameterSetCount = 0;
          size_t *parameterSetSizes = (size_t*)malloc(sizeof(size_t*) * parameterSetCount);
          uint8_t **parameterSetPointers = (uint8_t**)malloc(sizeof(uint8_t*) * parameterSetCount);

          size_t NALIndex = 0;
          for (size_t i = 0; i < numberOfArrays; i++)
          {
            // skip byte = array_completeness/reserved/NAL type
            ps_ptr++;
            size_t numberOfNALs = BS_RB16(ps_ptr);
            ps_ptr += 2;
            parameterSetCount += numberOfNALs;
            parameterSetSizes = (size_t*)realloc(parameterSetSizes, sizeof(size_t*) * parameterSetCount);
            parameterSetPointers = (uint8_t**)realloc(parameterSetPointers, sizeof(uint8_t*) * parameterSetCount);
            for (size_t i = 0; i < numberOfNALs; i++)
            {
              uint32_t ps_size = BS_RB16(ps_ptr);
              ps_ptr += 2;
              parameterSetSizes[NALIndex] = ps_size;
              parameterSetPointers[NALIndex++] = ps_ptr;
              ps_ptr += ps_size;
            }
          }

          OSStatus status = -1;
          CLog::Log(LOGNOTICE, "Constructing new format description");
          // check availability at runtime
          if (__builtin_available(macOS 10.13, ios 11, tvos 11, *))
          {
            status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(kCFAllocatorDefault,
              parameterSetCount, parameterSetPointers, parameterSetSizes, (size_t)4, nullptr, &m_fmt_desc);
          }
          free(parameterSetSizes);
          free(parameterSetPointers);

          if (status != noErr)
          {
            CLog::Log(LOGERROR, "%s - CMVideoFormatDescriptionCreateFromHEVCParameterSets failed status(%d)", __FUNCTION__, status);
            SAFE_DELETE(m_bitstream);
            return false;
          }
          const Boolean useCleanAperture = true;
          const Boolean usePixelAspectRatio = false;
          auto videoSize = CMVideoFormatDescriptionGetPresentationDimensions(m_fmt_desc, usePixelAspectRatio, useCleanAperture);

          m_width = hints.width = videoSize.width;
          m_height = hints.height = videoSize.height;
          m_max_ref_frames = 6;
          m_pFormatName = "avf-h265";
          // start with assuming SDR
          m_colorspace = hints.colorspace;
          m_colortransfer = hints.colortransfer;
          m_dynamicrange = DVP_DYNAMIC_RANGE_SDR;
          // HEVC Main 10 is always HDR10. Needs verify.
          if (hints.profile == FF_PROFILE_HEVC_MAIN_10 ||
              hints.profile == FF_PROFILE_HEVC_REXT)
          {
            if (hints.colortransfer >= AVCOL_TRC_SMPTE2084)
              m_dynamicrange = DVP_DYNAMIC_RANGE_HDR10;
          }
          // check for DolbyVision, hints.profile will be wrong
          // and we have to look at codec_tag
          if (hints.codec_tag == MKTAG('d','v','h','1') ||
              hints.codec_tag == MKTAG('d','v','h','e') ||
              hints.codec_tag == MKTAG('D','O','V','I'))
          {
            m_colorspace = AVCOL_SPC_BT2020_NCL; // BT2020_NCL (Non-Constant Luminance)
            m_dynamicrange = DVP_DYNAMIC_RANGE_DOLBYYVISION;
          }
        }
      }
      break;
     default:
        return false;
      break;
    }


    // VideoLayerView create MUST be done on main thread or
    // it will not get updates when a new video frame is decoded and presented.
    __block VideoLayerView *mcview = nullptr;
    dispatch_sync(dispatch_get_main_queue(),^{
      CGRect bounds = CGRectMake(0, 0, m_width, m_height);
      mcview = [[VideoLayerView alloc] initWithFrame:bounds];
      [g_xbmcController insertVideoView:mcview];
    });
    m_decoder = mcview;

    Create();
    g_renderManager.RegisterRenderFeaturesCallBack((const void*)this, RenderFeaturesCallBack);
    m_messages->enqueue(START);
    return true;
  }

  return false;
}

void CDVDVideoCodecAVFoundation::Dispose()
{
  StopThread();

  if (m_decoder)
  {
    StopSampleProvider();
    DrainQueues();

    m_providerQueue = nil;
    pthread_mutex_destroy(&m_trackerQueueMutex);
    pthread_mutex_destroy(&m_sampleBuffersMutex);

    dispatch_sync(dispatch_get_main_queue(),^{
      VideoLayerView *mcview = (VideoLayerView*)m_decoder;
      [g_xbmcController removeVideoView:mcview];
    });
    m_decoder = nullptr;
  }
  SAFE_DELETE(m_bitstream);
  SAFE_DELETE(m_messages);
}

int CDVDVideoCodecAVFoundation::Decode(uint8_t* pData, int iSize, double dts, double pts)
{
  if (m_codecControlFlags & DVD_CODEC_CTRL_DRAIN)
  {
    if (!m_trackerQueue.empty())
      return VC_PICTURE;
    else
      return VC_BUFFER;
  }

  if (pData)
  {
    int frameSize = iSize;
    uint8_t *frame = pData;
    if (m_bitstream->Convert(pData, iSize))
    {
      frameSize = m_bitstream->GetConvertSize();
      frame = m_bitstream->GetConvertBuffer();
    }

    //ProbeNALUnits(frame, frameSize);

    CMSampleTimingInfo sampleTimingInfo = kCMTimingInfoInvalid;
    if (m_fps > 0.0)
      sampleTimingInfo.duration = CMTimeMake((double)DVD_TIME_BASE / m_fps, DVD_TIME_BASE);
    if (dts != DVD_NOPTS_VALUE)
      sampleTimingInfo.decodeTimeStamp = CMTimeMake(dts, DVD_TIME_BASE);
    if (pts != DVD_NOPTS_VALUE)
      sampleTimingInfo.presentationTimeStamp = CMTimeMake(pts, DVD_TIME_BASE);

    CMSampleBufferRef sampleBuffer = CreateSampleBufferFrom(m_fmt_desc, &sampleTimingInfo, frame, frameSize);

    pthread_mutex_lock(&m_sampleBuffersMutex);
    m_sampleBuffers.push(sampleBuffer);
    pthread_mutex_unlock(&m_sampleBuffersMutex);

    m_dts = dts;
    m_pts = pts;

    pktTracker *tracker = new pktTracker;
    tracker->dts = dts;
    tracker->pts = pts;
    // want size as passed by player.
    tracker->size = iSize;

    pthread_mutex_lock(&m_trackerQueueMutex);
    m_trackerQueue.push_back(tracker);
    m_trackerQueue.sort(pktTrackerSortPredicate);

    if (m_trackerQueue.size() > (1.5 * m_max_ref_frames))
    {
      tracker = m_trackerQueue.front();
      if (tracker->dts != DVD_NOPTS_VALUE)
        UpdateFrameRateTracking(tracker->dts);
      else if (tracker->pts != DVD_NOPTS_VALUE)
        UpdateFrameRateTracking(tracker->pts);
    }
    pthread_mutex_unlock(&m_trackerQueueMutex);

    //DumpTrackingQueue();
    Sleep(5);
  }

  if (m_trackerQueue.size() < (2 * m_max_ref_frames))
    return VC_BUFFER;

  return VC_PICTURE;
}

void CDVDVideoCodecAVFoundation::Reset(void)
{
  m_messages->enqueue(RESET);
  m_messages->enqueue(START);
  m_lastTrackingTS = DVD_NOPTS_VALUE;
}

void CDVDVideoCodecAVFoundation::SetClock(CDVDClock *clock)
{
  m_clock = clock;
}

unsigned CDVDVideoCodecAVFoundation::GetAllowedReferences()
{
  return 3;
}

void CDVDVideoCodecAVFoundation::SetCodecControl(int flags)
{
  m_codecControlFlags = flags;
}


bool CDVDVideoCodecAVFoundation::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
  // need to fake the dts/pts to keep player happy so
  // force player to use duration to calc next fake pts
  pDvdVideoPicture->dts = DVD_NOPTS_VALUE;
  pDvdVideoPicture->pts = DVD_NOPTS_VALUE;
  if (m_fps > 0.0)
    pDvdVideoPicture->iDuration     = (double)DVD_TIME_BASE / m_fps;
  pDvdVideoPicture->format          = RENDER_FMT_BYPASS;
  pDvdVideoPicture->iFlags          = DVP_FLAG_ALLOCATED;
  if (m_codecControlFlags & DVD_CODEC_CTRL_DROP)
    pDvdVideoPicture->iFlags       |= DVP_FLAG_DROPPED;
  pDvdVideoPicture->color_range     = m_colorrange;
  pDvdVideoPicture->color_matrix    = m_colorspace;
  pDvdVideoPicture->color_transfer  = m_colortransfer;
  pDvdVideoPicture->dynamic_range   = m_dynamicrange;
  pDvdVideoPicture->iWidth          = m_width;
  pDvdVideoPicture->iHeight         = m_height;
  pDvdVideoPicture->iDisplayWidth   = pDvdVideoPicture->iWidth;
  pDvdVideoPicture->iDisplayHeight  = pDvdVideoPicture->iHeight;

  if (!m_trackerQueue.empty())
  {
    pthread_mutex_lock(&m_trackerQueueMutex);
    pktTracker *tracker = m_trackerQueue.front();
    m_trackerQueue.pop_front();
    pthread_mutex_unlock(&m_trackerQueueMutex);
    delete tracker;
  }

  return true;
}

void CDVDVideoCodecAVFoundation::SetDropState(bool bDrop)
{
  // more a message to decoder to hurry up.
  // AVSampleBufferDisplayLayer has no such ability so ignore it.
}

void CDVDVideoCodecAVFoundation::SetSpeed(int iSpeed)
{
  if (iSpeed == m_speed)
    return;

  switch(iSpeed)
  {
    case DVD_PLAYSPEED_PAUSE:
      m_messages->enqueue(PAUSE);
      break;
    default:
    case DVD_PLAYSPEED_NORMAL:
      m_messages->enqueue(PLAY);
      break;
  }
  m_speed = iSpeed;
}

void CDVDVideoCodecAVFoundation::Process()
{
  CLog::Log(LOGDEBUG, "CDVDVideoCodecAVFoundation::Process Started");

  // bump our priority to be level with the krAEken (ActiveAE)
  SetPriority(THREAD_PRIORITY_ABOVE_NORMAL);

  AVMESSAGE message = NONE;
  CRect oldSrcRect, oldDestRect, oldViewRect;

  while (!m_bStop)
  {
    if (m_messages->size())
      message = m_messages->dequeue();
    switch(message)
    {
      default:
      case NONE:
      // retain the last state and do nothing.
      break;

      case START: // we are just starting up.
      {
        while (!(GetPlayerClockSeconds() > 0.0))
        {
          if (m_bStop)
            break;
          Sleep(10);
        }
        // player clock returns < zero if reset.
        double player_s = GetPlayerClockSeconds();
        if (player_s > 0.0)
        {
          // startup with video timebase matching the player clock.
          dispatch_sync(dispatch_get_main_queue(),^{
            VideoLayerView *mcview = (VideoLayerView*)m_decoder;
            // video clock was stopped, set the starting time and crank it up.
            AVSampleBufferDisplayLayer *videolayer = (AVSampleBufferDisplayLayer*)mcview.layer;
            CMTimebaseSetTime(videolayer.controlTimebase, CMTimeMake(player_s, 1));
            CMTimebaseSetRate(videolayer.controlTimebase, 1.0);
          });
          message = NONE;
          m_messages->enqueue(PLAY);
          CLog::Log(LOGDEBUG, "%s - CDVDVideoCodecAVFoundation::Start player_s(%f)", __FUNCTION__, player_s);
        }
      }
      break;

      case RESET:
      {
        // just reset here, someone else will start us up again if needed.
        dispatch_sync(dispatch_get_main_queue(),^{
          // Flush the previous enqueued sample buffers for display while scrubbing
          DrainQueues();
          VideoLayerView *mcview = (VideoLayerView*)m_decoder;
          AVSampleBufferDisplayLayer *videolayer = (AVSampleBufferDisplayLayer*)mcview.layer;
          [videolayer flush];
          //CMTimebaseSetRate(videolayer.controlTimebase, 0.0);
          //m_withBlockRunning = false;
        });
        message = NONE;
        CLog::Log(LOGDEBUG, "%s - CDVDVideoCodecAVFoundation::Reset", __FUNCTION__);
      }
      break;

      case PAUSE:
      {
        // to pause, we just set the video timebase rate to zero.
        // buffers in flight are retained but not shown until the rate is non-zero.
        dispatch_sync(dispatch_get_main_queue(),^{
          VideoLayerView *mcview = (VideoLayerView*)m_decoder;
          AVSampleBufferDisplayLayer *videolayer = (AVSampleBufferDisplayLayer*)mcview.layer;
          CMTimebaseSetRate(videolayer.controlTimebase, 0.0);
        });
        CLog::Log(LOGDEBUG, "%s - CDVDVideoCodecAVFoundation::Pause", __FUNCTION__);
        message = NONE;
      }
      break;

      case PLAY:
      {
        VideoLayerView *mcview = (VideoLayerView*)m_decoder;
        dispatch_sync(dispatch_get_main_queue(),^{
          AVSampleBufferDisplayLayer *videolayer = (AVSampleBufferDisplayLayer*)mcview.layer;

          // check if the usingBlock is running, if not, start it up.
          if (!m_withBlockRunning && videolayer.readyForMoreMediaData == YES)
          {
              StartSampleProviderWithBlock();
              m_withBlockRunning = true;
          }

          // player clock returns < zero if reset. check it.
          double player_s = GetPlayerClockSeconds();
          if (player_s > 0.0)
          {
            CMTime cmtime  = CMTimebaseGetTime(videolayer.controlTimebase);
            Float64 timeBase_s = CMTimeGetSeconds(cmtime);

            // sync video layer time base to dvdplayer's player clock.
            double error = fabs(timeBase_s - player_s);
            if (error > 0.150)
            {
              //dispatch_sync(dispatch_get_main_queue(),^{
                CLog::Log(LOGDEBUG, "adjusting playback "
                  "timeBase_s(%f) player_s(%f), sampleBuffers(%lu), trackerQueue(%lu)",
                   timeBase_s, player_s, m_sampleBuffers.size(), m_trackerQueue.size());
                CMTimebaseSetTime(videolayer.controlTimebase, CMTimeMake(player_s, 1));
                CMTimebaseSetRate(videolayer.controlTimebase, 1.0);
              //});
            }
          }
        });

        // if renderer is configured, we now know size and
        // where to display the video.
        if (g_renderManager.IsConfigured())
        {
          CRect SrcRect, DestRect, ViewRect;
          g_renderManager.GetVideoRect(SrcRect, DestRect, ViewRect);
          // update where we show the video in the view/layer
          // once renderer inits, we only need to update if something changes.
          if (SrcRect  != oldSrcRect  ||
              DestRect != oldDestRect ||
              ViewRect != oldViewRect)
          {
            // g_renderManager lies, check for empty rects too.
            if (!SrcRect.IsEmpty() && !DestRect.IsEmpty() && !ViewRect.IsEmpty())
            {
              // this makes zero sense, what is really going on with video scaling under avsamplebufferdisplaylayer ?
              float realwidth  = [g_xbmcController getScreenSize].width  / g_xbmcController.m_screenScale;
              float realheight = [g_xbmcController getScreenSize].height / g_xbmcController.m_screenScale;
              CRect ScreenRect(0, 0, realwidth, realheight);
              CRect MappedRect = DestRect;
              MappedRect.MapRect(ViewRect, ScreenRect);

              // things that might touch iOS gui need to happen on main thread.
              dispatch_sync(dispatch_get_main_queue(),^{
                CGRect frame = CGRectMake(
                  MappedRect.x1, MappedRect.y1, MappedRect.Width(), MappedRect.Height());
                // save the offset
                CGPoint offset = frame.origin;
                // transform to zero x/y origin
                frame = CGRectOffset(frame, -frame.origin.x, -frame.origin.y);
                mcview.frame = frame;
                mcview.center= CGPointMake(mcview.center.x + offset.x, mcview.center.y + offset.y);
                // we startup hidden, show it now
                if (mcview.hidden == YES)
                  mcview.hidden = NO;
              });
              oldSrcRect  = SrcRect;
              oldDestRect = DestRect;
              oldViewRect = ViewRect;
            }
          }
        }
      }
      break;
    }

    Sleep(100);
  }

  SetPriority(THREAD_PRIORITY_NORMAL);
  CLog::Log(LOGDEBUG, "CDVDVideoCodecAVFoundation::Process Stopped");
}

void CDVDVideoCodecAVFoundation::DrainQueues()
{
  pthread_mutex_lock(&m_trackerQueueMutex);
  while (!m_trackerQueue.empty())
  {
    // run backwards, so list does not have to reorder.
    pktTracker *tracker = m_trackerQueue.back();
    delete tracker;
    m_trackerQueue.pop_back();
  }
  pthread_mutex_unlock(&m_trackerQueueMutex);

  pthread_mutex_lock(&m_sampleBuffersMutex);
  while (!m_sampleBuffers.empty())
  {
    CMSampleBufferRef sampleBuffer = m_sampleBuffers.front();
    m_sampleBuffers.pop();
    CFRelease(sampleBuffer);
  }
  pthread_mutex_unlock(&m_sampleBuffersMutex);
}

void CDVDVideoCodecAVFoundation::DumpTrackingQueue()
{
  pthread_mutex_lock(&m_trackerQueueMutex);
  int index = 0;
	std::list<pktTracker*>::iterator it;
	for (it = m_trackerQueue.begin(); it != m_trackerQueue.end(); ++it)
    CLog::Log(LOGDEBUG, "DumpTrackingQueue index(%d), ts(%f)", index++, (*it)->pts);
  pthread_mutex_unlock(&m_trackerQueueMutex);
}

void CDVDVideoCodecAVFoundation::StartSampleProviderWithBlock()
{
  VideoLayerView *mcview = (VideoLayerView*)m_decoder;
  AVSampleBufferDisplayLayer *videolayer = (AVSampleBufferDisplayLayer*)mcview.layer;

  // ok, for those that have never seen a usingBlock structure. these are
  // special, works like a mini-thread that fires when videoLayer
  // needs demux data. You need to pair this with StopSampleProvider
  // to stop the callbacks or very bad things might happen...
  [videolayer requestMediaDataWhenReadyOnQueue:m_providerQueue usingBlock:^
  {
    while(videolayer.readyForMoreMediaData)
    {
      pthread_mutex_lock(&m_sampleBuffersMutex);
      size_t bufferCount = m_sampleBuffers.size();
      pthread_mutex_unlock(&m_sampleBuffersMutex);

      if (bufferCount)
      {
        pthread_mutex_lock(&m_sampleBuffersMutex);
        CMSampleBufferRef nextSampleBuffer = m_sampleBuffers.front();
        if (nextSampleBuffer)
          m_sampleBuffers.pop();
        pthread_mutex_unlock(&m_sampleBuffersMutex);

        if (nextSampleBuffer)
        {
          [videolayer enqueueSampleBuffer:nextSampleBuffer];
          CFRelease(nextSampleBuffer);
          [mcview performSelectorOnMainThread:@selector(setNeedsDisplay) withObject:nil  waitUntilDone:YES];

          if ([videolayer status] == AVQueuedSampleBufferRenderingStatusFailed)
          {
            CLog::Log(LOGNOTICE, "%s - CDVDVideoCodecAVFoundation failed, status(%ld)",
              __FUNCTION__, (long)[videolayer error].code);
          }
        }
      }
      else
      {
        //CLog::Log(LOGDEBUG, "%s: no more sample buffers to enqueue", __FUNCTION__);
        break;
      }
    }
    // yield a little here or we hammer the cpu
    usleep(5 * 1000);
  }];
}

void CDVDVideoCodecAVFoundation::StopSampleProvider()
{
  dispatch_sync(dispatch_get_main_queue(),^{
    VideoLayerView *mcview = (VideoLayerView*)m_decoder;
    AVSampleBufferDisplayLayer *videolayer = (AVSampleBufferDisplayLayer*)mcview.layer;
    [videolayer stopRequestingMediaData];
  });
}

double CDVDVideoCodecAVFoundation::GetPlayerClockSeconds()
{
  if (!m_clock)
    return 0.0;
  // add in audio delay contribution
  double audioOffsetSeconds = 0 - CMediaSettings::GetInstance().GetCurrentVideoSettings().m_AudioDelay;
  return audioOffsetSeconds + (m_clock->GetClock() / DVD_TIME_BASE);
}

void CDVDVideoCodecAVFoundation::UpdateFrameRateTracking(double ts)
{
  m_framecount++;

  if (ts == DVD_NOPTS_VALUE)
  {
    m_lastTrackingTS = DVD_NOPTS_VALUE;
    return;
  }
  if (m_lastTrackingTS == DVD_NOPTS_VALUE)
  {
    m_lastTrackingTS = ts;
    return;
  }

  float pts_duration = ts - m_lastTrackingTS;
  if (pts_duration < 0.0)
    return;

  //CLog::Log(LOGDEBUG, "UpdateFrameRateTracking ts(%f), last_ts(%f), diff(%f)",
  //    ts, m_lastTrackingTS, ts - m_lastTrackingTS);

  m_lastTrackingTS = ts;

  // clamp duration to sensible range,
  // 66 fsp to 20 fsp
  if (pts_duration >= 15000.0 && pts_duration <= 50000.0)
  {
    double fps;
    switch((int)(0.5 + pts_duration))
    {
      // 59.940 (16683.333333)
      case 16000 ... 17000:
        fps = 60000.0 / 1001.0;
        break;

      // 50.000 (20000.000000)
      case 20000:
        fps = 50000.0 / 1000.0;
        break;

      // 49.950 (20020.000000)
      case 20020:
        fps = 50000.0 / 1001.0;
        break;

      // 29.970 (33366.666656)
      case 32000 ... 35000:
        fps = 30000.0 / 1001.0;
        break;

      // 25.000 (40000.000000)
      case 40000:
        fps = 25000.0 / 1000.0;
        break;

      // 24.975 (40040.000000)
      case 40040:
        fps = 25000.0 / 1001.0;
        break;

      /*
      // 24.000 (41666.666666)
      case 41667:
        framerate = 24000.0 / 1000.0;
        break;
      */

      // 23.976 (41708.33333)
      case 40200 ... 43200:
        // 23.976 seems to have the crappiest encodings :)
        fps = 24000.0 / 1001.0;
        break;

      default:
        fps = 0.0;
        //CLog::Log(LOGDEBUG, "%s: unknown duration(%f), cur_pts(%f)",
        //  __MODULE_NAME__, duration, cur_pts);
        break;
    }

    if (fps > 0.0 && (int)m_fps != (int)fps)
    {
      m_fps = fps;
      CLog::Log(LOGDEBUG, "%s: detected new fps(%f) at frame(%llu)",
        __FUNCTION__, m_fps, m_framecount);
    }
  }
}

void CDVDVideoCodecAVFoundation::GetRenderFeatures(Features &renderFeatures)
{
  renderFeatures.push_back(RENDERFEATURE_ZOOM);
  renderFeatures.push_back(RENDERFEATURE_STRETCH);
  renderFeatures.push_back(RENDERFEATURE_NONLINSTRETCH);
  renderFeatures.push_back(RENDERFEATURE_VERTICAL_SHIFT);
  renderFeatures.push_back(RENDERFEATURE_PIXEL_RATIO);
}

void CDVDVideoCodecAVFoundation::RenderFeaturesCallBack(const void *ctx, Features &renderFeatures)
{
  CDVDVideoCodecAVFoundation *codec = (CDVDVideoCodecAVFoundation*)ctx;
  if (codec)
    codec->GetRenderFeatures(renderFeatures);
}

/** Dolby Vision Profile enum type */
typedef enum OMX_VIDEO_DOLBYVISIONPROFILETYPE {
    OMX_VIDEO_DolbyVisionProfileUnknown = 0x0,
    OMX_VIDEO_DolbyVisionProfileDvavDer = 0x1,
    OMX_VIDEO_DolbyVisionProfileDvavDen = 0x2,
    OMX_VIDEO_DolbyVisionProfileDvheDer = 0x3,
    OMX_VIDEO_DolbyVisionProfileDvheDen = 0x4,
    OMX_VIDEO_DolbyVisionProfileDvheDtr = 0x5,
    OMX_VIDEO_DolbyVisionProfileDvheStn = 0x6,
    OMX_VIDEO_DolbyVisionProfileMax     = 0x7FFFFFFF
} OMX_VIDEO_DOLBYVISIONPROFILETYPE;
/** Dolby Vision Level enum type */
typedef enum OMX_VIDEO_DOLBYVISIONLEVELTYPE {
    OMX_VIDEO_DolbyVisionLevelUnknown = 0x0,
    OMX_VIDEO_DolbyVisionLevelHd24    = 0x1,
    OMX_VIDEO_DolbyVisionLevelHd30    = 0x2,
    OMX_VIDEO_DolbyVisionLevelFhd24   = 0x4,
    OMX_VIDEO_DolbyVisionLevelFhd30   = 0x8,
    OMX_VIDEO_DolbyVisionLevelFhd60   = 0x10,
    OMX_VIDEO_DolbyVisionLevelUhd24   = 0x20,
    OMX_VIDEO_DolbyVisionLevelUhd30   = 0x40,
    OMX_VIDEO_DolbyVisionLevelUhd48   = 0x80,
    OMX_VIDEO_DolbyVisionLevelUhd60   = 0x100,
    OMX_VIDEO_DolbyVisionLevelmax     = 0x7FFFFFFF
} OMX_VIDEO_DOLBYVISIONLEVELTYPE;

enum {
    HEVC_NAL_TRAIL_N    = 0,
    HEVC_NAL_TRAIL_R    = 1,
    HEVC_NAL_TSA_N      = 2,
    HEVC_NAL_TSA_R      = 3,
    HEVC_NAL_STSA_N     = 4,
    HEVC_NAL_STSA_R     = 5,
    HEVC_NAL_RADL_N     = 6,
    HEVC_NAL_RADL_R     = 7,
    HEVC_NAL_RASL_N     = 8,
    HEVC_NAL_RASL_R     = 9,
    HEVC_NAL_BLA_W_LP   = 16,
    HEVC_NAL_BLA_W_RADL = 17,
    HEVC_NAL_BLA_N_LP   = 18,
    HEVC_NAL_IDR_W_RADL = 19,
    HEVC_NAL_IDR_N_LP   = 20,
    HEVC_NAL_CRA_NUT    = 21,
    HEVC_NAL_RESERVED_IRAP_VCL22 = 22,
    HEVC_NAL_RESERVED_IRAP_VCL23 = 23,
    HEVC_NAL_VPS        = 32,
    HEVC_NAL_SPS        = 33,
    HEVC_NAL_PPS        = 34,
    HEVC_NAL_AUD        = 35,
    HEVC_NAL_EOS_NUT    = 36,
    HEVC_NAL_EOB_NUT    = 37,
    HEVC_NAL_FD_NUT     = 38,
    HEVC_NAL_SEI_PREFIX = 39,
    HEVC_NAL_SEI_SUFFIX = 40,
    HEVC_NAL_UNSPECIFIED_62 = 62,
    HEVC_NAL_UNSPECIFIED_63 = 63

};


enum HEVC_SEI_TYPE {
    SEI_TYPE_BUFFERING_PERIOD                     = 0,
    SEI_TYPE_PICTURE_TIMING                       = 1,
    SEI_TYPE_PAN_SCAN_RECT                        = 2,
    SEI_TYPE_FILLER_PAYLOAD                       = 3,
    SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35       = 4,
    SEI_TYPE_USER_DATA_UNREGISTERED               = 5,
    SEI_TYPE_RECOVERY_POINT                       = 6,
    SEI_TYPE_SCENE_INFO                           = 9,
    SEI_TYPE_FULL_FRAME_SNAPSHOT                  = 15,
    SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_START = 16,
    SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_END   = 17,
    SEI_TYPE_FILM_GRAIN_CHARACTERISTICS           = 19,
    SEI_TYPE_POST_FILTER_HINT                     = 22,
    SEI_TYPE_TONE_MAPPING_INFO                    = 23,
    SEI_TYPE_FRAME_PACKING                        = 45,
    SEI_TYPE_DISPLAY_ORIENTATION                  = 47,
    SEI_TYPE_SOP_DESCRIPTION                      = 128,
    SEI_TYPE_ACTIVE_PARAMETER_SETS                = 129,
    SEI_TYPE_DECODING_UNIT_INFO                   = 130,
    SEI_TYPE_TEMPORAL_LEVEL0_INDEX                = 131,
    SEI_TYPE_DECODED_PICTURE_HASH                 = 132,
    SEI_TYPE_SCALABLE_NESTING                     = 133,
    SEI_TYPE_REGION_REFRESH_INFO                  = 134,
    SEI_TYPE_MASTERING_DISPLAY_INFO               = 137,
    SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO             = 144,
};

void
CDVDVideoCodecAVFoundation::ProbeNALUnits(uint8_t *pData, int iSize)
{
  if (m_codec != AV_CODEC_ID_HEVC)
    return;

  static uint64_t frameCount = 0;

  // pData is in bit stream format, 32 size (big endian), followed by NAL value
  uint8_t *data = pData;
  uint8_t *dataEnd = data + iSize;
  // do a quick search for parameter sets in this frame
  while (data < dataEnd)
  {
    int nal_size = BS_RB32(data);
    data += 4;
    // bit(1) array_completeness;
    // bit(1) reserved = 0;
    // bit(6) NAL_unit_type;
    int nal_type_full = data[0] << 8 | data[1];
    int nal_type = (data[0] >> 1) & 0x3f;
    switch(nal_type)
    {
      default:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), nal_type(%d)",
          __FUNCTION__, frameCount, nal_type);
        break;

      case HEVC_NAL_UNSPECIFIED_62:
      case HEVC_NAL_UNSPECIFIED_63:
        // The dolby vision enhancement layer doesn't actually use NAL units 63 and 62,
        // it uses a special syntax that uses 0x7E01 and 0x7C01 as separator that makes
        // the Dolby Extension Layer (EL) appear as unspecified/unused NAL units (62 and 63),
        // but the actual NAL information starts after that extra header.
        if (nal_type_full == 0x7C01)
        {
          CLog::Log(LOGDEBUG, "%s - frame(%llu), dolby vision enhancement layer of 0x7C01 with size(%d)",
            __FUNCTION__, frameCount, nal_size);
        }
        else if (nal_type_full == 0x7E01)
          CLog::Log(LOGDEBUG, "%s - frame(%llu), dolby vision enhancement layer of 0x7E01 with size(%d)",
            __FUNCTION__, frameCount, nal_size);
        else
        {
          CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_UNSPECIFIED_62/63 with size(%d)",
            __FUNCTION__, frameCount, nal_size);
        }
        break;
      case HEVC_NAL_TRAIL_R:
      case HEVC_NAL_TRAIL_N:
      case HEVC_NAL_TSA_N:
      case HEVC_NAL_TSA_R:
      case HEVC_NAL_STSA_N:
      case HEVC_NAL_STSA_R:
      case HEVC_NAL_BLA_W_LP:
      case HEVC_NAL_BLA_W_RADL:
      case HEVC_NAL_BLA_N_LP:
      case HEVC_NAL_IDR_W_RADL:
      case HEVC_NAL_IDR_N_LP:
      case HEVC_NAL_CRA_NUT:
      case HEVC_NAL_RADL_N:
      case HEVC_NAL_RADL_R:
      case HEVC_NAL_RASL_N:
      case HEVC_NAL_RASL_R:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), nal slice of type(%d) with size(%d)",
          __FUNCTION__, frameCount, nal_type, nal_size);
        break;
      case HEVC_NAL_AUD:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_AUD",
          __FUNCTION__, frameCount);
        break;
      case HEVC_NAL_VPS:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_VPS",
          __FUNCTION__, frameCount);
        break;
      case HEVC_NAL_SPS:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_SPS",
          __FUNCTION__, frameCount);
        break;
      case HEVC_NAL_PPS:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_PPS",
          __FUNCTION__, frameCount);
        break;
      case HEVC_NAL_SEI_PREFIX:
      {
        // todo: this only handles the 1st sei prefix message
        // it should loop over until done, see ff_hevc_decode_nal_sei
        uint8_t *payload = data + 2;
        // once we get NAL type, switch to bit stream reader
        // as it has to do three byte emulation prevention handling.
        nal_bitstream bs;
        CBitstreamParser::nal_bs_init(&bs, payload, dataEnd - payload);

        int payload_type = 0;
        int payload_size = 0;
        int byte = 0xFF;
        while (byte == 0xFF) {
            byte = CBitstreamParser::nal_bs_read(&bs, 8);
            payload_type += byte;
        }
        byte = 0xFF;
        while (byte == 0xFF) {
            byte = CBitstreamParser::nal_bs_read(&bs, 8);
            payload_size += byte;
        }

        switch(payload_type)
        {
          default:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), sei prefix of type(%d) with size(%d)",
              __FUNCTION__, frameCount, nal_size, payload_type);
            break;
          case SEI_TYPE_BUFFERING_PERIOD:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_BUFFERING_PERIOD with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_PICTURE_TIMING:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_PICTURE_TIMING with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_USER_DATA_UNREGISTERED:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_USER_DATA_UNREGISTERED with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_RECOVERY_POINT:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_RECOVERY_POINT with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_SCENE_INFO:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_SCENE_INFO with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_ACTIVE_PARAMETER_SETS:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_ACTIVE_PARAMETER_SETS with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_MASTERING_DISPLAY_INFO:
            {
              // Mastering primaries (in g,b,r order)
              // specify the normalized x and y chromaticity coordinates
              // units of 0.00002, range 0 to 50,000
              uint16_t display_primaries[3][2];
              for (int i = 0; i < 3; i++)
              {
                display_primaries[i][0] = CBitstreamParser::nal_bs_read(&bs, 16);
                display_primaries[i][1] = CBitstreamParser::nal_bs_read(&bs, 16);
              }
              float displayPrimaries[3][2];
              for (int i = 0; i < 3; i++)
              {
                displayPrimaries[i][0] = 0.00002f * (float)display_primaries[i][0];
                displayPrimaries[i][1] = 0.00002f * (float)display_primaries[i][0];
              }
              // White point (x, y)
              // units of 0.00002, range 0 to 50,000
              uint16_t white_point[2];
              white_point[0] = CBitstreamParser::nal_bs_read(&bs, 16);
              white_point[1] = CBitstreamParser::nal_bs_read(&bs, 16);
              float whitePoint[2];
              whitePoint[0] = 0.00002f * (float)white_point[0];
              whitePoint[1] = 0.00002f * (float)white_point[1];

              // Max and min luminance of mastering display
              // units of 0.0001 cd/m2
              // no range specified but min < max.
              uint32_t max_display_mastering_luminance = CBitstreamParser::nal_bs_read(&bs, 32);
              uint32_t min_display_mastering_luminance = CBitstreamParser::nal_bs_read(&bs, 32);
              float maxMasteringLuminance = 0.0001f * (float)max_display_mastering_luminance;
              float minMasteringLuminance = 0.0001f * (float)min_display_mastering_luminance;
              CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_MASTERING_DISPLAY_INFO with size(%d)",
                __FUNCTION__, frameCount, payload_size);

              CLog::Log(LOGDEBUG, "Mastering display color primaries : R: x=%f y=%f, G: x=%f y=%f, B: x=%f y=%f, White point: x=%f y=%f",
                displayPrimaries[2][0], displayPrimaries[2][1],
                displayPrimaries[1][0], displayPrimaries[1][1],
                displayPrimaries[0][0], displayPrimaries[0][1],
                whitePoint[0], whitePoint[1]);
              CLog::Log(LOGDEBUG, "Mastering display luminance       : min: %f cd/m2, max: %f cd/m2",
                minMasteringLuminance, maxMasteringLuminance);
            }
            break;
          case SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
        }
      }
      break;
      case HEVC_NAL_SEI_SUFFIX:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), sei suffix with size(%d)", __FUNCTION__, frameCount, nal_size);
      break;
    }
    data += nal_size;
  }
  frameCount++;
}

#endif

#pragma once
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


#if defined(TARGET_DARWIN_IOS)

#include <list>
#include <queue>
#include <CoreMedia/CoreMedia.h>

#include "cores/VideoRenderers/RenderFeatures.h"
#include "cores/dvdplayer/DVDCodecs/Video/DVDVideoCodec.h"
#include "threads/Thread.h"

#ifdef __OBJC__
  @class VideoLayerView;
#else
  class VideoLayerView;
#endif

struct pktTracker;
class CDVDClock;
class CAVFCodecMessage;
class CBitstreamConverter;

class CDVDVideoCodecAVFoundation : public CDVDVideoCodec, CThread
{
public:
  CDVDVideoCodecAVFoundation();
  virtual ~CDVDVideoCodecAVFoundation();

  // Required overrides
  virtual bool  Open(CDVDStreamInfo &hints, CDVDCodecOptions &options);
  virtual void  Dispose(void);
  virtual int   Decode(uint8_t *pData, int iSize, double dts, double pts);
  virtual void  Reset(void);
  virtual void  SetClock(CDVDClock *clock);
  virtual bool  GetPicture(DVDVideoPicture *pDvdVideoPicture);
  virtual void  SetDropState(bool bDrop);
  virtual void  SetSpeed(int iSpeed);
  virtual const char* GetName(void) { return (const char*)m_pFormatName; }
  virtual unsigned GetAllowedReferences();
  virtual void SetCodecControl(int flags);

protected:
  virtual void  Process();

  void          DrainQueues();
  void          DumpTrackingQueue();
  void          StartSampleProviderWithBlock();
  void          StopSampleProvider();

  double        GetPlayerClockSeconds();
  void          UpdateFrameRateTracking(double ts);
  void          GetRenderFeatures(Features &renderFeatures);
  static void   RenderFeaturesCallBack(const void *ctx, Features &renderFeatures);
  void          ProbeNALUnits(uint8_t *pData, int iSize);

  VideoLayerView         *m_decoder;        // opaque decoder reference
	dispatch_queue_t        m_providerQueue;
  CMFormatDescriptionRef  m_fmt_desc;
  pthread_mutex_t         m_sampleBuffersMutex;    // mutex protecting queue manipulation
  std::queue<CMSampleBufferRef> m_sampleBuffers;

  size_t                  m_max_ref_frames = 4;
  pthread_mutex_t         m_trackerQueueMutex;       // mutex protecting queue manipulation
  std::list<pktTracker*>  m_trackerQueue;

  CDVDClock              *m_clock = nullptr;
  int32_t                 m_format;
  const char             *m_pFormatName;
  int                     m_codecControlFlags;
  double                  m_dts;
  double                  m_pts;
  int                     m_speed;
  AVCodecID               m_codec;
  int                     m_profile;
  int                     m_colorrange;
  int                     m_colorspace;
  int                     m_colortransfer;
  int                     m_dynamicrange;
  int                     m_width;
  int                     m_height;
  DVDVideoPicture         m_videobuffer;
  CBitstreamConverter    *m_bitstream;
  bool                    m_withBlockRunning;

  CAVFCodecMessage       *m_messages;
  uint64_t                m_framecount;
  double                  m_fps;
  double                  m_lastTrackingTS;
};
#endif


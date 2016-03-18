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


#include <list>
#include <queue>
#include <CoreMedia/CoreMedia.h>

#include "cores/dvdplayer/DVDCodecs/Video/DVDVideoCodec.h"
#include "threads/Thread.h"

struct pktTracker;
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
  virtual bool  GetPicture(DVDVideoPicture *pDvdVideoPicture);
  virtual void  SetDropState(bool bDrop);
  virtual void  SetSpeed(int iSpeed);
  virtual int   GetDataSize(void);
  virtual double GetTimeSize(void);
  virtual const char* GetName(void) { return (const char*)m_pFormatName; }

protected:
  virtual void  Process();

  void          DrainQueues();
  void          StartSampleProviderWithBlock();
  void          StopSampleProvider();

  double        GetRenderPtsSeconds();
  void          UpdateFrameRateTracking(double pts);

  void                   *m_decoder;        // opaque decoder reference
	dispatch_queue_t        m_providerQueue;
  CMFormatDescriptionRef  m_fmt_desc;
  pthread_mutex_t         m_sampleBuffersMutex;    // mutex protecting queue manipulation
  std::queue<CMSampleBufferRef> m_sampleBuffers;

  size_t                  m_max_ref_frames;
  pthread_mutex_t         m_trackerQueueMutex;       // mutex protecting queue manipulation
  std::list<pktTracker*>  m_trackerQueue;

  int32_t                 m_format;
  const char             *m_pFormatName;
  double                  m_dts;
  double                  m_pts;
  int                     m_speed;
  int                     m_width;
  int                     m_height;
  DVDVideoPicture         m_videobuffer;
  CBitstreamConverter    *m_bitstream;
  bool                    m_withBlockRunning;

  CAVFCodecMessage       *m_messages;
  uint64_t                m_framecount;
  double                  m_framerate_ms;
};

#pragma once
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

#include "cores/AudioEngine/Interfaces/AESink.h"
#include "cores/AudioEngine/Utils/AEDeviceInfo.h"
#include "threads/Thread.h"

#include <deque>
#include <set>

#include <androidjni/AudioTrack.h>

class CAESinkAUDIOTRACK : public IAESink
{
public:
  virtual const char *GetName() { return "AUDIOTRACK"; }

  CAESinkAUDIOTRACK();
  virtual ~CAESinkAUDIOTRACK();

  virtual bool          Initialize(AEAudioFormat &format, std::string &device);
  virtual void          Deinitialize();
  bool                  IsInitialized();

  virtual void          GetDelay        (AEDelayStatus& status);
  virtual double        GetLatency      ();
  virtual double        GetCacheTotal   ();
  virtual unsigned int  AddPackets      (uint8_t **data, unsigned int frames, unsigned int offset);
  virtual void          AddPause        (unsigned int millis);
  virtual void          Drain           ();

  static bool           FormatNeedsIECPacked(const AEAudioFormat &format);
  static void           EnumerateDevicesEx(AEDeviceInfoList &list, bool force = false);

protected:
  jni::CJNIAudioTrack *CreateAudioTrack(int stream, int sampleRate, int channelMask, int encoding, int bufferSize);
  static bool           IsSupported(int sampleRateInHz, int channelConfig, int audioFormat);

  int AudioTrackWrite(char* audioData, int offsetInBytes, int sizeInBytes);
  int AudioTrackWrite(char* audioData, int sizeInBytes, int64_t timestamp);

private:
  double                GetPresentedDelay();
  uint64_t              GetPlaybackHeadPosition();
  
  // Moving Average computes the weighted average delay over
  // a fixed size of delay values - current size: 20 values
  double                GetMovingAverageDelay(double newestdelay);

  static int            m_sdk;
  static CAEDeviceInfo  m_info;
  static std::set<int>  m_sink_sampleRates;

  jni::CJNIAudioTrack  *m_at_jni;
  int                   m_jniAudioFormat;
  AEAudioFormat         m_format;
  double                m_volume;
  int                   m_encoding;
  bool                  m_passthrough;
  bool                  m_passthroughIsIECPacked;

  int                   m_sink_frameSize;
  int                   m_sink_sampleRate;
  int                   m_sink_bufferSize;
  double                m_sink_bufferSeconds;
  int                   m_sink_sleepOnWriteStall;
  // the effective bytes per second, pcm is simple frames/samplerate
  // passthrough is more complicated to figure out as android wants obscure it.
  double                m_sink_bufferBytesPerSecond;

  // sink buffer filled handling
  uint64_t              m_writeBytes;
  double                m_writeSeconds;
  uint64_t              m_playbackHead;
  int64_t               m_playbackHeadOffset;

  // When AddPause is called the pause time is increased by the
  // package duration. This is only used for non IEC passthrough
  // as IEC packed will get an IEC packed pause burst injected.
  XbmcThreads::EndTime  m_nonIECPauseTimer;

  // We maintain our linear weighted average delay counter in here
  // The n-th value (timely oldest value) is weighted with 1/n
  // the newest value gets a weight of 1
  std::deque<double>    m_linearmovingaverage;

  std::vector<float> m_floatbuf;
  std::vector<int16_t> m_shortbuf;
  std::vector<char> m_charbuf;
};

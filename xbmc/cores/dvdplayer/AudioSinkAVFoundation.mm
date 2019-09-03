/*
 *      Copyright (C) 2019 Team MrMC
 *      http://mrmc.tv
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

#include "AudioSinkAVFoundation.h"

#include "DVDClock.h"
#include "DVDCodecs/Audio/DVDAudioCodec.h"
#include "cores/AudioEngine/AEFactory.h"
#include "platform/darwin/DllAVAudioSink.h"
#include "threads/SingleLock.h"
#include "utils/log.h"


#pragma mark - CAudioSinkAVFoundation
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
CAudioSinkAVFoundation::CAudioSinkAVFoundation(volatile bool &bStop, CDVDClock *clock)
: CThread("CAudioSinkAVFoundation")
, m_bStop(bStop)
, m_pClock(clock)
, m_startPtsSeconds(0)
, m_startPtsFlag(false)
, m_sink(nullptr)
{
  //CLog::Log(LOGDEBUG, "CAudioSinkAVFoundation::CAudioSinkAVFoundation");
  m_syncErrorDVDTime = 0.0;
  m_syncErrorDVDTimeSecondsOld = 0.0;
}

CAudioSinkAVFoundation::~CAudioSinkAVFoundation()
{
  //CLog::Log(LOGDEBUG, "CAudioSinkAVFoundation::~CAudioSinkAVFoundation");
  SAFE_DELETE(m_sink);
}

bool CAudioSinkAVFoundation::Create(const DVDAudioFrame &audioframe, AVCodecID codec, bool needresampler)
{
  if (codec != AV_CODEC_ID_EAC3)
    return false;

  CLog::Log(LOGNOTICE,
    "Creating audio stream (codec id: %i, channels: %i, sample rate: %i, %s)",
    codec,
    audioframe.format.m_channelLayout.Count(),
    audioframe.format.m_sampleRate,
    audioframe.passthrough ? "pass-through" : "no pass-through"
  );

  CSingleLock lock(m_critSection);

  CAEFactory::SetExternalDevice(true);
  // wait for AE to suspend
  XbmcThreads::EndTime timer(250);
  while (!CAEFactory::IsSuspended() && !timer.IsTimePast())
    usleep(1 * 1000);

  // EAC3 can have many audio blocks of 256 bytes per sync frame
  m_frameSize = audioframe.format.m_streamInfo.m_frameSize;
  m_startPtsFlag = false;
  m_startPtsSeconds = 0;
  m_sinkErrorSeconds = 0;
  SAFE_DELETE(m_sink);
  m_sink = new DllAVAudioSink();
  if (m_sink->Load() && m_sink->AVAudioSinkOpen(m_frameSize))
  {
    // uncomment to enable debug logging
    //CThread::Create();
    CLog::Log(LOGDEBUG, "CAudioSinkAVFoundation::Create");
    return true;
  }
  else
  {
    Destroy();
    CLog::Log(LOGWARNING, "CAudioSinkAVFoundation::Create, failed to open");
    return false;
  }
}

void CAudioSinkAVFoundation::Destroy()
{
  CSingleLock lock (m_critSection);
  CLog::Log(LOGDEBUG,"CAudioSinkAVFoundation::Destroy");

  m_bStop = true;
  StopThread();

  SAFE_DELETE(m_sink);

  if (CAEFactory::UsingExternalDevice())
    CAEFactory::SetExternalDevice(false);
  // wait for AE to wake
  XbmcThreads::EndTime timer(250);
  while (CAEFactory::IsSuspended() && !timer.IsTimePast())
    usleep(1 * 1000);
}

unsigned int CAudioSinkAVFoundation::AddPackets(const DVDAudioFrame &audioframe)
{
  CSingleLock lock (m_critSection);

  m_abortAddPacketWait = false;
  double syncErrorSeconds = CalcSyncErrorSeconds();
  if (abs(syncErrorSeconds) > 0.020 ||
      abs(syncErrorSeconds - m_syncErrorDVDTimeSecondsOld) > 0.020)
  {
    m_syncErrorDVDTimeSecondsOld = syncErrorSeconds;
    m_syncErrorDVDTime = DVD_SEC_TO_TIME(syncErrorSeconds);
  }
  else
  {
    m_syncErrorDVDTime = 0.0;
    m_syncErrorDVDTimeSecondsOld = 0.0;
  }

  if (!m_startPtsFlag && audioframe.nb_frames)
  {
    m_startPtsFlag = true;
    m_startPtsSeconds = (double)audioframe.pts / DVD_TIME_BASE;
  }
  unsigned int written = 0;
  while (!m_abortAddPacketWait && !m_bStop && written < audioframe.nb_frames)
  {
    double buffer_s = 0.0;
    double minbuffer_s = 1.0;
    if (m_sink)
    {
      written = m_sink->AVAudioSinkWrite(audioframe.data[0], audioframe.nb_frames);
      buffer_s = m_sink->AVAudioSinkDelaySeconds();
      minbuffer_s = m_sink->AVAudioSinkMinDelaySeconds();
    }
    // native sink needs about 2.5 seconds of buffer
    // it will get stuttery below 2 seconds as it pauses to
    // wait for internal buffers to fill.
    if (buffer_s > minbuffer_s)
    {
      lock.Leave();
      Sleep(50);
      if (m_abortAddPacketWait)
        break;
      lock.Enter();
    }
  }

  return audioframe.nb_frames;
}

void CAudioSinkAVFoundation::Drain()
{
  // let audio play out and wait for it to end.
  CSingleLock lock (m_critSection);
  CLog::Log(LOGDEBUG, "CAudioSinkAVFoundation::Drain");
  double delay_s = m_sink->AVAudioSinkDelaySeconds();
  XbmcThreads::EndTime timer(delay_s * 1000);
  while (!m_bStop && !timer.IsTimePast())
  {
    delay_s = m_sink->AVAudioSinkDelaySeconds();
    if (delay_s <= 0.0)
      return;

    sleep(50);
  }
}

void CAudioSinkAVFoundation::Flush()
{
  m_abortAddPacketWait = true;

  CSingleLock lock (m_critSection);
  CLog::Log(LOGDEBUG, "CAudioSinkAVFoundation::Flush");
  m_startPtsFlag = false;
  m_startPtsSeconds = 0;
  if (m_sink)
    m_sink->AVAudioSinkFlush();
}

bool CAudioSinkAVFoundation::Initialized()
{
  if (m_sink)
    return m_sink->AVAudioSinkReady();
  return true;
}

void CAudioSinkAVFoundation::Pause()
{
  CSingleLock lock (m_critSection);
  CLog::Log(LOGDEBUG,"CAudioSinkAVFoundation::Pause");
  if (m_sink)
    m_sink->AVAudioSinkPlay(false);
}

void CAudioSinkAVFoundation::Resume()
{
  CSingleLock lock(m_critSection);
  CLog::Log(LOGDEBUG,"CAudioSinkAVFoundation::Resume");
  if (m_sink)
    m_sink->AVAudioSinkPlay(true);
}

double CAudioSinkAVFoundation::GetDelay()
{
  // Returns the time (dvd timebase) that it will take
  // for the next added packet to be heard from the speakers.
  // 1) used as audio cachetime in player during startup
  // 2) in DVDPlayerAudio during RESYNC
  // 3) and internally to offset passed pts in AddPackets
  CSingleLock lock (m_critSection);
  double delay_s = 0.3;
  if (m_sink)
    delay_s = m_sink->AVAudioSinkDelaySeconds();
  return delay_s * DVD_TIME_BASE;
}

double CAudioSinkAVFoundation::GetMaxDelay()
{
  // returns total time (seconds) of audio in AE for the stream
  // used as audio cachetotal in player during startup
  CSingleLock lock (m_critSection);
  double maxdelay_s = 0.3;
  if (m_sink)
    maxdelay_s = m_sink->AVAudioSinkMaxDelaySeconds();
  return maxdelay_s;
}

void CAudioSinkAVFoundation::AbortAddPackets()
{
  CLog::Log(LOGDEBUG, "CAudioSinkAVFoundation::AbortAddPackets");
  m_abortAddPacketWait = true;
}

bool CAudioSinkAVFoundation::IsValidFormat(const DVDAudioFrame &audioframe)
{
  return audioframe.passthrough == true;
}

double CAudioSinkAVFoundation::GetCacheTime()
{
  // Returns the time in seconds that it will take
  // to underrun the cache if no sample is added.
  // ie. time of current cache in seconds.
  // 1) used in setting a timeout in CDVDPlayerAudio message queue
  // 2) used to signal start (cachetime >= cachetotal * 0.75)
  CSingleLock lock (m_critSection);
  double delay_s = 0.3;
  if (m_sink)
    delay_s = m_sink->AVAudioSinkDelaySeconds();
  return delay_s;
}

double CAudioSinkAVFoundation::GetCacheTotal()
{
  // total cache time of stream in seconds
  // returns total time a stream can buffer
  // only used to signal start (cachetime >= cachetotal * 0.75)
  CSingleLock lock (m_critSection);
  double maxdelay_s = 0.3;
  if (m_sink)
    maxdelay_s = m_sink->AVAudioSinkMaxDelaySeconds();
  return maxdelay_s;
}

double CAudioSinkAVFoundation::GetPlayingPts()
{
  // passed to CDVDPlayerAudio and accessed by CDVDPlayerAudio::GetCurrentPts()
  // which is used by CDVDPlayer to ONLY report a/v sync.
  // Is not used for correcting a/v sync.
  return DVD_MSEC_TO_TIME(GetClock());
}

double CAudioSinkAVFoundation::CalcSyncErrorSeconds()
{
  double syncError = 0.0;

  if (m_sink && m_sink->AVAudioSinkReady())
  {
    double absolute;
    double player_s = m_pClock->GetClock(absolute) / DVD_TIME_BASE;
    double sink_s = m_sink->AVAudioSinkTimeSeconds() + m_startPtsSeconds;
    if (player_s > 0.0 && sink_s > 0.0)
      syncError = sink_s - player_s;
  }
  return syncError;
}

double CAudioSinkAVFoundation::GetSyncError()
{
  return m_syncErrorDVDTime;
}

void CAudioSinkAVFoundation::SetSyncErrorCorrection(double correction)
{
  m_syncErrorDVDTime += correction;
}

double CAudioSinkAVFoundation::GetClock()
{
  // return clock time in milliseconds (corrected for starting pts)
  if (m_sink)
  {
    double sinkErrorSeconds = m_sink->AVAudioSinkErrorSeconds();
    if (m_sinkErrorSeconds != sinkErrorSeconds)
    {
      CLog::Log(LOGWARNING, "CAudioSinkAVFoundation::GetClock Error Adjust = %f", sinkErrorSeconds);
      m_sinkErrorSeconds = sinkErrorSeconds;
    }
    return (m_sink->AVAudioSinkTimeSeconds() + m_startPtsSeconds) * 1000.0;
  }
  return m_startPtsSeconds * 1000.0;
}

void CAudioSinkAVFoundation::Process()
{
  // debug only monitoring thread
  // disabled in release
  CLog::Log(LOGDEBUG, "CAudioSinkAVFoundation::Process Started");

  while (!m_bStop)
  {
    double sink_s = 0.0;
    double buffer_s = 0.0;
    double absolute;
    double player_s = m_pClock->GetClock(absolute) / DVD_TIME_BASE;
    if (m_startPtsFlag && m_sink)
    {
      sink_s = m_sink->AVAudioSinkTimeSeconds() + m_startPtsSeconds;
      buffer_s = m_sink->AVAudioSinkDelaySeconds();
    }
    CLog::Log(LOGDEBUG, "CAudioSinkAVFoundation::Process "
      "buffer_s (%f), player_s(%f), sink_s(%f), delta(%f)",
      buffer_s, player_s, sink_s, player_s - sink_s);

    Sleep(250);
  }

  CLog::Log(LOGDEBUG, "CAudioSinkAVFoundation::Process Stopped");
}

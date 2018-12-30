#pragma once

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

#include "IAudioSink.h"
#include "threads/SystemClock.h"
#include "threads/CriticalSection.h"
#include "cores/AudioEngine/Interfaces/AEStream.h"
#include "threads/Thread.h"

#include <atomic>

class DllAVAudioSink;

class CAudioSinkAVFoundation : public IAudioSink, IAEClockCallback, CThread
{
public:
  CAudioSinkAVFoundation(volatile bool& bStop, CDVDClock *clock);
 ~CAudioSinkAVFoundation();

  void SetVolume(float fVolume) {};
  void SetDynamicRangeCompression(long drc) {};
  float GetCurrentAttenuation() { return 1.0f; };
  void Pause();
  void Resume();
  bool Create(const DVDAudioFrame &audioframe, AVCodecID codec, bool needresampler);
  bool IsValidFormat(const DVDAudioFrame &audioframe);
  void Destroy();
  unsigned int AddPackets(const DVDAudioFrame &audioframe);
  double GetPlayingPts();
  double GetCacheTime();
  double GetCacheTotal(); // returns total time a stream can buffer
  double GetMaxDelay(); // returns total time of audio in AE for the stream
  double GetDelay(); // returns the time it takes to play a packet if we add one at this time
  double GetSyncError();
  void SetSyncErrorCorrection(double correction);
  double GetResampleRatio() { return 1.0; };
  void SetResampleMode(int mode) {};
  void Flush();
  void Drain();
  void AbortAddPackets();

  void SetSpeed(int iSpeed) {};
  void SetResampleRatio(double ratio) {};

  double GetClock();
  double GetClockSpeed() { return 1.0; };

protected:
  virtual void Process();
  double CalcSyncErrorSeconds();

  volatile bool& m_bStop;
  CDVDClock *m_pClock;

  double m_syncErrorDVDTime;
  double m_syncErrorDVDTimeSecondsOld;
  double m_startPtsSeconds;
  std::atomic_bool m_startPtsFlag;
  CCriticalSection m_critSection;
  std::atomic_bool m_abortAddPacketWait;
private:
  DllAVAudioSink *m_sink;
  int m_frameSize = 0;
  int m_sinkErrorSeconds = 0;
};

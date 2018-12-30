#pragma once

/*
 *      Copyright (C) 2018 Team MrMC
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

class CDVDClock;
typedef struct stDVDAudioFrame DVDAudioFrame;

extern "C" {
#include <libavcodec/avcodec.h>
}

class IAudioSink
{
public:
  virtual ~IAudioSink() {};

  virtual void SetVolume(float fVolume) = 0;
  virtual void SetDynamicRangeCompression(long drc) = 0;
  virtual float GetCurrentAttenuation() = 0;
  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual bool Create(const DVDAudioFrame &audioframe, AVCodecID codec, bool needresampler) = 0;
  virtual bool IsValidFormat(const DVDAudioFrame &audioframe) = 0;
  virtual void Destroy() = 0;
  virtual unsigned int AddPackets(const DVDAudioFrame &audioframe) = 0;
  virtual double GetPlayingPts() = 0;
  virtual double GetCacheTime() = 0;
  virtual double GetCacheTotal() = 0;   // returns total time a stream can buffer
  virtual double GetMaxDelay() = 0;     // returns total time of audio in AE for the stream
  virtual double GetDelay() = 0;        // returns the time it takes to play a packet if we add one at this time
  virtual double GetSyncError() = 0;
  virtual void   SetSyncErrorCorrection(double correction) = 0;
  virtual double GetResampleRatio() = 0;
  virtual void SetResampleMode(int mode) = 0;
  virtual void Flush() = 0;
  virtual void Drain() = 0;
  virtual void AbortAddPackets() = 0;

  virtual void SetSpeed(int iSpeed) = 0;
  virtual void SetResampleRatio(double ratio) = 0;
};

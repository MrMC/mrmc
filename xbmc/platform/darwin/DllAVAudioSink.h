#pragma once
/*
 *      Copyright (C) 2019 Team MrMC
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

#include <stdint.h>


#if !defined(HAVE_LIBAVAUDIOSINK)
class DllAVAudioSink
{
public:
  virtual ~DllAVAudioSink() {}

  virtual bool Load() { return false; }
  virtual bool AVAudioSinkOpen(const int framebytes) { return false; }
  virtual void AVAudioSinkClose() {}
  virtual int  AVAudioSinkWrite(const uint8_t *buf, int len) { return 0; }
  virtual void AVAudioSinkPlay(const bool playpause) {}
  virtual void AVAudioSinkFlush() {}
  virtual bool AVAudioSinkReady() { return false; }
  virtual double AVAudioSinkTimeSeconds() { return 0.0; }
  virtual double AVAudioSinkDelaySeconds() { return 0.0; }
  virtual double AVAudioSinkDelay2Seconds() { return 0.0; }
  virtual double AVAudioSinkErrorSeconds() { return 0.0; }
  virtual double AVAudioSinkMinDelaySeconds() { return 0.0; }
  virtual double AVAudioSinkMaxDelaySeconds() { return 0.0; }
};

#else

#include "DynamicDll.h"
#include <avaudiosink/avaudiosink.h>

class DllAVAudioSinkInterface
{
public:
  virtual ~DllAVAudioSinkInterface() {}

  virtual bool AVAudioSinkOpen(const int framebytes) = 0;
  virtual void AVAudioSinkClose() = 0;
  virtual int  AVAudioSinkWrite(const uint8_t *buf, int len) = 0;
  virtual void AVAudioSinkPlay(const bool playpause) = 0;
  virtual void AVAudioSinkFlush() = 0;
  virtual bool AVAudioSinkReady() = 0;
  virtual double AVAudioSinkTimeSeconds() = 0;
  virtual double AVAudioSinkDelaySeconds() = 0;
  virtual double AVAudioSinkDelay2Seconds() = 0;
  virtual double AVAudioSinkErrorSeconds() = 0;
  virtual double AVAudioSinkMinDelaySeconds() = 0;
  virtual double AVAudioSinkMaxDelaySeconds() = 0;
};

class DllAVAudioSink : public DllDynamic, public DllAVAudioSinkInterface
{
protected:
  AVAudioSinkSessionRef *m_ctx = nullptr;

public:
  virtual ~DllAVAudioSink()
  {
    AVAudioSinkClose();
  }
  virtual bool AVAudioSinkOpen(const int framebytes)
  {
    m_ctx = avaudiosink_open(framebytes);
    return m_ctx != nullptr;
  }
  // close, going away, avref is invalid on return
  virtual void AVAudioSinkClose()
  {
    if (m_ctx)
      avaudiosink_close(m_ctx);
    m_ctx = nullptr;
  }
  // add audio frame packets to sink buffers
  virtual int AVAudioSinkWrite(const uint8_t *buf, int len)
  {
    return avaudiosink_write(m_ctx, buf, len);
  }
  // start/stop audio playback
  virtual void AVAudioSinkPlay(const bool playpause)
  {
    avaudiosink_play(m_ctx, playpause);
  }
  // flush audio playback buffers
  virtual void AVAudioSinkFlush()
  {
    avaudiosink_flush(m_ctx);
  }
  // return true when sink is ready to output audio after filling
  virtual bool AVAudioSinkReady()
  {
    return avaudiosink_ready(m_ctx) == 1;
  }
  // time in seconds of when sound hits your ears
  virtual double AVAudioSinkTimeSeconds()
  {
    return avaudiosink_timeseconds(m_ctx);
  }
  // delay in seconds of adding data before it hits your ears
  virtual double AVAudioSinkDelaySeconds()
  {
    return avaudiosink_delayseconds(m_ctx);
  }
  // alternative delay_s method
  virtual double AVAudioSinkDelay2Seconds()
  {
    return avaudiosink_delay2seconds(m_ctx);
  }
  // sink error seconds
  virtual double AVAudioSinkErrorSeconds()
  {
    return avaudiosink_errorseconds(m_ctx);
  }
  virtual double AVAudioSinkMinDelaySeconds()
  {
    return avaudiosink_mindelayseconds(m_ctx);
  }
  virtual double AVAudioSinkMaxDelaySeconds()
  {
    return avaudiosink_maxdelayseconds(m_ctx);
  }
#if defined(TARGET_DARWIN_IOS) && !defined(__x86_64__)
  DECLARE_DLL_WRAPPER(DllAVAudioSink, "libavaudiosink.framework/libavaudiosink")
#else
  DECLARE_DLL_WRAPPER(DllAVAudioSink, DLL_PATH_LIBAVAUDIOSINK)
#endif

  DEFINE_METHOD1(AVAudioSinkSessionRef*,  avaudiosink_open, (const int p1))
  DEFINE_METHOD1(void, avaudiosink_close, (AVAudioSinkSessionRef *p1))
  DEFINE_METHOD3(int , avaudiosink_write, (AVAudioSinkSessionRef *p1, const uint8_t *p2, int p3))
  DEFINE_METHOD2(void, avaudiosink_play,  (AVAudioSinkSessionRef *p1, const bool p2))
  DEFINE_METHOD1(void, avaudiosink_flush, (AVAudioSinkSessionRef *p1))
  DEFINE_METHOD1(int , avaudiosink_ready, (AVAudioSinkSessionRef *p1))
  DEFINE_METHOD1(double, avaudiosink_timeseconds, (AVAudioSinkSessionRef *p1))
  DEFINE_METHOD1(double, avaudiosink_delayseconds, (AVAudioSinkSessionRef *p1))
  DEFINE_METHOD1(double, avaudiosink_delay2seconds, (AVAudioSinkSessionRef *p1))
  DEFINE_METHOD1(double, avaudiosink_errorseconds,  (AVAudioSinkSessionRef *p1))
  DEFINE_METHOD1(double, avaudiosink_mindelayseconds, (AVAudioSinkSessionRef *p1))
  DEFINE_METHOD1(double, avaudiosink_maxdelayseconds, (AVAudioSinkSessionRef *p1))
  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD_RENAME(avaudiosink_open, avaudiosink_open)
    RESOLVE_METHOD_RENAME(avaudiosink_close, avaudiosink_close)
    RESOLVE_METHOD_RENAME(avaudiosink_write, avaudiosink_write)
    RESOLVE_METHOD_RENAME(avaudiosink_play, avaudiosink_play)
    RESOLVE_METHOD_RENAME(avaudiosink_flush, avaudiosink_flush)
    RESOLVE_METHOD_RENAME(avaudiosink_ready, avaudiosink_ready)
    RESOLVE_METHOD_RENAME(avaudiosink_timeseconds, avaudiosink_timeseconds)
    RESOLVE_METHOD_RENAME(avaudiosink_delayseconds, avaudiosink_delayseconds)
    RESOLVE_METHOD_RENAME(avaudiosink_delay2seconds, avaudiosink_delay2seconds)
    RESOLVE_METHOD_RENAME(avaudiosink_errorseconds, avaudiosink_errorseconds)
    RESOLVE_METHOD_RENAME(avaudiosink_mindelayseconds, avaudiosink_mindelayseconds)
    RESOLVE_METHOD_RENAME(avaudiosink_maxdelayseconds, avaudiosink_maxdelayseconds)
 END_METHOD_RESOLVE()
};

#endif

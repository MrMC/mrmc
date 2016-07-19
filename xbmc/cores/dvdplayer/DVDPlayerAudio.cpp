/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#include "threads/SingleLock.h"
#include "DVDPlayerAudio.h"
#include "DVDCodecs/Audio/DVDAudioCodec.h"
#include "DVDCodecs/DVDFactoryCodec.h"
#include "settings/Settings.h"
#include "video/VideoReferenceClock.h"
#include "utils/log.h"
#include "utils/MathUtils.h"
#include "cores/AudioEngine/AEFactory.h"
#include "cores/AudioEngine/Utils/AEUtil.h"
#ifdef TARGET_RASPBERRY_PI
#include "linux/RBP.h"
#endif
#ifdef TARGET_DARWIN_IOS
#include "platform/darwin/DarwinUtils.h"
#endif

#include <sstream>
#include <iomanip>
#include <math.h>

// allow audio for slow and fast speeds (but not rewind/fastforward)
#define ALLOW_AUDIO(speed) ((speed) > 5*DVD_PLAYSPEED_NORMAL/10 && (speed) <= 15*DVD_PLAYSPEED_NORMAL/10)

class CDVDMsgAudioCodecChange : public CDVDMsg
{
public:
  CDVDMsgAudioCodecChange(const CDVDStreamInfo &hints, CDVDAudioCodec* codec)
    : CDVDMsg(GENERAL_STREAMCHANGE)
    , m_codec(codec)
    , m_hints(hints)
  {}
 ~CDVDMsgAudioCodecChange()
  {
    SAFE_DELETE(m_codec);
  }
  CDVDAudioCodec* m_codec;
  CDVDStreamInfo  m_hints;
};


CDVDPlayerAudio::CDVDPlayerAudio(CDVDClock* pClock, CDVDMessageQueue& parent)
: CThread("DVDPlayerAudio")
, m_messageQueue("audio")
, m_messageParent(parent)
, m_dvdAudio((bool&)m_bStop, pClock)
{
  m_pClock = pClock;
  m_pAudioCodec = NULL;
  m_audioClock = 0;
  m_speed = DVD_PLAYSPEED_NORMAL;
  m_stalled = true;
  m_paused = false;
  m_syncState = IDVDStreamPlayer::SYNC_STARTING;
  m_silence = false;
  m_synctype = SYNC_DISCON;
  m_setsynctype = SYNC_DISCON;
  m_prevsynctype = -1;
  m_prevskipped = false;
  m_maxspeedadjust = 0.0;

  m_messageQueue.SetMaxDataSize(6 * 1024 * 1024);
  m_messageQueue.SetMaxTimeSize(8.0);
}

CDVDPlayerAudio::~CDVDPlayerAudio()
{
  StopThread();

  // close the stream, and don't wait for the audio to be finished
  // CloseStream(true);
}

bool CDVDPlayerAudio::AllowDTSHDDecode()
{
#ifdef TARGET_RASPBERRY_PI
  if (g_RBP.RasberryPiVersion() == 1)
    return false;
#endif
  return true;
}

bool CDVDPlayerAudio::OpenStream(CDVDStreamInfo &hints)
{
  CLog::Log(LOGNOTICE, "Finding audio codec for: %i", hints.codec);

  bool allowpassthrough = CSettings::GetInstance().GetBool(CSettings::SETTING_AUDIOOUTPUT_PASSTHROUGH);
  // exceptions for passthrough
  if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEDISPLAYASCLOCK))
    allowpassthrough = false;

#ifdef TARGET_DARWIN_IOS
  // check for changes from/to hdmi -> headphones/bluetooth devices
  // AE can dynamic adapt but the passthrough setting will still bork us
  if (CDarwinUtils::GetAudioRoute().find("HDMI") == std::string::npos)
    allowpassthrough = false;
#endif

  //if (hints.realtime)
  //  allowpassthrough = false;

  CDVDAudioCodec* codec = CDVDFactoryCodec::CreateAudioCodec(hints, allowpassthrough);
  if(!codec)
  {
    CLog::Log(LOGERROR, "Unsupported audio codec");
    return false;
  }

  if(m_messageQueue.IsInited())
    m_messageQueue.Put(new CDVDMsgAudioCodecChange(hints, codec), 0);
  else
  {
    OpenStream(hints, codec);
    m_messageQueue.Init();
    CLog::Log(LOGNOTICE, "Creating audio thread");
    Create();
  }
  return true;
}

void CDVDPlayerAudio::OpenStream(CDVDStreamInfo &hints, CDVDAudioCodec* codec)
{
  SAFE_DELETE(m_pAudioCodec);
  m_pAudioCodec = codec;

  /* store our stream hints */
  m_streaminfo = hints;

  /* update codec information from what codec gave out, if any */
  int channelsFromCodec   = m_pAudioCodec->GetFormat().m_channelLayout.Count();
  int samplerateFromCodec = m_pAudioCodec->GetFormat().m_sampleRate;

  if (channelsFromCodec > 0)
    m_streaminfo.channels = channelsFromCodec;
  if (samplerateFromCodec > 0)
    m_streaminfo.samplerate = samplerateFromCodec;

  /* check if we only just got sample rate, in which case the previous call
   * to CreateAudioCodec() couldn't have started passthrough */
  if (hints.samplerate != m_streaminfo.samplerate)
    SwitchCodecIfNeeded();

  m_audioClock = 0;
  m_stalled = m_messageQueue.GetPacketCount(CDVDMsg::DEMUXER_PACKET) == 0;

  m_synctype = SYNC_DISCON;
  m_setsynctype = SYNC_DISCON;
  if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEDISPLAYASCLOCK))
    m_setsynctype = SYNC_RESAMPLE;
  else if (hints.realtime)
    m_setsynctype = SYNC_RESAMPLE;

  m_prevsynctype = -1;

  m_prevskipped = false;
  m_silence = false;

  m_maxspeedadjust = 5.0;

  m_messageParent.Put(new CDVDMsg(CDVDMsg::PLAYER_AVCHANGE));
  m_syncState = IDVDStreamPlayer::SYNC_STARTING;
}

void CDVDPlayerAudio::CloseStream(bool bWaitForBuffers)
{
  bool bWait = bWaitForBuffers && m_speed > 0 && !CAEFactory::IsSuspended();

  // wait until buffers are empty
  if (bWait)
    m_messageQueue.WaitUntilEmpty();

  // send abort message to the audio queue
  m_messageQueue.Abort();

  CLog::Log(LOGNOTICE, "Waiting for audio thread to exit");

  // shut down the adio_decode thread and wait for it
  StopThread(); // will set this->m_bStop to true

  // destroy audio device
  CLog::Log(LOGNOTICE, "Closing audio device");
  if (bWait)
  {
    m_bStop = false;
    m_dvdAudio.Drain();
    m_bStop = true;
  }
  else
  {
    m_dvdAudio.Flush();
  }

  m_dvdAudio.Destroy();

  // uninit queue
  m_messageQueue.End();

  CLog::Log(LOGNOTICE, "Deleting audio codec");
  if (m_pAudioCodec)
  {
    m_pAudioCodec->Dispose();
    SAFE_DELETE(m_pAudioCodec);
  }
}

void CDVDPlayerAudio::OnStartup()
{
  m_decode.Release();
}

void CDVDPlayerAudio::UpdatePlayerInfo()
{
  std::ostringstream s;
  s << "aq:"     << std::setw(2) << std::min(99,m_messageQueue.GetLevel()) << "%";
  s << ", Kb/s:" << std::fixed << std::setprecision(2) << (double)GetAudioBitrate() / 1024.0;

  //print the inverse of the resample ratio, since that makes more sense
  //if the resample ratio is 0.5, then we're playing twice as fast
  if (m_synctype == SYNC_RESAMPLE)
    s << ", rr:" << std::fixed << std::setprecision(5) << 1.0 / m_dvdAudio.GetResampleRatio();

  s << ", att:" << std::fixed << std::setprecision(1) << log(GetCurrentAttenuation()) * 20.0f << " dB";

  SInfo info;
  info.info        = s.str();
  info.pts         = m_dvdAudio.GetPlayingPts();
  info.passthrough = m_pAudioCodec && m_pAudioCodec->NeedPassthrough();

  { CSingleLock lock(m_info_section);
    m_info = info;
  }
}

void CDVDPlayerAudio::Process()
{
  CLog::Log(LOGNOTICE, "running thread: CDVDPlayerAudio::Process()");

  DVDAudioFrame audioframe;
  m_audioStats.Start();

  while (!m_bStop)
  {
    CDVDMsg* pMsg;
    int timeout  = (int)(1000 * m_dvdAudio.GetCacheTime()) + 100;

    // read next packet and return -1 on error
    int priority = 1;
    //Do we want a new audio frame?
    if (m_syncState == IDVDStreamPlayer::SYNC_STARTING ||              /* when not started */
        ALLOW_AUDIO(m_speed) || /* when playing normally */
        m_speed <  DVD_PLAYSPEED_PAUSE  || /* when rewinding */
        (m_speed >  DVD_PLAYSPEED_NORMAL && m_audioClock < m_pClock->GetClock())) /* when behind clock in ff */
      priority = 0;

    if (m_syncState == IDVDStreamPlayer::SYNC_WAITSYNC)
      priority = 1;

    if (m_paused)
      priority = 1;

    // consider stream stalled if queue is empty
    // we can't sync audio to clock with an empty queue
    if (ALLOW_AUDIO(m_speed) && !m_stalled && !m_paused)
    {
      timeout = 0;
    }

    MsgQueueReturnCode ret = m_messageQueue.Get(&pMsg, timeout, priority);

    if (MSGQ_IS_ERROR(ret))
    {
      CLog::Log(LOGERROR, "Got MSGQ_ABORT or MSGO_IS_ERROR return true");
      break;
    }
    else if (ret == MSGQ_TIMEOUT)
    {
      // Flush as the audio output may keep looping if we don't
      if (ALLOW_AUDIO(m_speed) && !m_stalled && m_syncState == IDVDStreamPlayer::SYNC_INSYNC)
      {
        // while AE sync is active, we still have time to fill buffers
        if (m_syncTimer.IsTimePast())
        {
          CLog::Log(LOGNOTICE, "CDVDPlayerAudio::Process - stream stalled");
          m_stalled = true;
        }
      }
      if (timeout == 0)
        Sleep(10);
      continue;
    }

    // handle messages
    if (pMsg->IsType(CDVDMsg::GENERAL_SYNCHRONIZE))
    {
      if(((CDVDMsgGeneralSynchronize*)pMsg)->Wait( 100, SYNCSOURCE_AUDIO ))
        CLog::Log(LOGDEBUG, "CDVDPlayerAudio - CDVDMsg::GENERAL_SYNCHRONIZE");
      else
        m_messageQueue.Put(pMsg->Acquire(), 1);  // push back as prio message, to process other prio messages
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESYNC))
    { //player asked us to set internal clock
      double pts = static_cast<CDVDMsgDouble*>(pMsg)->m_value;
      CLog::Log(LOGDEBUG, "CDVDPlayerAudio - CDVDMsg::GENERAL_RESYNC(%f)", pts);

      m_audioClock = pts + m_dvdAudio.GetDelay();
      if (m_speed != DVD_PLAYSPEED_PAUSE)
        m_dvdAudio.Resume();
      m_syncState = IDVDStreamPlayer::SYNC_INSYNC;
      m_syncTimer.Set(3000);
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESET))
    {
      if (m_pAudioCodec)
        m_pAudioCodec->Reset();
      m_syncState = IDVDStreamPlayer::SYNC_STARTING;
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_FLUSH))
    {
      bool sync = static_cast<CDVDMsgBool*>(pMsg)->m_value;
      m_dvdAudio.Flush();
      m_stalled = true;
      m_audioClock = 0;

      if (sync)
      {
        m_syncState = IDVDStreamPlayer::SYNC_STARTING;
        m_dvdAudio.Pause();
      }

      if (m_pAudioCodec)
        m_pAudioCodec->Reset();
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_EOF))
    {
      CLog::Log(LOGDEBUG, "CDVDPlayerAudio - CDVDMsg::GENERAL_EOF");
    }
    else if (pMsg->IsType(CDVDMsg::PLAYER_SETSPEED))
    {
      double speed = static_cast<CDVDMsgInt*>(pMsg)->m_value;

      if (ALLOW_AUDIO(speed))
      {
        if (speed != m_speed)
        {
          if (m_syncState == IDVDStreamPlayer::SYNC_INSYNC)
            m_dvdAudio.Resume();
        }
      }
      else
      {
        m_dvdAudio.Pause();
      }
      m_speed = speed;
    }
    else if (pMsg->IsType(CDVDMsg::AUDIO_SILENCE))
    {
      m_silence = static_cast<CDVDMsgBool*>(pMsg)->m_value;
      CLog::Log(LOGDEBUG, "CDVDPlayerAudio - CDVDMsg::AUDIO_SILENCE(%f, %d)"
                , m_audioClock, m_silence);
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_PAUSE))
    {
      m_paused = static_cast<CDVDMsgBool*>(pMsg)->m_value;
      CLog::Log(LOGDEBUG, "CDVDPlayerAudio - CDVDMsg::GENERAL_PAUSE: %d", m_paused);
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_STREAMCHANGE))
    {
      CDVDMsgAudioCodecChange* msg(static_cast<CDVDMsgAudioCodecChange*>(pMsg));
      OpenStream(msg->m_hints, msg->m_codec);
      msg->m_codec = NULL;
    }
    else if (pMsg->IsType(CDVDMsg::DEMUXER_PACKET))
    {
      DemuxPacket* pPacket = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacket();
      bool bPacketDrop  = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacketDrop();

      int consumed = m_pAudioCodec->Decode(pPacket->pData, pPacket->iSize, pPacket->dts, pPacket->pts);
      if (consumed < 0)
      {
        CLog::Log(LOGERROR, "CDVDPlayerAudio::Process - Decode Error. Skipping audio packet (%d)", consumed);
        m_pAudioCodec->Reset();
        pMsg->Release();
        continue;
      }

      m_audioStats.AddSampleBytes(pPacket->iSize);
      UpdatePlayerInfo();

      // loop while no error and decoder produces output
      while (!m_bStop)
      {
        // get decoded data and the size of it
        m_pAudioCodec->GetData(audioframe);

        if (audioframe.nb_frames == 0)
        {
          if (consumed >= pPacket->iSize)
            break;
          int ret = m_pAudioCodec->Decode(pPacket->pData+consumed, pPacket->iSize-consumed, DVD_NOPTS_VALUE, DVD_NOPTS_VALUE);
          if (ret < 0)
          {
            CLog::Log(LOGERROR, "CDVDPlayerAudio::DecodeFrame - Decode Error. Skipping audio packet (%d)", ret);
            m_pAudioCodec->Reset();
            break;
          }
          consumed += ret;
          continue;
        }

        audioframe.hasTimestamp = true;
        if (audioframe.pts == DVD_NOPTS_VALUE)
        {
          audioframe.pts = m_audioClock;
          audioframe.hasTimestamp = false;
        }
        else
        {
          m_audioClock = audioframe.pts;
        }

        //Drop when not playing normally
        if (!ALLOW_AUDIO(m_speed) && m_syncState == IDVDStreamPlayer::SYNC_INSYNC)
        {
          break;
        }

        if (audioframe.format.m_sampleRate && m_streaminfo.samplerate != (int) audioframe.format.m_sampleRate)
        {
          // The sample rate has changed or we just got it for the first time
          // for this stream. See if we should enable/disable passthrough due
          // to it.
          m_streaminfo.samplerate = audioframe.format.m_sampleRate;
          if (SwitchCodecIfNeeded())
          {
            break;
          }
        }

        // demuxer reads metatags that influence channel layout
        if (m_streaminfo.codec == AV_CODEC_ID_FLAC && m_streaminfo.channellayout)
          audioframe.format.m_channelLayout = CAEUtil::GetAEChannelLayout(m_streaminfo.channellayout);

        // we have succesfully decoded an audio frame, setup renderer to match
        if (!m_dvdAudio.IsValidFormat(audioframe))
        {
          if(m_speed)
            m_dvdAudio.Drain();

          m_dvdAudio.Destroy();

          if (!m_dvdAudio.Create(audioframe, m_streaminfo.codec, m_setsynctype == SYNC_RESAMPLE))
            CLog::Log(LOGERROR, "%s - failed to create audio renderer", __FUNCTION__);

          if (m_syncState == IDVDStreamPlayer::SYNC_INSYNC)
            m_dvdAudio.Resume();

          m_streaminfo.channels = audioframe.format.m_channelLayout.Count();

          m_messageParent.Put(new CDVDMsg(CDVDMsg::PLAYER_AVCHANGE));
        }

        // Zero out the frame data if we are supposed to silence the audio
        if (m_silence)
        {
          int size = audioframe.nb_frames * audioframe.framesize / audioframe.planes;
          for (unsigned int i=0; i<audioframe.planes; i++)
            memset(audioframe.data[i], 0, size);
        }

        SetSyncType(audioframe.passthrough);

        if (!bPacketDrop)
        {
          OutputPacket(audioframe);

          // signal to our parent that we have initialized
          if(m_syncState == IDVDStreamPlayer::SYNC_STARTING)
          {
            double cachetotal = DVD_SEC_TO_TIME(m_dvdAudio.GetCacheTotal());
            double cachetime = m_dvdAudio.GetDelay();
            if (cachetime >= cachetotal * 0.5)
            {
              m_syncState = IDVDStreamPlayer::SYNC_WAITSYNC;
              m_stalled = false;
              SStartMsg msg;
              msg.player = DVDPLAYER_AUDIO;
              msg.cachetotal = cachetotal;
              msg.cachetime = cachetime;
              msg.timestamp = audioframe.hasTimestamp ? audioframe.pts : DVD_NOPTS_VALUE;
              m_messageParent.Put(new CDVDMsgType<SStartMsg>(CDVDMsg::PLAYER_STARTED, msg));

              if (consumed < pPacket->iSize)
              {
                pPacket->iSize -= consumed;
                memmove(pPacket->pData, pPacket->pData + consumed, pPacket->iSize);
                m_messageQueue.Put(pMsg, 0, false);
                pMsg->Acquire();
                break;
              }
            }
          }
        }

        // guess next pts
        m_audioClock += audioframe.duration;

        int ret = m_pAudioCodec->Decode(nullptr, 0, DVD_NOPTS_VALUE, DVD_NOPTS_VALUE);
        if (ret < 0)
        {
          CLog::Log(LOGERROR, "CDVDPlayerAudio::Process - Decode Error. Skipping audio packet (%d)", ret);
          m_pAudioCodec->Reset();
          break;
        }
      } // while decoder produces output

    } // demuxer packet
    
    pMsg->Release();
  }
}

void CDVDPlayerAudio::SetSyncType(bool passthrough)
{
  //set the synctype from the gui
  m_synctype = m_setsynctype;
  if (passthrough && m_synctype == SYNC_RESAMPLE)
    m_synctype = SYNC_DISCON;

  //if SetMaxSpeedAdjust returns false, it means no video is played and we need to use clock feedback
  double maxspeedadjust = 0.0;
  if (m_synctype == SYNC_RESAMPLE)
    maxspeedadjust = m_maxspeedadjust;

  m_pClock->SetMaxSpeedAdjust(maxspeedadjust);

  if (m_synctype != m_prevsynctype)
  {
    const char *synctypes[] = {"clock feedback", "resample", "invalid"};
    int synctype = (m_synctype >= 0 && m_synctype <= 1) ? m_synctype : 2;
    CLog::Log(LOGDEBUG, "CDVDPlayerAudio:: synctype set to %i: %s", m_synctype, synctypes[synctype]);
    m_prevsynctype = m_synctype;
    if (m_synctype == SYNC_RESAMPLE)
      m_dvdAudio.SetResampleMode(1);
    else
      m_dvdAudio.SetResampleMode(0);
  }
}

bool CDVDPlayerAudio::OutputPacket(DVDAudioFrame &audioframe)
{
  double syncerror = m_dvdAudio.GetSyncError();

  if (m_synctype == SYNC_DISCON && fabs(syncerror) > DVD_MSEC_TO_TIME(32))
  {
    double limit, error;
    limit = DVD_MSEC_TO_TIME(32);
    error = syncerror;

    double absolute;
    double clock = m_pClock->GetClock(absolute);
    if (m_pClock->Update(clock + error, absolute, limit - 0.001, "CDVDPlayerAudio::OutputPacket"))
    {
      m_dvdAudio.SetSyncErrorCorrection(-error);
    }
  }
  m_dvdAudio.AddPackets(audioframe);

  return true;
}

void CDVDPlayerAudio::OnExit()
{
  CLog::Log(LOGNOTICE, "thread end: CDVDPlayerAudio::OnExit()");
}

void CDVDPlayerAudio::SetSpeed(int speed)
{
  if(m_messageQueue.IsInited())
    m_messageQueue.Put( new CDVDMsgInt(CDVDMsg::PLAYER_SETSPEED, speed), 1 );
  else
    m_speed = speed;
}

void CDVDPlayerAudio::Flush(bool sync)
{
  m_messageQueue.Flush();
  m_messageQueue.Put( new CDVDMsgBool(CDVDMsg::GENERAL_FLUSH, sync), 1);

  m_dvdAudio.AbortAddPackets();
}

void CDVDPlayerAudio::WaitForBuffers()
{
  // make sure there are no more packets available
  m_messageQueue.WaitUntilEmpty();

  // make sure almost all has been rendered
  // leave 500ms to avound buffer underruns
  double delay = m_dvdAudio.GetCacheTime();
  if(delay > 0.5)
    Sleep((int)(1000 * (delay - 0.5)));
}

bool CDVDPlayerAudio::AcceptsData() const
{
  bool full = m_messageQueue.IsFull();
  return !full;
}

bool CDVDPlayerAudio::SwitchCodecIfNeeded()
{
  CLog::Log(LOGDEBUG, "CDVDPlayerAudio: Sample rate changed, checking for passthrough");
/*
  bool allowpassthrough = !CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEDISPLAYASCLOCK);
  if (m_streaminfo.realtime)
    allowpassthrough = false;
  CDVDAudioCodec *codec = CDVDFactoryCodec::CreateAudioCodec(m_streaminfo, allowpassthrough, AllowDTSHDDecode());
*/
  CDVDAudioCodec *codec = CDVDFactoryCodec::CreateAudioCodec(m_streaminfo);
  if (!codec || codec->NeedPassthrough() == m_pAudioCodec->NeedPassthrough()) {
    // passthrough state has not changed
    SAFE_DELETE(codec);
    return false;
  }

  SAFE_DELETE(m_pAudioCodec);
  m_pAudioCodec = codec;

  return true;
}

std::string CDVDPlayerAudio::GetPlayerInfo()
{
  CSingleLock lock(m_info_section);
  return m_info.info;
}

int CDVDPlayerAudio::GetAudioBitrate()
{
  return (int)m_audioStats.GetBitrate();
}

int CDVDPlayerAudio::GetAudioChannels()
{
  return m_streaminfo.channels;
}

bool CDVDPlayerAudio::IsPassthrough() const
{
  CSingleLock lock(m_info_section);
  return m_info.passthrough;
}

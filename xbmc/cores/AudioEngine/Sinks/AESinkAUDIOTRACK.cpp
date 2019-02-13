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

#include "AESinkAUDIOTRACK.h"
#include "cores/AudioEngine/Utils/AEUtil.h"
#include "platform/android/activity/XBMCApp.h"
#include "platform/android/activity/AndroidFeatures.h"
#include "settings/Settings.h"
#include "settings/AdvancedSettings.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtils.h"
#include "utils/log.h"

#include <androidjni/AudioFormat.h>
#include <androidjni/AudioManager.h>
#include <androidjni/AudioTrack.h>
#include <androidjni/Build.h>
#include <androidjni/System.h>

#include <algorithm>

using namespace jni;

const int SMOOTHED_DELAY_MAX = 10;
const size_t MOVING_AVERAGE_MAX_MEMBERS = 5;
const uint64_t UINT64_LOWER_BYTES = 0x00000000FFFFFFFF;
const uint64_t UINT64_UPPER_BYTES = 0xFFFFFFFF00000000;
/*
 * ADT-1 on L preview as of 2014-10 downmixes all non-5.1/7.1 content
 * to stereo, so use 7.1 or 5.1 for all multichannel content for now to
 * avoid that (except passthrough).
 * If other devices surface that support other multichannel layouts,
 * this should be disabled or adapted accordingly.
 */
#define LIMIT_TO_STEREO_AND_5POINT1_AND_7POINT1 1

static const AEChannel KnownChannels[] = {
  AE_CH_FL, AE_CH_FR, AE_CH_FC, AE_CH_LFE, AE_CH_SL, AE_CH_SR, AE_CH_BL, AE_CH_BR, AE_CH_BC, AE_CH_BLOC, AE_CH_BROC, AE_CH_NULL
};

static bool Has71Support()
{
  /* Android 5.0 introduced side channels */
  return CJNIAudioManager::GetSDKVersion() >= 21;
}

static void DumpPossibleATFormats()
{

  CLog::Log(LOGINFO, "AESinkAUDIOTRACK - CJNIAudioManager::GetSDKVersion %d",   CJNIAudioManager::GetSDKVersion());
  CLog::Log(LOGINFO, "AESinkAUDIOTRACK - DumpATFormats: ENCODING_PCM_16BIT %d", CJNIAudioFormat::ENCODING_PCM_16BIT);
  CLog::Log(LOGINFO, "AESinkAUDIOTRACK - DumpATFormats: ENCODING_PCM_FLOAT %d", CJNIAudioFormat::ENCODING_PCM_FLOAT);

  CLog::Log(LOGINFO, "AESinkAUDIOTRACK - DumpATFormats: ENCODING_AC3 %d",       CJNIAudioFormat::ENCODING_AC3);
  CLog::Log(LOGINFO, "AESinkAUDIOTRACK - DumpATFormats: ENCODING_E_AC3 %d",     CJNIAudioFormat::ENCODING_E_AC3);
  CLog::Log(LOGINFO, "AESinkAUDIOTRACK - DumpATFormats: ENCODING_DOLBY_TRUEHD %d", CJNIAudioFormat::ENCODING_DOLBY_TRUEHD);

  CLog::Log(LOGINFO, "AESinkAUDIOTRACK - DumpATFormats: ENCODING_DTS %d",       CJNIAudioFormat::ENCODING_DTS);
  CLog::Log(LOGINFO, "AESinkAUDIOTRACK - DumpATFormats: ENCODING_DTS_HD %d",    CJNIAudioFormat::ENCODING_DTS_HD);

  CLog::Log(LOGINFO, "AESinkAUDIOTRACK - DumpATFormats: ENCODING_IEC61937 %d",  CJNIAudioFormat::ENCODING_IEC61937);
}

static AEChannel AUDIOTRACKChannelToAEChannel(int atChannel)
{
  AEChannel aeChannel;

  /* cannot use switch since CJNIAudioFormat is populated at runtime */

       if (atChannel == CJNIAudioFormat::CHANNEL_OUT_FRONT_LEFT)            aeChannel = AE_CH_FL;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_FRONT_RIGHT)           aeChannel = AE_CH_FR;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_FRONT_CENTER)          aeChannel = AE_CH_FC;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_LOW_FREQUENCY)         aeChannel = AE_CH_LFE;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_BACK_LEFT)             aeChannel = AE_CH_BL;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_BACK_RIGHT)            aeChannel = AE_CH_BR;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_SIDE_LEFT)             aeChannel = AE_CH_SL;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_SIDE_RIGHT)            aeChannel = AE_CH_SR;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_FRONT_LEFT_OF_CENTER)  aeChannel = AE_CH_FLOC;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_FRONT_RIGHT_OF_CENTER) aeChannel = AE_CH_FROC;
  else if (atChannel == CJNIAudioFormat::CHANNEL_OUT_BACK_CENTER)           aeChannel = AE_CH_BC;
  else                                                                      aeChannel = AE_CH_UNKNOWN1;

  return aeChannel;
}

static int AEChannelToAUDIOTRACKChannel(AEChannel aeChannel)
{
  int atChannel;
  switch (aeChannel)
  {
    case AE_CH_FL:    atChannel = CJNIAudioFormat::CHANNEL_OUT_FRONT_LEFT; break;
    case AE_CH_FR:    atChannel = CJNIAudioFormat::CHANNEL_OUT_FRONT_RIGHT; break;
    case AE_CH_FC:    atChannel = CJNIAudioFormat::CHANNEL_OUT_FRONT_CENTER; break;
    case AE_CH_LFE:   atChannel = CJNIAudioFormat::CHANNEL_OUT_LOW_FREQUENCY; break;
    case AE_CH_BL:    atChannel = CJNIAudioFormat::CHANNEL_OUT_BACK_LEFT; break;
    case AE_CH_BR:    atChannel = CJNIAudioFormat::CHANNEL_OUT_BACK_RIGHT; break;
    case AE_CH_SL:    atChannel = CJNIAudioFormat::CHANNEL_OUT_SIDE_LEFT; break;
    case AE_CH_SR:    atChannel = CJNIAudioFormat::CHANNEL_OUT_SIDE_RIGHT; break;
    case AE_CH_BC:    atChannel = CJNIAudioFormat::CHANNEL_OUT_BACK_CENTER; break;
    case AE_CH_FLOC:  atChannel = CJNIAudioFormat::CHANNEL_OUT_FRONT_LEFT_OF_CENTER; break;
    case AE_CH_FROC:  atChannel = CJNIAudioFormat::CHANNEL_OUT_FRONT_RIGHT_OF_CENTER; break;
    default:          atChannel = CJNIAudioFormat::CHANNEL_INVALID; break;
  }
  return atChannel;
}

static CAEChannelInfo AUDIOTRACKChannelMaskToAEChannelMap(int atMask)
{
  CAEChannelInfo info;

  int mask = 0x1;
  for (size_t i = 0; i < sizeof(int32_t) * 8; i++)
  {
    if (atMask & mask)
      info += AUDIOTRACKChannelToAEChannel(mask);
    mask <<= 1;
  }

  return info;
}

static int AEChannelMapToAUDIOTRACKChannelMask(CAEChannelInfo info)
{
#ifdef LIMIT_TO_STEREO_AND_5POINT1_AND_7POINT1
  if (info.Count() > 6 && Has71Support())
    return CJNIAudioFormat::CHANNEL_OUT_5POINT1
         | CJNIAudioFormat::CHANNEL_OUT_SIDE_LEFT
         | CJNIAudioFormat::CHANNEL_OUT_SIDE_RIGHT;
  else if (info.Count() > 2)
    return CJNIAudioFormat::CHANNEL_OUT_5POINT1;
  else
    return CJNIAudioFormat::CHANNEL_OUT_STEREO;
#endif

  info.ResolveChannels(KnownChannels);

  int atMask = 0;

  for (size_t i = 0; i < info.Count(); i++)
    atMask |= AEChannelToAUDIOTRACKChannel(info[i]);

  return atMask;
}

jni::CJNIAudioTrack *CAESinkAUDIOTRACK::CreateAudioTrack(int stream, int sampleRate, int channelMask, int encoding, int bufferSize)
{
  jni::CJNIAudioTrack *jniAt = nullptr;
  m_jniAudioFormat = encoding;

  try
  {
    jniAt = new CJNIAudioTrack(stream, sampleRate, channelMask, encoding, bufferSize, CJNIAudioTrack::MODE_STREAM);
  }
  catch (const std::invalid_argument& e)
  {
    CLog::Log(LOGINFO, "AESinkAUDIOTRACK - AudioTrack creation (channelMask 0x%08x): %s", channelMask, e.what());
    jniAt = nullptr;
  }

  return jniAt;
}

int CAESinkAUDIOTRACK::AudioTrackWrite(char* audioData, int offsetInBytes, int sizeInBytes)
{
  int     written = 0;
  if (CJNIBase::GetSDKVersion() >= 21 && m_jniAudioFormat == CJNIAudioFormat::ENCODING_PCM_FLOAT)
  {
    if (m_floatbuf.size() != (sizeInBytes - offsetInBytes) / sizeof(float))
      m_floatbuf.resize((sizeInBytes - offsetInBytes) / sizeof(float));
    memcpy(m_floatbuf.data(), audioData + offsetInBytes, sizeInBytes - offsetInBytes);
    written = m_at_jni->write(m_floatbuf, 0, (sizeInBytes - offsetInBytes) / sizeof(float), CJNIAudioTrack::WRITE_BLOCKING);
    written *= sizeof(float);
  }
  else if (m_jniAudioFormat == CJNIAudioFormat::ENCODING_IEC61937)
  {
    if (m_shortbuf.size() != (sizeInBytes - offsetInBytes) / sizeof(int16_t))
      m_shortbuf.resize((sizeInBytes - offsetInBytes) / sizeof(int16_t));
    memcpy(m_shortbuf.data(), audioData + offsetInBytes, sizeInBytes - offsetInBytes);
    if (CJNIBase::GetSDKVersion() >= 23)
      written = m_at_jni->write(m_shortbuf, 0, (sizeInBytes - offsetInBytes) / sizeof(int16_t), CJNIAudioTrack::WRITE_BLOCKING);
    else
      written = m_at_jni->write(m_shortbuf, 0, (sizeInBytes - offsetInBytes) / sizeof(int16_t));
    written *= sizeof(uint16_t);
  }
  else
  {
    if (m_charbuf.size() != (sizeInBytes - offsetInBytes))
      m_charbuf.resize(sizeInBytes - offsetInBytes);
    memcpy(m_charbuf.data(), audioData + offsetInBytes, sizeInBytes - offsetInBytes);
    if (CJNIBase::GetSDKVersion() >= 23)
      written = m_at_jni->write(m_charbuf, 0, sizeInBytes - offsetInBytes, CJNIAudioTrack::WRITE_BLOCKING);
    else
      written = m_at_jni->write(m_charbuf, 0, sizeInBytes - offsetInBytes);
  }

  return written;
}

int CAESinkAUDIOTRACK::AudioTrackWrite(char* audioData, int sizeInBytes, int64_t timestamp)
{
  int     written = 0;
  std::vector<char> buf;
  buf.reserve(sizeInBytes);
  memcpy(buf.data(), audioData, sizeInBytes);

  CJNIByteBuffer bytebuf = CJNIByteBuffer::wrap(buf);
  written = m_at_jni->write(bytebuf.get_raw(), sizeInBytes, CJNIAudioTrack::WRITE_NON_BLOCKING, timestamp);

  return written;
}

int CAESinkAUDIOTRACK::m_sdk;
CAEDeviceInfo CAESinkAUDIOTRACK::m_info;
std::set<int> CAESinkAUDIOTRACK::m_sink_sampleRates;

////////////////////////////////////////////////////////////////////////////////////////////
CAESinkAUDIOTRACK::CAESinkAUDIOTRACK()
: m_at_jni(nullptr)
, m_volume(-1)
, m_encoding(CJNIAudioFormat::ENCODING_PCM_16BIT)
, m_passthrough(false)
, m_passthroughIsIECPacked(false)
, m_sink_frameSize(0)
, m_sink_sampleRate(0)
, m_sink_bufferSize(0)
, m_sink_bufferSeconds(0)
, m_sink_sleepOnWriteStall(0)
, m_sink_bufferBytesPerSecond(0)
, m_writeSeconds(0.0)
, m_playbackHead(0)
, m_playbackHeadOffset(-1)
, m_lastdelay(0.0)
{
  m_nonIECPauseTimer.SetExpired();
}

CAESinkAUDIOTRACK::~CAESinkAUDIOTRACK()
{
  Deinitialize();
}

bool CAESinkAUDIOTRACK::IsSupported(int sampleRateInHz, int channelConfig, int encoding)
{
  int ret = CJNIAudioTrack::getMinBufferSize(sampleRateInHz, channelConfig, encoding);
  return (ret > 0);
}

bool CAESinkAUDIOTRACK::Initialize(AEAudioFormat &format, std::string &device)
{
//  DumpPossibleATFormats();

  m_format = format;
  m_volume = -1;
  m_writeSeconds = 0.0;
  m_playbackHead = 0;
  m_playbackHeadOffset = -1;
  m_lastdelay = 0.0;
  m_linearmovingaverage.clear();
  m_nonIECPauseTimer.SetExpired();

  CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::Initialize requested: sampleRate %u; format: %s; type: %s; channels: %d",
    format.m_sampleRate,
    CAEUtil::DataFormatToStr(format.m_dataFormat),
    CAEUtil::StreamTypeToStr(format.m_streamInfo.m_type),
    format.m_channelLayout.Count());

  int stream = CJNIAudioManager::STREAM_MUSIC;

  // Get equal or lower supported sample rate
  uint32_t distance = UINT32_MAX; // max upper distance, update at least ones to use one of our samplerates
  for (auto& s : m_sink_sampleRates)
  {
    // prefer best match or alternatively something that divides nicely and is not too far away
    uint32_t d = std::abs((int)m_format.m_sampleRate - s) + 8 * (s > (int)m_format.m_sampleRate ? (s % m_format.m_sampleRate) : (m_format.m_sampleRate % s));
    if (d < distance)
    {
      m_sink_sampleRate = s;
      distance = d;
    }
  }

  int atChannelMask = AEChannelMapToAUDIOTRACKChannelMask(m_format.m_channelLayout);
  m_format.m_channelLayout  = AUDIOTRACKChannelMaskToAEChannelMap(atChannelMask);

  if (m_format.m_dataFormat == AE_FMT_RAW && !CXBMCApp::IsHeadsetPlugged())
  {
    m_passthrough = true;
    m_passthroughIsIECPacked = m_format.m_streamInfo.m_IECPacked;
    m_sink_sleepOnWriteStall = m_format.m_streamInfo.GetDuration();

    // setup defaults for IEC packed format
    m_format.m_sampleRate = m_sink_sampleRate;
    m_format.m_frameSize = 1;
    m_encoding = CJNIAudioFormat::ENCODING_PCM_16BIT;

    if (CJNIAudioManager::GetSDKVersion() >= 23)
    {
      CJNIAudioDeviceInfos audiodevices = CXBMCApp::GetAudioDeviceInfos();
      for (auto dev : audiodevices)
      {
        if (dev.getType() == CJNIAudioDeviceInfo::TYPE_HDMI || dev.getType() == CJNIAudioDeviceInfo::TYPE_HDMI_ARC)
        {
          for (auto enc : dev.getEncodings())
          {
            if (enc == CJNIAudioFormat::ENCODING_IEC61937)
            {
              // defaults for ENCODING_IEC61937
              m_encoding = CJNIAudioFormat::ENCODING_IEC61937;
            }
          }
        }
      }
    }
    else if (CJNIAudioFormat::ENCODING_IEC61937 != -1)
    {
      // defaults for ENCODING_IEC61937
      m_encoding = CJNIAudioFormat::ENCODING_IEC61937;
    }

    if (m_passthroughIsIECPacked)
    {
      m_sink_frameSize = 2 * 2;
      atChannelMask = CJNIAudioFormat::CHANNEL_OUT_STEREO;
      switch (m_format.m_streamInfo.m_type)
      {
        // Digital Dolby
        case CAEStreamInfo::STREAM_TYPE_AC3:
          m_format.m_frames = 1536 * 4;
          break;
        case CAEStreamInfo::STREAM_TYPE_EAC3:
          m_format.m_frames = 6144 * 4;
          break;
        case CAEStreamInfo::STREAM_TYPE_TRUEHD:
          m_sink_frameSize = 8 * 2;
          atChannelMask = CJNIAudioFormat::CHANNEL_OUT_7POINT1_SURROUND;
          m_format.m_frames = 15360 * 4;
          break;

          // DTS
        case CAEStreamInfo::STREAM_TYPE_DTS_512:
        case CAEStreamInfo::STREAM_TYPE_DTSHD_CORE:
          m_format.m_frames = 512 * 4;
          break;
        case CAEStreamInfo::STREAM_TYPE_DTS_1024:
          m_format.m_frames = 1024 * 4;
          break;
        case CAEStreamInfo::STREAM_TYPE_DTS_2048:
          m_format.m_frames = 2048 * 4;
          break;
        case CAEStreamInfo::STREAM_TYPE_DTSHD:
          m_format.m_frames = m_format.m_streamInfo.m_dtsPeriod * 4 * 4;;
          break;
        case CAEStreamInfo::STREAM_TYPE_DTSHD_MA:
          m_sink_frameSize = 8 * 2;
          atChannelMask = CJNIAudioFormat::CHANNEL_OUT_7POINT1_SURROUND;
          m_format.m_frames = m_format.m_streamInfo.m_dtsPeriod * 4 * 4;;
          break;

        default:
          CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::Initialize unknown stream type %s",
                    CAEUtil::StreamTypeToStr(m_format.m_streamInfo.m_type));
          return false;
          break;
      }
      m_format.m_frames *= 2;
    }
    else
    {
      m_sink_frameSize = 1;
      m_sink_sampleRate = m_format.m_streamInfo.m_sampleRate;

      switch (m_format.m_streamInfo.m_type)
      {
        // Digital Dolby
        case CAEStreamInfo::STREAM_TYPE_AC3:
          m_format.m_frames = m_format.m_streamInfo.m_frameSize;
          if (m_format.m_frames == 0)
            m_format.m_frames = 1536;
          m_format.m_frames *= 2;
          if (!m_passthroughIsIECPacked)
            m_encoding = CJNIAudioFormat::ENCODING_AC3;
          break;
        case CAEStreamInfo::STREAM_TYPE_EAC3:
          m_format.m_frames = 10752;
          if (!m_passthroughIsIECPacked)
            m_encoding = CJNIAudioFormat::ENCODING_E_AC3;
          break;
        case CAEStreamInfo::STREAM_TYPE_TRUEHD:
          m_format.m_frames = 61440;
          if (!m_passthroughIsIECPacked && CJNIAudioFormat::ENCODING_DOLBY_TRUEHD != -1)
            m_encoding = CJNIAudioFormat::ENCODING_DOLBY_TRUEHD;
          break;

          // DTS
        case CAEStreamInfo::STREAM_TYPE_DTS_512:
        case CAEStreamInfo::STREAM_TYPE_DTSHD_CORE:
          m_format.m_frames = 2012;
          m_format.m_frames *= 2;
          if (!m_passthroughIsIECPacked)
            m_encoding = CJNIAudioFormat::ENCODING_DTS;
          break;
        case CAEStreamInfo::STREAM_TYPE_DTS_1024:
          m_format.m_frames = 4 * 5462;
          if (!m_passthroughIsIECPacked)
            m_encoding = CJNIAudioFormat::ENCODING_DTS;
          break;
        case CAEStreamInfo::STREAM_TYPE_DTS_2048:
          m_format.m_frames = 4 * 5462;
          if (!m_passthroughIsIECPacked)
            m_encoding = CJNIAudioFormat::ENCODING_DTS;
          break;
        case CAEStreamInfo::STREAM_TYPE_DTSHD_MA:
        case CAEStreamInfo::STREAM_TYPE_DTSHD:
          m_format.m_frames = 66432;
          if (!m_passthroughIsIECPacked && CJNIAudioFormat::ENCODING_DTS_HD != -1)
            m_encoding = CJNIAudioFormat::ENCODING_DTS_HD;
          break;

        default:
          CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::Initialize unknown stream type %s",
                    CAEUtil::StreamTypeToStr(m_format.m_streamInfo.m_type));
          return false;
          break;
      }
    }
  }
  else
  {
    m_passthrough = false;
    m_format.m_sampleRate = m_sink_sampleRate;
    if (m_sdk >= 21 && m_format.m_channelLayout.Count() == 2)
    {
      m_format.m_dataFormat = AE_FMT_FLOAT;
      m_encoding = CJNIAudioFormat::ENCODING_PCM_FLOAT;
    }
    else
    {
      m_format.m_dataFormat = AE_FMT_S16LE;
      m_encoding = CJNIAudioFormat::ENCODING_PCM_16BIT;
    }
    m_sink_frameSize = m_format.m_frameSize = m_format.m_channelLayout.Count() * (CAEUtil::DataFormatToBits(m_format.m_dataFormat) >> 3);
    m_format.m_frames = 0.050 * m_format.m_sampleRate;  // 50ms
    m_sink_sleepOnWriteStall = (double) m_format.m_frames / m_format.m_frameSize / 2.0 / (double) m_format.m_sampleRate * 1000;
  }

  int min_bufferSize = 0;
  while (!m_at_jni)
  {
    min_bufferSize = CJNIAudioTrack::getMinBufferSize(m_sink_sampleRate, atChannelMask, m_encoding);
    if (min_bufferSize < 0)
    {
      CLog::Log(LOGERROR, "Minimum Buffer Size was: %d - disable passthrough (?) your hw does not support it", min_bufferSize);
      CLog::Log(LOGERROR, "m_sink_sampleRate %d - atChannelMask %d - m_encoding %d", m_sink_sampleRate, atChannelMask, m_encoding);
      return false;
    }
    min_bufferSize += m_sink_frameSize - (min_bufferSize % m_sink_frameSize);  // Be sure the min buffer is a multiple of the frame size
    m_sink_bufferSize = std::max(min_bufferSize, std::min(min_bufferSize * 2, (int)(m_format.m_frames * m_sink_frameSize)));

    m_at_jni = CreateAudioTrack(stream, m_sink_sampleRate, atChannelMask, m_encoding, m_sink_bufferSize);

    if (!IsInitialized())
    {
      if (!m_passthrough)
      {
        if (atChannelMask != CJNIAudioFormat::CHANNEL_OUT_STEREO &&
            atChannelMask != CJNIAudioFormat::CHANNEL_OUT_5POINT1)
        {
          atChannelMask = CJNIAudioFormat::CHANNEL_OUT_5POINT1;
          CLog::Log(LOGDEBUG, "AESinkAUDIOTRACK - Retrying multichannel playback with a 5.1 layout");
          continue;
        }
        else if (atChannelMask != CJNIAudioFormat::CHANNEL_OUT_STEREO)
        {
          atChannelMask = CJNIAudioFormat::CHANNEL_OUT_STEREO;
          CLog::Log(LOGDEBUG, "AESinkAUDIOTRACK - Retrying with a stereo layout");
          continue;
        }
      }
      CLog::Log(LOGERROR, "AESinkAUDIOTRACK - Unable to create AudioTrack");
      Deinitialize();
      return false;
    }
  }
  m_sink_bufferSize = m_at_jni->getBufferSizeInFrames() * m_sink_frameSize;

  format = m_format;
  m_sink_bufferSeconds = (double)(m_sink_bufferSize / m_sink_frameSize) / (double)m_sink_sampleRate;

  const char *method = m_passthrough ? (m_passthroughIsIECPacked ? "IEC (PT)" : "RAW (PT)") : "PCM";
  CLog::Log(LOGNOTICE, "CAESinkAUDIOTRACK::Created with: "
    "sink# encoding: %s, sample rate: %u, frame size: %u, buffer size: %u(%d) // AE# format: %s, method: %s, stream-type: %s, m_frames: %u m_frameSize: %u channels: %d buffer (ms) %f",
    CXBMCApp::audioencode2string(m_encoding), m_sink_sampleRate, m_sink_frameSize, m_sink_bufferSize, min_bufferSize,
            CAEUtil::DataFormatToStr(m_format.m_dataFormat), method, m_passthrough ? CAEUtil::StreamTypeToStr(m_format.m_streamInfo.m_type) : "PCM-STREAM",
            m_format.m_frames, m_format.m_frameSize, m_format.m_channelLayout.Count(), m_sink_bufferSeconds * 1000);

  // Force volume to 100% for passthrough
  if (m_passthrough && (m_passthroughIsIECPacked && m_encoding != CJNIAudioFormat::ENCODING_IEC61937))
  {
    CXBMCApp::get()->AcquireAudioFocus();
    m_volume = CXBMCApp::get()->GetSystemVolume();
    CXBMCApp::get()->SetSystemVolume(1.0);
  }

  return true;
}

void CAESinkAUDIOTRACK::Deinitialize()
{
  if (m_at_jni)
  {
    if (IsInitialized())
    {
      m_at_jni->stop();
      m_at_jni->flush();
    }
    m_at_jni->release();
    SAFE_DELETE(m_at_jni);
  }

  // Restore volume
  if (m_volume != -1)
  {
    CXBMCApp::get()->SetSystemVolume(m_volume);
    CXBMCApp::get()->ReleaseAudioFocus();
    m_volume = -1;
  }

  m_writeSeconds = 0.0;
  m_playbackHead = 0;
  m_playbackHeadOffset = -1;
  m_lastdelay = 0.0;
  m_linearmovingaverage.clear();
  m_nonIECPauseTimer.SetExpired();
}

bool CAESinkAUDIOTRACK::IsInitialized()
{
  return (m_at_jni && m_at_jni->getState() == CJNIAudioTrack::STATE_INITIALIZED);
}

void CAESinkAUDIOTRACK::GetDelay(AEDelayStatus& status)
{
  if (!m_at_jni)
    return status.SetDelay(0);

  uint64_t head_pos = 0;
  double frameDiffMilli = 0;

  if (CJNIBuild::SDK_INT >= 23)
  {
    CJNIAudioTimestamp ts;
    int64_t systime = CJNISystem::nanoTime();
    if (m_at_jni->getTimestamp(ts))
    {
      head_pos = ts.get_framePosition();
      frameDiffMilli = (systime - ts.get_nanoTime()) / 1000000000.0;

      if (g_advancedSettings.CanLogComponent(LOGAUDIO))
        CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::GetDelay timestamp: pos(%lld) time(%lld) diff(%f)", ts.get_framePosition(), ts.get_nanoTime(), frameDiffMilli);
    }
  }
  if (!head_pos)
  {
    // In their infinite wisdom, Google decided to make getPlaybackHeadPosition
    // return a 32bit "int" that you should "interpret as unsigned."  As such,
    // for wrap saftey, we need to do all ops on it in 32bit integer math.
    head_pos = (uint32_t)m_at_jni->getPlaybackHeadPosition();
    while (m_playbackHead > head_pos)
      head_pos += (1 << 31);

    if (g_advancedSettings.CanLogComponent(LOGAUDIO))
      CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::GetDelay head position: pos(%lld)", head_pos);
  }
  m_playbackHead = head_pos;
  if (m_playbackHeadOffset == -1)
    m_playbackHeadOffset = head_pos;
  else
    m_playbackHead -= m_playbackHeadOffset;

  double headSeconds = (double)m_playbackHead / m_sink_sampleRate;
  if (headSeconds > m_writeSeconds)
  {
    // this should never happend, head should always
    // be less than or equal to what we have written.
    CLog::Log(LOGERROR, "AESinkAUDIOTRACK::GetDelay over-write error, "
      "frameSize=%d, headSeconds=%f, m_writeSeconds=%f", m_sink_frameSize, headSeconds, m_writeSeconds);
    status.SetDelay(0);
    return;
  }

  double delay = m_writeSeconds - headSeconds;

  if (m_nonIECPauseTimer.MillisLeft() > 0)
  {
    double delay = GetMovingAverageDelay(GetCacheTotal());
    m_lastdelay = delay;
    if (g_advancedSettings.CanLogComponent(LOGAUDIO))
      CLog::Log(LOGDEBUG, "AESinkAUDIOTRACK::GetDelay "
        "m_nonIECPauseTimer.MillisLeft=%d, delay=%f", m_nonIECPauseTimer.MillisLeft(), delay);
    status.SetDelay(delay);
    return;
  }

  if (g_advancedSettings.CanLogComponent(LOGAUDIO))
    CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::GetDelay "
      "headSeconds=%f, writeSeconds=%f, delay=%f",
       headSeconds, m_writeSeconds, delay);

  if (delay < 0)
    delay = 0;

  const double d = GetMovingAverageDelay(delay);
  m_lastdelay = d;
  if (g_advancedSettings.CanLogComponent(LOGAUDIO))
    CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::GetDelay frameSize=%d, maveraged=%f, d=%f", m_sink_frameSize, d, delay);
  status.SetDelay(d);
}

double CAESinkAUDIOTRACK::GetLatency()
{
  return 0.0;
}

double CAESinkAUDIOTRACK::GetCacheTotal()
{
  // total amount that the audio sink can buffer in units of seconds
  return m_sink_bufferSeconds;
}

// this method is supposed to block until all frames are written to the device buffer
// when it returns ActiveAESink will take the next buffer out of a queue
unsigned int CAESinkAUDIOTRACK::AddPackets(uint8_t **data, unsigned int frames, unsigned int offset, int64_t timestamp)
{
  if (!IsInitialized())
    return INT_MAX;

  // for debugging only - can be removed if everything is really stable
  uint64_t startTime = CurrentHostCounter();

  uint8_t *buffer = data[0] + (offset * m_format.m_frameSize);
  uint8_t *out_buf = buffer;
  int size = frames * m_format.m_frameSize;

  // write as many frames of audio as we can fit into the audiotrack buffer.
  int written = 0;
  int loop_written = 0;
  if (frames)
  {
    if (m_nonIECPauseTimer.MillisLeft() > 0)
    {
      double sleeptime = std::min((double) m_nonIECPauseTimer.MillisLeft(), m_format.m_streamInfo.GetDuration());
      //CLog::Log(LOGDEBUG, "AESinkAUDIOTRACK::AddPackets m_nonIECPauseTimer %d, sleeptime %f", m_nonIECPauseTimer.MillisLeft(), sleeptime);
      usleep(sleeptime * 1000);
    }

    // audiotrack needs to be in play state or write will fail
    if (m_at_jni->getPlayState() != CJNIAudioTrack::PLAYSTATE_PLAYING)
      m_at_jni->play();

    bool retried = false;
    int size_left = size;
    while (written < size)
    {
      loop_written = AudioTrackWrite((char*)out_buf, 0, size_left);
      if (loop_written < 0)
      {
        CLog::Log(LOGERROR, "CAESinkAUDIOTRACK::AddPackets write returned error:  %d", loop_written);
        return INT_MAX;
      }

      written += loop_written;
      size_left -= loop_written;

      // if we could not add any data - sleep a bit and retry
      if (loop_written == 0)
      {
        if (!retried)
        {
          int sleep_time_ms = m_sink_sleepOnWriteStall;
          usleep(sleep_time_ms * 1000);
          bool playing = m_at_jni->getPlayState() == CJNIAudioTrack::PLAYSTATE_PLAYING;
          CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::AddPackets: retry write after sleeping %d ms, playing: %s",
            sleep_time_ms, playing ? "yes" : "no");
          continue;
        }
        else
        {
          CLog::Log(LOGDEBUG, "AESinkAUDIOTRACK::AddPackets: Repeatedly tried to write onto the sink - giving up");
          break;
        }
      }

      retried = false; // at least one time there was more than zero data written
      if (m_passthrough && !m_passthroughIsIECPacked)
        m_writeSeconds += m_format.m_streamInfo.GetDuration() / 1000;
      else
        m_writeSeconds += ((double) written / m_sink_frameSize) / m_sink_sampleRate;

      // just try again to care for fragmentation
      if (written < size)
        out_buf = out_buf + loop_written;
    }
  }

  unsigned int written_frames = written / m_format.m_frameSize;
  double time_to_add_ms = 1000.0 * (CurrentHostCounter() - startTime) / CurrentHostFrequency();
  double extra_sleep = 0;
  if (m_passthrough)
  {
    // AT does not consume in a blocking way - it runs ahead and blocks
    // exactly once with the last package for some 100 ms
    // help it sleeping a bit
    if (time_to_add_ms < m_format.m_streamInfo.GetDuration() / 2.0)
    {
      // leave enough head room for eventualities
      extra_sleep = m_format.m_streamInfo.GetDuration() / 4.0;
      usleep(extra_sleep * 1000);
    }
  }
  else
  {
    double time_should_ms = written_frames / (double) m_format.m_sampleRate * 1000.0;
    double time_off = time_should_ms - time_to_add_ms;
    if (time_off > 0 && time_off > time_should_ms / 2.0)
    {
      extra_sleep = time_should_ms / 4.0;
      usleep(extra_sleep * 1000);
    }
  }
  if (g_advancedSettings.CanLogComponent(LOGAUDIO))
    CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::AddPackets: time_to_add_ms=%f, extra_sleep=%f, written_frames=%u", time_to_add_ms, extra_sleep, written_frames);

  if (written != size)
    CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::AddPackets: Error writing full package to sink, bytes left: %d", size - written);

  return written_frames;
}

void CAESinkAUDIOTRACK::AddPause(unsigned int millis)
{
  // fake a burst pause, used for a/v sync and only for non-IEC packed.
  if (!m_at_jni)
    return;

  if (g_advancedSettings.CanLogComponent(LOGAUDIO))
    CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::AddPause %d ms", millis);

  // on startup the buffer is empty, it "should" take the silence if we would really send some
  // without any delay. In between we need to sleep out the frames though
  if (m_playbackHeadOffset == -1 && m_nonIECPauseTimer.MillisLeft() + millis <= m_sink_bufferSeconds * 1000)
    m_nonIECPauseTimer.Set(m_nonIECPauseTimer.MillisLeft() + millis);
  else
  {
    usleep(millis * 1000);
    m_nonIECPauseTimer.Set(m_nonIECPauseTimer.MillisLeft() + millis);
  }
}

void CAESinkAUDIOTRACK::Drain()
{
  if (!m_at_jni)
    return;

  //CLog::Log(LOGDEBUG, "CAESinkAUDIOTRACK::Drain");
  // no need to stop, just pause, flush and go
  // Deinitialize will handle any thing else.
  m_at_jni->pause();
  m_at_jni->flush();
  //m_at_jni->stop();

  m_writeSeconds = 0.0;
  m_playbackHead = 0;
  m_playbackHeadOffset = -1;
  m_lastdelay = 0.0;
  m_linearmovingaverage.clear();
  m_nonIECPauseTimer.SetExpired();
}

bool CAESinkAUDIOTRACK::FormatNeedsIECPacked(const AEAudioFormat &format)
{
  // ENCODING_IEC61937 mean all bitstreamed formats are IEC packed
  if (CSettings::GetInstance().GetBool(CSettings::SETTING_AUDIOOUTPUT_PASSTHROUGHIECPACKED))
    return true;

  bool needsIECPacked = true;
  switch (format.m_streamInfo.m_type)
  {
    case CAEStreamInfo::STREAM_TYPE_AC3:
      if (CJNIAudioFormat::ENCODING_AC3 != -1)
        needsIECPacked = false;
      break;
    case CAEStreamInfo::STREAM_TYPE_EAC3:
      if (CJNIAudioFormat::ENCODING_E_AC3 != -1)
        needsIECPacked = false;
      break;
    case CAEStreamInfo::STREAM_TYPE_TRUEHD:
      if (CJNIAudioFormat::ENCODING_DOLBY_TRUEHD != -1)
        needsIECPacked = false;
      break;
    case CAEStreamInfo::STREAM_TYPE_DTS_512:
    case CAEStreamInfo::STREAM_TYPE_DTS_2048:
    case CAEStreamInfo::STREAM_TYPE_DTS_1024:
    case CAEStreamInfo::STREAM_TYPE_DTSHD_CORE:
      if (CJNIAudioFormat::ENCODING_DTS != -1)
        needsIECPacked = false;
      break;
    case CAEStreamInfo::STREAM_TYPE_DTSHD:
    case CAEStreamInfo::STREAM_TYPE_DTSHD_MA:
      if (CJNIAudioFormat::ENCODING_DTS_HD != -1)
        needsIECPacked = false;
      break;

    default:
      break;
  }

  return needsIECPacked;
}

void CAESinkAUDIOTRACK::EnumerateDevicesEx(AEDeviceInfoList &list, bool force)
{
  m_sdk = CJNIAudioManager::GetSDKVersion();

  m_info.m_channels.Reset();
  m_info.m_dataFormats.clear();
  m_info.m_sampleRates.clear();
  m_info.m_streamTypes.clear();

  m_info.m_deviceType = AE_DEVTYPE_PCM;
  m_info.m_deviceName = "AudioTrack";
  m_info.m_displayName = "android";
  m_info.m_displayNameExtra = "audiotrack";
#ifdef LIMIT_TO_STEREO_AND_5POINT1_AND_7POINT1
  if (Has71Support())
    m_info.m_channels = AE_CH_LAYOUT_7_1;
  else
    m_info.m_channels = AE_CH_LAYOUT_5_1;
#else
  m_info.m_channels = KnownChannels;
#endif

  m_info.m_dataFormats.push_back(AE_FMT_S16LE);
  if (m_sdk >= 21)
    m_info.m_dataFormats.push_back(AE_FMT_FLOAT);

  m_sink_sampleRates.insert(CJNIAudioTrack::getNativeOutputSampleRate(CJNIAudioManager::STREAM_MUSIC));

  if (CXBMCApp::IsHDMIPlugged() && !CXBMCApp::IsHeadsetPlugged())
  {
    m_info.m_deviceType = AE_DEVTYPE_HDMI;

    // enable passthrough (both non-IEC packed or IEC packed)
    m_info.m_dataFormats.push_back(AE_FMT_RAW);

    // digital dolby capabilities
    m_info.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_AC3);
    m_info.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_EAC3);
    m_info.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_TRUEHD);

    // dts capabilities
    m_info.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTS_512);
    m_info.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTS_2048);
    m_info.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTS_1024);
    m_info.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTSHD_CORE);
    m_info.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTSHD);
    m_info.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTSHD_MA);

    // check encoding capabilities
    int encoding = CJNIAudioFormat::ENCODING_PCM_16BIT;
    if (m_sdk >= 21)
      encoding = CJNIAudioFormat::ENCODING_PCM_FLOAT;

    // check sample rate capabilities
    int test_sample[] = { 32000, 44100, 48000, 88200, 96000, 176400, 192000 };
    int test_sample_sz = sizeof(test_sample) / sizeof(int);
    for (int i = 0; i < test_sample_sz; ++i)
    {
      if (IsSupported(test_sample[i], CJNIAudioFormat::CHANNEL_OUT_STEREO, encoding))
      {
        m_sink_sampleRates.insert(test_sample[i]);
        CLog::Log(LOGDEBUG, "AESinkAUDIOTRACK - CHANNEL_OUT_STEREO %d supported", test_sample[i]);
      }
    }
    std::copy(m_sink_sampleRates.begin(), m_sink_sampleRates.end(), std::back_inserter(m_info.m_sampleRates));
  }

  list.push_back(m_info);
}

double CAESinkAUDIOTRACK::GetMovingAverageDelay(double newestdelay)
{
  m_linearmovingaverage.push_back(newestdelay);

  // new values are in the back, old values are in the front
  // oldest value is removed if elements > MOVING_AVERAGE_MAX_MEMBERS
  // removing first element of a vector sucks - I know that
  // but hey - 10 elements - not 1 million
  size_t size = m_linearmovingaverage.size();
  if (size > MOVING_AVERAGE_MAX_MEMBERS)
  {
    m_linearmovingaverage.pop_front();
    size--;
  }
  // m_{LWMA}^{(n)}(t) = \frac{2}{n (n+1)} \sum_{i=1}^n i \; x(t-n+i)
  const double denom = 2.0 / (size * (size + 1));
  double sum = 0.0;
  for (size_t i = 0; i < m_linearmovingaverage.size(); i++)
    sum += (i + 1) * m_linearmovingaverage.at(i);

  return sum * denom;
}

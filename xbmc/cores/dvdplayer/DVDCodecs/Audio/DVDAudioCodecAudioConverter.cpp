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

#include "DVDAudioCodecAudioConverter.h"
#include "DVDCodecs/DVDCodecs.h"
#include "DVDStreamInfo.h"
#include "utils/log.h"

#include <algorithm>

#include "cores/AudioEngine/AEFactory.h"

DVDAudioCodecAudioConverter::DVDAudioCodecAudioConverter(void) :
  m_buffer(NULL),
  m_bufferSize(0),
{
}

DVDAudioCodecAudioConverter::~DVDAudioCodecAudioConverter(void)
{
  Dispose();
}

bool DVDAudioCodecAudioConverter::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  AEAudioFormat format;
  format.m_dataFormat = AE_FMT_RAW;
  format.m_sampleRate = hints.samplerate;
  switch (hints.codec)
  {
    case AV_CODEC_ID_AC3:
      format.m_streamInfo.m_type = CAEStreamInfo::STREAM_TYPE_AC3;
      format.m_streamInfo.m_sampleRate = hints.samplerate;
      break;

    case AV_CODEC_ID_EAC3:
      format.m_streamInfo.m_type = CAEStreamInfo::STREAM_TYPE_EAC3;
      format.m_streamInfo.m_sampleRate = hints.samplerate;
      break;

    default:
      return false;
  }

  m_dataSize = 0;
  m_bufferSize = 0;
  m_backlogSize = 0;
  m_currentPts = DVD_NOPTS_VALUE;
  m_nextPts = DVD_NOPTS_VALUE;
  return ret;
}

void DVDAudioCodecAudioConverter::Dispose()
{
  if (m_buffer)
  {
    delete[] m_buffer;
    m_buffer = NULL;
  }

  m_bufferSize = 0;
}

int DVDAudioCodecAudioConverter::Decode(uint8_t* pData, int iSize, double dts, double pts)
{
  int used = 0;
  if (m_backlogSize)
  {
    if (m_currentPts == DVD_NOPTS_VALUE)
    {
      m_currentPts = m_nextPts;
      m_nextPts = DVD_NOPTS_VALUE;
    }

    m_dataSize = m_bufferSize;
    unsigned int consumed = m_parser.AddData(m_backlogBuffer, m_backlogSize, &m_buffer, &m_dataSize);
    m_bufferSize = std::max(m_bufferSize, m_dataSize);
    if (consumed != m_backlogSize)
    {
      memmove(m_backlogBuffer, m_backlogBuffer+consumed, consumed);
      m_backlogSize -= consumed;
    }
  }

  if (pData && !m_dataSize)
  {
    if (iSize <= 0)
      return 0;

    if (m_currentPts == DVD_NOPTS_VALUE)
      m_currentPts = pts;

    m_dataSize = m_bufferSize;
    used = m_parser.AddData(pData, iSize, &m_buffer, &m_dataSize);
    m_bufferSize = std::max(m_bufferSize, m_dataSize);

    if (used != iSize)
    {
      m_backlogSize = iSize - used;
      memcpy(m_backlogBuffer, pData + used, m_backlogSize);
      if (m_nextPts != DVD_NOPTS_VALUE)
      {
        m_nextPts = pts;
      }
      used = iSize;
    }
  }
  else if (pData)
  {
    if (m_nextPts != DVD_NOPTS_VALUE)
    {
      m_nextPts = pts;
    }
    memcpy(m_backlogBuffer + m_backlogSize, pData, iSize);
    m_backlogSize += iSize;
    used = iSize;
  }

  if (!m_dataSize)
    return used;

  if (m_format.m_streamInfo.m_type == CAEStreamInfo::STREAM_TYPE_TRUEHD)
  {
    if (!m_trueHDoffset)
      memset(m_trueHDBuffer.get(), 0, TRUEHD_BUF_SIZE);

    memcpy(&(m_trueHDBuffer.get())[m_trueHDoffset], m_buffer, m_dataSize);
    uint8_t highByte = (m_dataSize >> 8) & 0xFF;
    uint8_t lowByte = m_dataSize & 0xFF;
    m_trueHDBuffer[m_trueHDoffset+2560-2] = highByte;
    m_trueHDBuffer[m_trueHDoffset+2560-1] = lowByte;
    m_trueHDoffset += 2560;

    if (m_trueHDoffset / 2560 == 24)
    {
      m_dataSize = m_trueHDoffset;
      m_trueHDoffset = 0;
    }
    else
      m_dataSize = 0;
  }

  if (m_dataSize)
  {
    m_format.m_dataFormat = AE_FMT_RAW;
    m_format.m_streamInfo = m_parser.GetStreamInfo();
    m_format.m_sampleRate = m_parser.GetSampleRate();
    m_format.m_frameSize = 1;
    CAEChannelInfo layout;
    for (unsigned int i=0; i<m_parser.GetChannels(); i++)
    {
      layout += AE_CH_RAW;
    }
    m_format.m_channelLayout = layout;
  }

  return used;
}

void DVDAudioCodecAudioConverter::GetData(DVDAudioFrame &frame)
{
  frame.nb_frames = GetData(frame.data);

  if (frame.nb_frames == 0)
    return;

  frame.passthrough = true;
  frame.format = m_format;
  frame.planes = 1;
  frame.bits_per_sample = 8;
  frame.duration = DVD_MSEC_TO_TIME(frame.format.m_streamInfo.GetDuration());
  frame.pts = m_currentPts;
  m_currentPts = DVD_NOPTS_VALUE;
  m_dataSize = 0;
}

int DVDAudioCodecAudioConverter::GetData(uint8_t** dst)
{
  *dst = m_buffer;
  return m_dataSize;
}

void DVDAudioCodecAudioConverter::Reset()
{
  m_dataSize = 0;
  m_bufferSize = 0;
  m_backlogSize = 0;
  m_currentPts = DVD_NOPTS_VALUE;
  m_nextPts = DVD_NOPTS_VALUE;
}

int DVDAudioCodecAudioConverter::GetBufferSize()
{
  return (int)m_parser.GetBufferSize();
}

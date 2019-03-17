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

#include "AEBitstreamPacker.h"
#include "AEPackIEC61937.h"
#include "AEStreamInfo.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "utils/log.h"

#define BURST_HEADER_SIZE       8
#define TRUEHD_FRAME_OFFSET     2560
#define MAT_MIDDLE_CODE_OFFSET -4
#define MAT_FRAME_SIZE          61424
#define EAC3_MAX_BURST_PAYLOAD_SIZE (24576 - BURST_HEADER_SIZE)

CAEBitstreamPacker::CAEBitstreamPacker() :
  m_dtsHD    (NULL),
  m_dtsHDSize(0),
  m_eac3     (NULL),
  m_eac3Size (0),
  m_eac3FramesCount(0),
  m_eac3FramesPerBurst(0),
  m_dataSize (0),
  m_pauseDuration(0)
{
  Reset();
}

CAEBitstreamPacker::~CAEBitstreamPacker()
{
  delete[] m_dtsHD;
  delete[] m_eac3;
}

void CAEBitstreamPacker::Pack(CAEStreamInfo &info, uint8_t* data, int size)
{
  m_pauseDuration = 0;
  switch (info.m_type)
  {
    case CAEStreamInfo::STREAM_TYPE_TRUEHD:
      PackTrueHD(info, data, size);
      break;

    case CAEStreamInfo::STREAM_TYPE_DTSHD:
    case CAEStreamInfo::STREAM_TYPE_DTSHD_MA:
      PackDTSHD (info, data, size);
      break;

    case CAEStreamInfo::STREAM_TYPE_AC3:
      m_dataSize = CAEPackIEC61937::PackAC3(data, size, m_packedBuffer);
      break;

    case CAEStreamInfo::STREAM_TYPE_EAC3:
      PackEAC3 (info, data, size);
      break;

    case CAEStreamInfo::STREAM_TYPE_DTSHD_CORE:
    case CAEStreamInfo::STREAM_TYPE_DTS_512:
      m_dataSize = CAEPackIEC61937::PackDTS_512(data, size, m_packedBuffer, info.m_dataIsLE);
      break;

    case CAEStreamInfo::STREAM_TYPE_DTS_1024:
      m_dataSize = CAEPackIEC61937::PackDTS_1024(data, size, m_packedBuffer, info.m_dataIsLE);
      break;

    case CAEStreamInfo::STREAM_TYPE_DTS_2048:
      m_dataSize = CAEPackIEC61937::PackDTS_2048(data, size, m_packedBuffer, info.m_dataIsLE);
      break;

    default:
      CLog::Log(LOGERROR, "CAEBitstreamPacker::Pack - no pack function");
  }
}

bool CAEBitstreamPacker::PackPause(CAEStreamInfo &info, unsigned int micros, bool iecBursts)
{
  // re-use last buffer
  if (m_pauseDuration == micros)
    return false;

  switch (info.m_type)
  {
    case CAEStreamInfo::STREAM_TYPE_TRUEHD:
    case CAEStreamInfo::STREAM_TYPE_EAC3:
      m_dataSize = CAEPackIEC61937::PackPause(m_packedBuffer, micros, GetOutputChannelMap(info).Count() * 2, GetOutputRate(info), 4, info.m_sampleRate);
      m_pauseDuration = micros;
      break;

    case CAEStreamInfo::STREAM_TYPE_AC3:
    case CAEStreamInfo::STREAM_TYPE_DTSHD:
    case CAEStreamInfo::STREAM_TYPE_DTSHD_MA:
    case CAEStreamInfo::STREAM_TYPE_DTSHD_CORE:
    case CAEStreamInfo::STREAM_TYPE_DTS_512:
    case CAEStreamInfo::STREAM_TYPE_DTS_1024:
    case CAEStreamInfo::STREAM_TYPE_DTS_2048:
      m_dataSize = CAEPackIEC61937::PackPause(m_packedBuffer, micros, GetOutputChannelMap(info).Count() * 2, GetOutputRate(info), 3, info.m_sampleRate);
      m_pauseDuration = micros;
      break;

    default:
      CLog::Log(LOGERROR, "CAEBitstreamPacker::Pack - no pack function");
  }

  if (!iecBursts)
  {
    memset(m_packedBuffer, 0, m_dataSize);
  }

  return true;
}

unsigned int CAEBitstreamPacker::GetSize()
{
  return m_dataSize;
}

uint8_t* CAEBitstreamPacker::GetBuffer()
{
  return m_packedBuffer;
}

void CAEBitstreamPacker::Reset()
{
  m_dataSize = 0;
  m_pauseDuration = 0;
  m_packedBuffer[0] = 0;

}

void CAEBitstreamPacker::PackTrueHD(CAEStreamInfo &info, uint8_t* data, int size)
{
  m_trueHDpacker.BitstreamTrueHD(data, size, &m_dataSize, m_packedBuffer);
}

void CAEBitstreamPacker::PackDTSHD(CAEStreamInfo &info, uint8_t* data, int size)
{
  static const uint8_t dtshd_start_code[10] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xfe };
  unsigned int dataSize = sizeof(dtshd_start_code) + 2 + size;

  if (dataSize > m_dtsHDSize)
  {
    delete[] m_dtsHD;
    m_dtsHDSize = dataSize;
    m_dtsHD     = new uint8_t[dataSize];
    memcpy(m_dtsHD, dtshd_start_code, sizeof(dtshd_start_code));
  }

  m_dtsHD[sizeof(dtshd_start_code) + 0] = ((uint16_t)size & 0xFF00) >> 8;
  m_dtsHD[sizeof(dtshd_start_code) + 1] = ((uint16_t)size & 0x00FF);
  memcpy(m_dtsHD + sizeof(dtshd_start_code) + 2, data, size);

  m_dataSize = CAEPackIEC61937::PackDTSHD(m_dtsHD, dataSize, m_packedBuffer, info.m_dtsPeriod);
}

void CAEBitstreamPacker::PackEAC3(CAEStreamInfo &info, uint8_t* data, int size)
{
  unsigned int framesPerBurst = info.m_repeat;

  if (m_eac3FramesPerBurst != framesPerBurst)
  {
    /* switched streams, discard partial burst */
    m_eac3Size = 0;
    m_eac3FramesPerBurst = framesPerBurst;
  }

  if (m_eac3FramesPerBurst == 1)
  {
    /* simple case, just pass through */
    m_dataSize = CAEPackIEC61937::PackEAC3(data, size, m_packedBuffer);
  }
  else
  {
    /* multiple frames needed to achieve 6 blocks as required by IEC 61937-3:2007 */

    if (m_eac3 == NULL)
      m_eac3 = new uint8_t[EAC3_MAX_BURST_PAYLOAD_SIZE];

    unsigned int newsize = m_eac3Size + size;
    bool overrun = newsize > EAC3_MAX_BURST_PAYLOAD_SIZE;

    if (!overrun)
    {
      memcpy(m_eac3 + m_eac3Size, data, size);
      m_eac3Size = newsize;
      m_eac3FramesCount++;
    }

    if (m_eac3FramesCount >= m_eac3FramesPerBurst || overrun)
    {
      m_dataSize = CAEPackIEC61937::PackEAC3(m_eac3, m_eac3Size, m_packedBuffer);
      m_eac3Size = 0;
      m_eac3FramesCount = 0;
    }
  }
}

unsigned int CAEBitstreamPacker::GetOutputRate(CAEStreamInfo &info)
{
  unsigned int rate;
  switch (info.m_type)
  {
    case CAEStreamInfo::STREAM_TYPE_AC3:
      rate = info.m_sampleRate;
      break;
    case CAEStreamInfo::STREAM_TYPE_EAC3:
      rate = info.m_sampleRate * 4;
      break;
    case CAEStreamInfo::STREAM_TYPE_TRUEHD:
      if (info.m_sampleRate == 48000 ||
          info.m_sampleRate == 96000 ||
          info.m_sampleRate == 192000)
        rate = 192000;
      else
        rate = 176400;
      break;
    case CAEStreamInfo::STREAM_TYPE_DTS_512:
    case CAEStreamInfo::STREAM_TYPE_DTS_1024:
    case CAEStreamInfo::STREAM_TYPE_DTS_2048:
    case CAEStreamInfo::STREAM_TYPE_DTSHD_CORE:
      rate = info.m_sampleRate;
      break;
    case CAEStreamInfo::STREAM_TYPE_DTSHD:
    case CAEStreamInfo::STREAM_TYPE_DTSHD_MA:
      rate = 192000;
      break;
    default:
      rate = 48000;
      break;
  }
  return rate;
}

CAEChannelInfo CAEBitstreamPacker::GetOutputChannelMap(CAEStreamInfo &info)
{
  int channels = 2;
  switch (info.m_type)
  {
    case CAEStreamInfo::STREAM_TYPE_AC3:
    case CAEStreamInfo::STREAM_TYPE_EAC3:
    case CAEStreamInfo::STREAM_TYPE_DTS_512:
    case CAEStreamInfo::STREAM_TYPE_DTS_1024:
    case CAEStreamInfo::STREAM_TYPE_DTS_2048:
    case CAEStreamInfo::STREAM_TYPE_DTSHD_CORE:
    case CAEStreamInfo::STREAM_TYPE_DTSHD:
      channels = 2;
      break;

    case CAEStreamInfo::STREAM_TYPE_TRUEHD:
    case CAEStreamInfo::STREAM_TYPE_DTSHD_MA:
      channels = 8;
      break;

    default:
      break;
  }

  CAEChannelInfo channelMap;
  for (int i=0; i<channels; i++)
  {
    channelMap += AE_CH_RAW;
  }

  return channelMap;
}

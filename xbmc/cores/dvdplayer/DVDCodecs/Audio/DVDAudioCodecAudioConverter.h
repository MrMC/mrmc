#pragma once

/*
 *      Copyright (C) 2016 Team MrMC
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

#include "queue"

#include "DVDAudioCodec.h"
#include "DVDStreamInfo.h"

class CAudioIBufferQueue;

class CDVDAudioCodecAudioConverter : public CDVDAudioCodec
{
public:
  CDVDAudioCodecAudioConverter();
  virtual ~CDVDAudioCodecAudioConverter();

  virtual bool  Open(CDVDStreamInfo &hints, CDVDCodecOptions &options);
  virtual void  Dispose();
  virtual int   Decode(uint8_t* pData, int iSize, double dts, double pts);
  virtual void  GetData(DVDAudioFrame &frame);
  virtual int   GetData(uint8_t** dst);
  virtual void  Reset();
  virtual AEAudioFormat GetFormat() { return m_format; }
  virtual int   GetBitRate()        { return m_hints.bitrate; }
  virtual const char* GetName()     { return m_formatName.c_str(); }
  virtual int   GetBufferSize()     { return m_oBufferSize; }
  virtual int   GetProfile()        { return m_hints.profile; }
private:
  CDVDStreamInfo  m_hints;
  AEAudioFormat   m_format;
  std::string     m_formatName;

  void           *m_codec;
  CAudioIBufferQueue *m_iBuffer;

  uint8_t        *m_oBuffer;
  int             m_oBufferSize;
  bool            m_gotFrame;
  double          m_currentPts;
};


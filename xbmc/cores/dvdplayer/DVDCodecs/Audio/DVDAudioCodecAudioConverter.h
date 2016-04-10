#pragma once

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

#include <list>
#include <memory>

#include "system.h"
#include "DVDAudioCodec.h"

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
  virtual bool  NeedPassthrough()   { return true; }
  virtual const char* GetName()     { return "audioconverter"; }
  virtual int   GetBufferSize();
private:
  CAEStreamParser m_parser;
  uint8_t        *m_buffer;
  unsigned int    m_bufferSize;
  unsigned int    m_dataSize;
  AEAudioFormat   m_format;
  double          m_currentPts;
  double          m_nextPts;
};


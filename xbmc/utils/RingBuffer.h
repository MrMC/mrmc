#pragma once
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

#include "threads/CriticalSection.h"

class CRingBuffer
{
  CCriticalSection m_critSection;
  char *m_buffer;
  size_t m_size;
  size_t m_readPtr;
  size_t m_writePtr;
  size_t m_fillCount;
public:
  CRingBuffer();
  ~CRingBuffer();
  bool Create(size_t size);
  void Destroy();
  void Clear();
  bool ReadData(char *buf, size_t size);
  bool ReadData(CRingBuffer &rBuf, size_t size);
  bool WriteData(const char *buf, size_t size);
  bool WriteData(CRingBuffer &rBuf, size_t size);
  bool SkipBytes(long skipSize);
  bool Append(CRingBuffer &rBuf);
  bool Copy(CRingBuffer &rBuf);
  char *getBuffer();
  size_t getSize();
  size_t getReadPtr() const;
  size_t getWritePtr();
  size_t getMaxReadSize();
  size_t getMaxWriteSize();
};

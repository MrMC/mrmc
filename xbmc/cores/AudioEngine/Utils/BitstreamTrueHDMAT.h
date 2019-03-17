/*
 *      Copyright (C) 2019 Team MrMC
 *      http://mrmc.tv
 *      Copyright (C) 2010-2019 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <stdint.h>
#include <vector>

// TrueHD Bitstreaming
struct {
  bool init = false;
  int ratebits = 0;

  uint16_t prev_frametime = 0;
  bool prev_frametime_valid = false;

  uint32_t mat_framesize = 0;
  uint32_t prev_mat_framesize = 0;

  uint16_t padding = 0;
} m_TrueHDMATState;

class CBitstreamTrueHDMAT
{
public:
  bool BitstreamTrueHD(const uint8_t *p, int buffsize, unsigned int *dataSize, uint8_t *packedBuffer);

private:
  void MATWriteHeader();
  void MATWritePadding();
  void MATAppendData(const uint8_t *p, int size);
  int MATFillDataBuffer(const uint8_t *p, int size, bool padding = false);
  void MATFlushPacket(unsigned int *dataSize, uint8_t *packedBuffer);

  std::vector<uint8_t> m_bsOutput;
};

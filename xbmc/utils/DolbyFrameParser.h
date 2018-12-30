#pragma once
/*
*      Copyright (C) 2019 Team MrMC
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

#include <string>
#include <stdint.h>

class CMemoryBitstream;
typedef struct eac3_info eac3_info;
typedef struct AC3HeaderInfo AC3HeaderInfo;

class CDolbyFrameParser
{
  public:
   ~CDolbyFrameParser() {};

    static bool isAtmos(const uint8_t *buf, int len);
    std::string parse(const uint8_t *buf, int len);
 private:
    int  analyze(eac3_info *info, uint8_t *frame, int size);
    int  parseheader(CMemoryBitstream &bs, AC3HeaderInfo *hdr);
    void checkforatmos(CMemoryBitstream &bs, AC3HeaderInfo *hdr);

    std::string mimeType;
};

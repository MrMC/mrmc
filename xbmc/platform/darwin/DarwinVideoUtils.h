#pragma once

/*
 *      Copyright (C) 2017-2018 Team MrMC
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

extern "C" {
#include "libavcodec/avcodec.h"
}

#include <CoreMedia/CoreMedia.h>

typedef struct VideoParameterSets {
  size_t    sps_count = 0;
  size_t   *sps_sizes = nullptr;
  uint8_t **sps_array = nullptr;
  size_t    pps_count = 0;
  size_t   *pps_sizes = nullptr;
  uint8_t **pps_array = nullptr;
  size_t    vps_count = 0;
  size_t   *vps_sizes = nullptr;
  uint8_t **vps_array = nullptr;
} VideoParameterSets;

class CDarwinVideoUtils
{
public:
  static bool CreateParameterSetArraysFromExtraData(
    VideoParameterSets &parameterSets, AVCodecID codec, uint8_t *extraData);
  static CMFormatDescriptionRef CreateFormatDescriptorFromParameterSetArrays(
    VideoParameterSets &parameterSets, AVCodecID codec);
  static void FreeParameterSets(VideoParameterSets &parameterSets);
  static bool ParsePacketForVideoParameterSets(
    VideoParameterSets &parameterSets, AVCodecID codec, uint8_t *pData, int iSize);
  static void ProbeNALUnits(AVCodecID codec, uint8_t *pData, int iSize);
};

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

#import "DarwinVideoUtils.h"
#import "BitstreamConverter.h"
#import "utils/log.h"

// Dolby Vision Profile enum type
typedef enum VIDEO_DOLBYVISIONPROFILETYPE {
  VIDEO_DolbyVisionProfileUnknown = 0x0,
  VIDEO_DolbyVisionProfileDvavDer = 0x1,
  VIDEO_DolbyVisionProfileDvavDen = 0x2,
  VIDEO_DolbyVisionProfileDvheDer = 0x3,
  VIDEO_DolbyVisionProfileDvheDen = 0x4,
  VIDEO_DolbyVisionProfileDvheDtr = 0x5,
  VIDEO_DolbyVisionProfileDvheStn = 0x6,
  VIDEO_DolbyVisionProfileMax     = 0x7FFFFFFF
} VIDEO_DOLBYVISIONPROFILETYPE;

// Dolby Vision Level enum type
typedef enum VIDEO_DOLBYVISIONLEVELTYPE {
  VIDEO_DolbyVisionLevelUnknown = 0x0,
  VIDEO_DolbyVisionLevelHd24    = 0x1,
  VIDEO_DolbyVisionLevelHd30    = 0x2,
  VIDEO_DolbyVisionLevelFhd24   = 0x4,
  VIDEO_DolbyVisionLevelFhd30   = 0x8,
  VIDEO_DolbyVisionLevelFhd60   = 0x10,
  VIDEO_DolbyVisionLevelUhd24   = 0x20,
  VIDEO_DolbyVisionLevelUhd30   = 0x40,
  VIDEO_DolbyVisionLevelUhd48   = 0x80,
  VIDEO_DolbyVisionLevelUhd60   = 0x100,
  VIDEO_DolbyVisionLevelmax     = 0x7FFFFFFF
} VIDEO_DOLBYVISIONLEVELTYPE;

enum {
  AVC_NAL_SLICE=1,
  AVC_NAL_DPA,
  AVC_NAL_DPB,
  AVC_NAL_DPC,
  AVC_NAL_IDR_SLICE,
  AVC_NAL_SEI,
  AVC_NAL_SPS,
  AVC_NAL_PPS,
  AVC_NAL_AUD,
  AVC_NAL_END_SEQUENCE,
  AVC_NAL_END_STREAM,
  AVC_NAL_FILLER_DATA,
  AVC_NAL_SPS_EXT,
  AVC_NAL_AUXILIARY_SLICE=19
};

enum {
  HEVC_NAL_TRAIL_N    = 0,
  HEVC_NAL_TRAIL_R    = 1,
  HEVC_NAL_TSA_N      = 2,
  HEVC_NAL_TSA_R      = 3,
  HEVC_NAL_STSA_N     = 4,
  HEVC_NAL_STSA_R     = 5,
  HEVC_NAL_RADL_N     = 6,
  HEVC_NAL_RADL_R     = 7,
  HEVC_NAL_RASL_N     = 8,
  HEVC_NAL_RASL_R     = 9,
  HEVC_NAL_BLA_W_LP   = 16,
  HEVC_NAL_BLA_W_RADL = 17,
  HEVC_NAL_BLA_N_LP   = 18,
  HEVC_NAL_IDR_W_RADL = 19,
  HEVC_NAL_IDR_N_LP   = 20,
  HEVC_NAL_CRA_NUT    = 21,
  HEVC_NAL_RESERVED_IRAP_VCL22 = 22,
  HEVC_NAL_RESERVED_IRAP_VCL23 = 23,
  HEVC_NAL_VPS        = 32,
  HEVC_NAL_SPS        = 33,
  HEVC_NAL_PPS        = 34,
  HEVC_NAL_AUD        = 35,
  HEVC_NAL_EOS_NUT    = 36,
  HEVC_NAL_EOB_NUT    = 37,
  HEVC_NAL_FD_NUT     = 38,
  HEVC_NAL_SEI_PREFIX = 39,
  HEVC_NAL_SEI_SUFFIX = 40,
  HEVC_NAL_UNSPECIFIED_62 = 62,
  HEVC_NAL_UNSPECIFIED_63 = 63
};

enum HEVC_SEI_TYPE {
  SEI_TYPE_BUFFERING_PERIOD                     = 0,
  SEI_TYPE_PICTURE_TIMING                       = 1,
  SEI_TYPE_PAN_SCAN_RECT                        = 2,
  SEI_TYPE_FILLER_PAYLOAD                       = 3,
  SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35       = 4,
  SEI_TYPE_USER_DATA_UNREGISTERED               = 5,
  SEI_TYPE_RECOVERY_POINT                       = 6,
  SEI_TYPE_SCENE_INFO                           = 9,
  SEI_TYPE_FULL_FRAME_SNAPSHOT                  = 15,
  SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_START = 16,
  SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_END   = 17,
  SEI_TYPE_FILM_GRAIN_CHARACTERISTICS           = 19,
  SEI_TYPE_POST_FILTER_HINT                     = 22,
  SEI_TYPE_TONE_MAPPING_INFO                    = 23,
  SEI_TYPE_FRAME_PACKING                        = 45,
  SEI_TYPE_DISPLAY_ORIENTATION                  = 47,
  SEI_TYPE_SOP_DESCRIPTION                      = 128,
  SEI_TYPE_ACTIVE_PARAMETER_SETS                = 129,
  SEI_TYPE_DECODING_UNIT_INFO                   = 130,
  SEI_TYPE_TEMPORAL_LEVEL0_INDEX                = 131,
  SEI_TYPE_DECODED_PICTURE_HASH                 = 132,
  SEI_TYPE_SCALABLE_NESTING                     = 133,
  SEI_TYPE_REGION_REFRESH_INFO                  = 134,
  SEI_TYPE_MASTERING_DISPLAY_INFO               = 137,
  SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO             = 144,
};

bool CDarwinVideoUtils::CreateParameterSetArraysFromExtraData(
    VideoParameterSets &parameterSets, AVCodecID codec, uint8_t *extraData)
{
  bool rtn = false;
  switch (codec)
  {
    default:
    break;
    case AV_CODEC_ID_H264:
    {
      uint8_t *ps_ptr = extraData;
      ps_ptr += 5; // skip over fixed length header
      size_t numberOfSPSs = 0x001F & *ps_ptr++; // sps count is lower five bits;

      // handle sps's
      parameterSets.sps_count = numberOfSPSs;
      parameterSets.sps_sizes = (size_t*)malloc(sizeof(size_t*) * numberOfSPSs);
      parameterSets.sps_array = (uint8_t**)malloc(sizeof(uint8_t*) * numberOfSPSs);
      for (size_t i = 0; i < numberOfSPSs; i++)
      {
        uint32_t ps_size = BS_RB16(ps_ptr);
        ps_ptr += 2;
        parameterSets.sps_sizes[i] = ps_size;
        parameterSets.sps_array[i] = (uint8_t*)malloc(sizeof(uint8_t) * ps_size);
        memcpy(parameterSets.sps_array[i], ps_ptr, ps_size);
        ps_ptr += ps_size;
      }

      // handle pps's
      size_t numberOfPPSs = *ps_ptr++;
      parameterSets.pps_count = numberOfPPSs;
      parameterSets.pps_sizes = (size_t*)malloc(sizeof(size_t*) * numberOfPPSs);
      parameterSets.pps_array = (uint8_t**)malloc(sizeof(uint8_t*) * numberOfPPSs);
      for (size_t i = 0; i < numberOfPPSs; i++)
      {
        uint32_t ps_size = BS_RB16(ps_ptr);
        ps_ptr += 2;
        parameterSets.pps_sizes[i] = ps_size;
        parameterSets.pps_array[i] = (uint8_t*)malloc(sizeof(uint8_t) * ps_size);
        memcpy(parameterSets.pps_array[i], ps_ptr, ps_size);
        ps_ptr += ps_size;
      }
      // h264 only requires sps
      if (parameterSets.sps_count >= 1)
        rtn = true;
    }
    break;
    case AV_CODEC_ID_HEVC:
    {
      uint8_t *ps_ptr = extraData;
      ps_ptr += 22; // skip over fixed length header

      // number of arrays
      size_t numberOfParameterSetArrays = *ps_ptr++;
      for (size_t i = 0; i < numberOfParameterSetArrays; i++)
      {
        // bit(1) array_completeness;
        // bit(1) reserved = 0;
        // bit(6) NAL_unit_type;
        int nal_type = 0x3F & *ps_ptr++;
        size_t numberOfParameterSets = BS_RB16(ps_ptr);
        ps_ptr += 2;
        switch(nal_type)
        {
          case HEVC_NAL_SPS:
            parameterSets.sps_count = numberOfParameterSets;
            parameterSets.sps_sizes = (size_t*)malloc(sizeof(size_t*) * numberOfParameterSets);
            parameterSets.sps_array = (uint8_t**)malloc(sizeof(uint8_t*) * numberOfParameterSets);
            break;
          case HEVC_NAL_PPS:
            parameterSets.pps_count = numberOfParameterSets;
            parameterSets.pps_sizes = (size_t*)malloc(sizeof(size_t*) * numberOfParameterSets);
            parameterSets.pps_array = (uint8_t**)malloc(sizeof(uint8_t*) * numberOfParameterSets);
            break;
          case HEVC_NAL_VPS:
            parameterSets.vps_count = numberOfParameterSets;
            parameterSets.vps_sizes = (size_t*)malloc(sizeof(size_t*) * numberOfParameterSets);
            parameterSets.vps_array = (uint8_t**)malloc(sizeof(uint8_t*) * numberOfParameterSets);
            break;
        }
        for (size_t i = 0; i < numberOfParameterSets; i++)
        {
          uint32_t ps_size = BS_RB16(ps_ptr);
          ps_ptr += 2;
          switch(nal_type)
          {
            case HEVC_NAL_SPS:
              parameterSets.sps_sizes[i] = ps_size;
              parameterSets.sps_array[i] = (uint8_t*)malloc(sizeof(uint8_t) * ps_size);
              memcpy(parameterSets.sps_array[i], ps_ptr, ps_size);
              break;
            case HEVC_NAL_PPS:
              parameterSets.pps_sizes[i] = ps_size;
              parameterSets.pps_array[i] = (uint8_t*)malloc(sizeof(uint8_t) * ps_size);
              memcpy(parameterSets.pps_array[i], ps_ptr, ps_size);
              break;
            case HEVC_NAL_VPS:
              parameterSets.vps_sizes[i] = ps_size;
              parameterSets.vps_array[i] = (uint8_t*)malloc(sizeof(uint8_t) * ps_size);
              memcpy(parameterSets.vps_array[i], ps_ptr, ps_size);
              break;
          }
          ps_ptr += ps_size;
        }
      }
      // h265 requires at least one sps, pps and vps
      if (parameterSets.sps_count >= 1 &&
          parameterSets.pps_count >= 1 &&
          parameterSets.vps_count >= 1)
        rtn = true;
    }
    break;
  }

  return rtn;
}

CMFormatDescriptionRef CDarwinVideoUtils::CreateFormatDescriptorFromParameterSetArrays(
    VideoParameterSets &parameterSets, AVCodecID codec)
{
  CMFormatDescriptionRef fmt_desc = nullptr;
  int arraySize = parameterSets.sps_count + parameterSets.pps_count + parameterSets.vps_count;
  if (arraySize < 1)
    return nullptr;

  size_t parameterSetSizes[arraySize];
  uint8_t *parameterSetPointers[arraySize];

  size_t parameterSetCount = 0;
  for (size_t i = 0; i < parameterSets.sps_count; i++)
  {
    parameterSetSizes[parameterSetCount] = parameterSets.sps_sizes[i];
    parameterSetPointers[parameterSetCount++] = parameterSets.sps_array[i];
  }
  for (size_t i = 0; i < parameterSets.pps_count; i++)
  {
    parameterSetSizes[parameterSetCount] = parameterSets.pps_sizes[i];
    parameterSetPointers[parameterSetCount++] = parameterSets.pps_array[i];
  }
  for (size_t i = 0; i < parameterSets.vps_count; i++)
  {
    parameterSetSizes[parameterSetCount] = parameterSets.vps_sizes[i];
    parameterSetPointers[parameterSetCount++] = parameterSets.vps_array[i];
  }

  OSStatus status = -1;
  int nalUnitHeaderLength = 4;
  switch (codec)
  {
    default:
    break;
    case AV_CODEC_ID_H264:
    {
      CLog::Log(LOGNOTICE, "Constructing new format description");
      status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
        parameterSetCount, parameterSetPointers, parameterSetSizes, nalUnitHeaderLength, &fmt_desc);
      if (status != noErr)
        CLog::Log(LOGERROR, "%s - CMVideoFormatDescriptionCreateFromH264ParameterSets failed status(%d)", __FUNCTION__, status);
    }
    break;
    case AV_CODEC_ID_HEVC:
    {
      CLog::Log(LOGNOTICE, "Constructing new format description");
      // check availability at runtime
      if (__builtin_available(macOS 10.13, ios 11, tvos 11, *))
      {
        status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(kCFAllocatorDefault,
          parameterSetCount, parameterSetPointers, parameterSetSizes, nalUnitHeaderLength, nullptr, &fmt_desc);
      }
      if (status != noErr)
        CLog::Log(LOGERROR, "%s - CMVideoFormatDescriptionCreateFromHEVCParameterSets failed status(%d)", __FUNCTION__, status);
    }
    break;
  }

  if (status != noErr)
  {
    if (fmt_desc)
      CFRelease(fmt_desc), fmt_desc = nullptr;
  }

  return fmt_desc;
}

void CDarwinVideoUtils::FreeParameterSets(VideoParameterSets &parameterSets)
{
  // free old saved sps's
  if (parameterSets.sps_count)
  {
    for (size_t i = 0; i < parameterSets.sps_count; i++)
      free(parameterSets.sps_array[i]), parameterSets.sps_array[i] = nullptr;
    free(parameterSets.sps_array), parameterSets.sps_array = nullptr;
    free(parameterSets.sps_sizes), parameterSets.sps_sizes = nullptr;
    parameterSets.sps_count = 0;
  }

  // free old saved pps's
  if (parameterSets.pps_count)
  {
    for (size_t i = 0; i < parameterSets.pps_count; i++)
      free(parameterSets.pps_array[i]), parameterSets.pps_array[i] = nullptr;
    free(parameterSets.pps_array), parameterSets.pps_array = nullptr;
    free(parameterSets.pps_sizes), parameterSets.pps_sizes = nullptr;
    parameterSets.pps_count = 0;
  }

  // free old saved vps's
  if (parameterSets.vps_count)
  {
    for (size_t i = 0; i < parameterSets.vps_count; i++)
      free(parameterSets.vps_array[i]), parameterSets.vps_array[i] = nullptr;
    free(parameterSets.vps_array), parameterSets.vps_array = nullptr;
    free(parameterSets.vps_sizes), parameterSets.vps_sizes = nullptr;
    parameterSets.vps_count = 0;
  }
}

bool
CDarwinVideoUtils::ParsePacketForVideoParameterSets(
  VideoParameterSets &parameterSets, AVCodecID codec, uint8_t *pData, int iSize)
{
  static uint64_t frameCount = 0;

  size_t spsCount = 0;
  size_t ppsCount = 0;
  size_t vpsCount = 0;

  // pData is in bit stream format, 32 size (big endian), followed by NAL
  uint8_t *data = pData;
  uint8_t *dataEnd = data + iSize;
  // do a quick search for parameter sets in this frame
  while (data < dataEnd)
  {
    int nal_size = BS_RB32(data);
    data += 4;
    if (codec == AV_CODEC_ID_H264)
    {
      int nal_type = data[0] & 0x1f;
      switch(nal_type)
      {
        case AVC_NAL_SPS:
          spsCount++;
          //CLog::Log(LOGDEBUG, "%s - frame(%llu), found sps of size(%d)", __FUNCTION__, frameCount, nal_size);
        break;
        case AVC_NAL_PPS:
          ppsCount++;
          //CLog::Log(LOGDEBUG, "%s - frame(%llu), found pps of size(%d)", __FUNCTION__, frameCount, nal_size);
        break;
      }
    }
    else if (codec == AV_CODEC_ID_HEVC)
    {
      int nal_type = (data[0] >> 1) & 0x3f;
      switch(nal_type)
      {
        case HEVC_NAL_SPS:
          spsCount++;
          //CLog::Log(LOGDEBUG, "%s - frame(%llu), found sps of size(%d)", __FUNCTION__, frameCount, nal_size);
        break;
        case HEVC_NAL_PPS:
          ppsCount++;
          //CLog::Log(LOGDEBUG, "%s - frame(%llu), found pps of size(%d)", __FUNCTION__, frameCount, nal_size);
        break;
        case HEVC_NAL_VPS:
          vpsCount++;
          //CLog::Log(LOGDEBUG, "%s - frame(%llu), found vps of size(%d)", __FUNCTION__, frameCount, nal_size);
        break;
      }
    }
    data += nal_size;
  }
  frameCount++;

  if (ppsCount > 0 && spsCount < 1)
  {
    // if no sps is found, skip parsing checks
    // pps can change per picture so ignore those.
    return false;
  }

  // found some parameter sets, now we can compare them to the original parameter sets
  if (spsCount && ppsCount)
  {
    FreeParameterSets(parameterSets);

    int spsIndex = 0;
    if (spsCount)
    {
      parameterSets.sps_count = spsCount;
      parameterSets.sps_sizes = (size_t*)malloc(sizeof(size_t*) * spsCount);
      parameterSets.sps_array = (uint8_t**)malloc(sizeof(uint8_t*) * spsCount);
    }

    int ppsIndex = 0;
    if (ppsCount)
    {
      parameterSets.pps_count = ppsCount;
      parameterSets.pps_sizes = (size_t*)malloc(sizeof(size_t*) * ppsCount);
      parameterSets.pps_array = (uint8_t**)malloc(sizeof(uint8_t*) * ppsCount);
    }

    int vpsIndex = 0;
    if (vpsCount)
    {
      parameterSets.vps_count = vpsCount;
      parameterSets.vps_sizes = (size_t*)malloc(sizeof(size_t*) * vpsCount);
      parameterSets.vps_array = (uint8_t**)malloc(sizeof(uint8_t*) * vpsCount);
    }
    data = pData;
    while (data < dataEnd)
    {
      int nal_size = BS_RB32(data);
      data += 4;
      if (codec == AV_CODEC_ID_H264)
      {
        int nal_type = data[0] & 0x1f;
        switch(nal_type)
        {
          case AVC_NAL_SPS:
            parameterSets.sps_sizes[spsIndex] = nal_size;
            parameterSets.sps_array[spsIndex] = (uint8_t*)malloc(sizeof(uint8_t) * nal_size);
            memcpy(parameterSets.sps_array[spsIndex], data, nal_size);
            spsIndex++;
            break;
          case AVC_NAL_PPS:
            parameterSets.pps_sizes[ppsIndex] = nal_size;
            parameterSets.pps_array[ppsIndex] = (uint8_t*)malloc(sizeof(uint8_t) * nal_size);
            memcpy(parameterSets.pps_array[ppsIndex], data, nal_size);
            ppsIndex++;
            break;
        }
      }
      else if (codec == AV_CODEC_ID_HEVC)
      {
        int nal_type = (data[0] >> 1) & 0x3f;
        switch(nal_type)
        {
          case HEVC_NAL_SPS:
            parameterSets.sps_sizes[spsIndex] = nal_size;
            parameterSets.sps_array[spsIndex] = (uint8_t*)malloc(sizeof(uint8_t) * nal_size);
            memcpy(parameterSets.sps_array[spsIndex], data, nal_size);
            spsIndex++;
            break;
          case HEVC_NAL_PPS:
            parameterSets.pps_sizes[ppsIndex] = nal_size;
            parameterSets.pps_array[ppsIndex] = (uint8_t*)malloc(sizeof(uint8_t) * nal_size);
            memcpy(parameterSets.pps_array[ppsIndex], data, nal_size);
            ppsIndex++;
            break;
          case HEVC_NAL_VPS:
            parameterSets.vps_sizes[vpsIndex] = nal_size;
            parameterSets.vps_array[vpsIndex] = (uint8_t*)malloc(sizeof(uint8_t) * nal_size);
            memcpy(parameterSets.vps_array[vpsIndex], data, nal_size);
            vpsIndex++;
            break;
        }
      }
      data += nal_size;
    }
    return true;
  }

  return false;
}

void
CDarwinVideoUtils::ProbeNALUnits(AVCodecID codec, uint8_t *pData, int iSize)
{
  if (codec != AV_CODEC_ID_HEVC)
    return;

  static uint64_t frameCount = 0;

  // pData is in bit stream format, 32 size (big endian), followed by NAL value
  uint8_t *data = pData;
  uint8_t *dataEnd = data + iSize;
  // do a quick search for parameter sets in this frame
  while (data < dataEnd)
  {
    int nal_size = BS_RB32(data);
    data += 4;
    // bit(1) array_completeness;
    // bit(1) reserved = 0;
    // bit(6) NAL_unit_type;
    int nal_type_full = data[0] << 8 | data[1];
    int nal_type = (data[0] >> 1) & 0x3f;
    switch(nal_type)
    {
      default:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), nal_type(%d)",
          __FUNCTION__, frameCount, nal_type);
        break;

      case HEVC_NAL_UNSPECIFIED_62:
      case HEVC_NAL_UNSPECIFIED_63:
        // The dolby vision enhancement layer doesn't actually use NAL units 63 and 62,
        // it uses a special syntax that uses 0x7E01 and 0x7C01 as separator that makes
        // the Dolby Extension Layer (EL) appear as unspecified/unused NAL units (62 and 63),
        // but the actual NAL information starts after that extra header.
        if (nal_type_full == 0x7C01)
        {
          CLog::Log(LOGDEBUG, "%s - frame(%llu), dolby vision enhancement layer of 0x7C01 with size(%d)",
            __FUNCTION__, frameCount, nal_size);
        }
        else if (nal_type_full == 0x7E01)
          CLog::Log(LOGDEBUG, "%s - frame(%llu), dolby vision enhancement layer of 0x7E01 with size(%d)",
            __FUNCTION__, frameCount, nal_size);
        else
        {
          CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_UNSPECIFIED_62/63 with size(%d)",
            __FUNCTION__, frameCount, nal_size);
        }
        break;
      case HEVC_NAL_TRAIL_R:
      case HEVC_NAL_TRAIL_N:
      case HEVC_NAL_TSA_N:
      case HEVC_NAL_TSA_R:
      case HEVC_NAL_STSA_N:
      case HEVC_NAL_STSA_R:
      case HEVC_NAL_BLA_W_LP:
      case HEVC_NAL_BLA_W_RADL:
      case HEVC_NAL_BLA_N_LP:
      case HEVC_NAL_IDR_W_RADL:
      case HEVC_NAL_IDR_N_LP:
      case HEVC_NAL_CRA_NUT:
      case HEVC_NAL_RADL_N:
      case HEVC_NAL_RADL_R:
      case HEVC_NAL_RASL_N:
      case HEVC_NAL_RASL_R:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), nal slice of type(%d) with size(%d)",
          __FUNCTION__, frameCount, nal_type, nal_size);
        break;
      case HEVC_NAL_AUD:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_AUD",
          __FUNCTION__, frameCount);
        break;
      case HEVC_NAL_VPS:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_VPS",
          __FUNCTION__, frameCount);
        break;
      case HEVC_NAL_SPS:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_SPS",
          __FUNCTION__, frameCount);
        break;
      case HEVC_NAL_PPS:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_PPS",
          __FUNCTION__, frameCount);
        break;
      case HEVC_NAL_SEI_PREFIX:
      {
        // todo: this only handles the 1st sei prefix message
        // it should loop over until done, see ff_hevc_decode_nal_sei
        uint8_t *payload = data + 2;
        // once we get NAL type, switch to bit stream reader
        // as it has to do three byte emulation prevention handling.
        nal_bitstream bs;
        CBitstreamParser::nal_bs_init(&bs, payload, dataEnd - payload);

        int payload_type = 0;
        int payload_size = 0;
        int byte = 0xFF;
        while (byte == 0xFF) {
            byte = CBitstreamParser::nal_bs_read(&bs, 8);
            payload_type += byte;
        }
        byte = 0xFF;
        while (byte == 0xFF) {
            byte = CBitstreamParser::nal_bs_read(&bs, 8);
            payload_size += byte;
        }

        switch(payload_type)
        {
          default:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), sei prefix of type(%d) with size(%d)",
              __FUNCTION__, frameCount, nal_size, payload_type);
            break;
          case SEI_TYPE_BUFFERING_PERIOD:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_BUFFERING_PERIOD with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_PICTURE_TIMING:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_PICTURE_TIMING with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_USER_DATA_UNREGISTERED:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_USER_DATA_UNREGISTERED with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_RECOVERY_POINT:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_RECOVERY_POINT with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_SCENE_INFO:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_SCENE_INFO with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_ACTIVE_PARAMETER_SETS:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_ACTIVE_PARAMETER_SETS with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
          case SEI_TYPE_MASTERING_DISPLAY_INFO:
            {
              // Mastering primaries (in g,b,r order)
              // specify the normalized x and y chromaticity coordinates
              // units of 0.00002, range 0 to 50,000
              uint16_t display_primaries[3][2];
              for (int i = 0; i < 3; i++)
              {
                display_primaries[i][0] = CBitstreamParser::nal_bs_read(&bs, 16);
                display_primaries[i][1] = CBitstreamParser::nal_bs_read(&bs, 16);
              }
              float displayPrimaries[3][2];
              for (int i = 0; i < 3; i++)
              {
                displayPrimaries[i][0] = 0.00002f * (float)display_primaries[i][0];
                displayPrimaries[i][1] = 0.00002f * (float)display_primaries[i][0];
              }
              // White point (x, y)
              // units of 0.00002, range 0 to 50,000
              uint16_t white_point[2];
              white_point[0] = CBitstreamParser::nal_bs_read(&bs, 16);
              white_point[1] = CBitstreamParser::nal_bs_read(&bs, 16);
              float whitePoint[2];
              whitePoint[0] = 0.00002f * (float)white_point[0];
              whitePoint[1] = 0.00002f * (float)white_point[1];

              // Max and min luminance of mastering display
              // units of 0.0001 cd/m2
              // no range specified but min < max.
              uint32_t max_display_mastering_luminance = CBitstreamParser::nal_bs_read(&bs, 32);
              uint32_t min_display_mastering_luminance = CBitstreamParser::nal_bs_read(&bs, 32);
              float maxMasteringLuminance = 0.0001f * (float)max_display_mastering_luminance;
              float minMasteringLuminance = 0.0001f * (float)min_display_mastering_luminance;
              CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_MASTERING_DISPLAY_INFO with size(%d)",
                __FUNCTION__, frameCount, payload_size);

              CLog::Log(LOGDEBUG, "Mastering display color primaries : R: x=%f y=%f, G: x=%f y=%f, B: x=%f y=%f, White point: x=%f y=%f",
                displayPrimaries[2][0], displayPrimaries[2][1],
                displayPrimaries[1][0], displayPrimaries[1][1],
                displayPrimaries[0][0], displayPrimaries[0][1],
                whitePoint[0], whitePoint[1]);
              CLog::Log(LOGDEBUG, "Mastering display luminance       : min: %f cd/m2, max: %f cd/m2",
                minMasteringLuminance, maxMasteringLuminance);
            }
            break;
          case SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO:
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            break;
        }
      }
      break;
      case HEVC_NAL_SEI_SUFFIX:
        CLog::Log(LOGDEBUG, "%s - frame(%llu), sei suffix with size(%d)", __FUNCTION__, frameCount, nal_size);
      break;
    }
    data += nal_size;
  }
  frameCount++;
}

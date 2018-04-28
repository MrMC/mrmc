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

#include "DVDVideoCodec.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "windowing/WindowingFactory.h"
#import "utils/BitstreamConverter.h"
#include "utils/log.h"

//#define PARSE_NAL_VERBOSE

bool CDVDVideoCodec::IsSettingVisible(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  if (setting == NULL || value.empty())
    return false;

  const std::string &settingId = setting->GetId();

  // check if we are running on nvidia hardware
  std::string gpuvendor = g_Windowing.GetRenderVendor();
  std::transform(gpuvendor.begin(), gpuvendor.end(), gpuvendor.begin(), ::tolower);
  bool isNvidia = (gpuvendor.compare(0, 6, "nvidia") == 0);
  bool isIntel = (gpuvendor.compare(0, 5, "intel") == 0);

  // nvidia does only need mpeg-4 setting
  if (isNvidia)
  {
    if (settingId == CSettings::SETTING_VIDEOPLAYER_USEVDPAUMPEG4)
      return true;

    return false; // will also hide intel settings on nvidia hardware
  }
  else if (isIntel) // intel needs vc1, mpeg-2 and mpeg4 setting
  {
    if (settingId == CSettings::SETTING_VIDEOPLAYER_USEVAAPIMPEG4)
      return true;
    if (settingId == CSettings::SETTING_VIDEOPLAYER_USEVAAPIVC1)
      return true;
    if (settingId == CSettings::SETTING_VIDEOPLAYER_USEVAAPIMPEG2)
      return true;

    return false; // this will also hide nvidia settings on intel hardware
  }
  // if we don't know the hardware we are running on e.g. amd oss vdpau 
  // or fglrx with xvba-driver we show everything
  return true;
}

bool CDVDVideoCodec::IsCodecDisabled(DVDCodecAvailableType* map, unsigned int size, AVCodecID id)
{
  int index = -1;
  for (unsigned int i = 0; i < size; ++i)
  {
    if (map[i].codec == id)
    {
      index = (int) i;
      break;
    }
  }
  if (index > -1)
  {
    return (!CSettings::GetInstance().GetBool(map[index].setting) ||
            !CDVDVideoCodec::IsSettingVisible("unused", "unused",
                                              CSettings::GetInstance().GetSetting(map[index].setting),
                                              NULL));
  }

  return false; // don't disable what we don't have
}

/** Dolby Vision Profile enum type */
typedef enum OMX_VIDEO_DOLBYVISIONPROFILETYPE {
    OMX_VIDEO_DolbyVisionProfileUnknown = 0x0,
    OMX_VIDEO_DolbyVisionProfileDvavDer = 0x1,
    OMX_VIDEO_DolbyVisionProfileDvavDen = 0x2,
    OMX_VIDEO_DolbyVisionProfileDvheDer = 0x3,
    OMX_VIDEO_DolbyVisionProfileDvheDen = 0x4,
    OMX_VIDEO_DolbyVisionProfileDvheDtr = 0x5,
    OMX_VIDEO_DolbyVisionProfileDvheStn = 0x6,
    OMX_VIDEO_DolbyVisionProfileMax     = 0x7FFFFFFF
} OMX_VIDEO_DOLBYVISIONPROFILETYPE;
/** Dolby Vision Level enum type */
typedef enum OMX_VIDEO_DOLBYVISIONLEVELTYPE {
    OMX_VIDEO_DolbyVisionLevelUnknown = 0x0,
    OMX_VIDEO_DolbyVisionLevelHd24    = 0x1,
    OMX_VIDEO_DolbyVisionLevelHd30    = 0x2,
    OMX_VIDEO_DolbyVisionLevelFhd24   = 0x4,
    OMX_VIDEO_DolbyVisionLevelFhd30   = 0x8,
    OMX_VIDEO_DolbyVisionLevelFhd60   = 0x10,
    OMX_VIDEO_DolbyVisionLevelUhd24   = 0x20,
    OMX_VIDEO_DolbyVisionLevelUhd30   = 0x40,
    OMX_VIDEO_DolbyVisionLevelUhd48   = 0x80,
    OMX_VIDEO_DolbyVisionLevelUhd60   = 0x100,
    OMX_VIDEO_DolbyVisionLevelmax     = 0x7FFFFFFF
} OMX_VIDEO_DOLBYVISIONLEVELTYPE;

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

NALInfo CDVDVideoCodec::ProbeHEVCNALUnits(uint8_t *pData, int iSize)
{
  static uint64_t frameCount = 0;

  NALInfo info;

  // pData is in bit stream format, 32 size (big endian), followed by NAL value
  uint8_t *data = pData;
  uint8_t *dataEnd = data + iSize;
  // do a quick search for parameter sets in this frame
  while (data < dataEnd)
  {
    if (
        data[0] == 0x00 &&
        data[1] == 0x00 &&
        data[2] == 0x01
        )
    {
      data += 3;
    }
    else if (
             data[0] == 0x00 &&
             data[1] == 0x00 &&
             data[2] == 0x00 &&
             data[3] == 0x01
             )
    {
      data += 4;
    }
    else
    {
      data += 1;
      continue;
    }

    int nal_size = 1;
    // bit(1) array_completeness;
    // bit(1) reserved = 0;
    // bit(6) NAL_unit_type;
    int nal_type_full = data[0] << 8 | data[1];
    int nal_type = (data[0] >> 1) & 0x3f;
    switch(nal_type)
    {
      default:
#ifdef PARSE_NAL_VERBOSE
        CLog::Log(LOGDEBUG, "%s - frame(%llu), nal_type(%d)",
          __FUNCTION__, frameCount, nal_type);
#endif
        break;

      case HEVC_NAL_UNSPECIFIED_62:
      case HEVC_NAL_UNSPECIFIED_63:
        // The dolby vision enhancement layer doesn't actually use NAL units 63 and 62,
        // it uses a special syntax that uses 0x7E01 and 0x7C01 as separator that makes
        // the Dolby Extension Layer (EL) appear as unspecified/unused NAL units (62 and 63),
        // but the actual NAL information starts after that extra header.
        if (nal_type_full == 0x7C01)
        {
#ifdef PARSE_NAL_VERBOSE
          CLog::Log(LOGDEBUG, "%s - frame(%llu), dolby vision enhancement layer of 0x7C01 with size(%d)",
            __FUNCTION__, frameCount, nal_size);
#endif
        }
        else if (nal_type_full == 0x7E01)
        {
#ifdef PARSE_NAL_VERBOSE
          CLog::Log(LOGDEBUG, "%s - frame(%llu), dolby vision enhancement layer of 0x7E01 with size(%d)",
            __FUNCTION__, frameCount, nal_size);
#endif
        }
        else
        {
#ifdef PARSE_NAL_VERBOSE
          CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_UNSPECIFIED_62/63 with size(%d)",
            __FUNCTION__, frameCount, nal_size);
#endif
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
#ifdef xxPARSE_NAL_VERBOSE
        CLog::Log(LOGDEBUG, "%s - frame(%llu), nal slice of type(%d) with size(%d)",
          __FUNCTION__, frameCount, nal_type, nal_size);
#endif
        break;
      case HEVC_NAL_AUD:
#ifdef PARSE_NAL_VERBOSE
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_AUD",
          __FUNCTION__, frameCount);
#endif
        break;
      case HEVC_NAL_VPS:
#ifdef PARSE_NAL_VERBOSE
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_VPS",
          __FUNCTION__, frameCount);
#endif
        break;
      case HEVC_NAL_SPS:
#ifdef PARSE_NAL_VERBOSE
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_SPS",
          __FUNCTION__, frameCount);
#endif
        break;
      case HEVC_NAL_PPS:
#ifdef PARSE_NAL_VERBOSE
        CLog::Log(LOGDEBUG, "%s - frame(%llu), HEVC_NAL_PPS",
          __FUNCTION__, frameCount);
#endif
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
#ifdef PARSE_NAL_VERBOSE
            CLog::Log(LOGDEBUG, "%s - frame(%llu), sei prefix of type(%d) with size(%d)",
              __FUNCTION__, frameCount, nal_size, payload_type);
#endif
           break;
          case SEI_TYPE_BUFFERING_PERIOD:
#ifdef PARSE_NAL_VERBOSE
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_BUFFERING_PERIOD with size(%d)",
              __FUNCTION__, frameCount, payload_size);
#endif
            break;
          case SEI_TYPE_PICTURE_TIMING:
#ifdef PARSE_NAL_VERBOSE
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_PICTURE_TIMING with size(%d)",
              __FUNCTION__, frameCount, payload_size);
#endif
            break;
          case SEI_TYPE_USER_DATA_UNREGISTERED:
#ifdef PARSE_NAL_VERBOSE
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_USER_DATA_UNREGISTERED with size(%d)",
              __FUNCTION__, frameCount, payload_size);
#endif
            break;
          case SEI_TYPE_RECOVERY_POINT:
#ifdef PARSE_NAL_VERBOSE
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_RECOVERY_POINT with size(%d)",
              __FUNCTION__, frameCount, payload_size);
#endif
            break;
          case SEI_TYPE_SCENE_INFO:
#ifdef PARSE_NAL_VERBOSE
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_SCENE_INFO with size(%d)",
              __FUNCTION__, frameCount, payload_size);
#endif
            break;
          case SEI_TYPE_ACTIVE_PARAMETER_SETS:
#ifdef PARSE_NAL_VERBOSE
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_ACTIVE_PARAMETER_SETS with size(%d)",
              __FUNCTION__, frameCount, payload_size);
#endif
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

              info.master_prim_rx = display_primaries[2][0];
              info.master_prim_ry = display_primaries[2][1];
              info.master_prim_gx = display_primaries[1][0];
              info.master_prim_gy = display_primaries[1][1];
              info.master_prim_bx = display_primaries[0][0];
              info.master_prim_by = display_primaries[0][1];
              info.master_prim_wx = white_point[0];
              info.master_prim_wy = white_point[1];

              info.master_prim_maxlum = (float)max_display_mastering_luminance;
              info.master_prim_minlum = (float)min_display_mastering_luminance;
              
              info.has_master_prim = true;

#ifdef PARSE_NAL_VERBOSE
              CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_MASTERING_DISPLAY_INFO with size(%d)",
                __FUNCTION__, frameCount, payload_size);

              CLog::Log(LOGDEBUG, "Mastering display color primaries : R: x=%f y=%f, G: x=%f y=%f, B: x=%f y=%f, White point: x=%f y=%f",
                displayPrimaries[2][0], displayPrimaries[2][1],
                displayPrimaries[1][0], displayPrimaries[1][1],
                displayPrimaries[0][0], displayPrimaries[0][1],
                whitePoint[0], whitePoint[1]);
              float maxMasteringLuminance = 0.0001f * (float)max_display_mastering_luminance;
              float minMasteringLuminance = 0.0001f * (float)min_display_mastering_luminance;
              CLog::Log(LOGDEBUG, "Mastering display luminance       : min: %f cd/m2, max: %f cd/m2",
                minMasteringLuminance, maxMasteringLuminance);
#endif
            }
            break;
          case SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO:
          {
            info.light_maxcll = CBitstreamParser::nal_bs_read(&bs, 16);
            info.light_maxfall = CBitstreamParser::nal_bs_read(&bs, 16);
            info.has_light = true;

#ifdef PARSE_NAL_VERBOSE
            CLog::Log(LOGDEBUG, "%s - frame(%llu), SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO with size(%d)",
              __FUNCTION__, frameCount, payload_size);
            CLog::Log(LOGDEBUG, " Light Levels: content %d/%f average %d/%f", info.light_maxcll, log10(100) / log10(info.light_maxcll), info.light_maxfall, log10(100) / log10(info.light_maxfall));
#endif
            break;
          }
        }
      }
      break;
      case HEVC_NAL_SEI_SUFFIX:
#ifdef PARSE_NAL_VERBOSE
        CLog::Log(LOGDEBUG, "%s - frame(%llu), sei suffix with size(%d)", __FUNCTION__, frameCount, nal_size);
#endif
      break;
    }
    data += nal_size;
  }
  frameCount++;
  return info;
}

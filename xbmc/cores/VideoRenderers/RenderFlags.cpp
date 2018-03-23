/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "RenderFlags.h"
#include <string>
#include <map>

extern "C" {
#include "libavutil/pixfmt.h"
}

namespace RenderManager {

  unsigned int GetFlagsColorMatrix(unsigned int color_matrix, unsigned width, unsigned height)
  {
    switch(color_matrix)
    {
      case AVCOL_SPC_SMPTE2085:   // smpte2085
      case AVCOL_SPC_BT2020_CL:   // BT2020_CL (Constant Luminance)
      case AVCOL_SPC_BT2020_NCL:  // BT2020_NCL (Non-Constant Luminance)
        return CONF_FLAGS_YUVCOEF_BT2020;
      case AVCOL_SPC_SMPTE240M: // SMPTE 240M (1987)
        return CONF_FLAGS_YUVCOEF_240M;
      case AVCOL_SPC_SMPTE170M: // SMPTE 170M
      case AVCOL_SPC_BT470BG: // ITU-R BT.470-2
      case AVCOL_SPC_FCC: // FCC
        return CONF_FLAGS_YUVCOEF_BT601;
      case AVCOL_SPC_BT709: // ITU-R Rec.709 (1990) -- BT.709
        return CONF_FLAGS_YUVCOEF_BT709;
      case AVCOL_SPC_RESERVED: // RESERVED
      case AVCOL_SPC_UNSPECIFIED: // UNSPECIFIED
      default:
        if (width > 1024 || height >= 600)
          return CONF_FLAGS_YUVCOEF_BT709;
        else
          return CONF_FLAGS_YUVCOEF_BT601;
        break;
    }
  }

  unsigned int GetFlagsChromaPosition(unsigned int chroma_position)
  {
    switch(chroma_position)
    {
      case AVCHROMA_LOC_LEFT: return CONF_FLAGS_CHROMA_LEFT;
      case AVCHROMA_LOC_CENTER: return CONF_FLAGS_CHROMA_CENTER;
      case AVCHROMA_LOC_TOPLEFT: return CONF_FLAGS_CHROMA_TOPLEFT;
    }
    return 0;
  }

  unsigned int GetFlagsColorPrimaries(unsigned int color_primaries)
  {
    switch(color_primaries)
    {
      case AVCOL_PRI_BT709: return CONF_FLAGS_COLPRI_BT709;
      case AVCOL_PRI_BT470M: return CONF_FLAGS_COLPRI_BT470M;
      case AVCOL_PRI_BT470BG: return CONF_FLAGS_COLPRI_BT470BG;
      case AVCOL_PRI_SMPTE170M: return CONF_FLAGS_COLPRI_170M;
      case AVCOL_PRI_SMPTE240M: return CONF_FLAGS_COLPRI_240M;
    }
    return 0;
  }

  unsigned int GetFlagsColorTransfer(unsigned int color_transfer)
  {
    switch(color_transfer)
    {
      case AVCOL_TRC_BT709: return CONF_FLAGS_TRC_BT709;
      case AVCOL_TRC_GAMMA22: return CONF_FLAGS_TRC_GAMMA22;
      case AVCOL_TRC_GAMMA28: return CONF_FLAGS_TRC_GAMMA28;
      case AVCOL_TRC_BT2020_10: return CONF_FLAGS_TRC_BT2020_10; // ITU-R BT2020 for 10-bit system
      case AVCOL_TRC_BT2020_12: return CONF_FLAGS_TRC_BT2020_12; // ITU-R BT2020 for 12-bit system
      case AVCOL_TRC_SMPTE2084: return CONF_FLAGS_TRC_SMPTE2084; // PQ/SMPTE ST 2084 for 10-, 12-, 14- and 16-bit systems
      case AVCOL_TRC_ARIB_STD_B67: return CONF_FLAGS_TRC_ARIB_STD_B67; // ARIB STD-B67, known as "Hybrid log-gamma"
    }
    return 0;
  }

  unsigned int GetFlagsDynamicRange(unsigned int dynamic_range)
  {
    switch(dynamic_range)
    {
      case 1: return CONF_FLAGS_DYNAMIC_RANGE_SDR;
      case 2: return CONF_FLAGS_DYNAMIC_RANGE_HDR10;
      case 3: return CONF_FLAGS_DYNAMIC_RANGE_DOLBYVISION;
    }
    return CONF_FLAGS_DYNAMIC_RANGE_SDR;
  }

  unsigned int GetStereoModeFlags(const std::string& mode)
  {
    unsigned int ret = 0u;
    if (!mode.empty())
    {
      static std::map<std::string, unsigned int> convert;
      if(convert.empty())
      {
        convert["mono"]                   = 0u;
        convert["left_right"]             = CONF_FLAGS_STEREO_MODE_SBS | CONF_FLAGS_STEREO_CADANCE_LEFT_RIGHT;
        convert["bottom_top"]             = CONF_FLAGS_STEREO_MODE_TAB | CONF_FLAGS_STEREO_CADANCE_RIGHT_LEFT;
        convert["top_bottom"]             = CONF_FLAGS_STEREO_MODE_TAB | CONF_FLAGS_STEREO_CADANCE_LEFT_RIGHT;
        convert["checkerboard_rl"]        = 0u;
        convert["checkerboard_lr"]        = 0u;
        convert["row_interleaved_rl"]     = 0u;
        convert["row_interleaved_lr"]     = 0u;
        convert["col_interleaved_rl"]     = 0u;
        convert["col_interleaved_lr"]     = 0u;
        convert["anaglyph_cyan_red"]      = 0u;
        convert["right_left"]             = CONF_FLAGS_STEREO_MODE_SBS | CONF_FLAGS_STEREO_CADANCE_RIGHT_LEFT;
        convert["anaglyph_green_magenta"] = 0u;
        convert["anaglyph_yellow_blue"]   = 0u;
        convert["block_lr"]               = 0u;
        convert["block_rl"]               = 0u;
      }
      if ( convert.find(mode) != convert.end())
        ret = convert[mode];
    }

    return ret;
  }

  std::string GetStereoModeInvert(const std::string& mode)
  {
    static std::map<std::string, std::string> convert;
    if(convert.empty())
    {
      convert["left_right"]             = "right_left";
      convert["right_left"]             = "left_right";
      convert["bottom_top"]             = "top_bottom";
      convert["top_bottom"]             = "bottom_top";
      convert["checkerboard_rl"]        = "checkerboard_lr";
      convert["checkerboard_lr"]        = "checkerboard_rl";
      convert["row_interleaved_rl"]     = "row_interleaved_lr";
      convert["row_interleaved_lr"]     = "row_interleaved_rl";
      convert["col_interleaved_rl"]     = "col_interleaved_lr";
      convert["col_interleaved_lr"]     = "col_interleaved_rl";
      convert["block_lr"]               = "block_lr";
      convert["block_rl"]               = "block_rl";
    }
    std::string res = convert[mode];
    if(res.empty())
      return mode;
    else
      return res;
  }
}

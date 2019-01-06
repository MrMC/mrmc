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

#include "DolbyFrameParser.h"
#include "utils/MemoryBitstream.h"

#include <sys/param.h>

extern "C" {
#include "libavformat/avformat.h"
// ffmpeg private header, so extern what we need
//#include "libavcodec/ac3tabs.h"
extern const uint16_t ff_ac3_sample_rate_tab[3];
extern const uint16_t ff_ac3_bitrate_tab[19];
extern const uint8_t  ff_ac3_channels_tab[8];
extern const uint16_t ff_ac3_frame_size_tab[38][3];
}

#define EC3Extension_JOC  1
#define EC3Extension_None 0

typedef struct eac3_info {
  uint8_t *frame;
  uint32_t size;

  uint8_t ec3_done;
  uint8_t num_blocks;

  // Layout of the EC3SpecificBox
  // maximum bitrate
  uint16_t data_rate;
  // number of independent substreams
  uint8_t num_ind_sub;
  //
   // See ETSI TS 103 420 V1.2.1 (2018-10)
   // 8.3.2 Semantics
   // 8.3.2.1 flag_ec3_extension_type_a
   // The element flag_ec3_extension_type_a is a flag that, if set to true,
   // indicates the enhanced AC-3 extension as defined in the present document.
   // 8.3.2.2 complexity_index_type_a
   // The element complexity_index_type_a is an unsigned integer that
   // indicates the decoding complexity of the enhanced AC-3 extension
   // defined in the present document. The value of this field shall be
   // equal to the total number of bed objects, ISF objects and dynamic
   // objects indicated by the parameters in the program_assignment
   // section of the object audio metadata payload.
   // The maximum value of this field shall be 16.

  uint8_t ec3_extension_type;      // 0x01 -> E-AC3 JOC extension
  uint8_t complexity_index;        // 0 <= complexity_index <= 16
  struct {
    // sample rate code (see ff_ac3_sample_rate_tab) 2 bits
    uint8_t fscod;
    // bit stream identification 5 bits
    uint8_t bsid;
    // one bit reserved
    // audio service mixing (not supported yet) 1 bit
    // bit stream mode 3 bits
    uint8_t bsmod;
    // audio coding mode 3 bits
    uint8_t acmod;
    // sub woofer on 1 bit
    uint8_t lfeon;
    // 3 bits reserved
    // number of dependent substreams associated with this substream 4 bits
    uint8_t num_dep_sub;
    // channel locations of the dependent substream(s), if any, 9 bits
    uint16_t chan_loc;
    // if there is no dependent substream, then one bit reserved instead
  } substream[1]; // TODO: support 8 independent substreams
} EAC3Info;


//* Channel mode (audio coding mode)
typedef enum {
  AC3_CHMODE_DUALMONO = 0,
  AC3_CHMODE_MONO,
  AC3_CHMODE_STEREO,
  AC3_CHMODE_3F,
  AC3_CHMODE_2F1R,
  AC3_CHMODE_3F1R,
  AC3_CHMODE_2F2R,
  AC3_CHMODE_3F2R
} AC3ChannelMode;

typedef enum {
  AAC_AC3_PARSE_ERROR_SYNC        = -0x1030c0a,
  AAC_AC3_PARSE_ERROR_BSID        = -0x2030c0a,
  AAC_AC3_PARSE_ERROR_SAMPLE_RATE = -0x3030c0a,
  AAC_AC3_PARSE_ERROR_FRAME_SIZE  = -0x4030c0a,
  AAC_AC3_PARSE_ERROR_FRAME_TYPE  = -0x5030c0a,
  AAC_AC3_PARSE_ERROR_CRC         = -0x6030c0a,
  AAC_AC3_PARSE_ERROR_CHANNEL_CFG = -0x7030c0a,
} AACAC3ParseError;

#define AC3_HEADER_SIZE 7

static const uint8_t eac3_blocks[4] = {1, 2, 3, 6};
// Table for center mix levels
// reference: Section 5.4.2.4 cmixlev
static const uint8_t center_levels[4] = { 4, 5, 6, 5 };
// Table for surround mix levels
// reference: Section 5.4.2.5 surmixlev

static const uint8_t surround_levels[4] = { 4, 6, 7, 6 };

//* Dolby Surround mode
typedef enum AC3DolbySurroundMode {
  AC3_DSURMOD_NOTINDICATED = 0,
  AC3_DSURMOD_OFF,
  AC3_DSURMOD_ON,
  AC3_DSURMOD_RESERVED
} AC3DolbySurroundMode;

typedef enum {
  EAC3_FRAME_TYPE_INDEPENDENT = 0,
  EAC3_FRAME_TYPE_DEPENDENT,
  EAC3_FRAME_TYPE_AC3_CONVERT,
  EAC3_FRAME_TYPE_RESERVED
} EAC3FrameType;

typedef enum {
  AC3_CUSTOM_CHANNEL_MAP_LFE              = 0x00000001,
  AC3_CUSTOM_CHANNEL_MAP_LFE2             = 0x00000002,
  AC3_CUSTOM_CHANNEL_MAP_LTS_RTS_PAIR     = 0x00000004,
  AC3_CUSTOM_CHANNEL_MAP_VHC              = 0x00000008,
  AC3_CUSTOM_CHANNEL_MAP_VHL_VHR_PAIR     = 0x00000010,
  AC3_CUSTOM_CHANNEL_MAP_LW_RW_PAIR       = 0x00000020,
  AC3_CUSTOM_CHANNEL_MAP_LSD_RSD_PAIR     = 0x00000040,
  AC3_CUSTOM_CHANNEL_MAP_TS               = 0x00000080,
  AC3_CUSTOM_CHANNEL_MAP_CS               = 0x00000100,
  AC3_CUSTOM_CHANNEL_MAP_LRS_RRS_PAIR     = 0x00000200,
  AC3_CUSTOM_CHANNEL_MAP_LC_RC_PAIR       = 0x00000400,
  AC3_CUSTOM_CHANNEL_MAP_RIGHT_SURROUND   = 0x00000800,
  AC3_CUSTOM_CHANNEL_MAP_LEFT_SURROUND    = 0x00001000,
  AC3_CUSTOM_CHANNEL_MAP_RIGHT            = 0x00002000,
  AC3_CUSTOM_CHANNEL_MAP_CENTRE           = 0x00004000,
  AC3_CUSTOM_CHANNEL_MAP_LEFT             = 0x00008000,
} AC3ChanLoc;

typedef enum {
  DEC3_CUSTOM_CHANNEL_MAP_LC_RC_PAIR       = 0x00000001,
  DEC3_CUSTOM_CHANNEL_MAP_LRS_RRS_PAIR     = 0x00000002,
  DEC3_CUSTOM_CHANNEL_MAP_CS               = 0x00000004,
  DEC3_CUSTOM_CHANNEL_MAP_TS               = 0x00000008,
  DEC3_CUSTOM_CHANNEL_MAP_LSD_RSD_PAIR     = 0x00000010,
  DEC3_CUSTOM_CHANNEL_MAP_LW_RW_PAIR       = 0x00000020,
  DEC3_CUSTOM_CHANNEL_MAP_VHL_VHR_PAIR     = 0x00000040,
  DEC3_CUSTOM_CHANNEL_MAP_VHC              = 0x00000080,
  DEC3_CUSTOM_CHANNEL_MAP_LFE2             = 0x00000100,
} DEC3ChanLoc;

// Map audio coding mode (acmod) to channel layout mask.
const uint16_t avpriv_ac3_channel_layout_tab[8] = {
  AV_CH_LAYOUT_STEREO,
  AV_CH_LAYOUT_MONO,
  AV_CH_LAYOUT_STEREO,
  AV_CH_LAYOUT_SURROUND,
  AV_CH_LAYOUT_2_1,
  AV_CH_LAYOUT_4POINT0,
  AV_CH_LAYOUT_2_2,
  AV_CH_LAYOUT_5POINT0
};

// AC3HeaderInfo
// Coded AC-3 header values up to the lfeon element, plus derived values.
typedef struct AC3HeaderInfo {
  uint16_t sync_word;
  uint16_t crc1;
  uint8_t  sr_code;
  uint8_t  bitstream_id;
  uint8_t  bitstream_mode;
  uint8_t  channel_mode;
  uint8_t  lfe_on;
  uint8_t  frame_type;
  int      substreamid;                        ///< substream identification
  int      center_mix_level;                   ///< Center mix level index
  int      surround_mix_level;                 ///< Surround mix level index
  uint16_t channel_map;
  int      num_blocks;                         ///< number of audio blocks
  int      dolby_surround_mode;
  uint8_t  blkswe;              //If true, full block switch syntax shall be present in each audio block
  uint8_t  dithflage;            //If true, full dither flag syntax shall be present in each audio block
  uint8_t  ec3_extension_type;        //E-AC3 Extension as per ETSI TS 103 420
  uint8_t  complexity_index;        //Decoding complexity of E-AC3 Extension as per ETSI TS 103 420
  // Derived values
  uint8_t    sr_shift;
  uint16_t  sample_rate;
  uint32_t  bit_rate;
  uint8_t    channels;
  uint16_t  frame_size;
  uint64_t  channel_layout;
} AC3HeaderInfo;

const uint16_t ff_ac3_dec3_chap_map[16][2] = {
  {AC3_CUSTOM_CHANNEL_MAP_LFE             , NULL},
  {AC3_CUSTOM_CHANNEL_MAP_LFE2            , DEC3_CUSTOM_CHANNEL_MAP_LFE2},
  {AC3_CUSTOM_CHANNEL_MAP_LTS_RTS_PAIR    , NULL},
  {AC3_CUSTOM_CHANNEL_MAP_VHC             , DEC3_CUSTOM_CHANNEL_MAP_VHC},
  {AC3_CUSTOM_CHANNEL_MAP_VHL_VHR_PAIR    , DEC3_CUSTOM_CHANNEL_MAP_VHL_VHR_PAIR},
  {AC3_CUSTOM_CHANNEL_MAP_LW_RW_PAIR      , DEC3_CUSTOM_CHANNEL_MAP_LW_RW_PAIR},
  {AC3_CUSTOM_CHANNEL_MAP_LSD_RSD_PAIR    , DEC3_CUSTOM_CHANNEL_MAP_LSD_RSD_PAIR},
  {AC3_CUSTOM_CHANNEL_MAP_TS              , DEC3_CUSTOM_CHANNEL_MAP_TS},
  {AC3_CUSTOM_CHANNEL_MAP_CS              , DEC3_CUSTOM_CHANNEL_MAP_CS},
  {AC3_CUSTOM_CHANNEL_MAP_LRS_RRS_PAIR    , DEC3_CUSTOM_CHANNEL_MAP_LRS_RRS_PAIR},
  {AC3_CUSTOM_CHANNEL_MAP_LC_RC_PAIR      , DEC3_CUSTOM_CHANNEL_MAP_LC_RC_PAIR},
  {AC3_CUSTOM_CHANNEL_MAP_RIGHT_SURROUND  , NULL},
  {AC3_CUSTOM_CHANNEL_MAP_LEFT_SURROUND   , NULL},
  {AC3_CUSTOM_CHANNEL_MAP_RIGHT           , NULL},
  {AC3_CUSTOM_CHANNEL_MAP_CENTRE          , NULL},
  {AC3_CUSTOM_CHANNEL_MAP_LEFT            , NULL},
};

static uint16_t ac3_to_dec3_chan_map(uint16_t ac3_chan_loc) {
  uint16_t dec3_chan_loc = 0;

  for (int i = 0; i < 16; i++)
  {
    uint16_t chan_loc = ac3_chan_loc & (0x1 << i);
    for (int j = 0; j < 16; j++)
    {
      if (ff_ac3_dec3_chap_map[j][0] ==  chan_loc)
        dec3_chan_loc |= ff_ac3_dec3_chap_map[j][1];
    }
  }

  return dec3_chan_loc;
}
std::string CDolbyFrameParser::parse(const uint8_t *buf, int len)
{
  // assume correct endian for arch
  if (buf[0] != 0x0b || buf[1] != 0x77)
    return "";

  mimeType = "";
  eac3_info info;
  analyze(&info, (uint8_t*)buf, len);
/*
  CMemoryBitstream bs;
  bs.SetBytes((uint8_t*)buf, len);
  AC3HeaderInfo hdr = {};
  int error = parse_header(bs, &hdr);
*/
  return mimeType;
}

int CDolbyFrameParser::analyze(eac3_info *info, uint8_t *frame, int size)
{
  CMemoryBitstream bs;
  bs.SetBytes(frame, size);
  AC3HeaderInfo hdr = {};
  if (parseheader(bs, &hdr) < 0)
    return -1;

  info->data_rate = MAX(info->data_rate, hdr.bit_rate / 1000);
  int num_blocks = hdr.num_blocks;

  // fill the info needed for the "dec3" atom
  if (!info->ec3_done)
  {
    // AC-3 substream must be the first one
    if (hdr.bitstream_id <= 10 && hdr.substreamid != 0)
        return -1;

    // this should always be the case, given that our AC-3 parser
    // concatenates dependent frames to their independent parent
    if (hdr.frame_type == EAC3_FRAME_TYPE_INDEPENDENT)
    {
      // substream ids must be incremental
      if (hdr.substreamid > info->num_ind_sub + 1)
        return -1;

      if (hdr.substreamid == info->num_ind_sub + 1)
      {
        // info->num_ind_sub++;
        return -1;
      } else if (hdr.substreamid < info->num_ind_sub ||
                (hdr.substreamid == 0 && info->substream[0].bsid)) {
        info->ec3_done = 1;
        goto concatenate;
      }
    }

    info->substream[hdr.substreamid].fscod = hdr.sr_code;
    info->substream[hdr.substreamid].bsid  = hdr.bitstream_id;
    info->substream[hdr.substreamid].bsmod = hdr.bitstream_mode;
    info->substream[hdr.substreamid].acmod = hdr.channel_mode;
    info->substream[hdr.substreamid].lfeon = hdr.lfe_on;
    // to count num_dep_sub's only in this frame! Otherwise,
    // if instantiated context passed in, num_dep_sub gets incremented cumulatively.
    info->substream[hdr.substreamid].num_dep_sub = 0;

    // Parse dependent substream(s), if any
    if (size != hdr.frame_size) {
      int cumul_size = hdr.frame_size;
      int parent = hdr.substreamid;

      while (cumul_size != size) {
        int i;
        CMemoryBitstream gbc;
        gbc.SetBytes(frame + cumul_size, (size - cumul_size));
        if (parseheader(gbc, &hdr) < 0)
          return -1;

        if (hdr.frame_type != EAC3_FRAME_TYPE_DEPENDENT)
          return -1;

        cumul_size += hdr.frame_size;
        info->substream[parent].num_dep_sub++;

        // header is parsed up to lfeon, but custom channel map may be needed
        // skip bsid
        //gbc.SkipBits(5); // PV: parse_header() parses up to dialnorm!

        // skip volume control params
        for (i = 0; i < (hdr.channel_mode ? 1 : 2); i++)
        {
          gbc.SkipBits(5); // skip dialog normalization
          if (gbc.GetBits(1))
            gbc.SkipBits(8); // skip compression gain word
        }
        // get the dependent stream channel map, if exists
        if (gbc.GetBits(1))
        {
          uint16_t value = gbc.GetBits(16);
          info->substream[parent].chan_loc = ac3_to_dec3_chan_map(value);
        }
        else
        {
          info->substream[parent].chan_loc |= hdr.channel_mode;
        }
      }
    }
  }

concatenate:

  info->ec3_extension_type |= hdr.ec3_extension_type;
  info->complexity_index    = MAX(hdr.complexity_index, info->complexity_index);

  if (!info->num_blocks && num_blocks == 6)
      return size;

  else if (info->num_blocks + num_blocks > 6)
      return -2;

  if (!info->num_blocks)
  {
      // Copy the frame
      if (info->frame)
          free(info->frame);
      info->frame = (uint8_t *)malloc(size);
      if (info->frame == NULL)
          return -2;

      memcpy(info->frame, frame, size);
      info->size = size;
      info->num_blocks = num_blocks;
      return 0;
  }
  else
  {
      info->frame = (uint8_t *)realloc(info->frame, info->size + size);
      if (info->frame == NULL)
          return -2;

      memcpy(info->frame + info->size, frame, size);
      info->size += size;
      info->num_blocks += num_blocks;

      if (info->num_blocks != 6)
          return 0;

      info->num_blocks = 0;
  }

  return 0;
}
int CDolbyFrameParser::parseheader(CMemoryBitstream &bs, AC3HeaderInfo *hdr)
{
  int frame_size_code;

  hdr->sync_word = bs.GetBits(16);
  if (hdr->sync_word != 0x0B77)
    return AAC_AC3_PARSE_ERROR_SYNC;

  // read ahead to bsid to distinguish between AC-3 and E-AC-3
  bs.SkipBits(24);
  hdr->bitstream_id = bs.GetBits(5); // bsid
  bs.SetBitPosition(bs.GetBitPosition() - 29); // back to start of bsi
  if (hdr->bitstream_id > 16)
    return AAC_AC3_PARSE_ERROR_BSID;

  hdr->num_blocks = 6;

  // set default mix levels
  hdr->center_mix_level   = 5; // -4.5dB
  hdr->surround_mix_level = 6; // -6.0dB

  // set default dolby surround mode
  hdr->dolby_surround_mode = AC3_DSURMOD_NOTINDICATED;

  if (hdr->bitstream_id <= 10)
  {
    // Normal AC-3
    hdr->crc1 = bs.GetBits(16);
    hdr->sr_code = bs.GetBits(2);
    if (hdr->sr_code == 3)
      return AAC_AC3_PARSE_ERROR_SAMPLE_RATE;

    frame_size_code = bs.GetBits(6);
    if (frame_size_code > 37)
      return AAC_AC3_PARSE_ERROR_FRAME_SIZE;

    //end syncinfo()
    //bsi()
    bs.SkipBits(5); // skip bsid, already got it

    hdr->bitstream_mode = bs.GetBits(3);
    hdr->channel_mode = bs.GetBits(3);

    if (hdr->channel_mode == AC3_CHMODE_STEREO)
    {
      hdr->dolby_surround_mode = bs.GetBits(2);
    }
    else
    {
      if ((hdr->channel_mode & 1) && hdr->channel_mode != AC3_CHMODE_MONO)
        hdr->center_mix_level = center_levels[bs.GetBits(2)];

      if (hdr->channel_mode & 4)
        hdr->surround_mix_level = surround_levels[bs.GetBits(2)];
    }
    hdr->lfe_on = bs.GetBits(1);
    //next unparsed field - dialnorm
    hdr->sr_shift = MAX(hdr->bitstream_id, 8) - 8;
    hdr->sample_rate = ff_ac3_sample_rate_tab[hdr->sr_code] >> hdr->sr_shift;
    hdr->bit_rate    = (ff_ac3_bitrate_tab[frame_size_code>>1] * 1000) >> hdr->sr_shift;
    hdr->channels    = ff_ac3_channels_tab[hdr->channel_mode] + hdr->lfe_on;
    hdr->frame_size  = ff_ac3_frame_size_tab[frame_size_code][hdr->sr_code] * 2;
    hdr->frame_type  = EAC3_FRAME_TYPE_AC3_CONVERT; //EAC3_FRAME_TYPE_INDEPENDENT;
    hdr->substreamid = 0;
    mimeType = "AC3";
  }
  else
  {
    // Enhanced AC-3
    hdr->crc1 = 0;
    //bsi()
    hdr->frame_type = bs.GetBits(2); //strmtyp
    if (hdr->frame_type == EAC3_FRAME_TYPE_RESERVED)
      return AAC_AC3_PARSE_ERROR_FRAME_TYPE;

    hdr->substreamid = bs.GetBits(3); //substreamid

    hdr->frame_size = (bs.GetBits(11) + 1) << 1; //frmsiz
    if (hdr->frame_size < AC3_HEADER_SIZE)
      return AAC_AC3_PARSE_ERROR_FRAME_SIZE;

    hdr->sr_code = bs.GetBits(2); //fscod
    if (hdr->sr_code == 3)
    {
      int sr_code2 = bs.GetBits(2);
      if (sr_code2 == 3)
        return AAC_AC3_PARSE_ERROR_SAMPLE_RATE;

      hdr->sample_rate = ff_ac3_sample_rate_tab[sr_code2] / 2;
      hdr->sr_shift = 1;
    }
    else
    {
      hdr->num_blocks = eac3_blocks[bs.GetBits(2)]; //numblkscod
      hdr->sample_rate = ff_ac3_sample_rate_tab[hdr->sr_code];
      hdr->sr_shift = 0;
    }

    hdr->channel_mode = bs.GetBits(3); //acmod
    hdr->lfe_on = bs.GetBits(1); //lfeon
    bs.SkipBits(5); //bsid already taken
    //next unparsed field - dialnorm
    hdr->bit_rate = 8LL * hdr->frame_size * hdr->sample_rate / (hdr->num_blocks * 256);
    hdr->channels = ff_ac3_channels_tab[hdr->channel_mode] + hdr->lfe_on;
  }
  hdr->channel_layout = avpriv_ac3_channel_layout_tab[hdr->channel_mode];
  if (hdr->lfe_on)
    hdr->channel_layout |= AV_CH_LOW_FREQUENCY;

  mimeType = "EAC3";
  //location in stream - bsi().dialnorm(5), bitpos = 45
  try {
    checkforatmos(bs, hdr);
  }
  catch (int e) {
    mimeType = "error";
  }

  return 0;
}

void CDolbyFrameParser::checkforatmos(CMemoryBitstream &bs, AC3HeaderInfo *hdr)
{
  if (hdr->bitstream_id != 16)
    return;

  // location in stream - bsi().dialnorm(5), savedbitpos = 63
  // calling routines assume bit pos in b after this fn returns!
  uint32_t savedbitpos = bs.GetBitPosition();


  bs.SetBitPosition(16); //skip syncword
  //strmtyp,substreamid,frmsiz,fscod,numblkscod,acmod,lfeon,bsid,dialnorm
  bs.SkipBits(2+3+11+2+2+3+1+5+5);
  if (bs.GetBits(1)) //compre
    bs.SkipBits(8); //{compr}
  // if 1+1 mode (dual mono, so some items need a second value)
  if (hdr->channel_mode == 0x0)
  {
    bs.SkipBits(5); //dialnorm2
    if (bs.GetBits(1)) //compr2e
      bs.SkipBits(8); //{compr2}
  }
  if (hdr->frame_type == EAC3_FRAME_TYPE_DEPENDENT)
  {
    if (bs.GetBits(1)) //chanmape
      bs.SkipBits(16); //{chanmap}
  }
  // mixing metadata
  if (bs.GetBits(1)) //mixmdate
  {
    // if more than 2 channels
    if (hdr->channel_mode > 0x2)
      bs.SkipBits(2); //{dmixmod}
    // if three front channels exist
    if ((hdr->channel_mode & 0x1) && (hdr->channel_mode > 0x2))
      bs.SkipBits(3+3); //ltrtcmixlev,lorocmixlev
    // if a surround channel exists
    if (hdr->channel_mode & 0x4)
      bs.SkipBits(3+3); //ltrtsurmixlev,lorosurmixlev
    // if the LFE channel exists
    if (hdr->lfe_on)
    {
      if (bs.GetBits(1)) //lfemixlevcode
        bs.SkipBits(5); //lfemixlevcod
    }
    if (hdr->frame_type == EAC3_FRAME_TYPE_INDEPENDENT)
    {
      if (bs.GetBits(1)) //pgmscle
        bs.SkipBits(6); //pgmscl
      if (hdr->channel_mode == 0x0) // if 1+1 mode (dual mono, so some items need a second value)
      {
        if (bs.GetBits(1)) //pgmscl2e
          bs.SkipBits(6); //pgmscl2
      }
      if (bs.GetBits(1)) //extpgmscle
        bs.SkipBits(6); //extpgmscl
      uint8_t mixdef = bs.GetBits(2);
      if (mixdef == 0x1) // mixing option 2
        bs.SkipBits(1+1+3); //premixcmpsel, drcsrc, premixcmpscl
      else if (mixdef == 0x2) // mixing option 3
        bs.SkipBits(12);
      else if (mixdef == 0x3) // mixing option 4
      {
        uint8_t mixdeflen = bs.GetBits(5); //mixdeflen
        if (bs.GetBits(1)) //mixdata2e
        {
          bs.SkipBits(1+1+3); //premixcmpsel,drcsrc,premixcmpscl
          if (bs.GetBits(1)) //extpgmlscle
            bs.SkipBits(4); //extpgmlscl
          if (bs.GetBits(1)) //extpgmcscle
            bs.SkipBits(4); //extpgmcscl
          if (bs.GetBits(1)) //extpgmrscle
            bs.SkipBits(4); //extpgmrscl
          if (bs.GetBits(1)) //extpgmlsscle
            bs.SkipBits(4); //extpgmlsscl
          if (bs.GetBits(1)) //extpgmrsscle
            bs.SkipBits(4); //extpgmrsscl
          if (bs.GetBits(1)) //extpgmlfescle
            bs.SkipBits(4); //extpgmlfescl
          if (bs.GetBits(1)) //dmixscle
            bs.SkipBits(4); //dmixscl
          if (bs.GetBits(1)) //addche
          {
            if (bs.GetBits(1)) //extpgmaux1scle
              bs.SkipBits(4); //extpgmaux1scl
            if (bs.GetBits(1)) //extpgmaux2scle
              bs.SkipBits(4); //extpgmaux2scl
          }
        }
        if (bs.GetBits(1)) //mixdata3e
        {
          bs.SkipBits(5); //spchdat
          if (bs.GetBits(1)) //addspchdate
          {
            bs.SkipBits(5+2); //spchdat1,spchan1att
            if (bs.GetBits(1)) //addspchdat1e
              bs.SkipBits(5+3); //spchdat2,spchan2att
          }
        }
        //mixdata (8*(mixdeflen+2)) - no. mixdata bits
        bs.SkipBytes(mixdeflen + 2);
        if (bs.GetBitPosition() & 0x7)
          //mixdatafill
          //used to round up the size of the mixdata field to the nearest byte
          bs.SkipBits(8 - (bs.GetBitPosition() & 0x7));
      }
      // if mono or dual mono source
      if (hdr->channel_mode < 0x2)
      {
        if (bs.GetBits(1)) //paninfoe
          bs.SkipBits(8+6); //panmean,paninfo
        // if 1+1 mode (dual mono - some items need a second value)
        if (hdr->channel_mode == 0x0)
        {
          if (bs.GetBits(1)) //paninfo2e
            bs.SkipBits(8+6); //panmean2,paninfo2
        }
      }
      // mixing configuration information
      if (bs.GetBits(1)) //frmmixcfginfoe
      {
        // if (numblkscod == 0x0)
        if (hdr->num_blocks == 1) {
          bs.SkipBits(5); //blkmixcfginfo[0]
        }
        else
        {
          for(int blk = 0; blk < hdr->num_blocks; blk++)
          {
            if (bs.GetBits(1)) //blkmixcfginfoe[blk]
              bs.SkipBits(5); //blkmixcfginfo[blk]
          }
        }
      }
    }
  }
  // informational metadata
  if (bs.GetBits(1)) //infomdate
  {
    bs.SkipBits(3+1+1); //bsmod,copyrightb,origbs
    if (hdr->channel_mode == 0x2) // if in 2/0 mode
      bs.SkipBits(2+2); //dsurmod,dheadphonmod
    if (hdr->channel_mode >= 0x6) // if both surround channels exist
      bs.SkipBits(2); //dsurexmod
    if (bs.GetBits(1)) //audprodie
      bs.SkipBits(5+2+1); //mixlevel,roomtyp,adconvtyp
    if (hdr->channel_mode == 0x0) // if 1+1 mode (dual mono, so some items need a second value)
    {
      if (bs.GetBits(1)) //audprodi2e
        bs.SkipBits(5+2+1); //mixlevel2,roomtyp2,adconvtyp2
    }
    if (hdr->sr_code < 0x3) // if not half sample rate
      bs.SkipBits(1); //sourcefscod
  }

  if (hdr->frame_type == EAC3_FRAME_TYPE_INDEPENDENT && hdr->num_blocks != 6) //(numblkscod != 0x3)
    bs.SkipBits(1); // convsync
  // if bit stream converted from AC-3
  if (hdr->frame_type == EAC3_FRAME_TYPE_AC3_CONVERT)
  {
    uint8_t blkid = 0;
    if (hdr->num_blocks == 6) // 6 blocks per syncframe
      blkid = 1;
    else
      blkid = bs.GetBits(1);
    if (blkid)
      bs.SkipBits(6); // frmsizecod
  }
  // JOC extension can be read in addbsi field. Everything prior was only necessary to get here.
  uint8_t addbsi[64] = {0};
  if (bs.GetBits(1)) // addbsie
  {
    uint8_t addbsil = bs.GetBits(6) + 1; //addbsil
    for(int i = 0; i < addbsil; i++)
      addbsi[i] = bs.GetBits(8); //addbsi
  }
  //so that analyze() does not raise exception
  bs.SetBitPosition(savedbitpos);

  // Defined in 8.3 of ETSI TS 103 420
  if (addbsi[0] & EC3Extension_JOC)
  {
    mimeType = "EAC3-ATMOS";
    hdr->ec3_extension_type = EC3Extension_JOC;
    hdr->complexity_index   = addbsi[1];
  }
}

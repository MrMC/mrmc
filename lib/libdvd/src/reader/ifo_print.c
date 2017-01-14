/*
 * This file is part of libdvdread.
 *
 * libdvdread is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdread is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdread; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "libDVD/src/reader/config/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "../logging.h"
#include "ifo_types.h"
#include "ifo_read.h"
#include "ifo_print.h"

/* Put this in some other file / package?  It's used in nav_print too. */
static void ifo_print_time(dvd_time_t *dtime) {
  const char *rate;
  assert((dtime->hour>>4) < 0xa && (dtime->hour&0xf) < 0xa);
  assert((dtime->minute>>4) < 0x7 && (dtime->minute&0xf) < 0xa);
  assert((dtime->second>>4) < 0x7 && (dtime->second&0xf) < 0xa);
  assert((dtime->frame_u&0xf) < 0xa);

  DVD_DEBUG(DBG_DVD, "%02x:%02x:%02x.%02x",
         dtime->hour,
         dtime->minute,
         dtime->second,
         dtime->frame_u & 0x3f);
  switch((dtime->frame_u & 0xc0) >> 6) {
  case 1:
    rate = "25.00";
    break;
  case 3:
    rate = "29.97";
    break;
  default:
    if(dtime->hour == 0 && dtime->minute == 0
       && dtime->second == 0 && dtime->frame_u == 0)
      rate = "no";
    else
      rate = "(please send a bug report)";
    break;
  }
  DVD_DEBUG(DBG_DVD, " @ %s fps", rate);
}

void dvdread_print_time(dvd_time_t *dtime) {
  ifo_print_time(dtime);
}

/* Put this in some other file / package?  It's used in nav_print too.
   Possibly also by the vm / navigator. */
static void ifo_print_cmd(int row, vm_cmd_t *command) {
  int i;

  DVD_DEBUG(DBG_DVD, "(%03d) ", row + 1);
  for(i=0;i<8;i++)
    DVD_DEBUG(DBG_DVD, "%02x ", command->bytes[i]);
  DVD_DEBUG(DBG_DVD, "| ");
#if 0
  //disabled call of dvdnav function
  vm_print_mnemonic(command);
#endif
  DVD_DEBUG(DBG_DVD, "\n");
}

static void ifo_print_video_attributes(video_attr_t *attr) {

  /* The following test is shorter but not correct ISO C,
     memcmp(attr,my_friendly_zeros, sizeof(video_attr_t)) */
  if(attr->mpeg_version == 0
     && attr->video_format == 0
     && attr->display_aspect_ratio == 0
     && attr->permitted_df == 0
     && attr->line21_cc_1 == 0
     && attr->line21_cc_2 == 0
     && attr->unknown1 == 0
     && attr->letterboxed == 0
     && attr->film_mode == 0) {
    DVD_DEBUG(DBG_DVD, "-- Unspecified --");
    return;
  }

  switch(attr->mpeg_version) {
  case 0:
    DVD_DEBUG(DBG_DVD, "mpeg1, ");
    break;
  case 1:
    DVD_DEBUG(DBG_DVD, "mpeg2, ");
    break;
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report), ");
  }

  switch(attr->video_format) {
  case 0:
    DVD_DEBUG(DBG_DVD, "ntsc, ");
    break;
  case 1:
    DVD_DEBUG(DBG_DVD, "pal, ");
    break;
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report), ");
  }

  switch(attr->display_aspect_ratio) {
  case 0:
    DVD_DEBUG(DBG_DVD, "4:3, ");
    break;
  case 3:
    DVD_DEBUG(DBG_DVD, "16:9, ");
    break;
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report), ");
  }

  // Wide is always allowed..!!!
  switch(attr->permitted_df) {
  case 0:
    DVD_DEBUG(DBG_DVD, "pan&scan+letterboxed, ");
    break;
  case 1:
    DVD_DEBUG(DBG_DVD, "only pan&scan, "); //??
    break;
  case 2:
    DVD_DEBUG(DBG_DVD, "only letterboxed, ");
    break;
  case 3:
    DVD_DEBUG(DBG_DVD, "not specified, ");
    break;
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report), ");
  }

  if(attr->line21_cc_1 || attr->line21_cc_2) {
    DVD_DEBUG(DBG_DVD, "NTSC CC ");
    if(attr->line21_cc_1)
      DVD_DEBUG(DBG_DVD, "1, ");
    if(attr->line21_cc_2)
      DVD_DEBUG(DBG_DVD, "2, ");
  }

  {
    int height = 480;
    if(attr->video_format != 0)
      height = 576;
    switch(attr->picture_size) {
    case 0:
      DVD_DEBUG(DBG_DVD, "720x%d, ", height);
      break;
    case 1:
      DVD_DEBUG(DBG_DVD, "704x%d, ", height);
      break;
    case 2:
      DVD_DEBUG(DBG_DVD, "352x%d, ", height);
      break;
    case 3:
      DVD_DEBUG(DBG_DVD, "352x%d, ", height/2);
      break;
    default:
      DVD_DEBUG(DBG_DVD, "(please send a bug report), ");
    }
  }

  if(attr->letterboxed) {
    DVD_DEBUG(DBG_DVD, "source letterboxed, ");
  }

  if(attr->film_mode) {
    DVD_DEBUG(DBG_DVD, "film, ");
  } else {
    DVD_DEBUG(DBG_DVD, "video, "); //camera
  }

  DVD_DEBUG(DBG_DVD, "Unknown1: %x", attr->unknown1);

}

static void ifo_print_audio_attributes(audio_attr_t *attr) {

  if(attr->audio_format == 0
     && attr->multichannel_extension == 0
     && attr->lang_type == 0
     && attr->application_mode == 0
     && attr->quantization == 0
     && attr->sample_frequency == 0
     && attr->unknown1 == 0
     && attr->channels == 0
     && attr->lang_extension == 0
     && attr->unknown3 == 0) {
    DVD_DEBUG(DBG_DVD, "-- Unspecified --");
    return;
  }

  switch(attr->audio_format) {
  case 0:
    DVD_DEBUG(DBG_DVD, "ac3 ");
    if(attr->quantization != 3)
      DVD_DEBUG(DBG_DVD, "(please send a bug report) ac3 quant/drc not 3 (%d)", attr->quantization);
    break;
  case 1:
    DVD_DEBUG(DBG_DVD, "(please send a bug report) ");
    break;
  case 2:
    DVD_DEBUG(DBG_DVD, "mpeg1 ");
  case 3:
    DVD_DEBUG(DBG_DVD, "mpeg2ext ");
    switch(attr->quantization) {
    case 0:
      DVD_DEBUG(DBG_DVD, "no drc ");
      break;
    case 1:
      DVD_DEBUG(DBG_DVD, "drc ");
      break;
    default:
      DVD_DEBUG(DBG_DVD, "(please send a bug report) mpeg reserved quant/drc  (%d)", attr->quantization);
    }
    break;
  case 4:
    DVD_DEBUG(DBG_DVD, "lpcm ");
    switch(attr->quantization) {
    case 0:
      DVD_DEBUG(DBG_DVD, "16bit ");
      break;
    case 1:
      DVD_DEBUG(DBG_DVD, "20bit ");
      break;
    case 2:
      DVD_DEBUG(DBG_DVD, "24bit ");
      break;
    case 3:
      DVD_DEBUG(DBG_DVD, "(please send a bug report) lpcm reserved quant/drc  (%d)", attr->quantization);
    break;
    }
    break;
  case 5:
    DVD_DEBUG(DBG_DVD, "(please send a bug report) ");
    break;
  case 6:
    DVD_DEBUG(DBG_DVD, "dts ");
    if(attr->quantization != 3)
      DVD_DEBUG(DBG_DVD, "(please send a bug report) dts quant/drc not 3 (%d)", attr->quantization);
    break;
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report) ");
  }

  if(attr->multichannel_extension)
    DVD_DEBUG(DBG_DVD, "multichannel_extension ");

  switch(attr->lang_type) {
  case 0:
    // not specified
    if(attr->lang_code != 0 && attr->lang_code != 0xffff)
        DVD_DEBUG(DBG_DVD, "Lang_code 0x%x, please send a bug report!", attr->lang_code);
    break;
  case 1:
    DVD_DEBUG(DBG_DVD, "%c%c ", attr->lang_code>>8, attr->lang_code & 0xff);
    break;
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report) ");
  }

  switch(attr->application_mode) {
  case 0:
    // not specified
    break;
  case 1:
    DVD_DEBUG(DBG_DVD, "karaoke mode ");
    break;
  case 2:
    DVD_DEBUG(DBG_DVD, "surround sound mode ");
    break;
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report) ");
  }

  switch(attr->quantization) {
  case 0:
    DVD_DEBUG(DBG_DVD, "16bit ");
    break;
  case 1:
    DVD_DEBUG(DBG_DVD, "20bit ");
    break;
  case 2:
    DVD_DEBUG(DBG_DVD, "24bit ");
    break;
  case 3:
    DVD_DEBUG(DBG_DVD, "drc ");
    break;
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report) ");
  }

  switch(attr->sample_frequency) {
  case 0:
    DVD_DEBUG(DBG_DVD, "48kHz ");
    break;
  case 1:
    DVD_DEBUG(DBG_DVD, "??kHz ");
    break;
  default:
    DVD_DEBUG(DBG_DVD, "sample_frequency %i (please send a bug report) ",
           attr->sample_frequency);
  }

  DVD_DEBUG(DBG_DVD, "%dCh ", attr->channels + 1);

  switch(attr->lang_extension) {
  case 0:
    DVD_DEBUG(DBG_DVD, "Not specified ");
    break;
  case 1: // Normal audio
    DVD_DEBUG(DBG_DVD, "Normal Caption ");
    break;
  case 2: // visually impaired
    DVD_DEBUG(DBG_DVD, "Audio for visually impaired ");
    break;
  case 3: // Directors 1
    DVD_DEBUG(DBG_DVD, "Director's comments 1 ");
    break;
  case 4: // Directors 2
    DVD_DEBUG(DBG_DVD, "Director's comments 2 ");
    break;
    //case 4: // Music score ?
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report) ");
  }

  DVD_DEBUG(DBG_DVD, "Unknown1: %d ", attr->unknown1);
  DVD_DEBUG(DBG_DVD, "Unknown3: %d ", attr->unknown3);
}

static void ifo_print_subp_attributes(subp_attr_t *attr) {

  if(attr->type == 0
     && attr->zero1 == 0
     && attr->zero2 == 0
     && attr->lang_code == 0
     && attr->lang_extension== 0) {
    DVD_DEBUG(DBG_DVD, "-- Unspecified --");
    return;
  }

  DVD_DEBUG(DBG_DVD, "type %02x ", attr->type);

  if(isalpha((int)(attr->lang_code >> 8))
     && isalpha((int)(attr->lang_code & 0xff))) {
    DVD_DEBUG(DBG_DVD, "%c%c ", attr->lang_code >> 8, attr->lang_code & 0xff);
  } else {
    DVD_DEBUG(DBG_DVD, "%02x%02x ", 0xff & (unsigned)(attr->lang_code >> 8),
           0xff & (unsigned)(attr->lang_code & 0xff));
  }

  DVD_DEBUG(DBG_DVD, "%d ", attr->zero1);
  DVD_DEBUG(DBG_DVD, "%d ", attr->zero2);

  switch(attr->lang_extension) {
  case 0:
    DVD_DEBUG(DBG_DVD, "Not specified ");
    break;
  case 1:
    DVD_DEBUG(DBG_DVD, "Caption with normal size character ");
    break;
  case 2:
    DVD_DEBUG(DBG_DVD, "Caption with bigger size character ");
    break;
  case 3:
    DVD_DEBUG(DBG_DVD, "Caption for children ");
    break;
  case 4:
    DVD_DEBUG(DBG_DVD, "reserved ");
    break;
  case 5:
    DVD_DEBUG(DBG_DVD, "Closed Caption with normal size character ");
    break;
  case 6:
    DVD_DEBUG(DBG_DVD, "Closed Caption with bigger size character ");
    break;
  case 7:
    DVD_DEBUG(DBG_DVD, "Closed Caption for children ");
    break;
  case 8:
    DVD_DEBUG(DBG_DVD, "reserved ");
    break;
  case 9:
    DVD_DEBUG(DBG_DVD, "Forced Caption");
    break;
  case 10:
    DVD_DEBUG(DBG_DVD, "reserved ");
    break;
  case 11:
    DVD_DEBUG(DBG_DVD, "reserved ");
    break;
  case 12:
    DVD_DEBUG(DBG_DVD, "reserved ");
    break;
  case 13:
    DVD_DEBUG(DBG_DVD, "Director's comments with normal size character ");
    break;
  case 14:
    DVD_DEBUG(DBG_DVD, "Director's comments with bigger size character ");
    break;
  case 15:
    DVD_DEBUG(DBG_DVD, "Director's comments for children ");
    break;
  default:
    DVD_DEBUG(DBG_DVD, "(please send a bug report) ");
  }

}


static void ifoPrint_USER_OPS(user_ops_t *user_ops) {
  uint32_t uops;
  unsigned char *ptr = (unsigned char *)user_ops;

  uops  = (*ptr++ << 24);
  uops |= (*ptr++ << 16);
  uops |= (*ptr++ << 8);
  uops |= (*ptr++);

  if(uops == 0) {
    DVD_DEBUG(DBG_DVD, "None\n");
  } else if(uops == 0x01ffffff) {
    DVD_DEBUG(DBG_DVD, "All\n");
  } else {
    if(user_ops->title_or_time_play)
      DVD_DEBUG(DBG_DVD, "Title or Time Play, ");
    if(user_ops->chapter_search_or_play)
      DVD_DEBUG(DBG_DVD, "Chapter Search or Play, ");
    if(user_ops->title_play)
      DVD_DEBUG(DBG_DVD, "Title Play, ");
    if(user_ops->stop)
      DVD_DEBUG(DBG_DVD, "Stop, ");
    if(user_ops->go_up)
      DVD_DEBUG(DBG_DVD, "Go Up, ");
    if(user_ops->time_or_chapter_search)
      DVD_DEBUG(DBG_DVD, "Time or Chapter Search, ");
    if(user_ops->prev_or_top_pg_search)
      DVD_DEBUG(DBG_DVD, "Prev or Top PG Search, ");
    if(user_ops->next_pg_search)
      DVD_DEBUG(DBG_DVD, "Next PG Search, ");
    if(user_ops->forward_scan)
      DVD_DEBUG(DBG_DVD, "Forward Scan, ");
    if(user_ops->backward_scan)
      DVD_DEBUG(DBG_DVD, "Backward Scan, ");
    if(user_ops->title_menu_call)
      DVD_DEBUG(DBG_DVD, "Title Menu Call, ");
    if(user_ops->root_menu_call)
      DVD_DEBUG(DBG_DVD, "Root Menu Call, ");
    if(user_ops->subpic_menu_call)
      DVD_DEBUG(DBG_DVD, "SubPic Menu Call, ");
    if(user_ops->audio_menu_call)
      DVD_DEBUG(DBG_DVD, "Audio Menu Call, ");
    if(user_ops->angle_menu_call)
      DVD_DEBUG(DBG_DVD, "Angle Menu Call, ");
    if(user_ops->chapter_menu_call)
      DVD_DEBUG(DBG_DVD, "Chapter Menu Call, ");
    if(user_ops->resume)
      DVD_DEBUG(DBG_DVD, "Resume, ");
    if(user_ops->button_select_or_activate)
      DVD_DEBUG(DBG_DVD, "Button Select or Activate, ");
    if(user_ops->still_off)
      DVD_DEBUG(DBG_DVD, "Still Off, ");
    if(user_ops->pause_on)
      DVD_DEBUG(DBG_DVD, "Pause On, ");
    if(user_ops->audio_stream_change)
      DVD_DEBUG(DBG_DVD, "Audio Stream Change, ");
    if(user_ops->subpic_stream_change)
      DVD_DEBUG(DBG_DVD, "SubPic Stream Change, ");
    if(user_ops->angle_change)
      DVD_DEBUG(DBG_DVD, "Angle Change, ");
    if(user_ops->karaoke_audio_pres_mode_change)
      DVD_DEBUG(DBG_DVD, "Karaoke Audio Pres Mode Change, ");
    if(user_ops->video_pres_mode_change)
      DVD_DEBUG(DBG_DVD, "Video Pres Mode Change, ");
    DVD_DEBUG(DBG_DVD, "\n");
  }
}


static void ifoPrint_VMGI_MAT(vmgi_mat_t *vmgi_mat) {

  DVD_DEBUG(DBG_DVD, "VMG Identifier: %.12s\n", vmgi_mat->vmg_identifier);
  DVD_DEBUG(DBG_DVD, "Last Sector of VMG: %08x\n", vmgi_mat->vmg_last_sector);
  DVD_DEBUG(DBG_DVD, "Last Sector of VMGI: %08x\n", vmgi_mat->vmgi_last_sector);
  DVD_DEBUG(DBG_DVD, "Specification version number: %01x.%01x\n",
         vmgi_mat->specification_version >> 4,
         vmgi_mat->specification_version & 0xf);
  /* Byte 2 of 'VMG Category' (00xx0000) is the Region Code */
  DVD_DEBUG(DBG_DVD, "VMG Category: %08x (Region Code=%02x)\n", vmgi_mat->vmg_category, ((vmgi_mat->vmg_category >> 16) & 0xff) ^0xff);
  DVD_DEBUG(DBG_DVD, "VMG Number of Volumes: %i\n", vmgi_mat->vmg_nr_of_volumes);
  DVD_DEBUG(DBG_DVD, "VMG This Volume: %i\n", vmgi_mat->vmg_this_volume_nr);
  DVD_DEBUG(DBG_DVD, "Disc side %i\n", vmgi_mat->disc_side);
  DVD_DEBUG(DBG_DVD, "VMG Number of Title Sets %i\n", vmgi_mat->vmg_nr_of_title_sets);
  DVD_DEBUG(DBG_DVD, "Provider ID: %.32s\n", vmgi_mat->provider_identifier);
  DVD_DEBUG(DBG_DVD, "VMG POS Code: %08x", (uint32_t)(vmgi_mat->vmg_pos_code >> 32));
  DVD_DEBUG(DBG_DVD, "%08x\n", (uint32_t)vmgi_mat->vmg_pos_code);
  DVD_DEBUG(DBG_DVD, "End byte of VMGI_MAT: %08x\n", vmgi_mat->vmgi_last_byte);
  DVD_DEBUG(DBG_DVD, "Start byte of First Play PGC (FP PGC): %08x\n",
         vmgi_mat->first_play_pgc);
  DVD_DEBUG(DBG_DVD, "Start sector of VMGM_VOBS: %08x\n", vmgi_mat->vmgm_vobs);
  DVD_DEBUG(DBG_DVD, "Start sector of TT_SRPT: %08x\n", vmgi_mat->tt_srpt);
  DVD_DEBUG(DBG_DVD, "Start sector of VMGM_PGCI_UT: %08x\n", vmgi_mat->vmgm_pgci_ut);
  DVD_DEBUG(DBG_DVD, "Start sector of PTL_MAIT: %08x\n", vmgi_mat->ptl_mait);
  DVD_DEBUG(DBG_DVD, "Start sector of VTS_ATRT: %08x\n", vmgi_mat->vts_atrt);
  DVD_DEBUG(DBG_DVD, "Start sector of TXTDT_MG: %08x\n", vmgi_mat->txtdt_mgi);
  DVD_DEBUG(DBG_DVD, "Start sector of VMGM_C_ADT: %08x\n", vmgi_mat->vmgm_c_adt);
  DVD_DEBUG(DBG_DVD, "Start sector of VMGM_VOBU_ADMAP: %08x\n",
         vmgi_mat->vmgm_vobu_admap);
  DVD_DEBUG(DBG_DVD, "Video attributes of VMGM_VOBS: ");
  ifo_print_video_attributes(&vmgi_mat->vmgm_video_attr);
  DVD_DEBUG(DBG_DVD, "\n");
  DVD_DEBUG(DBG_DVD, "VMGM Number of Audio attributes: %i\n",
         vmgi_mat->nr_of_vmgm_audio_streams);
  if(vmgi_mat->nr_of_vmgm_audio_streams > 0) {
    DVD_DEBUG(DBG_DVD, "\tstream %i status: ", 1);
    ifo_print_audio_attributes(&vmgi_mat->vmgm_audio_attr);
    DVD_DEBUG(DBG_DVD, "\n");
  }
  DVD_DEBUG(DBG_DVD, "VMGM Number of Sub-picture attributes: %i\n",
         vmgi_mat->nr_of_vmgm_subp_streams);
  if(vmgi_mat->nr_of_vmgm_subp_streams > 0) {
    DVD_DEBUG(DBG_DVD, "\tstream %2i status: ", 1);
    ifo_print_subp_attributes(&vmgi_mat->vmgm_subp_attr);
    DVD_DEBUG(DBG_DVD, "\n");
  }
}


static void ifoPrint_VTSI_MAT(vtsi_mat_t *vtsi_mat) {
  int i;

  DVD_DEBUG(DBG_DVD, "VTS Identifier: %.12s\n", vtsi_mat->vts_identifier);
  DVD_DEBUG(DBG_DVD, "Last Sector of VTS: %08x\n", vtsi_mat->vts_last_sector);
  DVD_DEBUG(DBG_DVD, "Last Sector of VTSI: %08x\n", vtsi_mat->vtsi_last_sector);
  DVD_DEBUG(DBG_DVD, "Specification version number: %01x.%01x\n",
         vtsi_mat->specification_version>>4,
         vtsi_mat->specification_version&0xf);
  DVD_DEBUG(DBG_DVD, "VTS Category: %08x\n", vtsi_mat->vts_category);
  DVD_DEBUG(DBG_DVD, "End byte of VTSI_MAT: %08x\n", vtsi_mat->vtsi_last_byte);
  DVD_DEBUG(DBG_DVD, "Start sector of VTSM_VOBS:  %08x\n", vtsi_mat->vtsm_vobs);
  DVD_DEBUG(DBG_DVD, "Start sector of VTSTT_VOBS: %08x\n", vtsi_mat->vtstt_vobs);
  DVD_DEBUG(DBG_DVD, "Start sector of VTS_PTT_SRPT: %08x\n", vtsi_mat->vts_ptt_srpt);
  DVD_DEBUG(DBG_DVD, "Start sector of VTS_PGCIT:    %08x\n", vtsi_mat->vts_pgcit);
  DVD_DEBUG(DBG_DVD, "Start sector of VTSM_PGCI_UT: %08x\n", vtsi_mat->vtsm_pgci_ut);
  DVD_DEBUG(DBG_DVD, "Start sector of VTS_TMAPT:    %08x\n", vtsi_mat->vts_tmapt);
  DVD_DEBUG(DBG_DVD, "Start sector of VTSM_C_ADT:      %08x\n", vtsi_mat->vtsm_c_adt);
  DVD_DEBUG(DBG_DVD, "Start sector of VTSM_VOBU_ADMAP: %08x\n",vtsi_mat->vtsm_vobu_admap);
  DVD_DEBUG(DBG_DVD, "Start sector of VTS_C_ADT:       %08x\n", vtsi_mat->vts_c_adt);
  DVD_DEBUG(DBG_DVD, "Start sector of VTS_VOBU_ADMAP:  %08x\n", vtsi_mat->vts_vobu_admap);

  DVD_DEBUG(DBG_DVD, "Video attributes of VTSM_VOBS: ");
  ifo_print_video_attributes(&vtsi_mat->vtsm_video_attr);
  DVD_DEBUG(DBG_DVD, "\n");

  DVD_DEBUG(DBG_DVD, "VTSM Number of Audio attributes: %i\n",
         vtsi_mat->nr_of_vtsm_audio_streams);
  if(vtsi_mat->nr_of_vtsm_audio_streams > 0) {
    DVD_DEBUG(DBG_DVD, "\tstream %i status: ", 1);
    ifo_print_audio_attributes(&vtsi_mat->vtsm_audio_attr);
    DVD_DEBUG(DBG_DVD, "\n");
  }

  DVD_DEBUG(DBG_DVD, "VTSM Number of Sub-picture attributes: %i\n",
         vtsi_mat->nr_of_vtsm_subp_streams);
  if(vtsi_mat->nr_of_vtsm_subp_streams > 0) {
    DVD_DEBUG(DBG_DVD, "\tstream %2i status: ", 1);
    ifo_print_subp_attributes(&vtsi_mat->vtsm_subp_attr);
    DVD_DEBUG(DBG_DVD, "\n");
  }

  DVD_DEBUG(DBG_DVD, "Video attributes of VTS_VOBS: ");
  ifo_print_video_attributes(&vtsi_mat->vts_video_attr);
  DVD_DEBUG(DBG_DVD, "\n");

  DVD_DEBUG(DBG_DVD, "VTS Number of Audio attributes: %i\n",
         vtsi_mat->nr_of_vts_audio_streams);
  for(i = 0; i < vtsi_mat->nr_of_vts_audio_streams; i++) {
    DVD_DEBUG(DBG_DVD, "\tstream %i status: ", i);
    ifo_print_audio_attributes(&vtsi_mat->vts_audio_attr[i]);
    DVD_DEBUG(DBG_DVD, "\n");
  }

  DVD_DEBUG(DBG_DVD, "VTS Number of Subpicture attributes: %i\n",
         vtsi_mat->nr_of_vts_subp_streams);
  for(i = 0; i < vtsi_mat->nr_of_vts_subp_streams; i++) {
    DVD_DEBUG(DBG_DVD, "\tstream %2i status: ", i);
    ifo_print_subp_attributes(&vtsi_mat->vts_subp_attr[i]);
    DVD_DEBUG(DBG_DVD, "\n");
  }
}


static void ifoPrint_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl) {
  int i;

  if(cmd_tbl == NULL) {
    DVD_DEBUG(DBG_DVD, "No Command table present\n");
    return;
  }

  DVD_DEBUG(DBG_DVD, "Number of Pre commands: %i\n", cmd_tbl->nr_of_pre);
  for(i = 0; i < cmd_tbl->nr_of_pre; i++) {
    ifo_print_cmd(i, &cmd_tbl->pre_cmds[i]);
  }

  DVD_DEBUG(DBG_DVD, "Number of Post commands: %i\n", cmd_tbl->nr_of_post);
  for(i = 0; i < cmd_tbl->nr_of_post; i++) {
    ifo_print_cmd(i, &cmd_tbl->post_cmds[i]);
  }

  DVD_DEBUG(DBG_DVD, "Number of Cell commands: %i\n", cmd_tbl->nr_of_cell);
  for(i = 0; i < cmd_tbl->nr_of_cell; i++) {
    ifo_print_cmd(i, &cmd_tbl->cell_cmds[i]);
  }
}


static void ifoPrint_PGC_PROGRAM_MAP(pgc_program_map_t *program_map, int nr) {
  int i;

  if(program_map == NULL) {
    DVD_DEBUG(DBG_DVD, "No Program map present\n");
    return;
  }

  for(i = 0; i < nr; i++) {
    DVD_DEBUG(DBG_DVD, "Program %3i Entry Cell: %3i\n", i + 1, program_map[i]);
  }
}


static void ifoPrint_CELL_PLAYBACK(cell_playback_t *cell_playback, int nr) {
  int i;

  if(cell_playback == NULL) {
    DVD_DEBUG(DBG_DVD, "No Cell Playback info present\n");
    return;
  }

  for(i=0;i<nr;i++) {
    DVD_DEBUG(DBG_DVD, "Cell: %3i ", i + 1);

    dvdread_print_time(&cell_playback[i].playback_time);
    DVD_DEBUG(DBG_DVD, "\t");

    if(cell_playback[i].block_mode || cell_playback[i].block_type) {
      const char *s;
      switch(cell_playback[i].block_mode) {
      case 0:
        s = "not a"; break;
      case 1:
        s = "the first"; break;
      case 2:
      default:
        s = ""; break;
      case 3:
        s = "last"; break;
      }
      DVD_DEBUG(DBG_DVD, "%s cell in the block ", s);

      switch(cell_playback[i].block_type) {
      case 0:
        DVD_DEBUG(DBG_DVD, "not part of the block ");
        break;
      case 1:
        DVD_DEBUG(DBG_DVD, "angle block ");
        break;
      case 2:
      case 3:
        DVD_DEBUG(DBG_DVD, "(send bug report) ");
        break;
      }
    }
    if(cell_playback[i].seamless_play)
      DVD_DEBUG(DBG_DVD, "presented seamlessly ");
    if(cell_playback[i].interleaved)
      DVD_DEBUG(DBG_DVD, "cell is interleaved ");
    if(cell_playback[i].stc_discontinuity)
      DVD_DEBUG(DBG_DVD, "STC_discontinuty ");
    if(cell_playback[i].seamless_angle)
      DVD_DEBUG(DBG_DVD, "only seamless angle ");
    if(cell_playback[i].playback_mode)
      DVD_DEBUG(DBG_DVD, "only still VOBUs ");
    if(cell_playback[i].restricted)
      DVD_DEBUG(DBG_DVD, "restricted cell ");
    if(cell_playback[i].unknown2)
      DVD_DEBUG(DBG_DVD, "Unknown 0x%x ", cell_playback[i].unknown2);
    if(cell_playback[i].still_time)
      DVD_DEBUG(DBG_DVD, "still time %d ", cell_playback[i].still_time);
    if(cell_playback[i].cell_cmd_nr)
      DVD_DEBUG(DBG_DVD, "cell command %d", cell_playback[i].cell_cmd_nr);

    DVD_DEBUG(DBG_DVD, "\n\tStart sector: %08x\tFirst ILVU end  sector: %08x\n",
           cell_playback[i].first_sector,
           cell_playback[i].first_ilvu_end_sector);
    DVD_DEBUG(DBG_DVD, "\tEnd   sector: %08x\tLast VOBU start sector: %08x\n",
           cell_playback[i].last_sector,
           cell_playback[i].last_vobu_start_sector);
  }
}

static void ifoPrint_CELL_POSITION(cell_position_t *cell_position, int nr) {
  int i;

  if(cell_position == NULL) {
    DVD_DEBUG(DBG_DVD, "No Cell Position info present\n");
    return;
  }

  for(i=0;i<nr;i++) {
    DVD_DEBUG(DBG_DVD, "Cell: %3i has VOB ID: %3i, Cell ID: %3i\n", i + 1,
           cell_position[i].vob_id_nr, cell_position[i].cell_nr);
  }
}


static void ifoPrint_PGC(pgc_t *pgc) {
  int i;

  if (!pgc) {
    DVD_DEBUG(DBG_DVD, "None\n");
    return;
  }
  DVD_DEBUG(DBG_DVD, "Number of Programs: %i\n", pgc->nr_of_programs);
  DVD_DEBUG(DBG_DVD, "Number of Cells: %i\n", pgc->nr_of_cells);
  /* Check that time is 0:0:0:0 also if nr_of_programs==0 */
  DVD_DEBUG(DBG_DVD, "Playback time: ");
  dvdread_print_time(&pgc->playback_time); DVD_DEBUG(DBG_DVD, "\n");

  /* If no programs/no time then does this mean anything? */
  DVD_DEBUG(DBG_DVD, "Prohibited user operations: ");
  ifoPrint_USER_OPS(&pgc->prohibited_ops);

  for(i = 0; i < 8; i++) {
    if(pgc->audio_control[i] & 0x8000) { /* The 'is present' bit */
      DVD_DEBUG(DBG_DVD, "Audio stream %i control: %04x\n",
             i, pgc->audio_control[i]);
    }
  }

  for(i = 0; i < 32; i++) {
    if(pgc->subp_control[i] & 0x80000000) { /* The 'is present' bit */
      DVD_DEBUG(DBG_DVD, "Subpicture stream %2i control: %08x: 4:3=%d, Wide=%d, Letterbox=%d, Pan-Scan=%d\n",
             i, pgc->subp_control[i],
             (pgc->subp_control[i] >>24) & 0x1f,
             (pgc->subp_control[i] >>16) & 0x1f,
             (pgc->subp_control[i] >>8) & 0x1f,
             (pgc->subp_control[i] ) & 0x1f);
    }
  }

  DVD_DEBUG(DBG_DVD, "Next PGC number: %i\n", pgc->next_pgc_nr);
  DVD_DEBUG(DBG_DVD, "Prev PGC number: %i\n", pgc->prev_pgc_nr);
  DVD_DEBUG(DBG_DVD, "GoUp PGC number: %i\n", pgc->goup_pgc_nr);
  if(pgc->nr_of_programs != 0) {
    DVD_DEBUG(DBG_DVD, "Still time: %i seconds (255=inf)\n", pgc->still_time);
    DVD_DEBUG(DBG_DVD, "PG Playback mode %02x\n", pgc->pg_playback_mode);
  }

  if(pgc->nr_of_programs != 0) {
    for(i = 0; i < 16; i++) {
      DVD_DEBUG(DBG_DVD, "Color %2i: %08x\n", i, pgc->palette[i]);
    }
  }

  /* Memory offsets to div. tables. */
  ifoPrint_PGC_COMMAND_TBL(pgc->command_tbl);
  ifoPrint_PGC_PROGRAM_MAP(pgc->program_map, pgc->nr_of_programs);
  ifoPrint_CELL_PLAYBACK(pgc->cell_playback, pgc->nr_of_cells);
  ifoPrint_CELL_POSITION(pgc->cell_position, pgc->nr_of_cells);
}


static void ifoPrint_TT_SRPT(tt_srpt_t *tt_srpt) {
  int i;

  DVD_DEBUG(DBG_DVD, "Number of TitleTrack search pointers: %i\n",
         tt_srpt->nr_of_srpts);
  for(i=0;i<tt_srpt->nr_of_srpts;i++) {
    DVD_DEBUG(DBG_DVD, "Title Track index %i\n", i + 1);
    DVD_DEBUG(DBG_DVD, "\tTitle set number (VTS): %i",
           tt_srpt->title[i].title_set_nr);
    DVD_DEBUG(DBG_DVD, "\tVTS_TTN: %i\n", tt_srpt->title[i].vts_ttn);
    DVD_DEBUG(DBG_DVD, "\tNumber of PTTs: %i\n", tt_srpt->title[i].nr_of_ptts);
    DVD_DEBUG(DBG_DVD, "\tNumber of angles: %i\n",
           tt_srpt->title[i].nr_of_angles);

    DVD_DEBUG(DBG_DVD, "\tTitle playback type: (%02x)\n",
           *(uint8_t *)&(tt_srpt->title[i].pb_ty));
    DVD_DEBUG(DBG_DVD, "\t\t%s\n",
           tt_srpt->title[i].pb_ty.multi_or_random_pgc_title ? "Random or Shuffle" : "Sequential");
    if (tt_srpt->title[i].pb_ty.jlc_exists_in_cell_cmd) DVD_DEBUG(DBG_DVD, "\t\tJump/Link/Call exists in cell cmd\n");
    if (tt_srpt->title[i].pb_ty.jlc_exists_in_prepost_cmd) DVD_DEBUG(DBG_DVD, "\t\tJump/Link/Call exists in pre/post cmd\n");
    if (tt_srpt->title[i].pb_ty.jlc_exists_in_button_cmd) DVD_DEBUG(DBG_DVD, "\t\tJump/Link/Call exists in button cmd\n");
    if (tt_srpt->title[i].pb_ty.jlc_exists_in_tt_dom) DVD_DEBUG(DBG_DVD, "\t\tJump/Link/Call exists in tt_dom cmd\n");
    DVD_DEBUG(DBG_DVD, "\t\tTitle or time play:%u\n", tt_srpt->title[i].pb_ty.title_or_time_play);
    DVD_DEBUG(DBG_DVD, "\t\tChapter search or play:%u\n", tt_srpt->title[i].pb_ty.chapter_search_or_play);

    DVD_DEBUG(DBG_DVD, "\tParental ID field: %04x\n",
           tt_srpt->title[i].parental_id);
    DVD_DEBUG(DBG_DVD, "\tTitle set starting sector %08x\n",
           tt_srpt->title[i].title_set_sector);
  }
}


static void ifoPrint_VTS_PTT_SRPT(vts_ptt_srpt_t *vts_ptt_srpt) {
  int i, j;
  DVD_DEBUG(DBG_DVD, " nr_of_srpts %i last byte %i\n",
         vts_ptt_srpt->nr_of_srpts,
         vts_ptt_srpt->last_byte);
  for(i=0;i<vts_ptt_srpt->nr_of_srpts;i++) {
    for(j=0;j<vts_ptt_srpt->title[i].nr_of_ptts;j++) {
      DVD_DEBUG(DBG_DVD, "VTS_PTT_SRPT - Title %3i part %3i: PGC: %3i PG: %3i\n",
             i + 1, j + 1,
             vts_ptt_srpt->title[i].ptt[j].pgcn,
             vts_ptt_srpt->title[i].ptt[j].pgn );
    }
  }
}


static void hexdump(uint8_t *ptr, int len) {
  while(len--)
    DVD_DEBUG(DBG_DVD, "%02x ", *ptr++);
}

static void ifoPrint_PTL_MAIT(ptl_mait_t *ptl_mait) {
  int i, j;

  DVD_DEBUG(DBG_DVD, "Number of Countries: %i\n", ptl_mait->nr_of_countries);
  DVD_DEBUG(DBG_DVD, "Number of VTSs: %i\n", ptl_mait->nr_of_vtss);
  //DVD_DEBUG(DBG_DVD, "Last byte: %i\n", ptl_mait->last_byte);

  for(i = 0; i < ptl_mait->nr_of_countries; i++) {
    DVD_DEBUG(DBG_DVD, "Country code: %c%c\n",
           ptl_mait->countries[i].country_code >> 8,
           ptl_mait->countries[i].country_code & 0xff);
    /*
      DVD_DEBUG(DBG_DVD, "Start byte: %04x %i\n",
      ptl_mait->countries[i].pf_ptl_mai_start_byte,
      ptl_mait->countries[i].pf_ptl_mai_start_byte);
    */
    /* This seems to be pointing at a array with 8 2byte fields per VTS
       ? and one extra for the menu? always an odd number of VTSs on
       all the dics I tested so it might be padding to even also.
       If it is for the menu it probably the first entry.  */
    for(j=0;j<8;j++) {
      hexdump( (uint8_t *)ptl_mait->countries - PTL_MAIT_COUNTRY_SIZE
               + ptl_mait->countries[i].pf_ptl_mai_start_byte
               + j*(ptl_mait->nr_of_vtss+1)*2, (ptl_mait->nr_of_vtss+1)*2);
      DVD_DEBUG(DBG_DVD, "\n");
    }
  }
}

static void ifoPrint_VTS_TMAPT(vts_tmapt_t *vts_tmapt) {
  unsigned int timeunit;
  int i, j;

  DVD_DEBUG(DBG_DVD, "Number of VTS_TMAPS: %i\n", vts_tmapt->nr_of_tmaps);
  DVD_DEBUG(DBG_DVD, "Last byte: %i\n", vts_tmapt->last_byte);

  for(i = 0; i < vts_tmapt->nr_of_tmaps; i++) {
    DVD_DEBUG(DBG_DVD, "TMAP %i (number matches title PGC number.)\n", i + 1);
    DVD_DEBUG(DBG_DVD, "  offset %d relative to VTS_TMAPTI\n", vts_tmapt->tmap_offset[i]);
    DVD_DEBUG(DBG_DVD, "  Time unit (seconds): %i\n", vts_tmapt->tmap[i].tmu);
    DVD_DEBUG(DBG_DVD, "  Number of entries: %i\n", vts_tmapt->tmap[i].nr_of_entries);
    timeunit = vts_tmapt->tmap[i].tmu;
    for(j = 0; j < vts_tmapt->tmap[i].nr_of_entries; j++) {
      unsigned int ac_time = timeunit * (j + 1);
      DVD_DEBUG(DBG_DVD, "Time: %2i:%02i:%02i  VOBU Sector: 0x%08x %s\n",
             ac_time / (60 * 60), (ac_time / 60) % 60, ac_time % 60,
             vts_tmapt->tmap[i].map_ent[j] & 0x7fffffff,
             (vts_tmapt->tmap[i].map_ent[j] >> 31) ? "discontinuity" : "");
    }
  }
}

static void ifoPrint_C_ADT(c_adt_t *c_adt) {
  int i, entries;

  DVD_DEBUG(DBG_DVD, "Number of VOBs in this VOBS: %i\n", c_adt->nr_of_vobs);
  //entries = c_adt->nr_of_vobs;
  entries = (c_adt->last_byte + 1 - C_ADT_SIZE)/sizeof(c_adt_t);

  for(i = 0; i < entries; i++) {
    DVD_DEBUG(DBG_DVD, "VOB ID: %3i, Cell ID: %3i   ",
           c_adt->cell_adr_table[i].vob_id, c_adt->cell_adr_table[i].cell_id);
    DVD_DEBUG(DBG_DVD, "Sector (first): 0x%08x   (last): 0x%08x\n",
           c_adt->cell_adr_table[i].start_sector,
           c_adt->cell_adr_table[i].last_sector);
  }
}


static void ifoPrint_VOBU_ADMAP(vobu_admap_t *vobu_admap) {
  int i, entries;

  entries = (vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE)/4;
  for(i = 0; i < entries; i++) {
    DVD_DEBUG(DBG_DVD, "VOBU %5i  First sector: 0x%08x\n", i + 1,
           vobu_admap->vobu_start_sectors[i]);
  }
}

static const char *ifo_print_menu_name(int type) {
  const char *menu_name;
  menu_name="";
  switch (type) {
  case 2:
    menu_name="Title";
    break;
  case 3:
    menu_name = "Root";
    break;
  case 4:
    menu_name = "Sub-Picture";
    break;
  case 5:
    menu_name = "Audio";
    break;
  case 6:
    menu_name = "Angle";
    break;
  case 7:
    menu_name = "PTT (Chapter)";
    break;
  default:
    menu_name = "Unknown";
    break;
  }
  return &menu_name[0];
}

/* pgc_type=1 for menu, 0 for title. */
static void ifoPrint_PGCIT(pgcit_t *pgcit, int pgc_type) {
  int i;

  DVD_DEBUG(DBG_DVD, "\nNumber of Program Chains: %3i\n", pgcit->nr_of_pgci_srp);
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    DVD_DEBUG(DBG_DVD, "\nProgram (PGC): %3i\n", i + 1);
    if (pgc_type) {
       DVD_DEBUG(DBG_DVD, "PGC Category: Entry PGC %d, Menu Type=0x%02x:%s (Entry id 0x%02x), ",
              pgcit->pgci_srp[i].entry_id >> 7,
              pgcit->pgci_srp[i].entry_id & 0xf,
              ifo_print_menu_name(pgcit->pgci_srp[i].entry_id & 0xf),
              pgcit->pgci_srp[i].entry_id);
    } else {
       DVD_DEBUG(DBG_DVD, "PGC Category: %s VTS_TTN:0x%02x (Entry id 0x%02x), ",
              pgcit->pgci_srp[i].entry_id >> 7 ? "At Start of" : "During",
              pgcit->pgci_srp[i].entry_id & 0xf,
              pgcit->pgci_srp[i].entry_id);
    }
    DVD_DEBUG(DBG_DVD, "Parental ID mask 0x%04x\n", pgcit->pgci_srp[i].ptl_id_mask);
    ifoPrint_PGC(pgcit->pgci_srp[i].pgc);
  }
}


static void ifoPrint_PGCI_UT(pgci_ut_t *pgci_ut) {
  int i, menu;

  DVD_DEBUG(DBG_DVD, "Number of Menu Language Units (PGCI_LU): %3i\n", pgci_ut->nr_of_lus);
  for(i = 0; i < pgci_ut->nr_of_lus; i++) {
    DVD_DEBUG(DBG_DVD, "\nMenu Language Unit %d\n", i+1);
    DVD_DEBUG(DBG_DVD, "\nMenu Language Code: %c%c\n",
           pgci_ut->lu[i].lang_code >> 8,
           pgci_ut->lu[i].lang_code & 0xff);

    menu = pgci_ut->lu[i].exists;
    DVD_DEBUG(DBG_DVD, "Menu Existence: %02x: ", menu);
    if (menu == 0) {
      DVD_DEBUG(DBG_DVD, "No menus ");
    }
    if (menu & 0x80) {
      DVD_DEBUG(DBG_DVD, "Root ");
      menu^=0x80;
    }
    if (menu & 0x40) {
      DVD_DEBUG(DBG_DVD, "Sub-Picture ");
      menu^=0x40;
    }
    if (menu & 0x20) {
      DVD_DEBUG(DBG_DVD, "Audio ");
      menu^=0x20;
    }
    if (menu & 0x10) {
      DVD_DEBUG(DBG_DVD, "Angle ");
      menu^=0x10;
    }
    if (menu & 0x08) {
      DVD_DEBUG(DBG_DVD, "PTT ");
      menu^=0x08;
    }
    if (menu > 0) {
      DVD_DEBUG(DBG_DVD, "Unknown extra menus ");
      menu^=0x08;
    }
    DVD_DEBUG(DBG_DVD, "\n");
    ifoPrint_PGCIT(pgci_ut->lu[i].pgcit, 1);
  }
}


static void ifoPrint_VTS_ATTRIBUTES(vts_attributes_t *vts_attributes) {
  int i;

  DVD_DEBUG(DBG_DVD, "VTS_CAT Application type: %08x\n", vts_attributes->vts_cat);

  DVD_DEBUG(DBG_DVD, "Video attributes of VTSM_VOBS: ");
  ifo_print_video_attributes(&vts_attributes->vtsm_vobs_attr);
  DVD_DEBUG(DBG_DVD, "\n");
  DVD_DEBUG(DBG_DVD, "Number of Audio streams: %i\n",
         vts_attributes->nr_of_vtsm_audio_streams);
  if(vts_attributes->nr_of_vtsm_audio_streams > 0) {
    DVD_DEBUG(DBG_DVD, "\tstream %i attributes: ", 1);
    ifo_print_audio_attributes(&vts_attributes->vtsm_audio_attr);
    DVD_DEBUG(DBG_DVD, "\n");
  }
  DVD_DEBUG(DBG_DVD, "Number of Subpicture streams: %i\n",
         vts_attributes->nr_of_vtsm_subp_streams);
  if(vts_attributes->nr_of_vtsm_subp_streams > 0) {
    DVD_DEBUG(DBG_DVD, "\tstream %2i attributes: ", 1);
    ifo_print_subp_attributes(&vts_attributes->vtsm_subp_attr);
    DVD_DEBUG(DBG_DVD, "\n");
  }

  DVD_DEBUG(DBG_DVD, "Video attributes of VTSTT_VOBS: ");
  ifo_print_video_attributes(&vts_attributes->vtstt_vobs_video_attr);
  DVD_DEBUG(DBG_DVD, "\n");
  DVD_DEBUG(DBG_DVD, "Number of Audio streams: %i\n",
         vts_attributes->nr_of_vtstt_audio_streams);
  for(i = 0; i < vts_attributes->nr_of_vtstt_audio_streams; i++) {
    DVD_DEBUG(DBG_DVD, "\tstream %i attributes: ", i);
    ifo_print_audio_attributes(&vts_attributes->vtstt_audio_attr[i]);
    DVD_DEBUG(DBG_DVD, "\n");
  }

  DVD_DEBUG(DBG_DVD, "Number of Subpicture streams: %i\n",
         vts_attributes->nr_of_vtstt_subp_streams);
  for(i = 0; i < vts_attributes->nr_of_vtstt_subp_streams; i++) {
    DVD_DEBUG(DBG_DVD, "\tstream %2i attributes: ", i);
    ifo_print_subp_attributes(&vts_attributes->vtstt_subp_attr[i]);
    DVD_DEBUG(DBG_DVD, "\n");
  }
}


static void ifoPrint_VTS_ATRT(vts_atrt_t *vts_atrt) {
  int i;

  DVD_DEBUG(DBG_DVD, "Number of Video Title Sets: %3i\n", vts_atrt->nr_of_vtss);
  for(i = 0; i < vts_atrt->nr_of_vtss; i++) {
    DVD_DEBUG(DBG_DVD, "\nVideo Title Set %i\n", i + 1);
    ifoPrint_VTS_ATTRIBUTES(&vts_atrt->vts[i]);
  }
}


void ifo_print(dvd_reader_t *dvd, int title) {
  ifo_handle_t *ifohandle;
  DVD_DEBUG(DBG_DVD, "Local ifo_print\n");
  ifohandle = ifoOpen(dvd, title);
  if(!ifohandle) {
    DVD_DEBUG(DBG_CRIT, "Can't open info file for title %d\n", title);
    return;
  }


  if(ifohandle->vmgi_mat) {

    DVD_DEBUG(DBG_DVD, "VMG top level\n-------------\n");
    ifoPrint_VMGI_MAT(ifohandle->vmgi_mat);

    DVD_DEBUG(DBG_DVD, "\nFirst Play PGC\n--------------\n");
    if(ifohandle->first_play_pgc)
      ifoPrint_PGC(ifohandle->first_play_pgc);
    else
      DVD_DEBUG(DBG_DVD, "No First Play PGC present\n");

    DVD_DEBUG(DBG_DVD, "\nTitle Track search pointer table\n");
    DVD_DEBUG(DBG_DVD,   "------------------------------------------------\n");
    ifoPrint_TT_SRPT(ifohandle->tt_srpt);

    DVD_DEBUG(DBG_DVD, "\nMenu PGCI Unit table\n");
    DVD_DEBUG(DBG_DVD,   "--------------------\n");
    if(ifohandle->pgci_ut) {
      ifoPrint_PGCI_UT(ifohandle->pgci_ut);
    } else {
      DVD_DEBUG(DBG_DVD, "No PGCI Unit table present\n");
    }

    DVD_DEBUG(DBG_DVD, "\nParental Management Information table\n");
    DVD_DEBUG(DBG_DVD,   "------------------------------------\n");
    if(ifohandle->ptl_mait) {
      ifoPrint_PTL_MAIT(ifohandle->ptl_mait);
    } else {
      DVD_DEBUG(DBG_DVD, "No Parental Management Information present\n");
    }

    DVD_DEBUG(DBG_DVD, "\nVideo Title Set Attribute Table\n");
    DVD_DEBUG(DBG_DVD,   "-------------------------------\n");
    ifoPrint_VTS_ATRT(ifohandle->vts_atrt);

    DVD_DEBUG(DBG_DVD, "\nText Data Manager Information\n");
    DVD_DEBUG(DBG_DVD,   "-----------------------------\n");
    if(ifohandle->txtdt_mgi) {
      //ifo_print_TXTDT_MGI(&(vmgi->txtdt_mgi));
    } else {
      DVD_DEBUG(DBG_DVD, "No Text Data Manager Information present\n");
    }

    DVD_DEBUG(DBG_DVD, "\nMenu Cell Address table\n");
    DVD_DEBUG(DBG_DVD,   "-----------------\n");
    if(ifohandle->menu_c_adt) {
      ifoPrint_C_ADT(ifohandle->menu_c_adt);
    } else {
      DVD_DEBUG(DBG_DVD, "No Menu Cell Address table present\n");
    }

    DVD_DEBUG(DBG_DVD, "\nVideo Manager Menu VOBU address map\n");
    DVD_DEBUG(DBG_DVD,   "-----------------\n");
    if(ifohandle->menu_vobu_admap) {
      ifoPrint_VOBU_ADMAP(ifohandle->menu_vobu_admap);
    } else {
      DVD_DEBUG(DBG_DVD, "No Menu VOBU address map present\n");
    }
  }


  if(ifohandle->vtsi_mat) {

    DVD_DEBUG(DBG_DVD, "VTS top level\n-------------\n");
    ifoPrint_VTSI_MAT(ifohandle->vtsi_mat);

    DVD_DEBUG(DBG_DVD, "\nPart of Title Track search pointer table\n");
    DVD_DEBUG(DBG_DVD,   "----------------------------------------------\n");
    ifoPrint_VTS_PTT_SRPT(ifohandle->vts_ptt_srpt);

    DVD_DEBUG(DBG_DVD, "\nPGCI Unit table\n");
    DVD_DEBUG(DBG_DVD,   "--------------------\n");
    ifoPrint_PGCIT(ifohandle->vts_pgcit, 0);

    DVD_DEBUG(DBG_DVD, "\nMenu PGCI Unit table\n");
    DVD_DEBUG(DBG_DVD,   "--------------------\n");
    if(ifohandle->pgci_ut) {
      ifoPrint_PGCI_UT(ifohandle->pgci_ut);
    } else {
      DVD_DEBUG(DBG_DVD, "No Menu PGCI Unit table present\n");
    }

    DVD_DEBUG(DBG_DVD, "\nVTS Time Map table\n");
    DVD_DEBUG(DBG_DVD,   "-----------------\n");
    if(ifohandle->vts_tmapt) {
      ifoPrint_VTS_TMAPT(ifohandle->vts_tmapt);
    } else {
      DVD_DEBUG(DBG_DVD, "No VTS Time Map table present\n");
    }

    DVD_DEBUG(DBG_DVD, "\nMenu Cell Address table\n");
    DVD_DEBUG(DBG_DVD,   "-----------------\n");
    if(ifohandle->menu_c_adt) {
      ifoPrint_C_ADT(ifohandle->menu_c_adt);
    } else {
      DVD_DEBUG(DBG_DVD, "No Cell Address table present\n");
    }

    DVD_DEBUG(DBG_DVD, "\nVideo Title Set Menu VOBU address map\n");
    DVD_DEBUG(DBG_DVD,   "-----------------\n");
    if(ifohandle->menu_vobu_admap) {
      ifoPrint_VOBU_ADMAP(ifohandle->menu_vobu_admap);
    } else {
      DVD_DEBUG(DBG_DVD, "No Menu VOBU address map present\n");
    }

    DVD_DEBUG(DBG_DVD, "\nCell Address table\n");
    DVD_DEBUG(DBG_DVD,   "-----------------\n");
    ifoPrint_C_ADT(ifohandle->vts_c_adt);

    DVD_DEBUG(DBG_DVD, "\nVideo Title Set VOBU address map\n");
    DVD_DEBUG(DBG_DVD,   "-----------------\n");
    ifoPrint_VOBU_ADMAP(ifohandle->vts_vobu_admap);
  }

  ifoClose(ifohandle);
}

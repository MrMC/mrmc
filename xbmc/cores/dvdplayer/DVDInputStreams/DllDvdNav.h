#pragma once

/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#if 1
extern "C" {
 #include <stdint.h>
 #include <pthread.h>

 #ifndef HAVE_CONFIG_H
 #define HAVE_CONFIG_H
 #endif

 #include "libdvd/src/libdvd.h"
 #include "libdvd/src/vm/decoder.h"
 #include "libdvd/src/vm/vm.h"
 #include "libdvd/src/libdvd_types.h"
 #include "libdvd/src/libdvd_internal.h"
 #include "libdvd/src/libdvd_filesystem.h"
 #include "libdvd/src/log_control.h"

 extern vm_t* dvdnav_get_vm(dvdnav_t *ctx);
 extern int dvdnav_get_button_info(dvdnav_t* ctx, int alpha[2][4], int color[2][4]);
}

#include "FileItem.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "filesystem/File.h"
#include "filesystem/Directory.h"

class DllDvdNavInterface
{
public:
  virtual ~DllDvdNavInterface() {}

  virtual DVD_FILE_OPEN dvd_register_file(DVD_FILE_OPEN p)
    { return ::dvd_register_file(p); }
  virtual DVD_DIR_OPEN dvd_register_dir(DVD_DIR_OPEN p)
    { return ::dvd_register_dir(p); }
  virtual void dvd_set_debug_handler(DVD_LOG_FUNC p)
    { ::dvd_set_debug_handler(p); }
  virtual void dvd_set_debug_mask(uint32_t mask)
    { ::dvd_set_debug_mask(mask); }
  virtual uint32_t dvd_get_debug_mask()
    { return ::dvd_get_debug_mask(); }

  virtual dvdnav_status_t dvdnav_open(dvdnav_t **dest, const char *path)
    { return ::dvdnav_open(dest, path); }
  virtual dvdnav_status_t dvdnav_close(dvdnav_t *ctx)
    { return ::dvdnav_close(ctx); }
  virtual dvdnav_status_t dvdnav_reset(dvdnav_t *ctx)
    { return ::dvdnav_reset(ctx); }
  virtual const char* dvdnav_err_to_string(dvdnav_t *ctx)
    { return ::dvdnav_err_to_string(ctx); }
  virtual dvdnav_status_t dvdnav_set_readahead_flag(dvdnav_t *ctx, int32_t read_ahead_flag)
    { return ::dvdnav_set_readahead_flag(ctx, read_ahead_flag); }
  virtual dvdnav_status_t dvdnav_set_PGC_positioning_flag(dvdnav_t *ctx, int32_t pgc_based_flag)
    { return ::dvdnav_set_PGC_positioning_flag(ctx, pgc_based_flag); }
  virtual dvdnav_status_t dvdnav_get_next_cache_block(dvdnav_t *ctx, uint8_t **buf, int32_t *event, int32_t *len)
    { return ::dvdnav_get_next_cache_block(ctx, buf, event, len); }
  virtual dvdnav_status_t dvdnav_free_cache_block(dvdnav_t *ctx, unsigned char *buf)
    { return ::dvdnav_free_cache_block(ctx, buf); }
  virtual dvdnav_status_t dvdnav_still_skip(dvdnav_t *ctx)
    { return ::dvdnav_still_skip(ctx); }
  virtual dvdnav_status_t dvdnav_wait_skip(dvdnav_t *ctx)
    { return ::dvdnav_wait_skip(ctx); }
  virtual dvdnav_status_t dvdnav_stop(dvdnav_t *ctx)
    { return ::dvdnav_stop(ctx); }
  virtual dvdnav_status_t dvdnav_button_select(dvdnav_t *ctx, pci_t *pci, int32_t button)
    { return ::dvdnav_button_select(ctx, pci, button); }
  virtual dvdnav_status_t dvdnav_button_activate(dvdnav_t *ctx, pci_t *pci)
    { return ::dvdnav_button_activate(ctx, pci); }
  virtual dvdnav_status_t dvdnav_upper_button_select(dvdnav_t *ctx, pci_t *pci)
    { return ::dvdnav_upper_button_select(ctx, pci); }
  virtual dvdnav_status_t dvdnav_lower_button_select(dvdnav_t *ctx, pci_t *pci)
    { return ::dvdnav_lower_button_select(ctx, pci); }
  virtual dvdnav_status_t dvdnav_right_button_select(dvdnav_t *ctx, pci_t *pci)
    { return ::dvdnav_right_button_select(ctx, pci); }
  virtual dvdnav_status_t dvdnav_left_button_select(dvdnav_t *ctx, pci_t *pci)
    { return ::dvdnav_left_button_select(ctx, pci); }
  virtual dvdnav_status_t dvdnav_sector_search(dvdnav_t *ctx, uint64_t offset, int32_t origin)
    { return ::dvdnav_sector_search(ctx, offset, origin); }
  virtual pci_t* dvdnav_get_current_nav_pci(dvdnav_t *ctx)
    { return ::dvdnav_get_current_nav_pci(ctx); }
  virtual dsi_t* dvdnav_get_current_nav_dsi(dvdnav_t *ctx)
    { return ::dvdnav_get_current_nav_dsi(ctx); }
  virtual dvdnav_status_t dvdnav_get_position(dvdnav_t *ctx, uint32_t *pos, uint32_t *len)
    { return ::dvdnav_get_position(ctx, pos, len); }
  virtual dvdnav_status_t dvdnav_current_title_info(dvdnav_t *ctx, int32_t *title, int32_t *part)
    { return ::dvdnav_current_title_info(ctx, title, part); }
  virtual dvdnav_status_t dvdnav_spu_language_select(dvdnav_t *ctx, char *code)
    { return ::dvdnav_spu_language_select(ctx, code); }
  virtual dvdnav_status_t dvdnav_audio_language_select(dvdnav_t *ctx, char *code)
    { return ::dvdnav_audio_language_select(ctx, code); }
  virtual dvdnav_status_t dvdnav_menu_language_select(dvdnav_t *ctx, char *code)
    { return ::dvdnav_menu_language_select(ctx, code); }
  virtual int8_t dvdnav_is_domain_vts(dvdnav_t *ctx)
    { return ::dvdnav_is_domain_vts(ctx); }
  virtual int8_t dvdnav_get_active_spu_stream(dvdnav_t *ctx)
    { return ::dvdnav_get_active_spu_stream(ctx); }
  virtual int8_t dvdnav_get_spu_logical_stream(dvdnav_t *ctx, uint8_t subp_num)
    { return ::dvdnav_get_spu_logical_stream(ctx, subp_num); }
  virtual uint16_t dvdnav_spu_stream_to_lang(dvdnav_t *ctx, uint8_t stream)
    { return ::dvdnav_spu_stream_to_lang(ctx, stream); }
  virtual dvdnav_status_t dvdnav_get_current_highlight(dvdnav_t *ctx, int32_t *button)
    { return ::dvdnav_get_current_highlight(ctx, button); }
  virtual dvdnav_status_t dvdnav_menu_call(dvdnav_t *ctx, DVDMenuID_t menu)
    { return ::dvdnav_menu_call(ctx, menu); }
  virtual dvdnav_status_t dvdnav_prev_pg_search(dvdnav_t *ctx)
    { return ::dvdnav_prev_pg_search(ctx); }
  virtual dvdnav_status_t dvdnav_next_pg_search(dvdnav_t *ctx)
    { return ::dvdnav_next_pg_search(ctx); }
  virtual dvdnav_status_t dvdnav_get_highlight_area(pci_t *nav_pci , int32_t button, int32_t mode, dvdnav_highlight_area_t *highlight)
    { return ::dvdnav_get_highlight_area(nav_pci, button, mode, highlight); }
  virtual dvdnav_status_t dvdnav_go_up(dvdnav_t *ctx)
    { return ::dvdnav_go_up(ctx); }
  virtual int8_t dvdnav_get_active_audio_stream(dvdnav_t *ctx)
    { return ::dvdnav_get_active_audio_stream(ctx); }
  virtual uint16_t dvdnav_audio_stream_to_lang(dvdnav_t *ctx, uint8_t stream)
    { return ::dvdnav_audio_stream_to_lang(ctx, stream); }
  virtual vm_t* dvdnav_get_vm(dvdnav_t *ctx)
    { return ::dvdnav_get_vm(ctx); }
  virtual int dvdnav_get_button_info(dvdnav_t* ctx, int alpha[2][4], int color[2][4])
    { return ::dvdnav_get_button_info(ctx, alpha, color); }
  virtual int8_t dvdnav_get_audio_logical_stream(dvdnav_t *ctx, uint8_t audio_num)
    { return ::dvdnav_get_audio_logical_stream(ctx, audio_num); }
  virtual dvdnav_status_t dvdnav_get_region_mask(dvdnav_t *ctx, int32_t *region_mask)
    { return ::dvdnav_get_region_mask(ctx, region_mask); }
  virtual dvdnav_status_t dvdnav_set_region_mask(dvdnav_t *ctx, int32_t region_mask)
    { return ::dvdnav_set_region_mask(ctx, region_mask); }
  virtual uint8_t dvdnav_get_video_aspect(dvdnav_t *ctx)
    { return ::dvdnav_get_video_aspect(ctx); }
  virtual uint8_t dvdnav_get_video_scale_permission(dvdnav_t *ctx)
    { return ::dvdnav_get_video_scale_permission(ctx); }
  virtual dvdnav_status_t dvdnav_get_number_of_titles(dvdnav_t *ctx, int32_t *titles)
    { return ::dvdnav_get_number_of_titles(ctx, titles); }
  virtual dvdnav_status_t dvdnav_get_number_of_parts(dvdnav_t *ctx, int32_t title, int32_t *parts)
    { return ::dvdnav_get_number_of_parts(ctx, title, parts); }
  virtual dvdnav_status_t dvdnav_title_play(dvdnav_t *ctx, int32_t title)
    { return ::dvdnav_title_play(ctx, title); }
  virtual dvdnav_status_t dvdnav_part_play(dvdnav_t *ctx, int32_t title, int32_t part)
    { return ::dvdnav_part_play(ctx, title, part); }
  virtual dvdnav_status_t dvdnav_get_audio_attr(dvdnav_t *ctx, int32_t streamid, audio_attr_t *audio_attributes)
    { return ::dvdnav_get_audio_attr(ctx, streamid, audio_attributes); }
  virtual dvdnav_status_t dvdnav_get_spu_attr(dvdnav_t *ctx, int32_t streamid, subp_attr_t *stitle_attributes)
    { return ::dvdnav_get_spu_attr(ctx, streamid, stitle_attributes); }
  virtual dvdnav_status_t dvdnav_time_search(dvdnav_t *ctx, uint64_t timepos)
    { return ::dvdnav_time_search(ctx, timepos); }
  virtual dvdnav_status_t dvdnav_jump_to_sector_by_time(dvdnav_t *ctx, uint64_t time_in_pts_ticks, int32_t mode)
    { return ::dvdnav_jump_to_sector_by_time(ctx, time_in_pts_ticks, mode); }
  virtual int64_t dvdnav_convert_time(dvd_time_t *time)
    { return ::dvdnav_convert_time(time); }
  virtual dvdnav_status_t dvdnav_get_state(dvdnav_t *ctx, dvd_state_t *save_state)
    { return ::dvdnav_get_state(ctx, save_state); }
  virtual dvdnav_status_t dvdnav_set_state(dvdnav_t *ctx, dvd_state_t *save_state)
    { return ::dvdnav_set_state(ctx, save_state); }
  virtual dvdnav_status_t dvdnav_get_angle_info(dvdnav_t *ctx, int32_t *current_angle, int32_t *number_of_angles)
    { return ::dvdnav_get_angle_info(ctx, current_angle, number_of_angles); }
  virtual dvdnav_status_t dvdnav_angle_change(dvdnav_t *ctx, int32_t angle)
    { return ::dvdnav_angle_change(ctx, angle); }
  virtual dvdnav_status_t dvdnav_mouse_activate(dvdnav_t *ctx, pci_t *pci, int32_t x, int32_t y)
    { return ::dvdnav_mouse_activate(ctx, pci, x, y); };
  virtual dvdnav_status_t dvdnav_mouse_select(dvdnav_t *ctx, pci_t *pci, int32_t x, int32_t y)
    { return ::dvdnav_mouse_select(ctx, pci, x, y); }
  virtual dvdnav_status_t dvdnav_get_title_string(dvdnav_t *ctx, const char **title_str)
    { return ::dvdnav_get_title_string(ctx, title_str); }
  virtual dvdnav_status_t dvdnav_get_serial_string(dvdnav_t *ctx, const char **serial_str)
    { return ::dvdnav_get_serial_string(ctx, serial_str); }
  virtual uint32_t dvdnav_describe_title_chapters(dvdnav_t *ctx, uint32_t title, uint64_t** times, uint64_t* duration)
    { return ::dvdnav_describe_title_chapters(ctx, title, times, duration); }
  virtual int64_t dvdnav_get_current_time(dvdnav_t *ctx)
    { return ::dvdnav_get_current_time(ctx); }
  virtual int dvdnav_get_video_resolution(dvdnav_t *ctx, uint32_t *width, uint32_t *height)
    { return ::dvdnav_get_video_resolution(ctx, width, height); }
};

class DllDvdNav : public DllDvdNavInterface
{
public:
  bool Load()
  {
    dvd_register_dir(dir_open);
    dvd_register_file(file_open);
    dvd_set_debug_handler(dvd_logger);
    dvd_set_debug_mask(DBG_CRIT | DBG_DVD | DBG_FILE);
    return true;
  }

  static void file_close(DVD_FILEIO_H *file)
  {
    if (file)
    {
      delete static_cast<XFILE::CFile*>(file->internal);
      delete file;
    }
  }

  static int64_t file_seek(DVD_FILEIO_H *file, int64_t offset, int32_t origin)
  {
    return static_cast<XFILE::CFile*>(file->internal)->Seek(offset, origin);
  }

  static int64_t file_tell(DVD_FILEIO_H *file)
  {
    return static_cast<XFILE::CFile*>(file->internal)->GetPosition();
  }

  static int file_eof(DVD_FILEIO_H *file)
  {
    if(static_cast<XFILE::CFile*>(file->internal)->GetPosition() == static_cast<XFILE::CFile*>(file->internal)->GetLength())
      return 1;
    else
      return 0;
  }

  static int64_t file_read(DVD_FILEIO_H *file, uint8_t *buf, int64_t size)
  {
    return static_cast<XFILE::CFile*>(file->internal)->Read(buf, size); // TODO: fix size cast
  }

  static int64_t file_write(DVD_FILEIO_H *file, const uint8_t *buf, int64_t size)
  {
    return -1;
  }

  static DVD_FILEIO_H* file_open(const char* filename, const char *mode)
  {
    DVD_FILEIO_H *file = new DVD_FILEIO_H;

    file->close = file_close;
    file->seek  = file_seek;
    file->read  = file_read;
    file->write = file_write;
    file->tell  = file_tell;
    file->eof   = file_eof;

    XFILE::CFile* fp = new XFILE::CFile();
    if(fp->Open(filename))
    {
      file->internal = (void*)fp;
      return file;
    }

    CLog::Log(LOGDEBUG, "DllDvdNav - Error opening file! (%p)", file);
    
    delete fp;
    delete file;

    return NULL;
  }

  struct SDirState
  {
    SDirState()
      : curr(0)
    {}

    CFileItemList list;
    int           curr;
  };

  static void dir_close(DVD_DIRIO_H *dir)
  {
    if (dir)
    {
      //CLog::Log(LOGDEBUG, "DllDvdNav - Closed dir (%p)\n", dir);
      delete static_cast<SDirState*>(dir->internal);
      delete dir;
    }
  }


  static int dir_read(DVD_DIRIO_H *dir, DVD_DIRENT *entry)
  {
    SDirState* state = static_cast<SDirState*>(dir->internal);

    if(state->curr >= state->list.Size())
      return 1;

    strncpy(entry->d_name, state->list[state->curr]->GetLabel().c_str(), sizeof(entry->d_name));
    entry->d_name[sizeof(entry->d_name)-1] = 0;
    state->curr++;

    return 0;
  }

  static DVD_DIRIO_H* dir_open(const char* dirname)
  {
    // iso/img files are treated as a directory by CDirectory
    // check and return null if found.
    if(URIUtils::HasExtension(dirname, ".iso|.img"))
      return NULL;

    //CLog::Log(LOGDEBUG, "DllDvdNav - Opening dir %s\n", dirname);
    SDirState *st = new SDirState();

    std::string strDirname(dirname);

    if(!XFILE::CDirectory::GetDirectory(strDirname, st->list))
    {
      CLog::Log(LOGDEBUG, "DllDvdNav - Error opening dir! (%s)\n", dirname);
      delete st;
      return NULL;
    }

    DVD_DIRIO_H *dir = new DVD_DIRIO_H;
    dir->close    = dir_close;
    dir->read     = dir_read;
    dir->internal = (void*)st;

    return dir;
  }

  static void dvd_logger(const char* msg)
  {
    CLog::Log(LOGDEBUG, "DllDvdNav::Logger - %s", msg);
  }

};

#else
extern "C" {
 #include <stdint.h>

 #include <dvdnav/dvdnav.h>

 #ifndef HAVE_CONFIG_H
 #define HAVE_CONFIG_H
 #endif

 #include <dvdnav/decoder.h>
 #include <dvdnav/vm.h>
 #include <dvdnav/dvdnav_internal.h>
 #include <dvdnav/dvd_types.h>
}
#include "DynamicDll.h"

class DllDvdNavInterface
{
public:
  virtual ~DllDvdNavInterface() {}
  virtual dvdnav_status_t dvdnav_open(dvdnav_t **dest, const char *path)=0;
  virtual dvdnav_status_t dvdnav_close(dvdnav_t *ctx)=0;
  virtual dvdnav_status_t dvdnav_reset(dvdnav_t *ctx)=0;
  virtual const char* dvdnav_err_to_string(dvdnav_t *ctx)=0;
  virtual dvdnav_status_t dvdnav_set_readahead_flag(dvdnav_t *ctx, int32_t read_ahead_flag)=0;
  virtual dvdnav_status_t dvdnav_set_PGC_positioning_flag(dvdnav_t *ctx, int32_t pgc_based_flag)=0;
  virtual dvdnav_status_t dvdnav_get_next_cache_block(dvdnav_t *ctx, uint8_t **buf, int32_t *event, int32_t *len)=0;
  virtual dvdnav_status_t dvdnav_free_cache_block(dvdnav_t *ctx, unsigned char *buf)=0;
  virtual dvdnav_status_t dvdnav_still_skip(dvdnav_t *ctx)=0;
  virtual dvdnav_status_t dvdnav_wait_skip(dvdnav_t *ctx)=0;
  virtual dvdnav_status_t dvdnav_stop(dvdnav_t *ctx)=0;
  virtual dvdnav_status_t dvdnav_button_select(dvdnav_t *ctx, pci_t *pci, int32_t button)=0;
  virtual dvdnav_status_t dvdnav_button_activate(dvdnav_t *ctx, pci_t *pci)=0;
  virtual dvdnav_status_t dvdnav_upper_button_select(dvdnav_t *ctx, pci_t *pci)=0;
  virtual dvdnav_status_t dvdnav_lower_button_select(dvdnav_t *ctx, pci_t *pci)=0;
  virtual dvdnav_status_t dvdnav_right_button_select(dvdnav_t *ctx, pci_t *pci)=0;
  virtual dvdnav_status_t dvdnav_left_button_select(dvdnav_t *ctx, pci_t *pci)=0;
  virtual dvdnav_status_t dvdnav_sector_search(dvdnav_t *ctx, uint64_t offset, int32_t origin)=0;
  virtual pci_t* dvdnav_get_current_nav_pci(dvdnav_t *ctx)=0;
  virtual dsi_t* dvdnav_get_current_nav_dsi(dvdnav_t *ctx)=0;
  virtual dvdnav_status_t dvdnav_get_position(dvdnav_t *ctx, uint32_t *pos, uint32_t *len)=0;
  virtual dvdnav_status_t dvdnav_current_title_info(dvdnav_t *ctx, int32_t *title, int32_t *part)=0;
  virtual dvdnav_status_t dvdnav_spu_language_select(dvdnav_t *ctx, char *code)=0;
  virtual dvdnav_status_t dvdnav_audio_language_select(dvdnav_t *ctx, char *code)=0;
  virtual dvdnav_status_t dvdnav_menu_language_select(dvdnav_t *ctx, char *code)=0;
  virtual int8_t dvdnav_is_domain_vts(dvdnav_t *ctx)=0;
  virtual int8_t dvdnav_get_active_spu_stream(dvdnav_t *ctx)=0;
  virtual int8_t dvdnav_get_spu_logical_stream(dvdnav_t *ctx, uint8_t subp_num)=0;
  virtual uint16_t dvdnav_spu_stream_to_lang(dvdnav_t *ctx, uint8_t stream)=0;
  virtual dvdnav_status_t dvdnav_get_current_highlight(dvdnav_t *ctx, int32_t *button)=0;
  virtual dvdnav_status_t dvdnav_menu_call(dvdnav_t *ctx, DVDMenuID_t menu)=0;
  virtual dvdnav_status_t dvdnav_prev_pg_search(dvdnav_t *ctx)=0;
  virtual dvdnav_status_t dvdnav_next_pg_search(dvdnav_t *ctx)=0;
  virtual dvdnav_status_t dvdnav_get_highlight_area(pci_t *nav_pci , int32_t button, int32_t mode, dvdnav_highlight_area_t *highlight)=0;
  virtual dvdnav_status_t dvdnav_go_up(dvdnav_t *ctx)=0;
  virtual int8_t dvdnav_get_active_audio_stream(dvdnav_t *ctx)=0;
  virtual uint16_t dvdnav_audio_stream_to_lang(dvdnav_t *ctx, uint8_t stream)=0;
  virtual vm_t* dvdnav_get_vm(dvdnav_t *ctx)=0;
  virtual int dvdnav_get_button_info(dvdnav_t* ctx, int alpha[2][4], int color[2][4])=0;
  virtual int8_t dvdnav_get_audio_logical_stream(dvdnav_t *ctx, uint8_t audio_num)=0;
  virtual dvdnav_status_t dvdnav_set_region_mask(dvdnav_t *ctx, int32_t region_mask)=0;
  virtual uint8_t dvdnav_get_video_aspect(dvdnav_t *ctx)=0;
  virtual uint8_t dvdnav_get_video_scale_permission(dvdnav_t *ctx)=0;
  virtual dvdnav_status_t dvdnav_get_number_of_titles(dvdnav_t *ctx, int32_t *titles)=0;
  virtual dvdnav_status_t dvdnav_get_number_of_parts(dvdnav_t *ctx, int32_t title, int32_t *parts)=0;
  virtual dvdnav_status_t dvdnav_title_play(dvdnav_t *ctx, int32_t title)=0;
  virtual dvdnav_status_t dvdnav_part_play(dvdnav_t *ctx, int32_t title, int32_t part)=0;
  virtual dvdnav_status_t dvdnav_get_audio_attr(dvdnav_t * ctx, int32_t streamid, audio_attr_t* audio_attributes)=0;
  virtual dvdnav_status_t dvdnav_get_spu_attr(dvdnav_t * ctx, int32_t streamid, subp_attr_t* stitle_attributes)=0;
  virtual dvdnav_status_t dvdnav_time_search(dvdnav_t * ctx, uint64_t timepos)=0;
  virtual dvdnav_status_t dvdnav_jump_to_sector_by_time(dvdnav_t *ctx, uint64_t time_in_pts_ticks, int32_t mode)=0;
  virtual int64_t dvdnav_convert_time(dvd_time_t *time)=0;
  virtual dvdnav_status_t dvdnav_get_state(dvdnav_t *ctx, dvd_state_t *save_state)=0;
  virtual dvdnav_status_t dvdnav_set_state(dvdnav_t *ctx, dvd_state_t *save_state)=0;
  virtual dvdnav_status_t dvdnav_get_angle_info(dvdnav_t *ctx, int32_t *current_angle,int32_t *number_of_angles)=0;
  virtual dvdnav_status_t dvdnav_angle_change(dvdnav_t *ctx, int32_t angle) = 0;
  virtual dvdnav_status_t dvdnav_mouse_activate(dvdnav_t *ctx, pci_t *pci, int32_t x, int32_t y)=0;
  virtual dvdnav_status_t dvdnav_mouse_select(dvdnav_t *ctx, pci_t *pci, int32_t x, int32_t y)=0;
  virtual dvdnav_status_t dvdnav_get_title_string(dvdnav_t *ctx, const char **title_str)=0;
  virtual dvdnav_status_t dvdnav_get_serial_string(dvdnav_t *ctx, const char **serial_str)=0;
  virtual uint32_t dvdnav_describe_title_chapters(dvdnav_t* ctx, uint32_t title, uint64_t** times, uint64_t* duration)=0;
  virtual int64_t dvdnav_get_current_time(dvdnav_t* ctx) = 0;
  virtual int dvdnav_get_video_resolution(dvdnav_t* ctx, uint32_t* width, uint32_t* height)=0;
};

class DllDvdNav : public DllDynamic, DllDvdNavInterface
{
#if defined(TARGET_DARWIN_IOS) && !defined(__x86_64__)
  DECLARE_DLL_WRAPPER(DllDvdNav, "libdvdnav.framework/libdvdnav")
#else
  DECLARE_DLL_WRAPPER(DllDvdNav, DLL_PATH_LIBDVDNAV)
#endif

  DEFINE_METHOD2(dvdnav_status_t, dvdnav_open, (dvdnav_t **p1, const char *p2))
  DEFINE_METHOD1(dvdnav_status_t, dvdnav_close, (dvdnav_t *p1))
  DEFINE_METHOD1(dvdnav_status_t, dvdnav_reset, (dvdnav_t *p1))
  DEFINE_METHOD1(const char*, dvdnav_err_to_string, (dvdnav_t *p1))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_set_readahead_flag, (dvdnav_t *p1, int32_t p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_set_PGC_positioning_flag, (dvdnav_t *p1, int32_t p2))
  DEFINE_METHOD4(dvdnav_status_t, dvdnav_get_next_cache_block, (dvdnav_t *p1, uint8_t **p2, int32_t *p3, int32_t *p4))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_free_cache_block, (dvdnav_t *p1, unsigned char *p2))
  DEFINE_METHOD1(dvdnav_status_t, dvdnav_still_skip, (dvdnav_t *p1))
  DEFINE_METHOD1(dvdnav_status_t, dvdnav_wait_skip, (dvdnav_t *p1))
  DEFINE_METHOD1(dvdnav_status_t, dvdnav_stop, (dvdnav_t *p1))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_button_select, (dvdnav_t *p1, pci_t *p2, int32_t p3))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_button_activate,(dvdnav_t *p1, pci_t *p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_upper_button_select, (dvdnav_t *p1, pci_t *p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_lower_button_select, (dvdnav_t *p1, pci_t *p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_right_button_select, (dvdnav_t *p1, pci_t *p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_left_button_select, (dvdnav_t *p1, pci_t *p2))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_sector_search, (dvdnav_t *p1, uint64_t p2, int32_t p3))
  DEFINE_METHOD1(pci_t*, dvdnav_get_current_nav_pci, (dvdnav_t *p1))
  DEFINE_METHOD1(dsi_t*, dvdnav_get_current_nav_dsi, (dvdnav_t *p1))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_get_position, (dvdnav_t *p1, uint32_t *p2, uint32_t *p3))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_current_title_info, (dvdnav_t *p1, int32_t *p2, int32_t *p3))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_spu_language_select, (dvdnav_t *p1, char *p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_audio_language_select, (dvdnav_t *p1, char *p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_menu_language_select, (dvdnav_t *p1, char *p2))
  DEFINE_METHOD1(int8_t, dvdnav_is_domain_vts, (dvdnav_t *p1))
  DEFINE_METHOD1(int8_t, dvdnav_get_active_spu_stream, (dvdnav_t *p1))
  DEFINE_METHOD2(int8_t, dvdnav_get_spu_logical_stream, (dvdnav_t *p1, uint8_t p2))
  DEFINE_METHOD2(uint16_t, dvdnav_spu_stream_to_lang, (dvdnav_t *p1, uint8_t p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_get_current_highlight, (dvdnav_t *p1, int32_t *p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_menu_call, (dvdnav_t *p1, DVDMenuID_t p2))
  DEFINE_METHOD1(dvdnav_status_t, dvdnav_prev_pg_search, (dvdnav_t *p1))
  DEFINE_METHOD1(dvdnav_status_t, dvdnav_next_pg_search, (dvdnav_t *p1))
  DEFINE_METHOD4(dvdnav_status_t, dvdnav_get_highlight_area, (pci_t *p1, int32_t p2, int32_t p3, dvdnav_highlight_area_t *p4))
  DEFINE_METHOD1(dvdnav_status_t, dvdnav_go_up, (dvdnav_t *p1))
  DEFINE_METHOD1(int8_t, dvdnav_get_active_audio_stream, (dvdnav_t *p1))
  DEFINE_METHOD2(uint16_t, dvdnav_audio_stream_to_lang, (dvdnav_t *p1, uint8_t p2))
  DEFINE_METHOD1(vm_t*, dvdnav_get_vm, (dvdnav_t *p1))
  DEFINE_METHOD3(int, dvdnav_get_button_info, (dvdnav_t* p1, int p2[2][4], int p3[2][4]))
  DEFINE_METHOD2(int8_t, dvdnav_get_audio_logical_stream, (dvdnav_t *p1, uint8_t p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_set_region_mask, (dvdnav_t *p1, int32_t p2))
  DEFINE_METHOD1(uint8_t, dvdnav_get_video_aspect, (dvdnav_t *p1))
  DEFINE_METHOD1(uint8_t, dvdnav_get_video_scale_permission, (dvdnav_t *p1))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_get_number_of_titles, (dvdnav_t *p1, int32_t *p2))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_get_number_of_parts, (dvdnav_t *p1, int32_t p2, int32_t *p3))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_title_play, (dvdnav_t *p1, int32_t p2))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_part_play, (dvdnav_t *p1, int32_t p2, int32_t p3))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_get_audio_attr, (dvdnav_t * p1, int32_t p2, audio_attr_t* p3))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_get_spu_attr, (dvdnav_t * p1, int32_t p2, subp_attr_t* p3))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_time_search, (dvdnav_t * p1, uint64_t p2))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_jump_to_sector_by_time, (dvdnav_t * p1, uint64_t p2, int32_t p3))
  DEFINE_METHOD1(int64_t, dvdnav_convert_time, (dvd_time_t *p1))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_get_state, (dvdnav_t *p1, dvd_state_t *p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_set_state, (dvdnav_t *p1, dvd_state_t *p2))
  DEFINE_METHOD3(dvdnav_status_t, dvdnav_get_angle_info, (dvdnav_t *p1, int32_t *p2,int32_t *p3))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_angle_change, (dvdnav_t *p1, int32_t p2))
  DEFINE_METHOD4(dvdnav_status_t, dvdnav_mouse_activate, (dvdnav_t *p1, pci_t *p2, int32_t p3, int32_t p4))
  DEFINE_METHOD4(dvdnav_status_t, dvdnav_mouse_select, (dvdnav_t *p1, pci_t *p2, int32_t p3, int32_t p4))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_get_title_string, (dvdnav_t *p1, const char **p2))
  DEFINE_METHOD2(dvdnav_status_t, dvdnav_get_serial_string, (dvdnav_t *p1, const char **p2))
  DEFINE_METHOD4(uint32_t, dvdnav_describe_title_chapters, (dvdnav_t* p1, uint32_t p2, uint64_t** p3, uint64_t* p4))
  DEFINE_METHOD1(int64_t, dvdnav_get_current_time, (dvdnav_t* p1))
  DEFINE_METHOD3(int, dvdnav_get_video_resolution, (dvdnav_t* p1, uint32_t* p2, uint32_t* p3))
  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD(dvdnav_open)
    RESOLVE_METHOD(dvdnav_close)
    RESOLVE_METHOD(dvdnav_reset)
    RESOLVE_METHOD(dvdnav_err_to_string)
    RESOLVE_METHOD(dvdnav_set_readahead_flag)
    RESOLVE_METHOD(dvdnav_set_PGC_positioning_flag)
    RESOLVE_METHOD(dvdnav_get_next_cache_block)
    RESOLVE_METHOD(dvdnav_free_cache_block)
    RESOLVE_METHOD(dvdnav_still_skip)
    RESOLVE_METHOD(dvdnav_wait_skip)
    RESOLVE_METHOD(dvdnav_stop)
    RESOLVE_METHOD(dvdnav_button_select)
    RESOLVE_METHOD(dvdnav_button_activate)
    RESOLVE_METHOD(dvdnav_upper_button_select)
    RESOLVE_METHOD(dvdnav_lower_button_select)
    RESOLVE_METHOD(dvdnav_right_button_select)
    RESOLVE_METHOD(dvdnav_left_button_select)
    RESOLVE_METHOD(dvdnav_sector_search)
    RESOLVE_METHOD(dvdnav_get_current_nav_pci)
    RESOLVE_METHOD(dvdnav_get_current_nav_dsi)
    RESOLVE_METHOD(dvdnav_get_position)
    RESOLVE_METHOD(dvdnav_current_title_info)
    RESOLVE_METHOD(dvdnav_spu_language_select)
    RESOLVE_METHOD(dvdnav_audio_language_select)
    RESOLVE_METHOD(dvdnav_menu_language_select)
    RESOLVE_METHOD(dvdnav_is_domain_vts)
    RESOLVE_METHOD(dvdnav_get_active_spu_stream)
    RESOLVE_METHOD(dvdnav_get_spu_logical_stream)
    RESOLVE_METHOD(dvdnav_spu_stream_to_lang)
    RESOLVE_METHOD(dvdnav_get_current_highlight)
    RESOLVE_METHOD(dvdnav_menu_call)
    RESOLVE_METHOD(dvdnav_prev_pg_search)
    RESOLVE_METHOD(dvdnav_next_pg_search)
    RESOLVE_METHOD(dvdnav_get_highlight_area)
    RESOLVE_METHOD(dvdnav_go_up)
    RESOLVE_METHOD(dvdnav_get_active_audio_stream)
    RESOLVE_METHOD(dvdnav_audio_stream_to_lang)
    RESOLVE_METHOD(dvdnav_get_vm)
    RESOLVE_METHOD(dvdnav_get_button_info)
    RESOLVE_METHOD(dvdnav_get_audio_logical_stream)
    RESOLVE_METHOD(dvdnav_set_region_mask)
    RESOLVE_METHOD(dvdnav_get_video_aspect)
    RESOLVE_METHOD(dvdnav_get_video_scale_permission)
    RESOLVE_METHOD(dvdnav_get_number_of_titles)
    RESOLVE_METHOD(dvdnav_get_number_of_parts)
    RESOLVE_METHOD(dvdnav_title_play)
    RESOLVE_METHOD(dvdnav_part_play)
    RESOLVE_METHOD(dvdnav_get_audio_attr)
    RESOLVE_METHOD(dvdnav_get_spu_attr)
    RESOLVE_METHOD(dvdnav_time_search)
    RESOLVE_METHOD(dvdnav_jump_to_sector_by_time)
    RESOLVE_METHOD(dvdnav_convert_time)
    RESOLVE_METHOD(dvdnav_get_state)
    RESOLVE_METHOD(dvdnav_set_state)
    RESOLVE_METHOD(dvdnav_get_angle_info)
    RESOLVE_METHOD(dvdnav_angle_change)
    RESOLVE_METHOD(dvdnav_mouse_activate)
    RESOLVE_METHOD(dvdnav_mouse_select)
    RESOLVE_METHOD(dvdnav_get_title_string)
    RESOLVE_METHOD(dvdnav_get_serial_string)
    RESOLVE_METHOD(dvdnav_describe_title_chapters)
    RESOLVE_METHOD(dvdnav_get_current_time)
    RESOLVE_METHOD(dvdnav_get_video_resolution)
END_METHOD_RESOLVE()
};
#endif

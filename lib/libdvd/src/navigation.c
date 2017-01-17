/*
 * Copyright (C) 2000 Rich Wareham <richwareham@users.sourceforge.net>
 *
 * ctx file is part of libdvdnav, a DVD navigation library.
 *
 * libdvdnav is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdnav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdnav; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
//#include "libDVD/src/config/config.h"
#endif

#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

#include "libdvd.h"
#include "libdvd_internal.h"
#include "logging.h"
#include "reader/nav_types.h"
#include "reader/ifo_types.h"
#include "vm/decoder.h"
#include "vm/vm.h"

/* Navigation API calls */

dvdnav_status_t dvdnav_still_skip(dvdnav_t *ctx) {
  pthread_mutex_lock(&ctx->vm_lock);
  ctx->position_current.still = 0;
  pthread_mutex_unlock(&ctx->vm_lock);
  ctx->skip_still = 1;
  ctx->sync_wait = 0;
  ctx->sync_wait_skip = 1;

  return DVDNAV_STATUS_OK;
}

dvdnav_status_t dvdnav_wait_skip(dvdnav_t *ctx) {
  ctx->sync_wait = 0;
  ctx->sync_wait_skip = 1;

  return DVDNAV_STATUS_OK;
}

dvdnav_status_t dvdnav_get_number_of_titles(dvdnav_t *ctx, int32_t *titles) {
  if (!ctx->vm->vmgi) {
    DVD_DEBUG(DBG_CRIT, "Bad VM state.");
    return DVDNAV_STATUS_ERR;
  }

  (*titles) = vm_get_vmgi(ctx->vm)->tt_srpt->nr_of_srpts;

  return DVDNAV_STATUS_OK;
}

dvdnav_status_t dvdnav_get_number_of_parts(dvdnav_t *ctx, int32_t title, int32_t *parts) {
  if (!ctx->vm->vmgi) {
    DVD_DEBUG(DBG_CRIT, "Bad VM state.");
    return DVDNAV_STATUS_ERR;
  }
  if ((title < 1) || (title > vm_get_vmgi(ctx->vm)->tt_srpt->nr_of_srpts) ) {
    DVD_DEBUG(DBG_CRIT, "Passed a title number out of range.");
    return DVDNAV_STATUS_ERR;
  }

  (*parts) = vm_get_vmgi(ctx->vm)->tt_srpt->title[title-1].nr_of_ptts;

  return DVDNAV_STATUS_OK;
}

dvdnav_status_t dvdnav_current_title_info(dvdnav_t *ctx, int32_t *title, int32_t *part) {
  int32_t retval;

  pthread_mutex_lock(&ctx->vm_lock);
  if (!ctx->vm->vtsi || !ctx->vm->vmgi) {
    DVD_DEBUG(DBG_CRIT, "Bad VM state.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if (!ctx->started) {
    DVD_DEBUG(DBG_CRIT, "Virtual DVD machine not started.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if (!ctx->vm->state.pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if ( (ctx->vm->state.domain == DVD_DOMAIN_VTSMenu)
      || (ctx->vm->state.domain == DVD_DOMAIN_VMGM) ) {
    /* Get current Menu ID: into *part. */
    if(! vm_get_current_menu(ctx->vm, part)) {
      pthread_mutex_unlock(&ctx->vm_lock);
      return DVDNAV_STATUS_ERR;
    }
    if (*part > -1) {
      *title = 0;
      pthread_mutex_unlock(&ctx->vm_lock);
      return DVDNAV_STATUS_OK;
    }
  }
  if (ctx->vm->state.domain == DVD_DOMAIN_VTSTitle) {
    retval = vm_get_current_title_part(ctx->vm, title, part);
    pthread_mutex_unlock(&ctx->vm_lock);
    return retval ? DVDNAV_STATUS_OK : DVDNAV_STATUS_ERR;
  }
  DVD_DEBUG(DBG_CRIT, "Not in a title or menu.");
  pthread_mutex_unlock(&ctx->vm_lock);
  return DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_current_title_program(dvdnav_t *ctx, int32_t *title, int32_t *pgcn, int32_t *pgn) {
  int32_t retval;
  int32_t part;

  pthread_mutex_lock(&ctx->vm_lock);
  if (!ctx->vm->vtsi || !ctx->vm->vmgi) {
    DVD_DEBUG(DBG_CRIT, "Bad VM state.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if (!ctx->started) {
    DVD_DEBUG(DBG_CRIT, "Virtual DVD machine not started.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if (!ctx->vm->state.pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if ( (ctx->vm->state.domain == DVD_DOMAIN_VTSMenu)
      || (ctx->vm->state.domain == DVD_DOMAIN_VMGM) ) {
    /* Get current Menu ID: into *part. */
    if(! vm_get_current_menu(ctx->vm, &part)) {
      pthread_mutex_unlock(&ctx->vm_lock);
      return DVDNAV_STATUS_ERR;
    }
    if (part > -1) {
      *title = 0;
      *pgcn = ctx->vm->state.pgcN;
      *pgn = ctx->vm->state.pgN;
      pthread_mutex_unlock(&ctx->vm_lock);
      return DVDNAV_STATUS_OK;
    }
  }
  if (ctx->vm->state.domain == DVD_DOMAIN_VTSTitle) {
    retval = vm_get_current_title_part(ctx->vm, title, &part);
    *pgcn = ctx->vm->state.pgcN;
    *pgn = ctx->vm->state.pgN;
    pthread_mutex_unlock(&ctx->vm_lock);
    return retval ? DVDNAV_STATUS_OK : DVDNAV_STATUS_ERR;
  }
  DVD_DEBUG(DBG_CRIT, "Not in a title or menu.");
  pthread_mutex_unlock(&ctx->vm_lock);
  return DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_title_play(dvdnav_t *ctx, int32_t title) {
  return dvdnav_part_play(ctx, title, 1);
}

dvdnav_status_t dvdnav_program_play(dvdnav_t *ctx, int32_t title, int32_t pgcn, int32_t pgn) {
  int32_t retval;

  pthread_mutex_lock(&ctx->vm_lock);
  if (!ctx->vm->vmgi) {
    DVD_DEBUG(DBG_CRIT, "Bad VM state.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if (!ctx->started) {
    /* don't report an error but be nice */
    vm_start(ctx->vm);
    ctx->started = 1;
  }
  if (!ctx->vm->state.pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if((title < 1) || (title > ctx->vm->vmgi->tt_srpt->nr_of_srpts)) {
    DVD_DEBUG(DBG_CRIT, "Title out of range.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }

  retval = vm_jump_title_program(ctx->vm, title, pgcn, pgn);
  if (retval)
    ctx->vm->hop_channel++;
  pthread_mutex_unlock(&ctx->vm_lock);

  return retval ? DVDNAV_STATUS_OK : DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_part_play(dvdnav_t *ctx, int32_t title, int32_t part) {
  int32_t retval;

  pthread_mutex_lock(&ctx->vm_lock);
  if (!ctx->vm->vmgi) {
    DVD_DEBUG(DBG_CRIT, "Bad VM state.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if (!ctx->started) {
    /* don't report an error but be nice */
    vm_start(ctx->vm);
    ctx->started = 1;
  }
  if (!ctx->vm->state.pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if((title < 1) || (title > ctx->vm->vmgi->tt_srpt->nr_of_srpts)) {
    DVD_DEBUG(DBG_CRIT, "Title out of range.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if((part < 1) || (part > ctx->vm->vmgi->tt_srpt->title[title-1].nr_of_ptts)) {
    DVD_DEBUG(DBG_CRIT, "Part out of range.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }

  retval = vm_jump_title_part(ctx->vm, title, part);
  if (retval)
    ctx->vm->hop_channel++;
  pthread_mutex_unlock(&ctx->vm_lock);

  return retval ? DVDNAV_STATUS_OK : DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_part_play_auto_stop(dvdnav_t *ctx, int32_t title,
                                           int32_t part, int32_t parts_to_play) {
  /* FIXME: Implement auto-stop */
 if (dvdnav_part_play(ctx, title, part) == DVDNAV_STATUS_OK)
   DVD_DEBUG(DBG_CRIT, "Not implemented yet.");
 return DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_time_play(dvdnav_t *ctx, int32_t title,
                                 uint64_t time) {
  /* FIXME: Implement */
  DVD_DEBUG(DBG_CRIT, "Not implemented yet.");
  return DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_stop(dvdnav_t *ctx) {
  pthread_mutex_lock(&ctx->vm_lock);
  ctx->vm->stopped = 1;
  pthread_mutex_unlock(&ctx->vm_lock);
  return DVDNAV_STATUS_OK;
}

dvdnav_status_t dvdnav_go_up(dvdnav_t *ctx) {
  /* A nice easy function... delegate to the VM */
  int retval;
  pthread_mutex_lock(&ctx->vm_lock);
  retval = vm_jump_up(ctx->vm);
  pthread_mutex_unlock(&ctx->vm_lock);

  return retval ? DVDNAV_STATUS_OK : DVDNAV_STATUS_ERR;
}

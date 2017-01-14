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
#include "libDVD/src/config/config.h"
#endif

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>

#include "libdvd.h"
#include "libdvd_internal.h"
#include "logging.h"
#include "reader/nav_types.h"
#include "reader/ifo_types.h"
#include "vm/decoder.h"
#include "vm/vm.h"
#include "reader/ifo_read.h"

/*
#define LOG_DEBUG
*/

/* Searching API calls */

/* Scan the ADMAP for a particular block number. */
/* Return placed in vobu. */
/* Returns error status */
/* FIXME: Maybe need to handle seeking outside current cell. */
static dvdnav_status_t dvdnav_scan_admap(dvdnav_t *ctx, int32_t domain, uint32_t seekto_block, int next, uint32_t *vobu) {
  vobu_admap_t *admap = NULL;

#ifdef LOG_DEBUG
  DVD_DEBUG(DBG_DVD, "libdvdnav: Seeking to target %u ...\n", seekto_block);
#endif
  *vobu = -1;

  /* Search through the VOBU_ADMAP for the nearest VOBU
   * to the target block */
  switch(domain) {
  case DVD_DOMAIN_FirstPlay:
  case DVD_DOMAIN_VMGM:
    admap = ctx->vm->vmgi->menu_vobu_admap;
    break;
  case DVD_DOMAIN_VTSMenu:
    admap = ctx->vm->vtsi->menu_vobu_admap;
    break;
  case DVD_DOMAIN_VTSTitle:
    admap = ctx->vm->vtsi->vts_vobu_admap;
    break;
  default:
    DVD_DEBUG(DBG_CRIT, "libdvdnav: Error: Unknown domain for seeking.\n");
  }
  if(admap) {
    uint32_t address = 0;
    uint32_t vobu_start, next_vobu = 0;
    uint32_t admap_entries = (admap->last_byte + 1 - VOBU_ADMAP_SIZE)/VOBU_ADMAP_SIZE;

    /* Search through ADMAP for best sector */
    vobu_start = SRI_END_OF_CELL;
    /* FIXME: Implement a faster search algorithm */
    while(address < admap_entries) {
      next_vobu = admap->vobu_start_sectors[address];

      /* DVD_DEBUG(DBG_CRIT, "libdvdnav: Found block %u\n", next_vobu); */

      if(vobu_start <= seekto_block && next_vobu > seekto_block)
        break;
      vobu_start = next_vobu;
      address++;
    }
    *vobu = next ? next_vobu : vobu_start;
    return DVDNAV_STATUS_OK;
  }
  DVD_DEBUG(DBG_CRIT, "libdvdnav: admap not located\n");
  return DVDNAV_STATUS_ERR;
}

/* FIXME: right now, ctx function does not use the time tables but interpolates
   only the cell times */
dvdnav_status_t dvdnav_time_search(dvdnav_t *ctx,
                                   uint64_t time) {

  uint64_t target = time;
  uint64_t length = 0;
  uint32_t first_cell_nr, last_cell_nr, cell_nr;
  int32_t found;
  cell_playback_t *cell;
  dvd_state_t *state;

  if(ctx->position_current.still != 0) {
    DVD_DEBUG(DBG_CRIT, "Cannot seek in a still frame.");
    return DVDNAV_STATUS_ERR;
  }

  pthread_mutex_lock(&ctx->vm_lock);
  state = &(ctx->vm->state);
  if(!state->pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }

  if ((state->pgc->prohibited_ops.title_or_time_play == 1) ||
    (ctx->pci.pci_gi.vobu_uop_ctl.title_or_time_play == 1)){
    DVD_DEBUG(DBG_CRIT, "operation forbidden.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }

  ctx->cur_cell_time = 0;
  if (ctx->pgc_based) {
    first_cell_nr = 1;
    last_cell_nr = state->pgc->nr_of_cells;
  } else {
    /* Find start cell of program. */
    first_cell_nr = state->pgc->program_map[state->pgN-1];
    /* Find end cell of program */
    if(state->pgN < state->pgc->nr_of_programs)
      last_cell_nr = state->pgc->program_map[state->pgN] - 1;
    else
      last_cell_nr = state->pgc->nr_of_cells;
  }


  /* FIXME: using time map is not going to work unless we are pgc_based */
  /*        we'd need to recalculate the time to be relative to full pgc first*/
  if (!ctx->pgc_based)
  {
#ifdef LOG_DEBUG
    DVD_DEBUG(DBG_CRIT, "libdvdnav: time_search - not pgc based\n");
#endif
    goto timemapdone;
  }

  if (!ctx->vm->vtsi->vts_tmapt){
  /* no time map for ctx program chain */
#ifdef LOG_DEBUG
    DVD_DEBUG(DBG_CRIT, "libdvdnav: time_search - no time map for ctx program chain\n");
#endif
    goto timemapdone;
  }

  if (ctx->vm->vtsi->vts_tmapt->nr_of_tmaps < state->pgcN){
  /* to few time maps for ctx program chain */
#ifdef LOG_DEBUG
    DVD_DEBUG(DBG_CRIT, "libdvdnav: time_search - to few time maps for ctx program chain\n");
#endif
    goto timemapdone;
  }

  /* get the tmpat corresponding to the pgc */
  vts_tmap_t *tmap = &(ctx->vm->vtsi->vts_tmapt->tmap[state->pgcN - 1]);
  if (tmap->tmu == 0){
  /* no time unit for ctx time map */
#ifdef LOG_DEBUG
    DVD_DEBUG(DBG_CRIT, "libdvdnav: time_search - no time unit for ctx time map\n");
#endif
    goto timemapdone;
  }

  /* time is in pts (90khz clock), get the number of tmu's that represent */
  /* first entry defines at time tmu not time zero */
  int entry = time / tmap->tmu / 90000 - 1;
  if (entry > tmap->nr_of_entries)
  entry = tmap->nr_of_entries - 1;

  if (entry > 0)
  {
    /* get the table entry, disregarding marking of discontinuity */
    target = tmap->map_ent[entry] & 0x7fffffff;
  }
  else
  {
    /* start from first vobunit */
    target = state->pgc->cell_playback[first_cell_nr - 1].first_sector;;
  }

  /* if we have an additional entry we can interpolate next position */
  /* allowed only if next entry isn't discontinious */

  if (entry < tmap->nr_of_entries - 1)
  {
    const uint32_t target2 = tmap->map_ent[entry + 1];
    const uint64_t timeunit = tmap->tmu * 90000;
    if (!(target2 & 0x80000000))
    {
      length = target2 - target;
      target += (uint32_t)(length * (time - (entry + 1)*timeunit) / timeunit);
    }
  }
  found = 1;

timemapdone:

  found = 0;
  for(cell_nr = first_cell_nr; (cell_nr <= last_cell_nr); cell_nr ++) {
    cell =  &(state->pgc->cell_playback[cell_nr-1]);
    if(cell->block_type == BLOCK_TYPE_ANGLE_BLOCK && cell->block_mode != BLOCK_MODE_FIRST_CELL)
      continue;

    if (found) {
      if (target >= cell->first_sector
      &&  target <= cell->last_sector)
        break;
    } else {
    length = dvdnav_convert_time(&cell->playback_time);
    if (time >= length) {
      time -= length;
    } else {
      /* FIXME: there must be a better way than interpolation */
      target = time * (cell->last_sector - cell->first_sector + 1) / length;
      target += cell->first_sector;

#ifdef LOG_DEBUG
      if (cell->first_sector > target || target > cell->last_sector)
      DVD_DEBUG(DBG_CRIT, "libdvdnav: time_search - sector is not within cell min:%u, max:%u, cur:%u\n", cell->first_sector, cell->last_sector, target);
#endif
      found = 1;
      break;
    }
  }
  }
  if(found) {
    uint32_t vobu;
#ifdef LOG_DEBUG
    DVD_DEBUG(DBG_CRIT, "libdvdnav: Seeking to cell %i from choice of %i to %i\n",
            cell_nr, first_cell_nr, last_cell_nr);
#endif
    if (dvdnav_scan_admap(ctx, state->domain, target, 0, &vobu) == DVDNAV_STATUS_OK) {
      uint32_t start = state->pgc->cell_playback[cell_nr-1].first_sector;

      if (vm_jump_cell_block(ctx->vm, cell_nr, vobu - start)) {
#ifdef LOG_DEBUG
        DVD_DEBUG(DBG_CRIT, "libdvdnav: After cellN=%u blockN=%u target=%x vobu=%x start=%x\n" ,
          state->cellN, state->blockN, target, vobu, start);
#endif
        ctx->vm->hop_channel += HOP_SEEK;
        pthread_mutex_unlock(&ctx->vm_lock);
        return DVDNAV_STATUS_OK;
      }
    }
  }

  DVD_DEBUG(DBG_CRIT, "libdvdnav: Error when seeking\n");
  DVD_DEBUG(DBG_CRIT, "Error when seeking.");
  pthread_mutex_unlock(&ctx->vm_lock);
  return DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_sector_search(dvdnav_t *ctx,
                                     int64_t offset, int32_t origin) {
  uint32_t target = 0;
  uint32_t current_pos;
  uint32_t cur_sector;
  uint32_t cur_cell_nr;
  uint32_t length = 0;
  uint32_t first_cell_nr, last_cell_nr, cell_nr;
  int32_t found;
  int forward = 0;
  cell_playback_t *cell;
  dvd_state_t *state;
  dvdnav_status_t result;

  if(ctx->position_current.still != 0) {
    DVD_DEBUG(DBG_CRIT, "Cannot seek in a still frame.");
    return DVDNAV_STATUS_ERR;
  }

  result = dvdnav_get_position(ctx, &target, &length);
  if(!result) {
    DVD_DEBUG(DBG_CRIT, "Cannot get current position");
    return DVDNAV_STATUS_ERR;
  }

  pthread_mutex_lock(&ctx->vm_lock);
  state = &(ctx->vm->state);
  if(!state->pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
#ifdef LOG_DEBUG
  DVD_DEBUG(DBG_CRIT, "libdvdnav: seeking to offset=%lld pos=%u length=%u\n", offset, target, length);
  DVD_DEBUG(DBG_CRIT, "libdvdnav: Before cellN=%u blockN=%u\n", state->cellN, state->blockN);
#endif

  current_pos = target;
  cur_sector = ctx->vobu.vobu_start + ctx->vobu.blockN;
  cur_cell_nr = state->cellN;

  switch(origin) {
   case SEEK_SET:
    if(offset >= length) {
      DVD_DEBUG(DBG_CRIT, "Request to seek behind end.");
      pthread_mutex_unlock(&ctx->vm_lock);
      return DVDNAV_STATUS_ERR;
    }
    target = offset;
    break;
   case SEEK_CUR:
    if((signed)target + offset >= length) {
      DVD_DEBUG(DBG_CRIT, "Request to seek behind end.");
      pthread_mutex_unlock(&ctx->vm_lock);
      return DVDNAV_STATUS_ERR;
    }
    if((signed)target + offset < 0) {
      DVD_DEBUG(DBG_CRIT, "Request to seek before start.");
      pthread_mutex_unlock(&ctx->vm_lock);
      return DVDNAV_STATUS_ERR;
    }
    target += offset;
    break;
   case SEEK_END:
    if(length < offset) {
      DVD_DEBUG(DBG_CRIT, "Request to seek before start.");
      pthread_mutex_unlock(&ctx->vm_lock);
      return DVDNAV_STATUS_ERR;
    }
    target = length - offset;
    break;
   default:
    /* Error occured */
    DVD_DEBUG(DBG_CRIT, "Illegal seek mode.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  forward = target > current_pos;

  ctx->cur_cell_time = 0;
  if (ctx->pgc_based) {
    first_cell_nr = 1;
    last_cell_nr = state->pgc->nr_of_cells;
  } else {
    /* Find start cell of program. */
    first_cell_nr = state->pgc->program_map[state->pgN-1];
    /* Find end cell of program */
    if(state->pgN < state->pgc->nr_of_programs)
      last_cell_nr = state->pgc->program_map[state->pgN] - 1;
    else
      last_cell_nr = state->pgc->nr_of_cells;
  }

  found = 0;
  for(cell_nr = first_cell_nr; (cell_nr <= last_cell_nr) && !found; cell_nr ++) {
    cell =  &(state->pgc->cell_playback[cell_nr-1]);
    if(cell->block_type == BLOCK_TYPE_ANGLE_BLOCK && cell->block_mode != BLOCK_MODE_FIRST_CELL)
      continue;
    length = cell->last_sector - cell->first_sector + 1;
    if (target >= length) {
      target -= length;
    } else {
      /* convert the target sector from Cell-relative to absolute physical sector */
      target += cell->first_sector;
      if (forward && (cell_nr == cur_cell_nr)) {
        uint32_t vobu;
        /* if we are seeking forward from the current position, make sure
         * we move to a new position that is after our current position.
         * simply truncating to the vobu will go backwards */
        if (dvdnav_scan_admap(ctx, state->domain, target, 0, &vobu) != DVDNAV_STATUS_OK)
          break;
        if (vobu <= cur_sector) {
          if (dvdnav_scan_admap(ctx, state->domain, target, 1, &vobu) != DVDNAV_STATUS_OK)
            break;
          if (vobu > cell->last_sector) {
            if (cell_nr == last_cell_nr)
              break;
            cell_nr++;
            cell =  &(state->pgc->cell_playback[cell_nr-1]);
            target = cell->first_sector;
          } else {
            target = vobu;
          }
        }
      }
      found = 1;
      break;
    }
  }

  if(found) {
    uint32_t vobu;
#ifdef LOG_DEBUG
    DVD_DEBUG(DBG_CRIT, "libdvdnav: Seeking to cell %i from choice of %i to %i\n",
            cell_nr, first_cell_nr, last_cell_nr);
#endif
    if (dvdnav_scan_admap(ctx, state->domain, target, 0, &vobu) == DVDNAV_STATUS_OK) {
      int32_t start = state->pgc->cell_playback[cell_nr-1].first_sector;

      if (vm_jump_cell_block(ctx->vm, cell_nr, vobu - start)) {
#ifdef LOG_DEBUG
        DVD_DEBUG(DBG_CRIT, "libdvdnav: After cellN=%u blockN=%u target=%x vobu=%x start=%x\n" ,
          state->cellN, state->blockN, target, vobu, start);
#endif
        ctx->vm->hop_channel += HOP_SEEK;
        pthread_mutex_unlock(&ctx->vm_lock);
        return DVDNAV_STATUS_OK;
      }
    }
  }

  DVD_DEBUG(DBG_CRIT, "libdvdnav: Error when seeking\n");
  DVD_DEBUG(DBG_CRIT, "libdvdnav: FIXME: Implement seeking to location %u\n", target);
  DVD_DEBUG(DBG_CRIT, "Error when seeking.");
  pthread_mutex_unlock(&ctx->vm_lock);
  return DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_part_search(dvdnav_t *ctx, int32_t part) {
  int32_t title, old_part;

  if (dvdnav_current_title_info(ctx, &title, &old_part) == DVDNAV_STATUS_OK)
    return dvdnav_part_play(ctx, title, part);
  return DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_prev_pg_search(dvdnav_t *ctx) {
  pthread_mutex_lock(&ctx->vm_lock);
  if(!ctx->vm->state.pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }

#ifdef LOG_DEBUG
  DVD_DEBUG(DBG_CRIT, "libdvdnav: previous chapter\n");
#endif
  if (!vm_jump_prev_pg(ctx->vm)) {
    DVD_DEBUG(DBG_CRIT, "libdvdnav: previous chapter failed.\n");
    DVD_DEBUG(DBG_CRIT, "Skip to previous chapter failed.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  ctx->cur_cell_time = 0;
  ctx->position_current.still = 0;
  ctx->vm->hop_channel++;
#ifdef LOG_DEBUG
  DVD_DEBUG(DBG_CRIT, "libdvdnav: previous chapter done\n");
#endif
  pthread_mutex_unlock(&ctx->vm_lock);

  return DVDNAV_STATUS_OK;
}

dvdnav_status_t dvdnav_top_pg_search(dvdnav_t *ctx) {
  pthread_mutex_lock(&ctx->vm_lock);
  if(!ctx->vm->state.pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }

#ifdef LOG_DEBUG
  DVD_DEBUG(DBG_CRIT, "libdvdnav: top chapter\n");
#endif
  if (!vm_jump_top_pg(ctx->vm)) {
    DVD_DEBUG(DBG_CRIT, "libdvdnav: top chapter failed.\n");
    DVD_DEBUG(DBG_CRIT, "Skip to top chapter failed.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  ctx->cur_cell_time = 0;
  ctx->position_current.still = 0;
  ctx->vm->hop_channel++;
#ifdef LOG_DEBUG
  DVD_DEBUG(DBG_CRIT, "libdvdnav: top chapter done\n");
#endif
  pthread_mutex_unlock(&ctx->vm_lock);

  return DVDNAV_STATUS_OK;
}

dvdnav_status_t dvdnav_next_pg_search(dvdnav_t *ctx) {
  vm_t *try_vm;

  pthread_mutex_lock(&ctx->vm_lock);
  if(!ctx->vm->state.pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    goto fail;
  }

#ifdef LOG_DEBUG
  DVD_DEBUG(DBG_CRIT, "libdvdnav: next chapter\n");
#endif
  /* make a copy of current VM and try to navigate the copy to the next PG */
  try_vm = vm_new_copy(ctx->vm);
  if (try_vm == NULL) {
    DVD_DEBUG(DBG_CRIT, "Unable to copy the VM.");
    goto fail;
  }

  if (!vm_jump_next_pg(try_vm) || try_vm->stopped) {
    vm_free_copy(try_vm);
    /* next_pg failed, try to jump at least to the next cell */
    try_vm = vm_new_copy(ctx->vm);
    if (try_vm == NULL) {
      DVD_DEBUG(DBG_CRIT, "Unable to copy the VM.");
      goto fail;
    }
    vm_get_next_cell(try_vm);
    if (try_vm->stopped) {
      vm_free_copy(try_vm);
      DVD_DEBUG(DBG_CRIT, "libdvdnav: next chapter failed.\n");
      DVD_DEBUG(DBG_CRIT, "Skip to next chapter failed.");
      goto fail;
    }
  }
  ctx->cur_cell_time = 0;
  /* merge changes on success */
  vm_merge(ctx->vm, try_vm);
  vm_free_copy(try_vm);
  ctx->position_current.still = 0;
  ctx->vm->hop_channel++;
#ifdef LOG_DEBUG
  DVD_DEBUG(DBG_CRIT, "libdvdnav: next chapter done\n");
#endif
  pthread_mutex_unlock(&ctx->vm_lock);

  return DVDNAV_STATUS_OK;

fail:
  pthread_mutex_unlock(&ctx->vm_lock);
  return DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_menu_call(dvdnav_t *ctx, DVDMenuID_t menu) {
  vm_t *try_vm;

  pthread_mutex_lock(&ctx->vm_lock);
  if(!ctx->vm->state.pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    goto fail;
  }

  ctx->cur_cell_time = 0;
  /* make a copy of current VM and try to navigate the copy to the menu */
  try_vm = vm_new_copy(ctx->vm);
  if (try_vm == NULL) {
    DVD_DEBUG(DBG_CRIT, "Unable to copy VM.");
    goto fail;
  }

  if ( (menu == DVD_MENU_Escape) && (ctx->vm->state.domain != DVD_DOMAIN_VTSTitle)) {
    /* Try resume */
    if (vm_jump_resume(try_vm) && !try_vm->stopped) {
        /* merge changes on success */
        vm_merge(ctx->vm, try_vm);
        vm_free_copy(try_vm);
        ctx->position_current.still = 0;
        ctx->vm->hop_channel++;
        pthread_mutex_unlock(&ctx->vm_lock);
        return DVDNAV_STATUS_OK;
    }
  }
  if (menu == DVD_MENU_Escape) menu = DVD_MENU_Root;

  if (vm_jump_menu(try_vm, menu) && !try_vm->stopped) {
    /* merge changes on success */
    vm_merge(ctx->vm, try_vm);
    vm_free_copy(try_vm);
    ctx->position_current.still = 0;
    ctx->vm->hop_channel++;
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_OK;
  } else {
    vm_free_copy(try_vm);
    DVD_DEBUG(DBG_CRIT, "No such menu or menu not reachable.");
    goto fail;
  }

fail:
  pthread_mutex_unlock(&ctx->vm_lock);
  return DVDNAV_STATUS_ERR;
}

dvdnav_status_t dvdnav_get_position(dvdnav_t *ctx, uint32_t *pos,
                                    uint32_t *len) {
  uint32_t cur_sector;
  int32_t cell_nr, first_cell_nr, last_cell_nr;
  cell_playback_t *cell;
  dvd_state_t *state;

  if(!ctx->started) {
    DVD_DEBUG(DBG_CRIT, "Virtual DVD machine not started.");
    return DVDNAV_STATUS_ERR;
  }

  pthread_mutex_lock(&ctx->vm_lock);
  state = &(ctx->vm->state);
  if(!state->pgc || ctx->vm->stopped) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }
  if (ctx->position_current.hop_channel  != ctx->vm->hop_channel ||
      ctx->position_current.domain       != state->domain         ||
      ctx->position_current.vts          != state->vtsN           ||
      ctx->position_current.cell_restart != state->cell_restart) {
    DVD_DEBUG(DBG_CRIT, "New position not yet determined.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }

  /* Get current sector */
  cur_sector = ctx->vobu.vobu_start + ctx->vobu.blockN;

  if (ctx->pgc_based) {
    first_cell_nr = 1;
    last_cell_nr = state->pgc->nr_of_cells;
  } else {
    /* Find start cell of program. */
    first_cell_nr = state->pgc->program_map[state->pgN-1];
    /* Find end cell of program */
    if(state->pgN < state->pgc->nr_of_programs)
      last_cell_nr = state->pgc->program_map[state->pgN] - 1;
    else
      last_cell_nr = state->pgc->nr_of_cells;
  }

  *pos = -1;
  *len = 0;
  for (cell_nr = first_cell_nr; cell_nr <= last_cell_nr; cell_nr++) {
    cell = &(state->pgc->cell_playback[cell_nr-1]);
    if(cell->block_type == BLOCK_TYPE_ANGLE_BLOCK && cell->block_mode != BLOCK_MODE_FIRST_CELL)
        continue;
    if (cell_nr == state->cellN) {
      /* the current sector is in ctx cell,
       * pos is length of PG up to here + sector's offset in ctx cell */
      *pos = *len + cur_sector - cell->first_sector;
    }
    *len += cell->last_sector - cell->first_sector + 1;
  }

  pthread_mutex_unlock(&ctx->vm_lock);

  if((signed)*pos == -1)
    return DVDNAV_STATUS_ERR;

  return DVDNAV_STATUS_OK;
}

dvdnav_status_t dvdnav_get_position_in_title(dvdnav_t *ctx,
                                             uint32_t *pos,
                                             uint32_t *len) {
  uint32_t cur_sector;
  uint32_t first_cell_nr;
  uint32_t last_cell_nr;
  cell_playback_t *first_cell;
  cell_playback_t *last_cell;
  dvd_state_t *state;

  state = &(ctx->vm->state);
  if(!state->pgc) {
    DVD_DEBUG(DBG_CRIT, "No current PGC.");
    return DVDNAV_STATUS_ERR;
  }

  if (state->pgc->program_map == NULL) {
    DVD_DEBUG(DBG_CRIT, "Program map missing.");
    return DVDNAV_STATUS_ERR;
  }

  /* Get current sector */
  cur_sector = ctx->vobu.vobu_start + ctx->vobu.blockN;

  /* Now find first and last cells in title. */
  first_cell_nr = state->pgc->program_map[0];
  first_cell = &(state->pgc->cell_playback[first_cell_nr-1]);
  last_cell_nr = state->pgc->nr_of_cells;
  last_cell = &(state->pgc->cell_playback[last_cell_nr-1]);

  *pos = cur_sector - first_cell->first_sector;
  *len = last_cell->last_sector - first_cell->first_sector;

  return DVDNAV_STATUS_OK;
}

uint32_t dvdnav_describe_title_chapters(dvdnav_t *ctx, int32_t title, uint64_t **times, uint64_t *duration) {
  int32_t retval=0;
  uint16_t parts, i;
  title_info_t *ptitle = NULL;
  ptt_info_t *ptt = NULL;
  ifo_handle_t *ifo = NULL;
  pgc_t *pgc;
  cell_playback_t *cell;
  uint64_t length, *tmp=NULL;

  *times = NULL;
  *duration = 0;
  pthread_mutex_lock(&ctx->vm_lock);
  if(!ctx->vm->vmgi) {
    DVD_DEBUG(DBG_CRIT, "Bad VM state or missing VTSI.");
    goto fail;
  }
  if(!ctx->started) {
    /* don't report an error but be nice */
    vm_start(ctx->vm);
    ctx->started = 1;
  }
  ifo = vm_get_title_ifo(ctx->vm, title);
  if(!ifo || !ifo->vts_pgcit) {
    DVD_DEBUG(DBG_CRIT, "Couldn't open IFO for chosen title, exit.");
    retval = 0;
    goto fail;
  }

  ptitle = &ctx->vm->vmgi->tt_srpt->title[title-1];
  parts = ptitle->nr_of_ptts;
  ptt = ifo->vts_ptt_srpt->title[ptitle->vts_ttn-1].ptt;

  tmp = calloc(1, sizeof(uint64_t)*parts);
  if(!tmp)
    goto fail;

  if(!ptt) {
      DVD_DEBUG(DBG_CRIT, "ptt NULL");
      goto fail;
  }

  length = 0;
  for(i=0; i<parts; i++) {
    uint32_t cellnr, endcellnr;
    if (ptt[i].pgcn == 0 || ptt[i].pgcn > ifo->vts_pgcit->nr_of_pgci_srp) {
      DVD_DEBUG(DBG_CRIT, "PGCN out of bounds.");
      continue;
    }
    if (ifo->vts_pgcit->pgci_srp[ptt[i].pgcn-1].pgc_start_byte >= ifo->vts_pgcit->last_byte) {
      DVD_DEBUG(DBG_CRIT, "PGC start out of bounds");
      continue;
    }
    if (0 == ifo->vts_pgcit->pgci_srp[ptt[i].pgcn-1].pgc_start_byte) {
      DVD_DEBUG(DBG_CRIT, "PGC start zero.");
      continue;
    }
    if (0 != (ifo->vts_pgcit->pgci_srp[ptt[i].pgcn-1].pgc_start_byte & 1)) {
      DVD_DEBUG(DBG_CRIT, "PGC start unaligned.");
      continue;
    }
    if (0 != ((uintptr_t)(ifo->vts_pgcit->pgci_srp[ptt[i].pgcn-1].pgc) & 1)) {
      DVD_DEBUG(DBG_CRIT, "PGC pointer unaligned.");
      continue;
    }
    pgc = ifo->vts_pgcit->pgci_srp[ptt[i].pgcn-1].pgc;
    if (pgc == NULL) {
      DVD_DEBUG(DBG_CRIT, "PGC missing.");
      continue;
    }
    if (pgc->program_map == NULL) {
      DVD_DEBUG(DBG_CRIT, "Program map missing.");
      continue;
    }
    if(ptt[i].pgn == 0 || ptt[i].pgn > pgc->nr_of_programs) {
      DVD_DEBUG(DBG_CRIT, "WRONG part number.");
      goto fail;
    }

    if (pgc->nr_of_cells == 0) {
      DVD_DEBUG(DBG_CRIT, "Number of cells cannot be 0");
      continue;
    }
    if ((cellnr = pgc->program_map[ptt[i].pgn-1]) == 0) {
      DVD_DEBUG(DBG_CRIT, "Cell new row cannot be 0");
      continue;
    }
    if (pgc->cell_playback == NULL) {
      DVD_DEBUG(DBG_CRIT, "Cell missing");
      continue;
    }

    if(ptt[i].pgn < pgc->nr_of_programs)
      endcellnr = pgc->program_map[ptt[i].pgn];
    else
      endcellnr = 0;

    do {
      cell = &pgc->cell_playback[cellnr-1];
      if(!(cell->block_type == BLOCK_TYPE_ANGLE_BLOCK &&
           cell->block_mode != BLOCK_MODE_FIRST_CELL
      ))
      {
        tmp[i] = length + dvdnav_convert_time(&cell->playback_time);
        length = tmp[i];
      }
      cellnr++;
    } while(cellnr < endcellnr);
  }
  *duration = length;
  vm_ifo_close(ifo);
  ifo = NULL;
  retval = parts;
  *times = tmp;

fail:
  pthread_mutex_unlock(&ctx->vm_lock);
  if(!retval && ifo)
    vm_ifo_close(ifo);
  if(!retval && tmp)
    free(tmp);
  return retval;
}

dvdnav_status_t dvdnav_get_state(dvdnav_t *ctx, dvd_state_t *save_state)
{
  if (!ctx || !ctx->vm) return DVDNAV_STATUS_ERR;

  pthread_mutex_lock(&ctx->vm_lock);

  if (!vm_get_state(ctx->vm, save_state))
  {
    DVD_DEBUG(DBG_CRIT, "Failed to get vm state.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }

  pthread_mutex_unlock(&ctx->vm_lock);
  return DVDNAV_STATUS_OK;
}

dvdnav_status_t dvdnav_set_state(dvdnav_t *ctx, dvd_state_t *save_state)
{
  if (!ctx || !ctx->vm)
  {
    DVD_DEBUG(DBG_CRIT, "Passed a NULL pointer.");
    return DVDNAV_STATUS_ERR;
  }

  if (!ctx->started) {
    DVD_DEBUG(DBG_CRIT, "Virtual DVD machine not started.");
    return DVDNAV_STATUS_ERR;
  }

  pthread_mutex_lock(&ctx->vm_lock);

  /* reset the dvdnav state */
  memset(&ctx->pci, 0, sizeof(ctx->pci));
  memset(&ctx->dsi, 0, sizeof(ctx->dsi));
  ctx->last_cmd_nav_lbn = SRI_END_OF_CELL;

  /* Set initial values of flags */
  ctx->position_current.still = 0;
  ctx->skip_still = 0;
  ctx->sync_wait = 0;
  ctx->sync_wait_skip = 0;
  ctx->spu_clut_changed = 0;

  /* set the state. ctx will also start the vm on that state */
  /* means the next read block should be comming from that new */
  /* state */
  if (!vm_set_state(ctx->vm, save_state))
  {
    DVD_DEBUG(DBG_CRIT, "Failed to set vm state.");
    pthread_mutex_unlock(&ctx->vm_lock);
    return DVDNAV_STATUS_ERR;
  }

  pthread_mutex_unlock(&ctx->vm_lock);
  return DVDNAV_STATUS_OK;
}

/* Get an admap and admap_len */
static vobu_admap_t* dvdnav_admap_get(dvdnav_t *ctx, dvd_state_t *state,
            int32_t *admap_len) {
  vobu_admap_t *admap = NULL;
  switch(state->domain) {
  case DVD_DOMAIN_FirstPlay:
  case DVD_DOMAIN_VMGM:
    admap = ctx->vm->vmgi->menu_vobu_admap;
    break;
  case DVD_DOMAIN_VTSMenu:
    admap = ctx->vm->vtsi->menu_vobu_admap;
    break;
  case DVD_DOMAIN_VTSTitle:
    admap = ctx->vm->vtsi->vts_vobu_admap;
    break;
  default: {
    DVD_DEBUG(DBG_CRIT, "Unknown domain");
    return NULL;
  }
  }
  if (admap == NULL) return NULL;

  *admap_len = (admap->last_byte + 1 - VOBU_ADMAP_SIZE) / VOBU_ADMAP_SIZE;
  if (*admap_len <= 0) {
    DVD_DEBUG(DBG_CRIT, "admap_len <= 0");
    return NULL;
  }
  return admap;
}

/* Get a tmap, tmap_len and tmap_interval */
static vts_tmap_t* dvdnav_tmap_get(dvdnav_t *ctx, dvd_state_t *state,
            int32_t *tmap_len, int32_t *tmap_interval) {
  int32_t domain;
  ifo_handle_t *ifo = NULL;
  vts_tmapt_t *tmapt = NULL;
  uint16_t tmap_count = 0;
  int32_t pgcN = 0;
  vts_tmap_t *tmap = NULL;
  int32_t result = 0;

  domain = state->domain;
  switch(domain) {
  case DVD_DOMAIN_FirstPlay:
  case DVD_DOMAIN_VTSMenu:
  case DVD_DOMAIN_VMGM: {
    ifo = ctx->vm->vmgi;
    break;
  }
  case DVD_DOMAIN_VTSTitle: {
    ifo = ctx->vm->vtsi;
    break;
  }
  default: {
    DVD_DEBUG(DBG_CRIT, "unknown domain for tmap");
    return NULL;
  }
  }
  if (ifo == NULL) return NULL;
  tmapt = ifo->vts_tmapt;
  /* HACK: ifo->vts_tmapt is NULL b/c ifo_read.c never loads it
   * load ifo->vts_tmapt directly*/
  if (tmapt == NULL) {
    result = ifoRead_VTS_TMAPT(ifo);
    if (!result) {
      return NULL;
    }
    tmapt = ifo->vts_tmapt;
    if (tmapt == NULL) return NULL;
  }

  tmap_count = tmapt->nr_of_tmaps;
  pgcN = state->pgcN - 1; /* -1 b/c pgcN is base1 */
  if (pgcN < 0) {
    DVD_DEBUG(DBG_CRIT, "pgcN < 0");
    return NULL;
  }

  /* get tmap */
  switch(domain) {
  case DVD_DOMAIN_FirstPlay:
  case DVD_DOMAIN_VMGM:
  case DVD_DOMAIN_VTSMenu: {
    if (tmap_count == 0) {
      DVD_DEBUG(DBG_CRIT, "tmap_count == 0");
      return NULL;
    }
    tmap = &tmapt->tmap[0]; /* ASSUME: vmgi only has one time map */
    break;
  }
  case DVD_DOMAIN_VTSTitle: {
    if (pgcN >= tmap_count) {
      DVD_DEBUG(DBG_CRIT, "pgcN >= tmap_count; pgcN=%i tmap_count=%i",
          pgcN, tmap_count);
      return NULL;
    }
    tmap = &tmapt->tmap[pgcN];
    break;
  }
  }
  if (tmap == NULL) return NULL;

  /* tmap->tmu is in seconds; convert to millisecs */
  *tmap_interval = tmap->tmu * 1000;
  if (*tmap_interval == 0) {
    DVD_DEBUG(DBG_CRIT, "tmap_interval == 0");
    return NULL;
  }
  *tmap_len = tmap->nr_of_entries;
  if (*tmap_len == 0) {
    DVD_DEBUG(DBG_CRIT, "tmap_len == 0");
    return NULL;
  }
  return tmap;
}

/* Get a sector from a tmap */
static int32_t dvdnav_tmap_get_entry(vts_tmap_t *tmap, uint16_t tmap_len,
            int32_t idx, uint32_t *sector) {
  /* tmaps start at idx 0 which represents a sector at time 1 * tmap_interval
   * ctx creates a "fake" tmap index at idx -1 for sector 0 */
  if (idx == TMAP_IDX_EDGE_BGN) {
    *sector = 0;
    return 1;
  }
  if (idx < TMAP_IDX_EDGE_BGN || idx >= tmap_len) {
    DVD_DEBUG(DBG_CRIT, "idx out of bounds idx=%i %i", idx, tmap_len);
    return 0;
  }
  /* 0x7fffffff unsets discontinuity bit if present */
  *sector = tmap->map_ent[idx] & 0x7fffffff;
  return 1;
}

/* Do a binary search for earlier admap index near find_sector */
static int32_t dvdnav_admap_search(vobu_admap_t *admap, uint32_t admap_len,
            uint32_t find_sector, uint32_t *vobu) {
  int32_t adj = 1;
  int32_t prv_pos = 0;
  int32_t prv_len = admap_len;
  int32_t cur_len = 0;
  int32_t cur_idx = 0;
  uint32_t cur_sector = 0;
  while (1) {
    cur_len = prv_len / 2;
    /* need to add 1 when prv_len == 3 (cur_len shoud go to 2, not 1) */
    if (prv_len % 2 == 1) ++cur_len;
    cur_idx = prv_pos + (cur_len * adj);
    if       (cur_idx < 0)
      cur_idx = 0;
    else if  (cur_idx >= (int32_t)admap_len)
      cur_idx = admap_len - 1;

    cur_sector = admap->vobu_start_sectors[cur_idx];
    if      (find_sector <  cur_sector) adj = -1;
    else if (find_sector >  cur_sector) adj =  1;
    else if (find_sector == cur_sector) {
      *vobu = cur_idx;
      return 1;
    }
    if (cur_len == 1) {/* no smaller intervals left */
      if (adj == -1) {/* last comparison was greater; take lesser */
          cur_idx -= 1;
          cur_sector = admap->vobu_start_sectors[cur_idx];
      }
      *vobu = cur_idx;
      return 1;
    }
    prv_len = cur_len;
    prv_pos = cur_idx;
  }
}

/* Do a binary search for the earlier tmap entry near find_sector */
static int32_t dvdnav_tmap_search(vts_tmap_t *tmap, uint32_t tmap_len,
            uint32_t find_sector, int32_t *tmap_idx, uint32_t *sector) {
  int32_t adj = 1;
  int32_t prv_pos = 0;
  int32_t prv_len = tmap_len;
  int32_t result = 0;
  int32_t cur_len = 0;
  int32_t cur_idx = 0;
  uint32_t cur_sector = 0;
  while (1) {
    cur_len = prv_len / 2;
    /* need to add 1 when prv_len == 3 (cur_len shoud go to 2, not 1) */
    if (prv_len % 2 == 1) ++cur_len;
    cur_idx = prv_pos + (cur_len * adj);
    if      (cur_idx < 0)
      cur_idx = 0;
    else if (cur_idx >= (int32_t)tmap_len)
      cur_idx = tmap_len - 1;
    cur_sector = 0;
    result = dvdnav_tmap_get_entry(tmap, tmap_len, cur_idx, &cur_sector);
    if (!result) return 0;
    if      (find_sector <  cur_sector) adj = -1;
    else if (find_sector >  cur_sector) adj =  1;
    else if (find_sector == cur_sector) {
      *tmap_idx = cur_idx;
      *sector = cur_sector;
      return 1;
    }
    if (cur_len == 1) {/* no smaller intervals left */
      if (adj == -1) {/* last comparison was greater; take lesser */
        if (cur_idx == 0) { /* fake tmap index for sector 0 */
          cur_idx = TMAP_IDX_EDGE_BGN;
          cur_sector = 0;
        }
        else {
          cur_idx -= 1;
          result = dvdnav_tmap_get_entry(tmap, tmap_len, cur_idx, &cur_sector);
          if (!result) return 0;
        }
      }
      *tmap_idx = cur_idx;
      *sector = cur_sector;
      return 1;
    }
    prv_len = cur_len;
    prv_pos = cur_idx;
  }
}

/* Find the cell for a given time */
static int32_t dvdnav_cell_find(dvdnav_t *ctx, dvd_state_t *state,
            uint64_t find_val, dvdnav_cell_data_t *cell_data) {
  uint32_t cells_len = 0;
  uint32_t cells_bgn = 0;
  uint32_t cells_end = 0;
  uint32_t cell_idx = 0;
  pgc_t *pgc = NULL;
  int pgN = 0;
  cell_playback_t *cell = NULL;
  int found = 0;

  pgc = state->pgc;
  if (pgc == NULL) return 0;
  cells_len = pgc->nr_of_cells;
  if (cells_len == 0) {
    DVD_DEBUG(DBG_CRIT, "cells_len == 0");
    return 0;
  }

  /* get cells_bgn, cells_end */
  if (ctx->pgc_based) {
    cells_bgn = 1;
    cells_end = cells_len;
  }
  else {
    pgN = state->pgN;
    cells_bgn = pgc->program_map[pgN - 1]; /* -1 b/c pgN is 1 based? */
    if (pgN < pgc->nr_of_programs) {
      cells_end = pgc->program_map[pgN] - 1;
    }
    else {
      cells_end = cells_len;
    }
  }

  /* search cells */
  for (cell_idx = cells_bgn; cell_idx <= cells_end; cell_idx++) {
    cell = &(pgc->cell_playback[cell_idx - 1]); /* -1 b/c cell is base1 */
    /* if angle block, only consider first angleBlock
     * (others are "redundant" for purpose of search) */
    if ( cell->block_type == BLOCK_TYPE_ANGLE_BLOCK
      && cell->block_mode != BLOCK_MODE_FIRST_CELL) {
      continue;
    }
    cell_data->bgn->sector = cell->first_sector;
    cell_data->end->sector = cell->last_sector;

    /* 90 pts to ms */
    cell_data->end->time += (dvdnav_convert_time(&cell->playback_time) / 90);
    if (  find_val >= cell_data->bgn->time
       && find_val <= cell_data->end->time) {
      found = 1;
      break;
    }
    cell_data->bgn->time = cell_data->end->time;
  }

  /* found cell: set var */
  if (found) {
    cell_data->idx = cell_idx;
  }
  else
    DVD_DEBUG(DBG_CRIT, "cell not found; find=%"PRId64"", find_val);
  return found;
}

/* Given two sectors and a fraction, calc the corresponding vobu */
static int32_t dvdnav_admap_interpolate_vobu(dvdnav_jump_args_t *args,
            dvdnav_pos_data_t *bgn, dvdnav_pos_data_t *end, uint32_t fraction,
            uint32_t *jump_sector) {
  int32_t result = 0;
  uint32_t vobu_len = 0;
  uint32_t vobu_adj = 0;
  uint32_t vobu_idx = 0;

  /* get bgn->vobu_idx */
  result = dvdnav_admap_search(args->admap, args->admap_len,
      bgn->sector, &bgn->vobu_idx);
  if (!result) {
    DVD_DEBUG(DBG_CRIT, "admap_interpolate: could not find sector_bgn");
    return 0;
  }

  /* get end->vobu_idx */
  result = dvdnav_admap_search(args->admap, args->admap_len,
      end->sector, &end->vobu_idx);
  if (!result) {
    DVD_DEBUG(DBG_CRIT, "admap_interpolate: could not find sector_end");
    return 0;
  }

  vobu_len = end->vobu_idx - bgn->vobu_idx;
  /* +500 to round up else 74% of a 4 sec interval = 2 sec */
  vobu_adj = ((fraction * vobu_len) + 500) / 1000;
  /* HACK: need to add +1, or else will land too soon (not sure why) */
  vobu_adj++;
  vobu_idx = bgn->vobu_idx + vobu_adj;
  if ((int32_t)vobu_idx >= args->admap_len) {
    DVD_DEBUG(DBG_CRIT, "admap_interpolate: vobu_idx >= admap_len");
    return 0;
  }
  *jump_sector = args->admap->vobu_start_sectors[vobu_idx];
  return 1;
}

/* Given two tmap entries and a time, calc the time for the lo tmap entry */
static int32_t dvdnav_tmap_calc_time_for_tmap_entry(dvdnav_jump_args_t *args,
            dvdnav_pos_data_t *lo, dvdnav_pos_data_t *hi,
            dvdnav_pos_data_t *pos, uint64_t *out_time) {
  int32_t result = 0;
  int32_t vobu_pct = 0;
  uint64_t time_adj = 0;

  if (lo->sector == hi->sector) {
    DVD_DEBUG(DBG_CRIT, "lo->sector == hi->sector: %i", lo->sector);
    return 0;
  }

  /* get vobus corresponding to lo, hi, pos */
  result = dvdnav_admap_search(args->admap, args->admap_len,
      lo->sector, &lo->vobu_idx);
  if (!result) {
    DVD_DEBUG(DBG_CRIT, "lo->vobu: lo->sector=%i", lo->sector);
    return 0;
  }
  result = dvdnav_admap_search(args->admap, args->admap_len,
      hi->sector, &hi->vobu_idx);
  if (!result) {
    DVD_DEBUG(DBG_CRIT, "hi->vobu: hi->sector=%i", hi->sector);
    return 0;
  }
  result = dvdnav_admap_search(args->admap, args->admap_len,
      pos->sector, &pos->vobu_idx);
  if (!result) {
    DVD_DEBUG(DBG_CRIT, "pos->vobu: pos->sector=%i", pos->sector);
    return 0;
  }

  /* calc position of cell relative to lo */
  vobu_pct = ((pos->vobu_idx - lo->vobu_idx) * 1000)
            / ( hi->vobu_idx - lo->vobu_idx);
  if (vobu_pct < 0 || vobu_pct > 1000) {
    DVD_DEBUG(DBG_CRIT, "vobu_pct must be between 0 and 1000");
    return 0;
  }

  /* calc time of lo */
  time_adj = ((uint64_t)args->tmap_interval * vobu_pct) / 1000;
  *out_time = pos->time - time_adj;
  return 1;
}

/* Find the tmap entries on either side of a given sector */
static int32_t dvdnav_tmap_get_entries_for_sector(
            dvdnav_jump_args_t *args,
            dvdnav_cell_data_t *cell_data, uint32_t find_sector,
            dvdnav_pos_data_t *lo, dvdnav_pos_data_t *hi) {
  int32_t result = 0;

  result = dvdnav_tmap_search(args->tmap, args->tmap_len, find_sector,
      &lo->tmap_idx, &lo->sector);
  if (!result) {
    DVD_DEBUG(DBG_CRIT, "could not find lo idx: %i", find_sector);
    return 0;
  }

  /* HACK: Most DVDs have a tmap that starts at sector 0
   * However, some have initial dummy cells that are not seekable
   * (restricted = y).
   * These cells will throw off the tmap calcs when in the first playable cell.
   * For now, assume that lo->sector is equal to the cell->bgn->sector
   * Note that for most DVDs ctx will be 0
   * (Since they will have no dummy cells and cell 1 will start at sector 0)
   */
  if (lo->tmap_idx == TMAP_IDX_EDGE_BGN) {
    lo->sector = cell_data->bgn->sector;
  }

  if (lo->tmap_idx == args->tmap_len - 1) {
    /* lo is last tmap entry; "fake" entry for one beyond
     * and mark it with cell_end_sector */
    hi->tmap_idx = TMAP_IDX_EDGE_END;
    hi->sector = cell_data->end->sector;
  }
  else {
    hi->tmap_idx = lo->tmap_idx + 1;
    result = dvdnav_tmap_get_entry(args->tmap, args->tmap_len,
        hi->tmap_idx, &hi->sector);
    if (!result) {
      DVD_DEBUG(DBG_CRIT, "could not find hi idx: %i", find_sector);
      return 0;
    }
  }
  return 1;
}

/* Find the nearest vobu by using the tmap */
static int32_t dvdnav_find_vobu_by_tmap(dvdnav_t *ctx, dvd_state_t *state,
            dvdnav_jump_args_t *args, dvdnav_cell_data_t *cell_data,
            dvdnav_pos_data_t *jump) {
  uint64_t seek_offset = 0;
  uint32_t seek_idx = 0;
  int32_t result = 0;
  dvdnav_pos_data_t *cell_bgn_lo = NULL;
  dvdnav_pos_data_t *cell_bgn_hi = NULL;
  dvdnav_pos_data_t *jump_lo = NULL;
  dvdnav_pos_data_t *jump_hi = NULL;

  /* get tmap, tmap_len, tmap_interval */
  args->tmap = dvdnav_tmap_get(ctx, state,
      &args->tmap_len, &args->tmap_interval);
  if (args->tmap == NULL) return 0;

  /* get tmap entries on either side of cell_bgn */
  cell_bgn_lo = &(dvdnav_pos_data_t){0};
  cell_bgn_hi = &(dvdnav_pos_data_t){0};
  result = dvdnav_tmap_get_entries_for_sector(args, cell_data,
      cell_data->bgn->sector, cell_bgn_lo, cell_bgn_hi);
  if (!result) return 0;

  /* calc time of cell_bgn_lo */
  result = dvdnav_tmap_calc_time_for_tmap_entry(args, cell_bgn_lo, cell_bgn_hi,
      cell_data->bgn, &cell_bgn_lo->time);
  if (!result) return 0;

  /* calc time of jump_time relative to cell_bgn_lo */
  seek_offset = jump->time - cell_bgn_lo->time;
  seek_idx = (uint32_t)(seek_offset / args->tmap_interval);
  uint32_t seek_remainder = seek_offset - (seek_idx * args->tmap_interval);
  uint32_t seek_pct = (seek_remainder * 1000) / args->tmap_interval;

  /* get tmap entries on either side of jump_time */
  jump_lo = &(dvdnav_pos_data_t){0};
  jump_hi = &(dvdnav_pos_data_t){0};

  /* if seek_idx == 0, then tmap_indexes are the same, do not re-get
   * also, note cell_bgn_lo will already have sector if TMAP_IDX_EDGE_BGN */
  if (seek_idx == 0) {
    jump_lo = cell_bgn_lo;
    jump_hi = cell_bgn_hi;
  }
  else {
    jump_lo->tmap_idx = (uint32_t)(cell_bgn_lo->tmap_idx + seek_idx);
    result = dvdnav_tmap_get_entry(args->tmap, args->tmap_len,
        jump_lo->tmap_idx, &jump_lo->sector);
    if (!result) return 0;

    /* +1 handled by dvdnav_tmap_get_entry */
    jump_hi->tmap_idx = jump_lo->tmap_idx + 1;
    result = dvdnav_tmap_get_entry(args->tmap, args->tmap_len,
        jump_hi->tmap_idx, &jump_hi->sector);
    if (!result) return 0;
  }

  /* interpolate sector */
  result = dvdnav_admap_interpolate_vobu(args, jump_lo, jump_hi,
      seek_pct, &jump->sector);

  return result;
}

/* Find the nearest vobu by using the cell boundaries */
static int32_t dvdnav_find_vobu_by_cell_boundaries(
            dvdnav_jump_args_t *args, dvdnav_cell_data_t *cell_data,
            dvdnav_pos_data_t *jump) {
  int64_t jump_offset = 0;
  int64_t cell_len = 0;
  uint32_t jump_pct = 0;
  int32_t result = 0;

  /* get jump_offset */
  jump_offset = jump->time - cell_data->bgn->time;
  if (jump_offset < 0) {
    DVD_DEBUG(DBG_CRIT, "jump_offset < 0");
    return 0;
  }
  cell_len = cell_data->end->time - cell_data->bgn->time;
  if (cell_len < 0) {
    DVD_DEBUG(DBG_CRIT, "cell_len < 0");
    return 0;
  }
  jump_pct = (jump_offset * 1000) / cell_len;

  /* get sector */
  /* NOTE: end cell sector in VTS_PGC is last sector of cell
   * ctx last sector is not the start of a VOBU
   * +1 to get sector that is the start of a VOBU
   * start of a VOBU is needed in order to index into admap */
  cell_data->end->sector += 1;
  result = dvdnav_admap_interpolate_vobu(args,
      cell_data->bgn, cell_data->end, jump_pct, &jump->sector);
  if (!result) {
    DVD_DEBUG(DBG_CRIT, "find_by_admap.interpolate");
    return 0;
  }
  return 1;
}

/* Jump to sector by time */
/* NOTE: Mode is currently unimplemented. Only 0 should be passed. */
/* 1 and -1 are for future implementation */
/*  0: Default. Jump to a time which may be either <> time_in_pts_ticks */
/*  1: After. Always jump to a time that is > time_in_pts_ticks */
/* -1: Before. Always jump to a time that is < time_in_pts_ticks */
dvdnav_status_t dvdnav_jump_to_sector_by_time(dvdnav_t *ctx,
            uint64_t time_in_pts_ticks, int32_t mode) {
  if (mode != JUMP_MODE_TIME_DEFAULT) return DVDNAV_STATUS_ERR;
  int32_t result = DVDNAV_STATUS_ERR;
  dvd_state_t *state = NULL;
  uint32_t sector_off = 0;
  dvdnav_pos_data_t *jump = NULL;
  dvdnav_cell_data_t *cell_data = NULL;
  dvdnav_jump_args_t *args = NULL;

  jump = &(dvdnav_pos_data_t){0};
  /* convert time to milliseconds */
  jump->time = time_in_pts_ticks / 90;

  /* get variables that will be used across both functions */
  state = &(ctx->vm->state);
  if (state == NULL) goto exit;

  /* get cell info */
  cell_data = &(dvdnav_cell_data_t){0};
  cell_data->bgn = &(dvdnav_pos_data_t){0};
  cell_data->end = &(dvdnav_pos_data_t){0};
  result = dvdnav_cell_find(ctx, state, jump->time, cell_data);
  if (!result) goto exit;

  /* get admap */
  args = &(dvdnav_jump_args_t){0};
  args->admap = dvdnav_admap_get(ctx, state, &args->admap_len);
  if (args->admap == NULL) goto exit;

  /* find sector */
  result = dvdnav_find_vobu_by_tmap(ctx, state, args, cell_data, jump);
  if (!result) {/* bad tmap; interpolate over cell */
    result = dvdnav_find_vobu_by_cell_boundaries(args, cell_data, jump);
    if (!result) {
      goto exit;
    }
  }

  /* jump to sector */
  sector_off = jump->sector - cell_data->bgn->sector;
  result = vm_jump_cell_block(ctx->vm, cell_data->idx, sector_off);
  pthread_mutex_lock(&ctx->vm_lock);
  ctx->cur_cell_time = 0;
  if (result) { /* vm_jump_cell_block was sucessful */
    ctx->vm->hop_channel += HOP_SEEK;
  }
  pthread_mutex_unlock(&ctx->vm_lock);

exit:
  return result;
}


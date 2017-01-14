/*
 * This file is part of libbluray/libdvd
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
 * Copyright (C) 2017 Rootcoder, LLC (adapted for libdvd)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef DVD_LOG_CONTROL_H_
#define DVD_LOG_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


enum debug_mask_enum {
    DBG_RESERVED   = 0x00001,
    DBG_CONFIGFILE = 0x00002,
    DBG_FILE       = 0x00004,
    DBG_DVD        = 0x00040,
    DBG_DIR        = 0x00080,
    DBG_NAV        = 0x00100,
    DBG_CRIT       = 0x00800, /* this is libdvd's default debug mask so use this if you want to display critical info */
    DBG_STREAM     = 0x04000,
};

typedef enum debug_mask_enum debug_mask_t;

typedef void (*DVD_LOG_FUNC)(const char *);

/*
 *
 */

void dvd_set_debug_handler(DVD_LOG_FUNC);

void dvd_set_debug_mask(uint32_t mask);
uint32_t dvd_get_debug_mask(void);

#ifdef __cplusplus
}
#endif

#endif /* DVD_LOG_CONTROL_H_ */

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

#ifndef DVD_LOGGING_H_
#define DVD_LOGGING_H_

#include "log_control.h"

#include "attributes.h"

#include <stdint.h>

DVD_PRIVATE extern uint32_t debug_mask;

#define DVD_DEBUG(MASK,...) \
  do {                                                  \
    if (DVD_UNLIKELY((MASK) & debug_mask)) {             \
      dvd_debug(__FILE__,__LINE__,MASK,__VA_ARGS__);     \
    }                                                   \
  } while (0)

DVD_PRIVATE void dvd_debug(const char *file, int line, uint32_t mask, const char *format, ...) DVD_ATTR_FORMAT_PRINTF(4,5);


#endif /* DVD_LOGGING_H_ */

/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  John Stebbins
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

#ifndef DVD_STRUTL_H_
#define DVD_STRUTL_H_

#include "attributes.h"

#include <stdint.h>

DVD_PRIVATE char * str_dup(const char *str) DVD_ATTR_MALLOC;
DVD_PRIVATE char * str_printf(const char *fmt, ...) DVD_ATTR_FORMAT_PRINTF(1,2) DVD_ATTR_MALLOC;

DVD_PRIVATE uint32_t str_to_uint32(const char *s, int n);
DVD_PRIVATE void     str_tolower(char *s);

DVD_PRIVATE char * str_print_hex(char *out, const uint8_t *str, int count);

#endif // DVD_STRUTL_H_

/*
 * Copyright (C) 2001, 2002 Samuel Hocevar <sam@zoy.org>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
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

#ifndef LIBDVD_DVD_INPUT_H
#define LIBDVD_DVD_INPUT_H

/**
 * Defines and flags.
 */
#define DVDINPUT_NOFLAGS         0

#define DVDINPUT_READ_DECRYPT    (1 << 0)

typedef struct dvd_input_s *dvd_input_t;
struct dvd_input_s {
  DVD_FILEIO_H *fd;
};

dvd_input_t dvdinput_open(const char*);
void        dvdinput_close(dvd_input_t);
int         dvdinput_seek(dvd_input_t, int);
int         dvdinput_title(dvd_input_t, int);
int         dvdinput_read(dvd_input_t, void *, int, int);

#endif /* LIBDVD_DVD_INPUT_H */

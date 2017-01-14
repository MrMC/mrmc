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

#ifndef DVD_FILESYSTEM_H_
#define DVD_FILESYSTEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * file access
 */

typedef struct dvd_fileio_s DVD_FILEIO_H;
struct dvd_fileio_s
{
  void *internal;
  void    (*close) (DVD_FILEIO_H *file);
  int64_t (*seek)  (DVD_FILEIO_H *file, int64_t offset, int32_t origin);
  int64_t (*tell)  (DVD_FILEIO_H *file);
  int     (*eof)   (DVD_FILEIO_H *file);
  int64_t (*read)  (DVD_FILEIO_H *file, uint8_t *buf, int64_t size);
  int64_t (*write) (DVD_FILEIO_H *file, const uint8_t *buf, int64_t size);
};

/*
 * directory access
 */

// Our dirent struct only contains the parts we care about.
typedef struct
{
  char d_name[256];
} DVD_DIRENT;

typedef struct dvd_dirio_s DVD_DIRIO_H;
struct dvd_dirio_s
{
  void *internal;
  void (*close) (DVD_DIRIO_H *dir);
  int  (*read)  (DVD_DIRIO_H *dir, DVD_DIRENT *entry);
};

typedef DVD_FILEIO_H* (*DVD_FILE_OPEN)(const char *filename, const char *mode);
typedef DVD_DIRIO_H*  (*DVD_DIR_OPEN)(const char *dirname);

/**
 *
 *  Register function pointer that will be used to open a file
 *
 * @param p function pointer
 * @return previous function pointer registered
 */
DVD_FILE_OPEN dvd_register_file(DVD_FILE_OPEN p);

/**
 *
 *  Register function pointer that will be used to open a directory
 *
 * @param p function pointer
 * @return previous function pointer registered
 */
DVD_DIR_OPEN dvd_register_dir(DVD_DIR_OPEN p);

#ifdef __cplusplus
}
#endif

#endif /* DVD_FILESYSTEM_H_ */

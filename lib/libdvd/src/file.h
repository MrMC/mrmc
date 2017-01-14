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

#ifndef FILE_H_
#define FILE_H_

#include "libdvd_filesystem.h"

#include "attributes.h"

#include <stdint.h>

#ifdef _WIN32
# define DIR_SEP "\\"
# define DIR_SEP_CHAR '\\'
#else
# define DIR_SEP "/"
# define DIR_SEP_CHAR '/'
#endif

/*
 * file access
 */

#define file_close(X)    X->close(X)
#define file_seek(X,Y,Z) X->seek(X,Y,Z)
#define file_tell(X)     X->tell(X)
#define file_read(X,Y,Z) (size_t)X->read(X,Y,Z)
DVD_PRIVATE int64_t file_size(DVD_FILEIO_H *fp);
DVD_PRIVATE int file_exists(const char* path);
DVD_PRIVATE int directory_exists(const char* path);

DVD_PRIVATE extern DVD_FILEIO_H* (*file_open)(const char* filename, const char *mode);

DVD_PRIVATE DVD_FILE_OPEN file_open_default(void);


/*
 * directory access
 */

#define dir_close(X) X->close(X)
#define dir_read(X,Y) X->read(X,Y)

DVD_PRIVATE extern DVD_DIRIO_H* (*dir_open)(const char* dirname);

DVD_PRIVATE DVD_DIR_OPEN dir_open_default(void);

#endif /* FILE_H_ */

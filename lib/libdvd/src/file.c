/*
 * This file is part of libbluray/libdvd
 * Copyright (C) 2014  Petri Hintukainen
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

#include "file.h"

#include <stdio.h>  // SEEK_*


int64_t file_size(DVD_FILEIO_H *fp)
{
  int64_t pos    = file_tell(fp);
  int64_t res1   = file_seek(fp, 0, SEEK_END);
  int64_t length = file_tell(fp);
  int64_t res2   = file_seek(fp, pos, SEEK_SET);

  if (res1 < 0 || res2 < 0 || pos < 0 || length < 0) {
    return -1;
  }

  return length;
}

DVD_PRIVATE int file_exists(const char* path)
{
  DVD_FILEIO_H *file = file_open(path, "rb");
  if (!file)
    return 0;

  file_close(file);
  return 1;
}

int directory_exists(const char* path)
{
  DVD_DIRIO_H *dir = dir_open(path);
  if (!dir)
    return 0;

  dir_close(dir);
  return 1;
}

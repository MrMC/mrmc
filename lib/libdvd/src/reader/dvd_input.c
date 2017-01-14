/*
 * Copyright (C) 2002 Samuel Hocevar <sam@zoy.org>,
 *                    Håkan Hjort <d95hjort@dtek.chalmers.se>
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

#include <stdio.h>                               /* seek flags */
#include <stdlib.h>                              /* free */

#include "../file.h"
#include "../logging.h"
#include "dvd_reader.h"      /* DVD_VIDEO_LB_LEN */
#include "dvd_input.h"


/**
 * initialize and open a DVD device or file.
 */
dvd_input_t dvdinput_open(const char *target)
{
  dvd_input_t dev;

  if(target == NULL)
    return NULL;

  /* Allocate the library structure */
  dev = malloc(sizeof(*dev));
  if(dev == NULL) {
    DVD_DEBUG(DBG_FILE, "Could not allocate memory\n");
    return NULL;
  }

  /* Open the device */
  dev->fd = file_open(target, "rb");
  if(!dev->fd) {
    DVD_DEBUG(DBG_FILE, "open input\n");
    free(dev);
    return NULL;
  }
  return dev;
}

/**
 * seek into the device.
 */
int dvdinput_seek(dvd_input_t dev, int blocks)
{
  int64_t pos;
  pos = dev->fd->seek(dev->fd, (int64_t)blocks * (int64_t)DVD_VIDEO_LB_LEN, SEEK_SET);

  if(pos < 0) {
    return pos;
  }
  /* assert pos % DVD_VIDEO_LB_LEN == 0 */
  return (int) (pos / DVD_VIDEO_LB_LEN);
}

/**
 * set the block for the beginning of a new title (key).
 */
int dvdinput_title(dvd_input_t dev, int block)
{
  return -1;
}

/**
 * read data from the device.
 */
int dvdinput_read(dvd_input_t dev, void *buffer, int blocks,
		     int flags)
{
  size_t len, bytes;

  len = (size_t)blocks * DVD_VIDEO_LB_LEN;
  bytes = 0;

  while(len > 0) {
    ssize_t ret = dev->fd->read(dev->fd, ((uint8_t*)buffer) + bytes, len);

    if(ret < 0) {
      /* One of the reads failed, too bad.  We won't even bother
       * returning the reads that went OK, and as in the POSIX spec
       * the file position is left unspecified after a failure. */
      return ret;
    }

    if(ret == 0) {
      /* Nothing more to read.  Return all of the whole blocks, if any.
       * Adjust the file position back to the previous block boundary. */
      int64_t over_read = -(bytes % DVD_VIDEO_LB_LEN);
      int64_t pos = dev->fd->seek(dev->fd, over_read, SEEK_CUR);
      if(pos % 2048 != 0)
        DVD_DEBUG(DBG_FILE, "seek not multiple of 2048! Something is wrong!\n\n");
      return (int) (bytes / DVD_VIDEO_LB_LEN);
    }

    len -= ret;
    bytes += ret;
  }

  return blocks;
}

/**
 * close the DVD device and clean up.
 */
void dvdinput_close(dvd_input_t dev)
{
  dev->fd->close(dev->fd);
  free(dev);
}

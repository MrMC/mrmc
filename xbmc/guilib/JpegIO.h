/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GUILIB_JPEGIO_H
#define GUILIB_JPEGIO_H

#include <jpeglib.h>
#include "iimage.h"

class CJpegIO : public IImage
{

public:
  CJpegIO();
 ~CJpegIO();

  // methods for the imagefactory
  virtual bool   LoadImageFromMemory(unsigned char* buffer, unsigned int bufSize, unsigned int width, unsigned int height);
  virtual bool   Decode(const unsigned char *pixels, unsigned int pitch);
  virtual bool   CreateThumbnailFromSurface(unsigned char* bufferin, unsigned int width, unsigned int height,
                   unsigned int pitch, const std::string& destFile,
                   unsigned char* &bufferout, unsigned int &bufferoutSize);
  virtual void   ReleaseThumbnailBuffer();

protected:
  static  void   jpeg_error_exit(j_common_ptr cinfo);
  static unsigned int GetExifOrientation(unsigned char* exif_data, unsigned int exif_data_size);

  unsigned char *m_thumbnailbuffer;
  struct         jpeg_decompress_struct m_cinfo;
};

#endif

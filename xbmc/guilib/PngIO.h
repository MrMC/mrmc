#pragma once

/*
 *      Copyright (C) 2015 Team MrMC
 *      https://github.com/MrMC
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
 *  along with MrMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <png.h>
#include "guilib/iimage.h"

class CPngIO : public IImage
{
public:
  CPngIO();
 ~CPngIO();

  // methods for the imagefactory
  virtual bool   LoadImageFromMemory(unsigned char* buffer, unsigned int bufSize, unsigned int width, unsigned int height);
  virtual bool   Decode(unsigned char* const pixels, unsigned int width, unsigned int height, unsigned int pitch, unsigned int format);
  virtual bool   CreateThumbnailFromSurface(unsigned char* bufferin, unsigned int width, unsigned int height, unsigned int format, unsigned int pitch, const std::string& destFile, unsigned char* &bufferout, unsigned int &bufferoutSize);
  virtual void   ReleaseThumbnailBuffer();

protected:
  png_infop      pngInfoPtr;
  png_structp    pngStructPtr;
  static void    ReadMemoryCallback(png_structp png_ptr, png_bytep buffer, png_size_t count);
  static void    WriteMemoryCallback(png_structp png_ptr, png_bytep buffer, png_size_t count);

  unsigned char *m_compressedPtr;
  unsigned int   m_compressedCnt;
  unsigned int   m_compressedSize;
  unsigned char* m_compressedBuffer;
  std::string    m_texturePath;
};

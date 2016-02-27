/*
 *      Copyright (C) 2012-2013 Team XBMC
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

#include "system.h"

#include "imagefactory.h"
#include "guilib/PngIO.h"
#include "guilib/JpegIO.h"
#include "guilib/GifIO.h"
#include "utils/Mime.h"

IImage* ImageFactory::CreateLoader(const std::string& strFileName)
{
  CURL url(strFileName);
  return CreateLoader(url);
}

IImage* ImageFactory::CreateLoader(const CURL& url)
{
  if(!url.GetFileType().empty())
    return CreateLoaderFromMimeType("image/"+url.GetFileType());

  return CreateLoaderFromMimeType(CMime::GetMimeType(url));
}

IImage* ImageFactory::CreateLoaderFromMimeType(const std::string& strMimeType)
{
  if(strMimeType == "image/jpeg" || strMimeType == "image/tbn" || strMimeType == "image/jpg")
    return new CJpegIO();
  else if(strMimeType == "image/png")
    return new CPngIO();
  else if(strMimeType == "image/gif")
    return new CGifIO();

  return NULL;
}

IImage* ImageFactory::CreateLoaderFromProbe(unsigned char* buffer, size_t size)
{
  if (size <= 5)
    return NULL;
  if (buffer[1] == 'P' && buffer[2] == 'N' && buffer[3] == 'G')
    return new CPngIO();
  if (buffer[0] == 0xFF && buffer[1] == 0xD8 && buffer[2] == 0xFF)
    // don't include the last APP0 byte (0xE0), as some (non-conforming) JPEG/JFIF files might have some other
    // APPn-specific data here, and we should skip over this.
    return new CJpegIO();
  if (buffer[0] == 'G' && buffer[1] == 'I' && buffer[2] == 'F')
    return new CGifIO();

  return NULL;
}

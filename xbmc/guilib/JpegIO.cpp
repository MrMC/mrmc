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
 *  Parts of this code taken from Guido Vollbeding <http://sylvana.net/jpegcrop/exif_orientation.html>
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <setjmp.h>

#include "guilib/JpegIO.h"

#include "guilib/XBTF.h"
//#include "filesystem/File.h"
#include "settings/AdvancedSettings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "windowing/WindowingFactory.h"

struct my_error_mgr
{
  struct jpeg_error_mgr pub;    // "public" fields
  jmp_buf setjmp_buffer;        // for return to caller
};

CJpegIO::CJpegIO()
: IImage()
, m_thumbnailbuffer(nullptr)
{
  m_cinfo = new struct jpeg_decompress_struct;
  memset(m_cinfo, 0, sizeof(*m_cinfo));
}

CJpegIO::~CJpegIO()
{
  delete m_cinfo, m_cinfo = nullptr;
  ReleaseThumbnailBuffer();
}

bool CJpegIO::LoadImageFromMemory(unsigned char *buffer, unsigned int bufSize, unsigned int width, unsigned int height)
{
  // buffer will persist, width and height are 1) real size of image or 2) max surface size.
  if (buffer == nullptr || !bufSize)
    return false;
  
  jpeg_create_decompress(m_cinfo);
  jpeg_mem_src(m_cinfo, buffer, bufSize);
  
  struct my_error_mgr jerr;
  m_cinfo->err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_error_exit;

  if (setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_decompress(m_cinfo);
    return false;
  }
  else
  {
    jpeg_save_markers (m_cinfo, JPEG_APP0 + 1, 0xFFFF);
    jpeg_read_header(m_cinfo, true);
    
    /*  libjpeg can scale the image for us if it is too big. It must be in the format
     num/denom, where (for our purposes) that is [1-8]/8 where 8/8 is the unscaled image.
     The only way to know how big a resulting image will be is to try a ratio and
     test its resulting size.
     If the res is greater than the one desired, use that one since there's no need
     to decode a bigger one just to squish it back down. If the res is greater than
     the gpu can hold, use the previous one.*/
    if (width == 0 || height == 0)
    {
      height = g_advancedSettings.m_imageRes;
      if (g_advancedSettings.m_fanartRes > g_advancedSettings.m_imageRes)
      { // a separate fanart resolution is specified - check if the image is exactly equal to this res
        if (m_cinfo->image_width == (unsigned int)g_advancedSettings.m_fanartRes * 16/9 &&
            m_cinfo->image_height == (unsigned int)g_advancedSettings.m_fanartRes)
        { // special case for fanart res
          height = g_advancedSettings.m_fanartRes;
        }
      }
      width = height * 16/9;
    }

    m_cinfo->scale_denom = 8;
    m_cinfo->out_color_space = JCS_RGB;
    unsigned int maxtexsize = g_Windowing.GetMaxTextureSize();
    for (unsigned int scale = 1; scale <= 8; scale++)
    {
      m_cinfo->scale_num = scale;
      jpeg_calc_output_dimensions(m_cinfo);
      if ((m_cinfo->output_width > maxtexsize) || (m_cinfo->output_height > maxtexsize))
      {
        m_cinfo->scale_num--;
        break;
      }
      if (m_cinfo->output_width >= width && m_cinfo->output_height >= height)
        break;
    }
    jpeg_calc_output_dimensions(m_cinfo);
    m_width  = m_cinfo->output_width;
    m_height = m_cinfo->output_height;

    if (m_cinfo->marker_list)
      m_orientation = GetExifOrientation(m_cinfo->marker_list->data, m_cinfo->marker_list->data_length);
  }

  return true;
}

bool CJpegIO::Decode(unsigned char* const pixels, unsigned int pitch)
{
  struct my_error_mgr jerr;
  m_cinfo->err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_error_exit;

  if (setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_decompress(m_cinfo);
    return false;
  }
  else
  {
    jpeg_start_decompress(m_cinfo);
    
    // pixels format is XB_FMT_A8R8G8B8
    unsigned char *dst = (unsigned char*)pixels;
    unsigned char* row = new unsigned char[m_width * 3];
    while (m_cinfo->output_scanline < m_height)
    {
      jpeg_read_scanlines(m_cinfo, &row, 1);
      unsigned char *src2 = row;
      unsigned char *dst2 = dst;
      for (unsigned int x = 0; x < m_width; x++, src2 += 3)
      {
        *dst2++ = src2[2];
        *dst2++ = src2[1];
        *dst2++ = src2[0];
        *dst2++ = 0xff;
      }
      dst += pitch;
    }
    delete[] row;

    jpeg_finish_decompress(m_cinfo);
  }
  jpeg_destroy_decompress(m_cinfo);
  return true;
}

bool CJpegIO::CreateThumbnailFromSurface(unsigned char *bufferin, unsigned int width, unsigned int height,
  unsigned int pitch, const std::string& destFile,
  unsigned char* &bufferout, unsigned int &bufferoutSize)
{
  //Encode raw data from buffer, save to destbuffer, surface format is XB_FMT_A8R8G8B8
  struct jpeg_compress_struct cinfo;
  struct my_error_mgr jerr;
  JSAMPROW row_pointer[1];

  if(bufferin == nullptr)
  {
    CLog::Log(LOGERROR, "JpegIO::CreateThumbnailFromSurface no buffer");
    return false;
  }

  // create a copy for bgra -> rgb.
  unsigned char *rgbbuf = new unsigned char [(width * height * 3)];
  unsigned char *src = bufferin;
  unsigned char* dst = rgbbuf;
  for (unsigned int y = 0; y < height; y++)
  {

    unsigned char* dst2 = dst;
    unsigned char* src2 = src;
    for (unsigned int x = 0; x < width; x++, src2 += 4)
    {
      *dst2++ = src2[2];
      *dst2++ = src2[1];
      *dst2++ = src2[0];
    }
    dst += width * 3;
    src += pitch;
  }

  long unsigned int outBufSize = width * height;
  m_thumbnailbuffer = (unsigned char*)malloc(outBufSize); //Initial buffer. Grows as-needed.
  if (m_thumbnailbuffer == nullptr)
  {
    CLog::Log(LOGERROR, "JpegIO::CreateThumbnailFromSurface error allocating memory for image buffer");
    return false;
  }

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_error_exit;
  jpeg_create_compress(&cinfo);

  if (setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_compress(&cinfo);
    delete [] rgbbuf;
    return false;
  }
  else
  {
    jpeg_mem_dest(&cinfo, &m_thumbnailbuffer, &outBufSize);
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, true);
    jpeg_start_compress(&cinfo, true);

    while (cinfo.next_scanline < cinfo.image_height)
    {
      row_pointer[0] = &rgbbuf[cinfo.next_scanline * width * 3];
      jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
  }
  delete [] rgbbuf;

  bufferout = m_thumbnailbuffer;
  bufferoutSize = outBufSize;

  return true;
}

void CJpegIO::ReleaseThumbnailBuffer()
{
  if(m_thumbnailbuffer != nullptr)
  {
    free(m_thumbnailbuffer);
    m_thumbnailbuffer = nullptr;
  }
}

// override libjpeg's error function to avoid an exit() call
void CJpegIO::jpeg_error_exit(struct jpeg_common_struct *cinfo)
{
  std::string msg = StringUtils::Format("Error %i: %s",cinfo->err->msg_code, cinfo->err->jpeg_message_table[cinfo->err->msg_code]);
  CLog::Log(LOGWARNING, "JpegIO: %s", msg.c_str());
  
  my_error_mgr *myerr = (my_error_mgr*)cinfo->err;
  longjmp(myerr->setjmp_buffer, 1);
}

unsigned int CJpegIO::GetExifOrientation(unsigned char *exif_data, unsigned int exif_data_size)
{
  #define EXIF_TAG_ORIENTATION 0x0112

  bool isMotorola = false;
  unsigned int offset = 0;
  unsigned int numberOfTags = 0;
  unsigned int tagNumber = 0;
  unsigned int orientation = 0;
  unsigned const char ExifHeader[] = "Exif\0\0";
  
  // read exif head, check for "Exif"
  //   next we want to read to current offset + length
  //   check if buffer is big enough
  if (exif_data_size && memcmp(exif_data, ExifHeader, 6) == 0)
  {
    //read exif body
    exif_data += 6;
  }
  else
  {
    return 0;
  }
  
  // Discover byte order
  if (exif_data[0] == 'I' && exif_data[1] == 'I')
    isMotorola = false;
  else if (exif_data[0] == 'M' && exif_data[1] == 'M')
    isMotorola = true;
  else
    return 0;
  
  // Check Tag Mark
  if (isMotorola)
  {
    if (exif_data[2] != 0 || exif_data[3] != 0x2A)
      return 0;
  }
  else
  {
    if (exif_data[3] != 0 || exif_data[2] != 0x2A)
      return 0;
  }
  
  // Get first IFD offset (offset to IFD0)
  if (isMotorola)
  {
    if (exif_data[4] != 0 || exif_data[5] != 0)
      return 0;
    offset = exif_data[6];
    offset <<= 8;
    offset += exif_data[7];
  }
  else
  {
    if (exif_data[7] != 0 || exif_data[6] != 0)
      return 0;
    offset = exif_data[5];
    offset <<= 8;
    offset += exif_data[4];
  }
  
  if (offset > exif_data_size - 2)
    return 0; // check end of data segment
  
  // Get the number of directory entries contained in this IFD
  if (isMotorola)
  {
    numberOfTags = exif_data[offset];
    numberOfTags <<= 8;
    numberOfTags += exif_data[offset+1];
  }
  else
  {
    numberOfTags = exif_data[offset+1];
    numberOfTags <<= 8;
    numberOfTags += exif_data[offset];
  }
  
  if (numberOfTags == 0)
    return 0;
  offset += 2;
  
  // Search for Orientation Tag in IFD0 - hey almost there! :D
  while(1)//hopefully this jpeg has correct exif data...
  {
    if (offset > exif_data_size - 12)
      return 0; // check end of data segment
    
    // Get Tag number
    if (isMotorola)
    {
      tagNumber = exif_data[offset];
      tagNumber <<= 8;
      tagNumber += exif_data[offset+1];
    }
    else
    {
      tagNumber = exif_data[offset+1];
      tagNumber <<= 8;
      tagNumber += exif_data[offset];
    }
    
    if (tagNumber == EXIF_TAG_ORIENTATION)
      break; //found orientation tag
    
    if ( --numberOfTags == 0)
      return 0;//no orientation found
    offset += 12;//jump to next tag
  }
  
  // Get the Orientation value
  if (isMotorola)
  {
    if (exif_data[offset+8] != 0)
      return 0;
    orientation = exif_data[offset+9];
  }
  else
  {
    if (exif_data[offset+9] != 0)
      return 0;
    orientation = exif_data[offset+8];
  }
  if (orientation > 8)
    orientation = 0;
  
  return orientation;
}

/*
 *      Copyright (C) 2005-2007 Team XboxMediaCenter
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
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

//--------------------------------------------------------------------------
// This module gathers information about a digital image file. This includes:
//   - File name and path
//   - File size
//   - Resolution (if available)
//   - IPTC information (if available)
//   - EXIF information (if available)
// All gathered information is stored in a vector of 'description' and 'value'
// pairs (where both description and value fields are of CStdString types).
//--------------------------------------------------------------------------

#include <memory.h>
#include <cstring>

#include "filesystem/File.h"
#include "utils/exif/JpegParse.h"
#include "utils/log.h"

#define min(a,b) (a)>(b)?(b):(a)

namespace XEXIF
{
  
//--------------------------------------------------------------------------
#define JPEG_PARSE_STRING_ID_BASE       21500
enum {
  ProcessUnknown = JPEG_PARSE_STRING_ID_BASE,
  ProcessSof0,
  ProcessSof1,
  ProcessSof2,
  ProcessSof3,
  ProcessSof5,
  ProcessSof6,
  ProcessSof7,
  ProcessSof9,
  ProcessSof10,
  ProcessSof11,
  ProcessSof13,
  ProcessSof14,
  ProcessSof15,
};

//--------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------
CJpegParse::CJpegParse():
  m_SectionBuffer(nullptr)
{
  memset(&m_ExifInfo, 0, sizeof(m_ExifInfo));
  memset(&m_IPTCInfo, 0, sizeof(m_IPTCInfo));
}

//--------------------------------------------------------------------------
// Process a SOFn marker.  This is useful for the image dimensions
//--------------------------------------------------------------------------
void CJpegParse::ProcessSOFn()
{
  m_ExifInfo.Height = CExifParse::Get16(m_SectionBuffer+3);
  m_ExifInfo.Width  = CExifParse::Get16(m_SectionBuffer+5);

  unsigned char num_components = m_SectionBuffer[7];
  if (num_components != 3)
  {
    m_ExifInfo.IsColor = 0;
  }
  else
  {
    m_ExifInfo.IsColor = 1;
  }
}

//--------------------------------------------------------------------------
// Read a section from a JPEG file. Note that this function allocates memory.
// It must be called in pair with ReleaseSection
//--------------------------------------------------------------------------
bool CJpegParse::GetSection(XFILE::CFile &infile, const unsigned short sectionLength)
{
  if (sectionLength < 2)
  {
    CLog::Log(LOGERROR, "JpgParse: invalid section length");
    return false;
  }

  m_SectionBuffer = new unsigned char[sectionLength];
  if (m_SectionBuffer == NULL)
  {
    CLog::Log(LOGERROR, "JpgParse: could not allocate memory");
    return false;
  }
  // Store first two pre-read bytes.
  m_SectionBuffer[0] = (unsigned char)(sectionLength >> 8);
  m_SectionBuffer[1] = (unsigned char)(sectionLength & 0x00FF);

  unsigned int len = (unsigned int)sectionLength;
  size_t bytesRead = infile.Read(m_SectionBuffer+sizeof(sectionLength), len-sizeof(sectionLength));
  if (bytesRead != sectionLength-sizeof(sectionLength))
  {
    CLog::Log(LOGERROR, "JpgParse: premature end of file?");
    ReleaseSection();
    return false;
  }
  return true;
}

//--------------------------------------------------------------------------
// Deallocate memory allocated in GetSection. This function must always
// be paired by a preceeding GetSection call.
//--------------------------------------------------------------------------
void CJpegParse::ReleaseSection()
{
  delete[] m_SectionBuffer;
  m_SectionBuffer = NULL;
}

//--------------------------------------------------------------------------
// Parse the marker stream until SOS or EOI is seen; infile has already been
// successfully open
//--------------------------------------------------------------------------
bool CJpegParse::ExtractInfo(XFILE::CFile &infile)
{
  // Get file marker (two bytes - must be 0xFFD8 for JPEG files
  uint8_t a;
  size_t bytesRead = infile.Read(&a, sizeof(uint8_t));
  if ((bytesRead != sizeof(uint8_t)) || (a != 0xFF))
  {
    return false;
  }
  bytesRead = infile.Read(&a, sizeof(uint8_t));
  if ((bytesRead != sizeof(uint8_t)) || (a != M_SOI))
  {
    return false;
  }

  for(;;)
  {
    uint8_t marker = 0;
    for (a=0; a<7; a++) {
      bytesRead = infile.Read(&marker, sizeof(uint8_t));
      if (marker != 0xFF)
        break;

      if (a >= 6)
      {
        CLog::Log(LOGERROR, "JpgParse: too many padding bytes");
        return false;
      }
      marker = 0;
    }

    if (marker == 0xff)
    {
      // 0xff is legal padding, but if we get that many, something's wrong.
      CLog::Log(LOGERROR, "JpgParse: too many padding bytes");
      return false;
    }

    // Read the length of the section.
    unsigned short itemlen = 0;
    bytesRead = infile.Read(&itemlen, sizeof(itemlen));
    itemlen = CExifParse::Get16(&itemlen);

    if ((bytesRead != sizeof(itemlen)) || (itemlen < sizeof(itemlen)))
    {
      CLog::Log(LOGERROR, "JpgParse: invalid marker");
      return false;
    }

    switch(marker)
    {
      case M_SOS:   // stop before hitting compressed data
      return true;

      case M_EOI:   // in case it's a tables-only JPEG stream
        CLog::Log(LOGERROR, "JpgParse: No image in jpeg!");
        return false;
      break;

      case M_COM: // Comment section
        GetSection(infile, itemlen);
        if (m_SectionBuffer != NULL)
        {
       //   CExifParse::FixComment(comment);          // Ensure comment is printable
          unsigned short length = min(itemlen - 2, MAX_COMMENT);
          strncpy(m_ExifInfo.FileComment, (char *)&m_SectionBuffer[2], length);
          m_ExifInfo.FileComment[length] = '\0';
		    }
        ReleaseSection();
      break;

      case M_SOF0:
      case M_SOF1:
      case M_SOF2:
      case M_SOF3:
      case M_SOF5:
      case M_SOF6:
      case M_SOF7:
      case M_SOF9:
      case M_SOF10:
      case M_SOF11:
      case M_SOF13:
      case M_SOF14:
      case M_SOF15:
        GetSection(infile, itemlen);
        if ((m_SectionBuffer != NULL) && (itemlen >= 7))
        {
          ProcessSOFn();
          m_ExifInfo.Process = marker;
        }
        ReleaseSection();
      break;

      case M_IPTC:
        GetSection(infile, itemlen);
        if (m_SectionBuffer != NULL)
        {
          CIptcParse::Process(m_SectionBuffer, itemlen, &m_IPTCInfo);
        }
        ReleaseSection();
      break;

      case M_EXIF:
        // Seen files from some 'U-lead' software with Vivitar scanner
        // that uses marker 31 for non exif stuff.  Thus make sure
        // it says 'Exif' in the section before treating it as exif.
        GetSection(infile, itemlen);
        if (m_SectionBuffer != NULL)
        {
          CExifParse exif;
          exif.Process(m_SectionBuffer, itemlen, &m_ExifInfo);
        }
        ReleaseSection();
      break;

      case M_JFIF:
        // Regular jpegs always have this tag, exif images have the exif
        // marker instead, althogh ACDsee will write images with both markers.
        // this program will re-create this marker on absence of exif marker.
        // hence no need to keep the copy from the file.
      // fall through to default case
      default:
        // Skip any other sections.
        GetSection(infile, itemlen);
        ReleaseSection();
      break;
    }
  }
  return true;
}

//--------------------------------------------------------------------------
// Process a file. Check if it is JPEG. Extract exif/iptc info if it is.
//--------------------------------------------------------------------------
bool CJpegParse::Process(const char *picFileName)
{
  XFILE::CFile file;

  if (!file.Open(picFileName))
    return false;

  bool result = ExtractInfo(file);

  if (result == false)
    CLog::Log(LOGERROR, "JpgParse: Not a JPEG file %s", picFileName);

  file.Close();

  return result;
}

}

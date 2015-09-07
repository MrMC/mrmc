#pragma once

/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://www.xbmc.org
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

#include <vector>
#include <gif_lib.h>

#include "guilib/iimage.h"

#pragma pack(1)
struct GifColor
{
  uint8_t b, g, r, a;
};
#pragma pack()

namespace XFILE
{
  class CFile;
};
class CGifFrame;

class CGifIO : public IImage
{
public:
  typedef std::shared_ptr<CGifFrame> FramePtr;

  CGifIO();
  virtual ~CGifIO();

  virtual bool LoadImageFromMemory(unsigned char* buffer, unsigned int bufSize, unsigned int width, unsigned int height);
  virtual bool Decode(unsigned char* const pixels, unsigned int pitch);
  virtual bool CreateThumbnailFromSurface(unsigned char* bufferin, unsigned int width, unsigned int height,
    unsigned int pitch, const std::string& destFile,
    unsigned char* &bufferout, unsigned int &bufferoutSize);

  bool LoadGifMetaData(const char* file);
  bool LoadGif(const char* file);
  
  bool IsAnimated(const char* file);

  const std::vector<FramePtr>& GetFrames() const { return m_frames; }
  unsigned int GetPitch()    const { return m_pitch; }
  unsigned int GetNumLoops() const { return m_loops; }

private:
  bool Open(GifFileType*& gif, void *dataPtr, InputFunc readFunc);
  void Close(GifFileType* gif);
  void Release();
  void InitTemplateAndColormap();
  bool LoadGifMetaData(GifFileType* gif);

  bool Slurp(GifFileType* gif);
  static void ConvertColorTable(std::vector<GifColor> &dest, ColorMapObject* src, unsigned int size);
  bool gcbToFrame(CGifFrame &frame, unsigned int imgIdx);
  bool ExtractFrames(unsigned int count);
  void SetFrameAreaToBack(unsigned char* dest, const CGifFrame &frame);
  void ConstructFrame(CGifFrame &frame, const unsigned char* src) const;
  bool PrepareTemplate(const CGifFrame &frame);
  void PrettyPrintError(std::string messageTemplate, int reason);

  inline std::string memOrFile()
  {
    return m_filename.empty() ? std::string("memory file") : m_filename;
  }

  std::vector<FramePtr> m_frames;
  unsigned int          m_imageSize;
  unsigned int          m_pitch;
  unsigned int          m_loops;
  unsigned int          m_numFrames;
  
  std::string           m_filename;
  GifFileType          *m_gif;
  bool                  m_hasBackground;
  GifColor              m_backColor;
  std::vector<GifColor> m_globalPalette;
  unsigned char        *m_pTemplate;
  int                   m_isAnimated;
  XFILE::CFile         *m_gifFile;
  
};

class CGifFrame
{
  friend class CGifIO;
  
public:
  
  CGifFrame();
  virtual ~CGifFrame();
  
  unsigned char* m_pImage;
  unsigned int m_delay;
  
private:
  CGifFrame(const CGifFrame& src);
  
  unsigned int m_imageSize;
  unsigned int m_height;
  unsigned int m_width;
  unsigned int m_top;
  unsigned int m_left;
  std::vector<GifColor> m_palette;
  unsigned int m_disposal;
};

#pragma once
/*
 *      Copyright (C) 2018 Team MrMC
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

#import <atomic>
#import <string>
#import <queue>
#import "FileItem.h"
#import "threads/Thread.h"
#import "threads/CriticalSection.h"

class CDVDInputStream;
class CDVDDemux;
class CDVDVideoCodec;
typedef struct CGImage* CGImageRef;

typedef struct ThumbNailerImage
{
  int time = 0;
  CGImageRef image = nullptr;
} ThumbNailerImage;

class CProgressThumbNailer
: public CThread
{
public:
  CProgressThumbNailer(const CFileItem& item, int width, id obj);
 ~CProgressThumbNailer();

  bool IsInitialized() { return m_videoCodec != nullptr; };
  void RequestThumbAsPercentage(double percentage);
  ThumbNailerImage GetThumb();

private:
  void Process();
  void QueueExtractThumb(int seekTime);

  id m_obj;
  int m_width;
  std::string m_path;
  std::string m_redactPath;
  float m_aspect;
  bool m_forced_aspect;
  int m_seekTimeMilliSeconds = -1;
  int m_totalTimeMilliSeconds = -1;
  CEvent m_processSleep;
  std::queue<double> m_seekQueue;
  CCriticalSection m_seekQueueCritical;
  std::queue<ThumbNailerImage> m_thumbImages;
  CCriticalSection m_thumbImagesCritical;
  int m_videoStream = -1;
  CDVDInputStream *m_inputStream = nullptr;
  CDVDDemux *m_videoDemuxer = nullptr;
  CDVDVideoCodec *m_videoCodec = nullptr;
};

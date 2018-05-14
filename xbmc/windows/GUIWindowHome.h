#pragma once

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

#include <atomic>

#include "guilib/GUIWindow.h"
#include "interfaces/IAnnouncer.h"
#include "utils/Job.h"

class CVariant;

class CGUIWindowHome :
      public CGUIWindow,
      public ANNOUNCEMENT::IAnnouncer,
      public IJobCallback
{
public:
  CGUIWindowHome(void);
  virtual ~CGUIWindowHome(void);
  virtual void OnInitWindow();
  virtual void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data);

  virtual bool OnMessage(CGUIMessage& message);
  virtual bool OnAction(const CAction &action);
  virtual void OnJobComplete(unsigned int jobID, bool success, CJob *job);
private:
  int  m_updateHS; // flag for which home shelf items needs to be queried
  bool m_firstRun;
  void AddHomeShelfJobs(int flag);
  bool OnClickHomeShelfItem(CFileItem itemPtr, int action);
  bool PlayHomeShelfItem(CFileItem itemPtr);
  std::atomic<bool> m_HomeShelfRunning;
  std::atomic<int> m_cumulativeUpdateFlag;
  int m_countBackCalled;
  CCriticalSection             m_critsection;
  CFileItemList*               m_HomeShelfTVRA;
  CFileItemList*               m_HomeShelfTVPR;
  CFileItemList*               m_HomeShelfMoviesRA;
  CFileItemList*               m_HomeShelfMoviesPR;
  CFileItemList*               m_HomeShelfMusicSongs;
  CFileItemList*               m_HomeShelfMusicVideos;
  CFileItemList*               m_HomeShelfMusicAlbums;
};

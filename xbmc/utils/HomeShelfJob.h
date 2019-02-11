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

#include "Job.h"

enum EHomeShelfFlag
{
  Audio = 0x1,
  Video = 0x2,
};

class CHomeButtonJob : public CJob
{
public:
  CHomeButtonJob();
  ~CHomeButtonJob();
  
  virtual bool DoWork();
};

class CHomeShelfJob : public CJob
{
public:
  CHomeShelfJob(int flag);
 ~CHomeShelfJob();

  bool UpdateVideo();
  bool UpdateMusic();
  void UpdateTvItemsRA(CFileItemList *list);
  void UpdateTvItemsPR(CFileItemList *list);
  void UpdateMovieItemsRA(CFileItemList *list);
  void UpdateMovieItemsPR(CFileItemList *list);
  void UpdateContinueWatchingItems(CFileItemList *list);
  void UpdateMusicAlbumItems(CFileItemList *list);
  void UpdateMusicVideoItems(CFileItemList *list);
  const int GetFlag() const { return m_flag; };

  virtual bool DoWork();

private:
  int m_flag;
  bool m_compatibleSkin;
  CCriticalSection m_critsection;
  CFileItemList *m_HomeShelfTVRA;
  CFileItemList *m_HomeShelfTVPR;
  CFileItemList *m_HomeShelfMoviesRA;
  CFileItemList *m_HomeShelfMoviesPR;
  CFileItemList *m_HomeShelfMusicAlbums;
  CFileItemList *m_HomeShelfMusicVideos;
  CFileItemList *m_HomeShelfContinueWatching;
};

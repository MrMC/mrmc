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
  Totals = 0x4
};

class CHomeShelfJob : public CJob
{
public:
  CHomeShelfJob(int flag);
 ~CHomeShelfJob();

  bool UpdateVideo();
  bool UpdateMusic();
  bool UpdateTotal();
  const CFileItemList *GetMovieItems() const { return m_HomeShelfMovies; };
  const CFileItemList *GetMusicSongItems() const { return m_HomeShelfMusicSongs; };
  const CFileItemList *GetMusicAlbumItems() const { return m_HomeShelfMusicAlbums; };
  const CFileItemList *GetMusicVideoItems() const { return m_HomeShelfMusicVideos; };
  const CFileItemList *GetTvItems() const { return m_HomeShelfTV; };
  const int GetFlag() const { return m_flag; };

  virtual bool DoWork();

private:
  int m_flag;
  CFileItemList *m_HomeShelfTV;
  CFileItemList *m_HomeShelfMovies;
  CFileItemList *m_HomeShelfMusicAlbums;
  CFileItemList *m_HomeShelfMusicSongs;
  CFileItemList *m_HomeShelfMusicVideos;
};

/*
 *      Copyright (C) 2016 Team Kodi
 *      http://kodi.tv
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
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "DirectoryNodeInProgressMovies.h"
#include "video/VideoDatabase.h"

using namespace XFILE::VIDEODATABASEDIRECTORY;

CDirectoryNodeInProgressMovies::CDirectoryNodeInProgressMovies(const std::string& strName, CDirectoryNode* pParent)
  : CDirectoryNode(NODE_TYPE_INPROGRESS_MOVIES, strName, pParent)
{

}

NODE_TYPE CDirectoryNodeInProgressMovies::GetChildType() const
{
  return NODE_TYPE_INPROGRESS_MOVIES;
}

bool CDirectoryNodeInProgressMovies::GetContent(CFileItemList& items) const
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return false;
  
  bool bSuccess=videodatabase.GetInProgressMoviesNav(BuildPath(), items);

  videodatabase.Close();

  return bSuccess;
}

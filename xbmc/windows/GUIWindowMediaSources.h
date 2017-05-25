#pragma once

/*
 *      Copyright (C) 2017 Team MrMC
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

//#include "video/windows/GUIWindowVideoBase.h"
#include "windows/GUIMediaWindow.h"

class CFileItemList;


class CGUIWindowMediaSources : public CGUIMediaWindow
{
public:

  CGUIWindowMediaSources(void);
  virtual ~CGUIWindowMediaSources(void);

  static CGUIWindowMediaSources &GetInstance();

  virtual bool OnAction(const CAction &action);
  virtual bool OnMessage(CGUIMessage& message);

protected:

  // override base class methods
  virtual bool Update(const std::string &strDirectory, bool updateFilterPath = true);
  virtual bool GetDirectory(const std::string &strDirectory, CFileItemList &items);
  virtual bool OnClick(int iItem);
  virtual std::string GetStartFolder(const std::string &dir);

  VECSOURCES m_shares;

};

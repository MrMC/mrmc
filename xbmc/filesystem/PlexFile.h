#pragma once
/*
 *      Copyright (C) 2016 Team MrMC
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

#include "filesystem/CurlFile.h"

namespace XFILE
{
  class CPlexFile : public CCurlFile
  {
  public:
    CPlexFile();
    virtual ~CPlexFile();
    virtual bool Open(const CURL& url);
    virtual bool Exists(const CURL& url);

    static bool TranslatePath(const std::string &path, std::string &translatedPath);
    static bool TranslatePath(const CURL &url, std::string &translatedPath);

  protected:
    virtual std::string TranslatePath(const CURL &url);
  };
}

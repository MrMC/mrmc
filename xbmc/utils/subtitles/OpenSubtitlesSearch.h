#pragma once
/*
 *      Copyright (C) 2015 MrMC
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

#include <string>
#include "FileItem.h"
#include "Application.h"


class COpenSubtitlesSearch
{
public:
  COpenSubtitlesSearch();
  bool SubtitleFileSizeAndHash(const std::string &path, std::string &strSize, std::string &strHash);
  bool SubtitleSearch(const std::string &path,const std::string strLanguages,
                      const std::string preferredLanguage,std::vector<std::map<std::string, std::string>> &subtitlesList);
  bool LogIn();
  bool Download(const std::string subID,const std::string format,std::vector<std::string> &items);
private:
  bool gzipInflate( const std::string& compressedBytes, std::string& uncompressedBytes );
  std::string base64_decode(std::string const& encoded_string);
  std::string m_strToken;
};

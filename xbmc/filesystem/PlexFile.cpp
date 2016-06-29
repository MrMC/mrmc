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

#include "PlexFile.h"
#include "URL.h"
#include "Util.h"
#include "utils/Base64.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "utils/StringUtils.h"

using namespace XFILE;

CPlexFile::CPlexFile()
: CCurlFile()
{ }

CPlexFile::~CPlexFile()
{ }

bool CPlexFile::Open(const CURL& url)
{
  // not sure what we should do here
  bool rtn = CCurlFile::Open(url);
  return rtn;
}

bool CPlexFile::Exists(const CURL& url)
{
  bool rtn = false;
  std::string strUrl = url.Get();
  if (StringUtils::StartsWithNoCase(url.Get(), "plex://movies/titles/"))
  {
    std::string parentpath = URIUtils::GetParentPath(strUrl);
    URIUtils::RemoveSlashAtEnd(parentpath);
    std::string encoded_url = URIUtils::GetFileName(parentpath);
    CURL plex_url(Base64::Decode(encoded_url));
    CLog::Log(LOGDEBUG, "CPlexFile::Exists() %s", plex_url.Get().c_str());
  }
  else if (StringUtils::StartsWithNoCase(url.Get(), "plex://movies/filter/"))
  {
    std::string parentpath = URIUtils::GetParentPath(strUrl);
    URIUtils::RemoveSlashAtEnd(parentpath);
    std::string encoded_url = URIUtils::GetFileName(parentpath);
    CURL plex_url(Base64::Decode(encoded_url));
    CLog::Log(LOGDEBUG, "CPlexFile::Exists() %s", plex_url.Get().c_str());
  }
  else if (StringUtils::StartsWithNoCase(url.Get(), "plex://tvshows/title/"))
  {
    std::string parentpath = URIUtils::GetParentPath(strUrl);
    URIUtils::RemoveSlashAtEnd(parentpath);
    std::string encoded_url = URIUtils::GetFileName(parentpath);
    CURL plex_url(Base64::Decode(encoded_url));
    CLog::Log(LOGDEBUG, "CPlexFile::Exists() %s", plex_url.Get().c_str());
  }
  else if (StringUtils::StartsWithNoCase(url.Get(), "plex://tvshows/filter/"))
  {
    std::string parentpath = URIUtils::GetParentPath(strUrl);
    URIUtils::RemoveSlashAtEnd(parentpath);
    std::string encoded_url = URIUtils::GetFileName(parentpath);
    CURL plex_url(Base64::Decode(encoded_url));
    CLog::Log(LOGDEBUG, "CPlexFile::Exists() %s", plex_url.Get().c_str());
  }
  else
  {
    // not sure what we should do here
    rtn = CCurlFile::Exists(url);
  }
  return rtn;
}

bool CPlexFile::TranslatePath(const std::string &path, std::string &translatedPath)
{
  return TranslatePath(CURL(path), translatedPath);
}

bool CPlexFile::TranslatePath(const CURL &url, std::string &translatedPath)
{
  translatedPath = url.Get();

  return true;
}

std::string CPlexFile::TranslatePath(const CURL &url)
{
  std::string translatedPath;
  if (!TranslatePath(url, translatedPath))
    return "";

  return translatedPath;
}

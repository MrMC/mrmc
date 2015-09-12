/*
 *      Copyright (C) 2009-2013 Team XBMC
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

#if defined(TARGET_DARWIN_OSX)
#include "utils/URIUtils.h"
#include "platform/darwin/DarwinUtils.h"
#elif defined(TARGET_POSIX)
#else
#endif

#include "AliasShortcutUtils.h"

bool IsAliasShortcut(const std::string& path, bool isdirectory)
{
  bool  rtn = false;

#if defined(TARGET_DARWIN_OSX)
  // Note: regular files that have an .alias extension can be
  //   reported as an alias when clearly, they are not. Trap them out.
  if (!URIUtils::HasExtension(path, ".alias"))// TODO - check if this is still needed with the new API
  {
    rtn = CDarwinUtils::IsAliasShortcut(path, isdirectory);
  }
#elif defined(TARGET_POSIX)
  // Linux does not use alias or shortcut methods
#endif
  return(rtn);
}

void TranslateAliasShortcut(std::string& path)
{
#if defined(TARGET_DARWIN_OSX)
  CDarwinUtils::TranslateAliasShortcut(path);
#elif defined(TARGET_POSIX)
  // Linux does not use alias or shortcut methods
#endif
}

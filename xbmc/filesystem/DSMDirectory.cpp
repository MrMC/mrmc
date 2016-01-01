/*
 *      Copyright (C) 2015 Team MrMC
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

#include "system.h"

#include "DSMDirectory.h"

#include "FileItem.h"
#include "PasswordManager.h"
#include "guilib/LocalizeStrings.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "utils/URIUtils.h"

using namespace XFILE;

CDSMDirectory::CDSMDirectory(void)
{
}

CDSMDirectory::~CDSMDirectory(void)
{
}

bool CDSMDirectory::GetDirectory(const CURL& url, CFileItemList &items)
{
  CDSMSessionPtr session = CDSMSessionManager::CreateSession(url);
  if (session)
    return session->GetDirectory(url.GetWithoutFilename().c_str(), url.GetFileName().c_str(), items);
  else
    RequireAuthentication(url);

  return false;
}

bool CDSMDirectory::Create(const CURL& url)
{
  CDSMSessionPtr session = CDSMSessionManager::CreateSession(url);
  if (session)
    return session->CreateDirectory(url.GetFileName().c_str());
  else
  {
    CLog::Log(LOGERROR, "CDSMDirectory: Failed to create %s", url.GetFileName().c_str());
    return false;
  }
}

bool CDSMDirectory::Exists(const CURL& url)
{
  CDSMSessionPtr session = CDSMSessionManager::CreateSession(url);
  if (session)
    return session->DirectoryExists(url.GetFileName().c_str());
  else
  {
    CLog::Log(LOGERROR, "CDSMDirectory: Failed to create session to check exists");
    return false;
  }
}

bool CDSMDirectory::Remove(const CURL& url)
{
  CDSMSessionPtr session = CDSMSessionManager::CreateSession(url);
  if (session)
    return session->RemoveDirectory(url.GetFileName().c_str());
  else
  {
    CLog::Log(LOGERROR, "CDSMDirectory: Failed to remove %s", url.GetFileName().c_str());
    return false;
  }
}

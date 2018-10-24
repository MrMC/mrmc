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

#include "SMB2Directory.h"
#include "SMB2File.h"
#include "FileItem.h"
#include "URL.h"

using namespace XFILE;

CSMB2Directory::CSMB2Directory(void)
{
}

CSMB2Directory::~CSMB2Directory(void)
{
}

bool CSMB2Directory::GetDirectory(const CURL& url, CFileItemList &items)
{
  assert(url.IsProtocol("smb"));

  bool rtn = false;
  if (url.GetShareName().empty())
  {
    // we are browsing a server for shares,
    // do not retain the session
    CSMB2SessionPtr conn = CSMB2SessionManager::Open(url, false);
    if (!conn)
    {
      int err = CSMB2SessionManager::GetLastError();
      if ( err == -EACCES       // SMB2_STATUS_ACCESS_DENIED
        || err == -ECONNREFUSED // SMB2_STATUS_LOGON_FAILURE
        )
      {
        RequireAuthentication(url);
      }
      return false;
    }
    rtn = conn->GetShares(url, items);
    conn->Close();
  }
  else
  {
    CSMB2SessionPtr conn = CSMB2SessionManager::Open(url);
    if (!conn)
    {
      int err = CSMB2SessionManager::GetLastError();
      if ( err == -EACCES       // SMB2_STATUS_ACCESS_DENIED
        || err == -ECONNREFUSED // SMB2_STATUS_LOGON_FAILURE
        )
      {
        RequireAuthentication(url);
      }
      return false;
    }
    rtn = conn->GetDirectory(url, items);
  }

  return rtn;
}

bool CSMB2Directory::Create(const CURL& url)
{
  if (!strlen(url.GetShareName().c_str()))
    return false;

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url);
  if (!conn)
    return false;

  auto res = conn->CreateDirectory(url);

  return res;
}

bool CSMB2Directory::Exists(const CURL& url)
{
  if (!strlen(url.GetShareName().c_str()))
    return false;

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url);
  if (!conn)
    return false;

  struct __stat64 st;
  return conn->Stat(url, &st) == 0 && S_ISDIR(st.st_mode);
}

bool CSMB2Directory::Remove(const CURL& url)
{
  if (!strlen(url.GetShareName().c_str()))
    return false;

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url);
  if (!conn)
    return false;

  auto res = conn->RemoveDirectory(url);

  return res;
}

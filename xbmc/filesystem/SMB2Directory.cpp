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
#include "network/DNSNameCache.h"

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
  if (!strlen(url.GetShareName().c_str()))
    return false;

  // libsmb2 wants ip address
  CURL url2(url);
  std::string ip;
  if (CDNSNameCache::Lookup(url2.GetHostName(), ip))
    url2.SetHostName(ip);

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url2);
  if (!conn)
  {
    int err = CSMB2SessionManager::GetLastError();
    if ( err == -EACCES       // SMB2_STATUS_ACCESS_DENIED
      || err == -ECONNREFUSED // SMB2_STATUS_LOGON_FAILURE
      )
    {
      RequireAuthentication(url2);
    }
    return false;
  }

  auto res = conn->GetDirectory(url2, items);
  return res;

}

bool CSMB2Directory::Create(const CURL& url)
{
  if (!strlen(url.GetShareName().c_str()))
    return false;

  // libsmb2 wants ip address
  CURL url2(url);
  std::string ip;
  if (CDNSNameCache::Lookup(url2.GetHostName(), ip))
    url2.SetHostName(ip);

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url2);
  if (!conn)
    return false;

  auto res = conn->CreateDirectory(url2);

  return res;
}

bool CSMB2Directory::Exists(const CURL& url)
{
  if (!strlen(url.GetShareName().c_str()))
    return false;

  // libsmb2 wants ip address
  CURL url2(url);
  std::string ip;
  if (CDNSNameCache::Lookup(url2.GetHostName(), ip))
    url2.SetHostName(ip);

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url2);
  if (!conn)
    return false;

  struct __stat64 st;
  return conn->Stat(url, &st) == 0 && S_ISDIR(st.st_mode);
}

bool CSMB2Directory::Remove(const CURL& url)
{
  if (!strlen(url.GetShareName().c_str()))
    return false;

  // libsmb2 wants ip address
  CURL url2(url);
  std::string ip;
  if (CDNSNameCache::Lookup(url2.GetHostName(), ip))
    url2.SetHostName(ip);

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url2);
  if (!conn)
    return false;

  auto res = conn->RemoveDirectory(url2);

  return res;
}

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

#include "DSMFile.h"

#include "DSMDirectory.h"
#include "PasswordManager.h"

#include "network/DNSNameCache.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"

#include "arpa/inet.h"

using namespace XFILE;

/********************************************************************************************/
/********************************************************************************************/
#define TICKS_PER_SECOND 10000000
#define EPOCH_DIFFERENCE 11644473600LL
static time_t convertWindowsTimeToPosixTime(const uint64_t input)
{
  // convert from 100ns intervals to seconds
  uint64_t temp = input / TICKS_PER_SECOND;
  // subtract number of seconds between epochs
  // posix   epoch 1970-01-01T00:00:00Z
  // windows epoch 1601-01-01T00:00:00Z
  temp -= EPOCH_DIFFERENCE;
  return (time_t)temp;
}

static std::string strip_share_name_convert(const std::string &path)
{
  // strip off leading name, that is the share name
  // and dsm has that already planted via m_smb_tid
  std::string pathname = path;
  size_t pos = pathname.find("/");
  pathname.erase(0, pos+1);
  // libdsm does not like an ending slash, remove it
  URIUtils::RemoveSlashAtEnd(pathname);
  // paths are posix style on entry,
  // windows style on return.
  StringUtils::Replace(pathname, '/', '\\');

  return pathname;
}

static std::string extract_share_name_convert(const std::string &path)
{
  // extract the share name from the path,
  // this is either the passed name or the
  // first dir in the passed path. So we plant
  // the path, then tokenize, if the tokenize fails,
  // then use the original.
  std::string sharename = path;
  std::vector<std::string> parts;
  std::vector<std::string>::iterator it;
  StringUtils::Tokenize(path, parts, "/");
  for( it = parts.begin(); it != parts.end(); ++it )
  {
    sharename = *it;
    break;
  }
  // libdsm does not like an ending slash, remove it
  URIUtils::RemoveSlashAtEnd(sharename);
  // paths are posix style on entry,
  // windows style on return.
  StringUtils::Replace(sharename, '/', '\\');

  return sharename;
}


/********************************************************************************************/
/********************************************************************************************/
CDSMSession::CDSMSession(DllLibDSM *lib)
  : m_dsmlib(lib)
  , m_smb_session(nullptr)
  , m_smb_tid(0)
{
}

CDSMSession::~CDSMSession()
{
  CSingleLock lock(m_critSect);
  DisconnectSession();
}

int CDSMSession::ConnectSession(const CURL &url)
{
  if (url.GetHostName().empty())
    return NT_STATUS_INVALID_SMB;

  CSingleLock lock(m_critSect);
  m_lastActive = XbmcThreads::SystemClockMillis();

  m_smb_session = m_dsmlib->smb_session_new();
  if (m_smb_session == nullptr)
  {
    CLog::Log(LOGERROR, "CDSMSession: Failed to initialize session for host '%s'", url.GetHostName().c_str());
    return NT_STATUS_INVALID_SMB;
  }

  CLog::Log(LOGDEBUG, "CDSMSession: Creating new session on host '%s' with session %p", url.GetHostName().c_str(), m_smb_session);

  std::string ip;
  CDNSNameCache::Lookup(url.GetHostName(), ip);
  if (ip.empty())
  {
    CLog::Log(LOGERROR, "CDSMSession: Failed to connect");
    DisconnectSession();
    return NT_STATUS_INVALID_SMB;
  }

  // we need an in_addr for netbios_ns_inverse lookup.
  struct in_addr addr = {0};
  inet_aton(ip.c_str(), &addr);
  const char *netbios_name = m_dsmlib->netbios_ns_inverse(addr.s_addr);
  if (netbios_name == nullptr)
  {
    CLog::Log(LOGDEBUG, "CDSMSession: Failed to resolve netbios name, using hostname");
    netbios_name = url.GetHostName().c_str();
  }

  if (!m_dsmlib->smb_session_connect(m_smb_session,
    netbios_name, addr.s_addr, SMB_TRANSPORT_TCP))
  {
    CLog::Log(LOGDEBUG, "CDSMSession: Failed to connect using SMB_TRANSPORT_TCP, trying SMB_TRANSPORT_NBT");
    if (!m_dsmlib->smb_session_connect(m_smb_session,
      netbios_name, addr.s_addr, SMB_TRANSPORT_NBT))
    {
      CLog::Log(LOGERROR, "CDSMSession: Failed to connect");
      DisconnectSession();
      return NT_STATUS_INVALID_SMB;
    }
  }

  std::string domain = url.GetDomain();
  if (domain.empty())
    domain = CSettings::GetInstance().GetString(CSettings::SETTING_SMB_WORKGROUP);

  // default to 'Guest' if no username
  std::string login = url.GetUserName();
  std::string password = url.GetPassWord();
  if (login.empty())
  {
    login = "Guest";

    // default to 'Guest' if no password
    if (password.empty())
      password = "Guest";
  }

  // setup credentials and login.
  m_dsmlib->smb_session_set_creds(m_smb_session,
    netbios_name, login.c_str(), password.c_str());
  if (m_dsmlib->smb_session_login(m_smb_session))
  {
    int response = m_dsmlib->smb_session_is_guest(m_smb_session);
    if (response == 0)
      CLog::Log(LOGDEBUG, "CDSMSession: Logged in as regular user");
    else if (response == 1)
      CLog::Log(LOGDEBUG, "CDSMSession: Logged in as guest");
    else
    {
      CLog::Log(LOGERROR, "CDSMSession: not logged in, invalid session, etc");
      return NT_STATUS_LOGON_FAILURE;
    }
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMSession: Auth failed");
    DisconnectSession();
    return NT_STATUS_LOGON_FAILURE;
  }

  return NT_STATUS_SUCCESS;
}

void CDSMSession::DisconnectSession()
{
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    CLog::Log(LOGINFO, "CDSMSession::DisconnectSession - %p", m_smb_session);
    if (m_smb_tid)
      m_dsmlib->smb_tree_disconnect(m_smb_session, m_smb_tid), m_smb_tid = 0;
    m_dsmlib->smb_session_destroy(m_smb_session), m_smb_session = nullptr;
    // just null it, it is just a copy from CDSMSessionManager
    m_dsmlib = nullptr;
  }
}

bool CDSMSession::ConnectShare(const std::string &path)
{
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    if (m_smb_tid == 0)
    {
      // trees are always relative to share point.
      std::string sharename = extract_share_name_convert(path);
      m_smb_tid = m_dsmlib->smb_tree_connect(m_smb_session, sharename.c_str());
      if (m_smb_tid < 0)
      {
        CLog::Log(LOGERROR, "CDSMSession: Unable to connect to share");
        return false;
      }
    }
  }

  return true;
}

smb_fd CDSMSession::CreateFileHande(const std::string &file)
{
  smb_fd fd = 0;
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    m_lastActive = XbmcThreads::SystemClockMillis();

    // always check that we are connected to a share,
    // this will make sure m_smb_tid is always setup
    if (!ConnectShare(file))
      return 0;

    // paths are relative to the m_smb_tid, which is the share name
    std::string filepath = strip_share_name_convert(file);
    fd = m_dsmlib->smb_fopen(m_smb_session, m_smb_tid, filepath.c_str(), SMB_MOD_RO);
    if (!fd)
      CLog::Log(LOGERROR, "CDSMSession: Was connected but could not create filehandle for '%s'", file.c_str());
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMSession: Not connected and can not create file handle for '%s'", file.c_str());
  }

  return fd;
}

smb_fd CDSMSession::CreateFileHandeForWrite(const std::string &file, bool bOverWrite)
{
  smb_fd fd = 0;
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    m_lastActive = XbmcThreads::SystemClockMillis();

    // always check that we are connected to a share,
    // this will make sure m_smb_tid is always setup
    if (!ConnectShare(file))
      return 0;

    // paths are relative to the m_smb_tid, which is the share name.
    std::string filepath = strip_share_name_convert(file);
    fd = m_dsmlib->smb_fopen(m_smb_session, m_smb_tid, filepath.c_str(), SMB_MOD_RW);
    if (!fd)
      CLog::Log(LOGERROR, "CDSMSession: Was connected but could not create filehandle for '%s'", file.c_str());
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMSession: Not connected and can not create file handle for '%s'", file.c_str());
  }

  return fd;
}

void CDSMSession::CloseFileHandle(const smb_fd fd)
{
  CSingleLock lock(m_critSect);
  if (m_smb_session)
    m_dsmlib->smb_fclose(m_smb_session, fd);
}

bool CDSMSession::GetShares(const std::string &base, CFileItemList &items)
{
  bool status = false;
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    m_lastActive = XbmcThreads::SystemClockMillis();

    smb_share_list shares;
    size_t share_count = m_dsmlib->smb_share_get_list(m_smb_session, &shares);
    if (share_count)
    {
      for(size_t i = 0; i < share_count; ++i)
      {
        std::string itemName = m_dsmlib->smb_share_list_at(shares, i);
        if (itemName.back() == '$')
          continue;

        CFileItemPtr pItem(new CFileItem);
        pItem->m_dwSize = 0;
        pItem->m_bIsFolder = true;
        //pItem->m_dateTime = mtime;
        pItem->SetLabel(itemName);
        pItem->SetPath(base + itemName + "/");
        items.Add(pItem);

        status = true;
      }
      // 'shares' is only created if returned share_count is not zero
      m_dsmlib->smb_share_list_destroy(shares);
    }
  }

  return status;
}

bool CDSMSession::GetDirectory(const std::string &base, const std::string &folder, CFileItemList &items)
{
  if (m_smb_session)
  {
    // if the folder is empty, that is a hint to just return the list of shares
    if (folder.empty())
      return GetShares(base, items);

    // always check that we are connected to a share,
    // this will make sure m_smb_tid is always setup
    if (!ConnectShare(folder))
      return false;

    // paths are relative to the m_smb_tid, which is the share name
    std::string foldername = strip_share_name_convert(folder);
    foldername += "\\*";

    size_t itemcount = 0;
    smb_stat_list stat_list;
    {
      CSingleLock lock(m_critSect);
      m_lastActive = XbmcThreads::SystemClockMillis();
      stat_list = m_dsmlib->smb_find(m_smb_session, m_smb_tid, foldername.c_str());
      itemcount = m_dsmlib->smb_stat_list_count(stat_list);
    }

    if (!stat_list || itemcount == 0)
    {
      CLog::Log(LOGERROR, "CDSMSession: No directory found for '%s'", folder.c_str());
    }
    else
    {
      for(size_t indx = 0; indx < itemcount; ++indx)
      {
        smb_stat fstat;
        const char *name;
        uint64_t size, isdir, mtime;

        {
          CSingleLock lock(m_critSect);
          // smb_stat is a pointer to a smb_file, do not destroy it.
          fstat = m_dsmlib->smb_stat_list_at(stat_list, indx);
          if (fstat)
          {
            name = m_dsmlib->smb_stat_name(fstat);
            if (name == nullptr || strcmp(name, "..") == 0 || strcmp(name, ".") == 0)
              continue;

            size  = m_dsmlib->smb_stat_get(fstat, SMB_STAT_SIZE);
            isdir = m_dsmlib->smb_stat_get(fstat, SMB_STAT_ISDIR);
            uint64_t windowsDateTime = m_dsmlib->smb_stat_get(fstat, SMB_STAT_MTIME);
            mtime = convertWindowsTimeToPosixTime(windowsDateTime);
          }
        }

        if (fstat)
        {
          std::string itemName = name;
          std::string localPath = folder;
          localPath.append(itemName);

          CFileItemPtr pItem(new CFileItem);
          pItem->m_dwSize = size;
          pItem->m_dateTime = mtime;
          pItem->SetLabel(itemName);

          if (itemName[0] == '.')
            pItem->SetProperty("file:hidden", true);

          if (isdir)
          {
            localPath.append("/");
            pItem->m_dwSize = 0;
            pItem->m_bIsFolder = true;
          }

          pItem->SetPath(base + localPath);
          items.Add(pItem);
        }
      }

      {
        CSingleLock lock(m_critSect);
        m_dsmlib->smb_stat_list_destroy(stat_list);
      }

      return true;
    }
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMSession: Not connected, can not list directory '%s'", folder.c_str());
  }

  return false;
}

bool CDSMSession::CreateDirectory(const char *path)
{
  bool status = false;
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    m_lastActive = XbmcThreads::SystemClockMillis();

    // always check that we are connected to a share,
    // this will make sure m_smb_tid is always setup
    if (!ConnectShare(path))
      return false;

    // paths are relative to the m_smb_tid, which is the share name
    std::string pathname = strip_share_name_convert(path);
    uint32_t nt_code = m_dsmlib->smb_directory_create(m_smb_session, m_smb_tid, pathname.c_str());
    if (nt_code)
      CLog::Log(LOGERROR, "CDSMSession: Was connected but could not create directory for '%s'", path);
    else
      status = true;
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMSession: Not connected and can not create directory for '%s'", path);
  }

  return status;
}

bool CDSMSession::RemoveDirectory(const char *path)
{
  bool status = false;
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    m_lastActive = XbmcThreads::SystemClockMillis();

    // always check that we are connected to a share,
    // this will make sure m_smb_tid is always setup
    if (!ConnectShare(path))
      return false;

    // paths are relative to the m_smb_tid, which is the share name
    std::string pathname = strip_share_name_convert(path);
    uint32_t nt_code = m_dsmlib->smb_directory_rm(m_smb_session, m_smb_tid, pathname.c_str());
    if (nt_code)
      CLog::Log(LOGERROR, "CDSMSession: Was connected but could not remove directory for '%s'", path);
    else
      status = true;
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMSession: Not connected and can not remove directory for '%s'", path);
  }

  return status;
}

bool CDSMSession::DirectoryExists(const char *path)
{
  struct __stat64 stat_buffer = {0};
  int exists = Stat(path, &stat_buffer);
  return ((exists != -1) && (stat_buffer.st_mode == _S_IFDIR));
}

bool CDSMSession::FileExists(const char *path)
{
  struct __stat64 stat_buffer = {0};
  int exists = Stat(path, &stat_buffer);
  return ((exists != -1) && (stat_buffer.st_mode == _S_IFREG));
}

bool CDSMSession::RemoveFile(const char *path)
{
  bool status = false;
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    m_lastActive = XbmcThreads::SystemClockMillis();

    // always check that we are connected to a share,
    // this will make sure m_smb_tid is always setup
    if (!ConnectShare(path))
      return false;

    // paths are relative to the m_smb_tid, which is the share name
    std::string pathname = strip_share_name_convert(path);
    uint32_t nt_code = m_dsmlib->smb_file_rm(m_smb_session, m_smb_tid, pathname.c_str());
    if (nt_code)
      CLog::Log(LOGERROR, "CDSMSession: Was connected but could not remove file for '%s'", path);
    else
      status = true;
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMSession: Not connected and can not remove file for '%s'", path);
  }

  return status;
}

bool CDSMSession::RenameFile(const char *path, const char *newpath)
{
  // does not handle rename across shares
  bool status = false;
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    m_lastActive = XbmcThreads::SystemClockMillis();

    // always check that we are connected to a share,
    // this will make sure m_smb_tid is always setup
    if (!ConnectShare(path))
      return false;

    // paths are relative to the m_smb_tid, which is the share name
    std::string pathname = strip_share_name_convert(path);
    std::string newpathname = strip_share_name_convert(newpath);
    int rtn_code = m_dsmlib->smb_file_mv(m_smb_session, m_smb_tid, pathname.c_str(), newpathname.c_str());
    if (rtn_code == -1)
      CLog::Log(LOGERROR, "CDSMSession: Was connected but could not rename file for '%s'", path);
    else
      status = true;
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMSession: Not connected and can not rename file for '%s'", path);
  }

  return status;
}

int CDSMSession::Stat(const char *path, struct __stat64* buffer)
{
  CSingleLock lock(m_critSect);
  if (m_smb_session)
  {
    m_lastActive = XbmcThreads::SystemClockMillis();

    // always check that we are connected to a share,
    // this will make sure m_smb_tid is always setup
    if (!ConnectShare(path))
      return -1;

    // paths are relative to the m_smb_tid, which is the share name
    std::string pathname = strip_share_name_convert(path);
    smb_stat attributes = m_dsmlib->smb_fstat(m_smb_session, m_smb_tid, pathname.c_str());
    if (attributes)
    {
      uint64_t windowsDateTime;
      memset(buffer, 0x00, sizeof(struct __stat64));

      buffer->st_size  = m_dsmlib->smb_stat_get(attributes, SMB_STAT_SIZE);
      // times come back as windows based, convert them to posix based.
      windowsDateTime = m_dsmlib->smb_stat_get(attributes, SMB_STAT_CTIME);
      buffer->st_ctime = convertWindowsTimeToPosixTime(windowsDateTime);
      windowsDateTime = m_dsmlib->smb_stat_get(attributes, SMB_STAT_MTIME);
      buffer->st_mtime = convertWindowsTimeToPosixTime(windowsDateTime);
      windowsDateTime = m_dsmlib->smb_stat_get(attributes, SMB_STAT_ATIME);
      buffer->st_atime = convertWindowsTimeToPosixTime(windowsDateTime);
      if (m_dsmlib->smb_stat_get(attributes, SMB_STAT_ISDIR))
        buffer->st_mode = _S_IFDIR;
      else
        buffer->st_mode = _S_IFREG;

      return 0;
    }
    else
    {
      // this might not be a real error, the file might not exist and we are testing that
      //CLog::Log(LOGERROR, "CDSMSession::Stat - Failed to get attributes for '%s'", path);
      return -1;
    }
  }
  else
  {
    CLog::Log(LOGERROR, "SFTPSession::Stat - Failed because not connected for '%s'", path);
    return -1;
  }
}

int64_t CDSMSession::Seek(const smb_fd fd, uint64_t position, int iWhence)
{
  CSingleLock lock(m_critSect);
  m_lastActive = XbmcThreads::SystemClockMillis();
  ssize_t offset = position;
  ssize_t curpos = m_dsmlib->smb_fseek(m_smb_session, fd, offset, iWhence);
  return curpos;
}

int64_t CDSMSession::Read(const smb_fd fd, void *buffer, size_t size)
{
  CSingleLock lock(m_critSect);
  m_lastActive = XbmcThreads::SystemClockMillis();
  ssize_t bytesread = m_dsmlib->smb_fread(m_smb_session, fd, buffer, size);
  return bytesread;
}

ssize_t CDSMSession::Write(const smb_fd fd, const void *buffer, size_t size)
{
  CSingleLock lock(m_critSect);
  m_lastActive = XbmcThreads::SystemClockMillis();
  ssize_t byteswritten = m_dsmlib->smb_fwrite(m_smb_session, fd, buffer, size);
  return byteswritten;
}

int64_t CDSMSession::GetPosition(const smb_fd fd)
{
  CSingleLock lock(m_critSect);
  m_lastActive = XbmcThreads::SystemClockMillis();
  ssize_t offset = 0;
  ssize_t curpos = m_dsmlib->smb_fseek(m_smb_session, fd, offset, SMB_SEEK_CUR);
  return curpos;
}

int CDSMFile::IoControl(EIoControl request, void* param)
{
  // we pre-checked seeking on open, no need to retest.
  if (request == IOCTRL_SEEK_POSSIBLE)
    return 1;

  return -1;
}

bool CDSMSession::IsIdle()
{
  // idle session 90 seconds after last access
  return (XbmcThreads::SystemClockMillis() - m_lastActive) > 90000;
}

/********************************************************************************************/
/********************************************************************************************/
DllLibDSM* CDSMSessionManager::m_dsmlib = nullptr;
CCriticalSection CDSMSessionManager::m_critSect;
std::map<std::string, CDSMSessionPtr> CDSMSessionManager::m_dsmSessions;

CDSMSessionPtr CDSMSessionManager::CreateSession(const CURL &url)
{
  CSingleLock lock(m_critSect);

  if (!m_dsmlib)
  {
    m_dsmlib = new DllLibDSM();
    m_dsmlib->Load();
    m_dsmlib->EnableDelayedUnload(false);
  }

  CURL authURL(url);
  CPasswordManager::GetInstance().AuthenticateURL(authURL);

  std::string key = authURL.GetHostName()
    + ':' + authURL.GetShareName()
    + ':' + authURL.GetUserName()
    + ':' + authURL.GetPassWord();

  CDSMSessionPtr ptr = m_dsmSessions[key];
  if (ptr == nullptr)
  {
    // create a new session if the session key does not exist
    CDSMSession *session = new CDSMSession(m_dsmlib);
    if (session->ConnectSession(authURL) == NT_STATUS_SUCCESS)
    {
      ptr = CDSMSessionPtr(session);
      m_dsmSessions[key] = ptr;
    }
    else
    {
      // if the connect fails, remove the key so the session
      // object does not hang around waiting for idle. It is most
      // likely a failed auth and must get removed.
      delete session;
      m_dsmSessions.erase(key);
    }
  }

  return ptr;
}

void CDSMSessionManager::ClearOutIdleSessions()
{
  CSingleLock lock(m_critSect);
  bool session_removed = false;
  for (std::map<std::string, CDSMSessionPtr>::iterator iter = m_dsmSessions.begin(); iter != m_dsmSessions.end();)
  {
    // check if there are no other shared_ptr refs and
    // the session has been idle for 90 seconds after last access
    if (iter->second.unique() && iter->second->IsIdle())
    {
      iter->second->DisconnectSession();
      m_dsmSessions.erase(iter++);
      session_removed = true;
    }
    else
      ++iter;
  }
  if (session_removed && m_dsmSessions.empty())
  {
    CLog::Log(LOGDEBUG, "CDSMSessionManager: idle, unloading libdsm");
    if (m_dsmlib)
      SAFE_DELETE(m_dsmlib);
  }
}

void CDSMSessionManager::DisconnectAllSessions()
{
  CSingleLock lock(m_critSect);

  for (std::map<std::string, CDSMSessionPtr>::iterator iter = m_dsmSessions.begin(); iter != m_dsmSessions.end();)
  {
    iter->second->DisconnectSession();
    m_dsmSessions.erase(iter++);
  }

  if (m_dsmlib)
    SAFE_DELETE(m_dsmlib);
}


/********************************************************************************************/
/********************************************************************************************/
CDSMFile::CDSMFile()
: m_dsmSession(nullptr)
, m_smb_fd(0)
, m_fileSize(0)
{
}

CDSMFile::~CDSMFile()
{
  Close();
}

bool CDSMFile::Open(const CURL& url)
{
  m_dsmSession = CDSMSessionManager::CreateSession(url);
  if (m_dsmSession)
  {
    m_file = url.GetFileName().c_str();
    m_smb_fd = m_dsmSession->CreateFileHande(m_file);
    if (m_smb_fd)
    {
      // cache file size
      struct __stat64 stat_buffer = {0};
      if (m_dsmSession->Stat(url.GetFileName().c_str(), &stat_buffer) < 0)
      {
        m_dsmSession->CloseFileHandle(m_smb_fd);
        m_smb_fd = 0;
      }
      else
      {
        m_fileSize = stat_buffer.st_size;
        // test for seeking, if we can not seek, fail the open
        int64_t ret = m_dsmSession->Seek(m_smb_fd, 0, SMB_SEEK_SET);
        if (ret < 0)
        {
          m_dsmSession->CloseFileHandle(m_smb_fd);
          m_smb_fd = 0;
        }
      }
    }

    return (m_smb_fd != 0);
  }
  else
  {
    CLog::Log(LOGERROR, "DSMFile: Failed to allocate session");
    return false;
  }
}

void CDSMFile::Close()
{
  if (m_dsmSession && m_smb_fd)
  {
    m_dsmSession->CloseFileHandle(m_smb_fd);
    m_smb_fd = 0;
    m_dsmSession = CDSMSessionPtr();
  }
}

int64_t CDSMFile::Seek(int64_t iFilePosition, int iWhence)
{
  if (m_dsmSession && m_smb_fd)
  {
    uint64_t position = iFilePosition;
    if (iWhence == SEEK_SET)
      return m_dsmSession->Seek(m_smb_fd, position, SMB_SEEK_SET);
    else if (iWhence == SEEK_CUR)
      return m_dsmSession->Seek(m_smb_fd, position, SMB_SEEK_CUR);
    else if (iWhence == SEEK_END)
    {
      position = GetLength() + iFilePosition;
      return m_dsmSession->Seek(m_smb_fd, position, SMB_SEEK_SET);
    }

    CLog::Log(LOGERROR, "CDSMFile: Unknown whence = %d", iWhence);
    return -1;
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMFile: Can not seek without a filehandle");
    return -1;
  }
}

ssize_t CDSMFile::Read(void* lpBuf, size_t uiBufSize)
{
  // TODO: check this for libdsm, might not be needed
  // Some external libs (libass) use test read with zero size and
  // nullptr buffer pointer to check whether file is readable, but
  // libsmbclient always return "-1" if called with null buffer
  // regardless of buffer size.
  // To overcome this, force return "0" in that case.
  if (uiBufSize == 0 && lpBuf == nullptr)
    return 0;

  // TODO: check this for libdsm, might not be needed
  // work around stupid bug in samba
  // some samba servers has a bug in it where the
  // 17th bit will be ignored in a request of data
  // this can lead to a very small return of data
  // also worse, a request of exactly 64k will return
  // as if eof, client has a workaround for windows
  // thou it seems other servers are affected too
  if (uiBufSize >= 64*1024-2)
    uiBufSize = 64*1024-2;

  if (m_dsmSession && m_smb_fd)
  {
    if (uiBufSize > SSIZE_MAX)
      uiBufSize = SSIZE_MAX;

    int rc = m_dsmSession->Read(m_smb_fd, lpBuf, uiBufSize);
    if (rc >= 0)
      return rc;
    else
    {
      CLog::Log(LOGERROR, "CDSMFile: Read failed - Retrying");
      rc = m_dsmSession->Read(m_smb_fd, lpBuf, uiBufSize);
      if (rc >= 0)
        return rc;
    }
    CLog::Log(LOGERROR, "CDSMFile: Failed to read %i", rc);
  }
  else
    CLog::Log(LOGERROR, "CDSMFile: Can not read without a filehandle");

  return 0;
}

int CDSMFile::Truncate(int64_t size)
{
  if (m_dsmSession && m_smb_fd)
  {
  }

  // TODO: implement this for libdsm
  /*
   * This would force us to be dependant on SMBv3.2 which is GPLv3
   * This is only used by the TagLib writers, which are not currently in use
   * So log and warn until we implement TagLib writing & can re-implement this better.
    CSingleLock lock(smb); // Init not called since it has to be "inited" by now

  #if defined(TARGET_ANDROID)
    int iResult = 0;
  #else
    int iResult = smb.GetImpl()->smbc_ftruncate(m_fd, size);
  #endif
  */
  CLog::Log(LOGWARNING, "CDSMFile: Truncate called and not implemented)");
  return 0;
}

bool CDSMFile::Exists(const CURL& url)
{
  CDSMSessionPtr session = CDSMSessionManager::CreateSession(url);
  if (session)
    return session->FileExists(url.GetFileName().c_str());
  else
  {
    CLog::Log(LOGERROR, "CDSMFile: Failed to create session to check exists for '%s'", url.GetFileName().c_str());
    return false;
  }
}

int CDSMFile::Stat(const CURL& url, struct __stat64* buffer)
{
  CDSMSessionPtr session = CDSMSessionManager::CreateSession(url);
  if (session)
    return session->Stat(url.GetFileName().c_str(), buffer);
  else
  {
    CLog::Log(LOGERROR, "CDSMFile: Failed to create session to stat for '%s'", url.GetFileName().c_str());
    return -1;
  }
}

int CDSMFile::Stat(struct __stat64* buffer)
{
  if (m_dsmSession)
    return m_dsmSession->Stat(m_file.c_str(), buffer);

  CLog::Log(LOGERROR, "CDSMFile: Can't stat without a session for '%s'", m_file.c_str());
  return -1;
}

int64_t CDSMFile::GetLength()
{
  if (m_dsmSession && m_smb_fd)
    return m_fileSize;

  CLog::Log(LOGERROR, "CDSMFile: Can not get size without a filehandle for '%s'", m_file.c_str());
  return -1;
}

int64_t CDSMFile::GetPosition()
{
  if (m_dsmSession && m_smb_fd)
    return m_dsmSession->GetPosition(m_smb_fd);

  CLog::Log(LOGERROR, "CDSMFile: Can not get position without a filehandle for '%s'", m_file.c_str());
  return -1;
}

bool CDSMFile::Delete(const CURL& url)
{
  CDSMSessionPtr session = CDSMSessionManager::CreateSession(url);
  if (session)
    return session->RemoveFile(url.GetFileName().c_str());
  else
  {
    CLog::Log(LOGERROR, "CDSMFile: Failed to create session to delete file '%s'", url.GetFileName().c_str());
    return false;
  }
}

bool CDSMFile::OpenForWrite(const CURL& url, bool bOverWrite)
{
  m_dsmSession = CDSMSessionManager::CreateSession(url);
  if (m_dsmSession)
  {
    m_file = url.GetFileName().c_str();
    m_smb_fd = m_dsmSession->CreateFileHandeForWrite(m_file, bOverWrite);
    return (m_smb_fd != 0);
  }
  else
  {
    CLog::Log(LOGERROR, "DSMFile: Failed to allocate session");
    return false;
  }
}

ssize_t CDSMFile::Write(const void* lpBuf, size_t uiBufSize)
{
  if (m_dsmSession && m_smb_fd)
  {
    if (uiBufSize > SSIZE_MAX)
      uiBufSize = SSIZE_MAX;

    int rc = m_dsmSession->Write(m_smb_fd, lpBuf, uiBufSize);
    if (rc >= 0)
      return rc;
    else
      CLog::Log(LOGERROR, "CDSMFile: Failed to write %i", rc);
  }
  else
    CLog::Log(LOGERROR, "CDSMFile: Can not write without a filehandle");

  return 0;
}

bool CDSMFile::Rename(const CURL& url, const CURL& urlnew)
{
  CDSMSessionPtr session = CDSMSessionManager::CreateSession(url);
  if (session)
  {
    // the session url will be authenticated,
    // also authenticate the new url
    CURL newAuthURL(urlnew);
    CPasswordManager::GetInstance().AuthenticateURL(newAuthURL);
    return session->RenameFile(url.GetFileName().c_str(), newAuthURL.GetFileName().c_str());
  }
  else
  {
    CLog::Log(LOGERROR, "CDSMFile: Failed to create session to rename file '%s'", url.GetFileName().c_str());
    return false;
  }
}

/*
 *      Copyright (C) 2005-2018 Team Kodi
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "system.h"

extern "C"
{
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
}

#include "SMB2File.h"
#include "FileItem.h"
#include "DllLibSMB2.h"
#include "PasswordManager.h"
#include "network/DNSNameCache.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "URL.h"

#include <poll.h>

using namespace XFILE;

//6 mins (360s) cached context timeout
#define CONTEXT_TIMEOUT 360000
// max size of smb credit
#define SMB2_MAX_CREDIT_SIZE 65536

struct file_open
{
  CSMB2SessionPtr session;
  struct smb2fh* handle;
  int mode;
  std::string path;
  uint64_t size;
  uint64_t offset;
};

struct sync_cb_data
{
  bool completed;
  bool reconnect;
  int status;
  void *data;
};


static std::string to_tree_path(const CURL &url)
{
  if (strlen(url.GetFileName().c_str()) <= strlen(url.GetShareName().c_str()) + 1)
    return "";

  std::string strPath(url.GetFileName().c_str() + strlen(url.GetShareName().c_str()) + 1);
  std::replace(strPath.begin(), strPath.end(), '/', '\\');

  if (strPath.back() == '\\')
    strPath = strPath.substr(0, strPath.size() - 1);

  return strPath;
}

static void cmd_cb(struct smb2_context* smb2, int status, void* command_data, void* private_data)
{
  struct sync_cb_data *cb_data = static_cast<struct sync_cb_data *>(private_data);
  cb_data->data = command_data;
  cb_data->completed = true;
  cb_data->status = status;
}

static int wait_for_reply(DllLibSMB2 *smb2lib, struct smb2_context* smb2, sync_cb_data &cb_data)
{
  struct pollfd pfd;
  while (!cb_data.completed)
  {
    pfd.fd = smb2lib->smb2_get_fd(smb2);
    pfd.events = smb2lib->smb2_which_events(smb2);

    if (poll(&pfd, 1, 500) < 0)
    {
      CLog::Log(LOGERROR, "SMB2: poll failed with: %s", smb2lib->smb2_get_error(smb2));
      cb_data.reconnect = true;
      return -1;
    }

    // TODO add timeout
    if (pfd.revents == 0)
    {
      continue;
    }

    if (smb2lib->smb2_service(smb2, pfd.revents) < 0)
    {
      CLog::Log(LOGERROR, "SMB2: smb2_service failed with: %s", smb2lib->smb2_get_error(smb2));
      cb_data.reconnect = true;
      cb_data.status = -1;
      return -1;
    }
  }
  return 0;
}

static std::string get_host_name()
{
  std::string result;
  char* buf = new char[256];
  if (!gethostname(buf, 256))
    result = buf;
  delete[] buf;
  return result;
}

static void smb2_stat_to_system(struct smb2_stat_64& smb2_st, struct __stat64& sys_st)
{
  sys_st.st_ino = static_cast<ino_t>(smb2_st.smb2_ino);
  sys_st.st_mode = smb2_st.smb2_type == SMB2_TYPE_DIRECTORY ? S_IFDIR : 0;
  sys_st.st_nlink = smb2_st.smb2_nlink;
  sys_st.st_size = smb2_st.smb2_size;
  sys_st.st_atime = smb2_st.smb2_atime;
  sys_st.st_mtime = smb2_st.smb2_mtime;
  sys_st.st_ctime = smb2_st.smb2_ctime;
}

#pragma mark - CSMB2SessionManager
mutex_t CSMB2SessionManager::m_sess_mutex;
session_map_t CSMB2SessionManager::m_sessions;
int CSMB2SessionManager::m_lastError = 0;
DllLibSMB2* CSMB2SessionManager::m_smb2lib = nullptr;

CSMB2SessionPtr CSMB2SessionManager::Open(const CURL &url, bool retain)
{
  CURL authURL(url);
  CPasswordManager::GetInstance().AuthenticateURL(authURL);

  std::string hostname = authURL.GetHostName();
  std::string sharename = authURL.GetShareName();
  if (sharename.empty())
    sharename = "IPC$";
  std::string domain = !strlen(authURL.GetDomain().c_str()) ? "MicrosoftAccount" : authURL.GetDomain();
  std::string username = !strlen(authURL.GetUserName().c_str()) ? "Guest" : authURL.GetUserName();
  std::string password = authURL.GetPassWord();
  std::string key = domain + ';' + username + '@' + hostname + '/' + sharename;
  StringUtils::ToLower(key);

  locker_t lock(m_sess_mutex);

  if (!m_smb2lib)
  {
    m_smb2lib = new DllLibSMB2();
    m_smb2lib->Load();
  }

  CSMB2SessionPtr session = m_sessions[key];
  if (session && session->IsValid())
  {
    return session;
  }

  // open new session
  CSMB2SessionPtr newSession = CSMB2SessionPtr(new CSMB2Session(m_smb2lib, hostname, domain, username, password, sharename));
  m_lastError = newSession->GetLastError();

  if (!newSession->IsValid())
    return nullptr;

  if (retain)
    m_sessions[key] = newSession;

  return newSession;
}

void* CSMB2SessionManager::OpenFile(const CURL& url, int mode /*= O_RDONLY*/)
{
  CURL authURL(url);
  CPasswordManager::GetInstance().AuthenticateURL(authURL);

  CSMB2SessionPtr session = Open(authURL);
  if (!session)
    return nullptr;

  struct file_open* file = session->OpenFile(authURL, mode);
  if (file)
    file->session = session;

  return file;
}

void CSMB2SessionManager::DisconnectAll()
{
  locker_t lock(m_sess_mutex);
  m_sessions.clear();
}

void CSMB2SessionManager::CheckIfIdle()
{
  for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it)
  {
    if (it->second && it->second->IsIdle())
    {
      if (it->second->HasOpens())
      {
        // send ping to keep session alive
        it->second->Echo();
      }
      else
      {
        // close unused sessions
        it->second.reset();
      }
    }
  }
}

#pragma mark - CSMB2Session
CSMB2Session::CSMB2Session(DllLibSMB2 *lib, std::string& hostname, std::string& domain, std::string& username
                       , std::string& password, std::string& sharename)
  : m_smb2lib(lib)
  , m_reconnect(false)
{
  if (!Connect(hostname, domain, username, password, sharename))
    Close();

  m_lastAccess = XbmcThreads::SystemClockMillis();
}

CSMB2SessionPtr CSMB2Session::GetForContext(void* context)
{
  struct file_open* file = static_cast<struct file_open*>(context);
  return file->session;
}

bool CSMB2Session::Connect(std::string& hostname, std::string& domain, std::string& username
                        , std::string& password, std::string& sharename)
{
  m_smb_context = m_smb2lib->smb2_init_context();

  std::string localhost = get_host_name();
  if (!localhost.empty())
    m_smb2lib->smb2_set_workstation(m_smb_context, localhost.c_str());
  m_smb2lib->smb2_set_domain(m_smb_context, domain.c_str());
  m_smb2lib->smb2_set_user(m_smb_context, username.c_str());
  m_smb2lib->smb2_set_password(m_smb_context, password.c_str());

  m_smb2lib->smb2_set_security_mode(m_smb_context, SMB2_NEGOTIATE_SIGNING_ENABLED);

  std::string ip;
  CDNSNameCache::Lookup(hostname, ip);
  m_lastError = m_smb2lib->smb2_connect_share(m_smb_context, ip.c_str(), sharename.c_str(), nullptr);

  if (m_lastError < 0)
  {
    CLog::Log(LOGERROR, "SMB2: connect to share '%s' at server '%s' failed. %s"
                             , sharename.c_str(), hostname.c_str(), m_smb2lib->smb2_get_error(m_smb_context));
    m_smb2lib->smb2_destroy_context(m_smb_context);
    m_smb_context = nullptr;

    return false;
  }

  CLog::Log(LOGDEBUG, "SMB2: connected to server '%s' and share '%s'", hostname.c_str(), sharename.c_str());

  return true;
}

CSMB2Session::~CSMB2Session()
{
  locker_t lock(m_ctx_mutex);
  Close();
}

bool CSMB2Session::IsIdle() const
{
  return (XbmcThreads::SystemClockMillis() - m_lastAccess) > CONTEXT_TIMEOUT;
}

bool CSMB2Session::GetShares(const CURL& url, CFileItemList &items)
{
  bool rtn = false;
  DllLibSMB2 *smb2lib = m_smb2lib;
  struct sync_cb_data cb_data = { 0 };

  int ret = ProcessAsync(m_smb2lib, "getshares", cb_data, [&smb2lib](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_share_enum_async(ctx, cb, &data);
  });

  if (cb_data.status)
  {
    m_lastError = ret;
    CLog::Log(LOGERROR, "SMB2: getshares failed: %s", m_smb2lib->smb2_get_error(m_smb_context));
    return rtn;
  }

  m_lastError = 0;
  std::string rootpath = url.Get();
  struct srvsvc_netshareenumall_rep* rep = static_cast<struct srvsvc_netshareenumall_rep*>(cb_data.data);

  //CLog::Log(LOGDEBUG, "Number of shares:%d", rep->ctr->ctr1.count);
  for (uint32_t i = 0; i < rep->ctr->ctr1.count; i++)
  {
    if ((rep->ctr->ctr1.array[i].type & 3) == SHARE_TYPE_DISKTREE)
    {
      //CLog::Log(LOGDEBUG, "%-20s %-20s", rep->ctr->ctr1.array[i].name, rep->ctr->ctr1.array[i].comment);
      CFileItemPtr pItem(new CFileItem(rep->ctr->ctr1.array[i].name));
      std::string path(rootpath);
      path = URIUtils::AddFileToFolder(path, rep->ctr->ctr1.array[i].name);
      URIUtils::AddSlashAtEnd(path);
      pItem->SetPath(path);
      pItem->m_bIsFolder = true;
      pItem->m_bIsShareOrDrive = true;
      // set the default folder icon
      pItem->FillInDefaultIcon();
      items.Add(pItem);
      rtn = true;
    }
  }

  m_smb2lib->smb2_free_data(m_smb_context, rep);
  m_smb2lib->smb2_disconnect_share(m_smb_context);
  // smb2_share_enum_async cannot reuse the context
  m_smb2lib->smb2_destroy_context(m_smb_context);
  m_smb_context = nullptr;

  return rtn;
}

bool CSMB2Session::GetDirectory(const CURL& url, CFileItemList &items)
{
  struct sync_cb_data cb_data = { 0 };
  std::string path = to_tree_path(url);

  if (!IsValid())
    return false;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  int ret = ProcessAsync(m_smb2lib, "opendir", cb_data, [&smb2lib, &path](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_opendir_async(ctx, path.c_str(), cb, &data);
  });

  if (cb_data.status)
  {
    m_lastError = ret;
    CLog::Log(LOGERROR, "SMB2: opendir failed: %s", m_smb2lib->smb2_get_error(m_smb_context));
    return false;
  }

  m_lastError = 0;
  struct smb2dir* smbdir = static_cast<struct smb2dir*>(cb_data.data);
  smb2dirent *smbdirent;
  while ((smbdirent = m_smb2lib->smb2_readdir(m_smb_context, smbdir)) != nullptr)
  {
    if (smbdirent->name == nullptr)
      continue;

    // don't add parent for tree root
    if (path.empty() && smbdirent->name[0] == '.')
      continue;

    if (strcmp(smbdirent->name, ".") == 0 ||
      strcmp(smbdirent->name, "..") == 0 ||
      strcmp(smbdirent->name, "lost+found") == 0)
      continue;

    bool bIsDir = smbdirent->st.smb2_type == SMB2_TYPE_DIRECTORY;
    int64_t iSize = smbdirent->st.smb2_size;
    int64_t lTimeDate = smbdirent->st.smb2_mtime;
    std::string item_path = std::string(url.Get()) + std::string(smbdirent->name);

    if (lTimeDate == 0)
    {
      lTimeDate = smbdirent->st.smb2_ctime;
    }

    CFileItemPtr pItem(new CFileItem());
    pItem->SetLabel(smbdirent->name);
    pItem->m_dwSize = iSize;
    pItem->m_dateTime = lTimeDate;

    if (bIsDir)
    {
      if (item_path[item_path.size() - 1] != '/')
        item_path += '/';
      pItem->m_bIsFolder = true;
    }
    else
    {
      pItem->m_bIsFolder = false;
    }

    if (smbdirent->name[0] == '.')
      pItem->SetProperty("file:hidden", true);
    else
      pItem->ClearProperties();
    pItem->SetPath(item_path);
    items.Add(pItem);
  }

  m_smb2lib->smb2_closedir(m_smb_context, smbdir);
  return true;
}

int CSMB2Session::Stat(const CURL& url, struct __stat64* buffer)
{
  std::string path = to_tree_path(url);

  struct sync_cb_data cb_data = { 0 };
  struct smb2_stat_64 st;

  if (!IsValid())
    return -1;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  m_lastError = ProcessAsync(m_smb2lib, "stat", cb_data, [&smb2lib, &path, &st](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_stat_async(ctx, path.c_str(), &st, cb, &data);
  });

  if (cb_data.status == 0 && buffer)
  {
    memset(buffer, 0, sizeof(struct __stat64));
    smb2_stat_to_system(st, *buffer);
  }

  return cb_data.status;
}

bool CSMB2Session::Delete(const CURL& url)
{
  struct sync_cb_data cb_data = { 0 };
  std::string path = to_tree_path(url);

  if (!IsValid())
    return false;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  ProcessAsync(m_smb2lib, "unlink", cb_data, [&smb2lib, &path](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_unlink_async(ctx, path.c_str(), cb, &data);
  });

  return cb_data.status == 0;
}

bool CSMB2Session::RemoveDirectory(const CURL& url)
{
  struct sync_cb_data cb_data = { 0 };

  std::string path = to_tree_path(url);
  if (path.empty())
  {
    CLog::Log(LOGERROR, "SMB2: cannot delete tree root");
    return false;
  }

  if (!IsValid())
    return false;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  ProcessAsync(m_smb2lib, "rmdir", cb_data, [&smb2lib, &path](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_rmdir_async(ctx, path.c_str(), cb, &data);
  });

  return cb_data.status = 0;
}

bool CSMB2Session::CreateDirectory(const CURL& url)
{
  struct sync_cb_data cb_data = { 0 };

  std::string path = to_tree_path(url);
  if (path.empty())
  {
    CLog::Log(LOGERROR, "SMB2: path must be in a tree");
    return false;
  }

  if (!IsValid())
    return false;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  m_lastError = ProcessAsync(m_smb2lib, "mkdir", cb_data, [&smb2lib, &path](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_mkdir_async(ctx, path.c_str(), cb, &data);
  });

  return cb_data.status == 0;
}

struct file_open* CSMB2Session::OpenFile(const CURL& url, int mode /*= O_RDONLY*/)
{
  struct file_open *file = nullptr;
  struct sync_cb_data cb_data = { 0 };
  std::string path = to_tree_path(url);

  if (!IsValid())
    return nullptr;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  int ret = ProcessAsync(m_smb2lib, "open", cb_data, [&smb2lib, &path, &mode](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_open_async(ctx, path.c_str(), mode, cb, &data);
  });

  if (cb_data.status)
  {
    m_lastError = ret;
    CLog::Log(LOGINFO, "SMB2: unable to open file: '%s' error: '%s'", path.c_str(), m_smb2lib->smb2_get_error(m_smb_context));
    return nullptr;
  }

  struct smb2fh* fh = static_cast<struct smb2fh*>(cb_data.data);
  if (fh)
  {
    struct __stat64 st;
    if (!StatPrivate(fh, &st))
    {
      file = new file_open;
      file->handle = fh;
      file->path = path;
      file->size = st.st_size;
      file->offset = 0;
      file->mode = mode;

      CLog::Log(LOGDEBUG, "SMB2: opened %s", path.c_str());

      locker_t lock(m_open_mutex);
      m_files.push_back(file);
    }
    else
    {
      CLog::Log(LOGINFO, "SMB2: unable to stat file: '%s' error: '%s'", path.c_str(), m_smb2lib->smb2_get_error(m_smb_context));
      CloseHandle(fh);
    }
  }

  return file;
}

bool CSMB2Session::Rename(const CURL & url, const CURL & url2)
{
  struct sync_cb_data cb_data = { 0 };
  std::string oldpath = to_tree_path(url);
  std::string newpath = to_tree_path(url2);

  if (!IsValid())
    return nullptr;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  int ret = ProcessAsync(m_smb2lib, "rename", cb_data, [&smb2lib, &oldpath, &newpath](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_rename_async(ctx, oldpath.c_str(), newpath.c_str(), cb, &data);
  });

  if (cb_data.status)
  {
    m_lastError = ret;
    CLog::Log(LOGINFO, "SMB2: unable to rename file: '%s' error: '%s'", oldpath.c_str(), m_smb2lib->smb2_get_error(m_smb_context));
    return nullptr;
  }

  return cb_data.status == 0;
}

bool CSMB2Session::CloseFile(void* context)
{
  struct file_open* file = static_cast<struct file_open*>(context);
  if (!file)
    return false;

  auto it = std::find(m_files.begin(), m_files.end(), file);
  if (it != m_files.end())
  {
    locker_t lock(m_open_mutex);
    m_files.erase(it);
  }

  CloseHandle(file->handle);
  delete file;

  return true;
}

int CSMB2Session::Stat(void* context, struct __stat64* buffer)
{
  struct file_open* file = static_cast<struct file_open*>(context);
  return StatPrivate(file->handle, buffer);
}

ssize_t CSMB2Session::Read(void* context, void* lpBuf, size_t uiBufSize)
{
  struct file_open* file = static_cast<struct file_open*>(context);
  if (!file->handle || !IsValid())
    return -1;

  struct sync_cb_data cb_data = { 0 };
  m_lastAccess = XbmcThreads::SystemClockMillis();

  // don't read more than file has
  if ((file->offset + uiBufSize) > file->size)
    uiBufSize = file->size - file->offset;

  if (!uiBufSize)
    return 0;

  // it's possible
  size_t max_size = GetChunkSize(file);
  if (uiBufSize > max_size)
    uiBufSize = max_size;

  struct smb2fh* fh = file->handle;
  DllLibSMB2 *smb2lib = m_smb2lib;
  int ret = ProcessAsync(m_smb2lib, "open", cb_data, [&smb2lib, &fh, &lpBuf, &uiBufSize](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_read_async(ctx, fh, static_cast<uint8_t*>(lpBuf), static_cast<uint32_t>(uiBufSize), cb, &data);
  });

  if (ret < 0)
    return -1;

  m_lastError = 0;
  // set offset from handle
  smb2lib->smb2_lseek(m_smb_context, fh, 0, SEEK_CUR, &file->offset);

  return static_cast<ssize_t>(cb_data.status);
}

ssize_t CSMB2Session::Write(void* context, const void* lpBuf, size_t uiBufSize)
{
  struct file_open* file = static_cast<struct file_open*>(context);
  if (!file->handle || !IsValid())
    return -1;

  struct sync_cb_data cb_data = { 0 };
  m_lastAccess = XbmcThreads::SystemClockMillis();

  if (!uiBufSize)
    return 0;

  // it's possible
  size_t max_size = GetChunkSize(file);
  // limit writes to 32k, helps on some servers that fail with POLLHUP errors.
  max_size = std::min(max_size, size_t(32 * 1024) );

  if (uiBufSize > max_size)
    uiBufSize = max_size;

  struct smb2fh* fh = file->handle;
  DllLibSMB2 *smb2lib = m_smb2lib;
  int ret = ProcessAsync(m_smb2lib, "write", cb_data, [&smb2lib, &fh, &lpBuf, &uiBufSize](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_write_async(ctx, fh, (uint8_t*)lpBuf, static_cast<uint32_t>(uiBufSize), cb, &data);
  });

  if (ret < 0)
    return -1;

  m_lastError = 0;
  // set offset from handle
  m_smb2lib->smb2_lseek(m_smb_context, fh, 0, SEEK_CUR, &file->offset);

  return static_cast<ssize_t>(cb_data.status);
}

int64_t CSMB2Session::Seek(void* context, int64_t iFilePosition, int iWhence)
{
  struct file_open* file = static_cast<struct file_open*>(context);
  if (!file->handle)
    return -1;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  // smb2 does not support SEEK_END yet, emulate it
  if (iWhence == SEEK_END)
  {
    iWhence = SEEK_SET;
    iFilePosition += file->size;
  }

  // no need to lock lseek (it does nothing on connection)
  int ret = m_smb2lib->smb2_lseek(m_smb_context, file->handle, iFilePosition, iWhence, &file->offset);
  if (ret == -EINVAL)
  {
    CLog::Log(LOGERROR, "SMB2: seek failed. error( seekpos: %" PRId64 ", whence: %i, %s)"
      , iFilePosition, iWhence, m_smb2lib->smb2_get_error(m_smb_context));
    return -1;
  }

  return file->offset;
}

int CSMB2Session::Truncate(void* context, int64_t size)
{
  struct file_open* file = static_cast<struct file_open*>(context);
  if (!file->handle || !IsValid())
    return -1;

  struct sync_cb_data cb_data = { 0 };

  m_lastAccess = XbmcThreads::SystemClockMillis();

  struct smb2fh* fh = file->handle;
  DllLibSMB2 *smb2lib = m_smb2lib;
  int ret = ProcessAsync(m_smb2lib, "ftruncate", cb_data, [&smb2lib, &fh, &size](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_ftruncate_async(ctx, fh, static_cast<uint64_t>(size), cb, &data);
  });

  m_lastError = ret;
  if (ret != 0)
    return -1;

  return cb_data.status;
}

int64_t CSMB2Session::GetLength(void* context)
{
  struct file_open* file = static_cast<struct file_open*>(context);
  if (!file->handle || !IsValid())
    return -1;

  struct sync_cb_data cb_data = { 0 };
  struct smb2_stat_64 tmp;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  struct smb2fh* fh = file->handle;
  DllLibSMB2 *smb2lib = m_smb2lib;
  int ret = ProcessAsync(m_smb2lib, "fstat", cb_data, [&smb2lib, &fh, &tmp](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_fstat_async(ctx, fh, &tmp, cb, &data);
  });
  m_lastError = ret;

  if (ret != 0)
    return -1;

  // it may change
  file->size = tmp.smb2_size;

  return tmp.smb2_size;
}

int64_t CSMB2Session::GetPosition(void* context) const
{
  struct file_open* file = static_cast<struct file_open*>(context);
  if (!file->handle)
    return -1;

  return static_cast<int64_t>(file->offset);
}

void CSMB2Session::Close()
{
  locker_t lock(m_ctx_mutex);

  if (!m_files.empty())
  {
    auto copy = m_files;
    for (auto it = copy.begin(); it != copy.end(); ++it)
    {
      CloseFile((*it));
    }
  }

  if (m_smb_context)
  {
    if (!m_reconnect) // means that already disconnected
      m_smb2lib->smb2_disconnect_share(m_smb_context);

    m_smb2lib->smb2_destroy_context(m_smb_context);
  }
  m_smb_context = nullptr;
}

void CSMB2Session::CloseHandle(struct smb2fh* file)
{
  if (!file)
    return;

  if (!IsValid())
  {
    free(file);
    return;
  }

  struct sync_cb_data cb_data = { 0 };
  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  m_lastError = ProcessAsync(m_smb2lib, "close", cb_data, [&smb2lib, &file](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_close_async(ctx, file, cb, &data);
  });
}

int CSMB2Session::GetChunkSize(void* context) const
{
  struct file_open* file = static_cast<struct file_open*>(context);

  uint32_t chunk_size;
  uint32_t smb_chunks = static_cast<uint32_t>(file->size / SMB2_MAX_CREDIT_SIZE);

  if (smb_chunks <= 0x10) // 1MB
    chunk_size = SMB2_MAX_CREDIT_SIZE;
  else if (smb_chunks <= 0x100) // 16MB
    chunk_size = SMB2_MAX_CREDIT_SIZE << 1;
  else if (smb_chunks <= 0x1000) // 256MB
    chunk_size = SMB2_MAX_CREDIT_SIZE << 2;
  else
    chunk_size = SMB2_MAX_CREDIT_SIZE << 4; // 1Mb for large files

  return std::min(chunk_size, m_smb2lib->smb2_get_max_read_size(m_smb_context));
}

bool CSMB2Session::Echo()
{
  if (!IsValid())
    return false;

  struct sync_cb_data cb_data = { 0 };
  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  m_lastError = ProcessAsync(m_smb2lib, "echo", cb_data, [&smb2lib](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_echo_async(ctx, cb, &data);
  });

  return m_lastError == 0;
}

int CSMB2Session::StatPrivate(smb2fh* file, struct __stat64* buffer)
{
  struct sync_cb_data cb_data = { 0 };
  struct smb2_stat_64 st;
  
  if (!IsValid())
    return -1;

  m_lastAccess = XbmcThreads::SystemClockMillis();

  DllLibSMB2 *smb2lib = m_smb2lib;
  ProcessAsync(m_smb2lib, "fstat", cb_data, [&smb2lib, &file, &st](smb_ctx ctx, smb_cb cb, smb_data data) {
    return smb2lib->smb2_fstat_async(ctx, file, &st, cb, &data);
  });


  if (!cb_data.status && buffer)
  {
    memset(buffer, 0, sizeof(struct __stat64));
    smb2_stat_to_system(st, *buffer);
  }

  return cb_data.status;
}

int CSMB2Session::ProcessAsync(DllLibSMB2 *smb2lib, const std::string& cmd, struct sync_cb_data& cb_data, async_func func)
{
  locker_t lock(m_ctx_mutex);

  int ret;
  if ((ret = func(m_smb_context, cmd_cb, cb_data)) != 0)
  {
    CLog::Log(LOGERROR, "SMB2: smb2_%s_async failed : %s", cmd.c_str(), m_smb2lib->smb2_get_error(m_smb_context));
    return ret;
  }

  if (wait_for_reply(smb2lib, m_smb_context, cb_data) < 0)
  {
    CLog::Log(LOGERROR, "SMB2: %s error : %s", cmd.c_str(), m_smb2lib->smb2_get_error(m_smb_context));
    if (cb_data.reconnect)
      m_reconnect = true;
    return -1;
  }

  return cb_data.status;
}

#pragma mark - CSMB2MFile
/********************************************************************************************/
/********************************************************************************************/
CSMB2File::CSMB2File()
: m_context(nullptr)
{
}

CSMB2File::~CSMB2File()
{
  Close();
}

bool CSMB2File::Open(const CURL& url)
{
  m_context = CSMB2SessionManager::OpenFile(url);
  return m_context != nullptr;
}

bool CSMB2File::OpenForWrite(const CURL& url, bool overWrite)
{
  int mode = O_RDWR | O_WRONLY;
  if (!Exists(url))
    mode |= O_CREAT;

  m_context = CSMB2SessionManager::OpenFile(url, mode);
  return m_context != nullptr;
}

ssize_t CSMB2File::Read(void* lpBuf, size_t uiBufSize)
{
  if (!m_context)
    return -1;

  CSMB2SessionPtr conn = CSMB2Session::GetForContext(m_context);
  if (!conn)
    return -1;

  return conn->Read(m_context, lpBuf, uiBufSize);
}

ssize_t CSMB2File::Write(const void* buffer, size_t uiBufSize)
{
  if (!m_context)
    return -1;

  CSMB2SessionPtr conn = CSMB2Session::GetForContext(m_context);
  if (!conn)
    return -1;

  return conn->Write(m_context, buffer, uiBufSize);
}

int64_t CSMB2File::Seek(int64_t iFilePosition, int iWhence)
{
  if (!m_context)
    return -1;

  CSMB2SessionPtr conn = CSMB2Session::GetForContext(m_context);
  if (!conn)
    return -1;

  return conn->Seek(m_context, iFilePosition, iWhence);
}

int CSMB2File::Truncate(int64_t size)
{
  if (!m_context)
    return -1;

  CSMB2SessionPtr conn = CSMB2Session::GetForContext(m_context);
  if (!conn)
    return -1;

  return conn->Truncate(m_context, size);
}

int64_t CSMB2File::GetLength()
{
  struct file_open* file = reinterpret_cast<struct file_open*>(m_context);
  if (!file)
    return -1;

  CSMB2SessionPtr conn = CSMB2Session::GetForContext(file);
  if (!conn)
    return -1;

  return conn->GetLength(file);
}

int64_t CSMB2File::GetPosition()
{
  if (!m_context)
    return -1;

  CSMB2SessionPtr conn = CSMB2Session::GetForContext(m_context);
  if (!conn)
    return -1;

  return conn->GetPosition(m_context);
}

int CSMB2File::GetChunkSize()
{
  if (!m_context)
    return -1;

  CSMB2SessionPtr conn = CSMB2Session::GetForContext(m_context);
  if (!conn)
    return -1;

  return conn->GetChunkSize(m_context);
}

int CSMB2File::IoControl(XFILE::EIoControl request, void* param)
{
  if (request == XFILE::IOCTRL_SEEK_POSSIBLE)
    return 1;

  return -1;
}

void CSMB2File::Close()
{
  if (!m_context)
    return;

  CSMB2SessionPtr conn = CSMB2Session::GetForContext(m_context);
  if (!conn)
    return;

  conn->CloseFile(m_context);
  m_context = nullptr;
}

int CSMB2File::Stat(const CURL& url, struct __stat64* buffer)
{
  if (!strlen(url.GetShareName().c_str()))
    return -1;

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url);
  if (!conn)
    return -1;

  auto res = conn->Stat(url, buffer);

  return res;
}

int CSMB2File::Stat(struct __stat64* buffer)
{
  if (!m_context)
    return -1;

  CSMB2SessionPtr conn = CSMB2Session::GetForContext(m_context);
  if (!conn)
    return -1;

  auto res = conn->Stat(m_context, buffer);

  return res;
}

bool CSMB2File::Exists(const CURL& url)
{
  if (!strlen(url.GetShareName().c_str()))
    return false;

  struct __stat64 st;
  return Stat(url, &st) == 0 && !S_ISDIR(st.st_mode);
}

bool CSMB2File::Delete(const CURL& url)
{
  if (!strlen(url.GetShareName().c_str()))
    return false;

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url);
  if (!conn)
    return false;

  auto res = conn->Delete(url);

  return res;
}

bool CSMB2File::Rename(const CURL& url, const CURL& url2)
{
  if (!strlen(url.GetShareName().c_str()) || !strlen(url2.GetShareName().c_str()))
    return false;
  // rename is possible only inside a tree
  if (stricmp(url.GetShareName().c_str(), url2.GetShareName().c_str()))
    return false;

  CSMB2SessionPtr conn = CSMB2SessionManager::Open(url);
  if (!conn)
    return false;

  auto res = conn->Rename(url, url2);
  return res;
}

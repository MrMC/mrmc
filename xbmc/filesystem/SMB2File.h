#pragma once
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

#include "IFile.h"

extern "C"
{
#include <smb2/libsmb2.h>
}

#include <map>
#include <mutex>
#include <vector>
#include <string>


class CURL;
class DllLibSMB2;
class CSMB2Session;
class CFileItemList;

// mutex implementation
typedef std::recursive_mutex mutex_t;
// locker implementation
typedef std::lock_guard<mutex_t> locker_t;
// pointer type
typedef std::shared_ptr<CSMB2Session> CSMB2SessionPtr;
// connections for a domain;user@host/share
typedef std::map<std::string, CSMB2SessionPtr> session_map_t;
// oppened files on session
typedef std::vector<struct file_open*> files_vec_t;

class CSMB2SessionManager
{
public:
  static CSMB2SessionPtr Open(const CURL& url, bool retain = true);
  static void* OpenFile(const CURL& url, int mode = O_RDONLY);

  static void DisconnectAll();
  static void CheckIfIdle();
  static int GetLastError() { return m_lastError; }

private:
  static mutex_t m_sess_mutex;
  static session_map_t m_sessions;
  static int m_lastError;
  static DllLibSMB2 *m_smb2lib;
};

class CSMB2Session
{
public:
  virtual ~CSMB2Session();

  static CSMB2SessionPtr GetForContext(void* context);

  // static operations
  bool GetShares(const CURL& url, CFileItemList &items);
  bool GetDirectory(const CURL& url, CFileItemList &items);
  int Stat(const CURL& url, struct __stat64* buffer);
  bool Delete(const CURL& url);
  bool Rename(const CURL& url, const CURL& url2);
  bool RemoveDirectory(const CURL& url);
  bool CreateDirectory(const CURL& url);

  // file operations
  bool CloseFile(void* context);
  int Stat(void* context, struct __stat64* buffer);
  ssize_t Read(void* context, void* lpBuf, size_t uiBufSize);
  ssize_t Write(void* context, const void* lpBuf, size_t uiBufSize);
  int64_t Seek(void* context, int64_t iFilePosition, int iWhence);
  int Truncate(void* context, int64_t size);
  int64_t GetLength(void* context);
  int64_t GetPosition(void* context) const;
  int GetChunkSize(void* context) const;

  // session operations
  void Close();
  bool Echo();
  bool IsIdle() const;
  int GetLastError() { return m_lastError; }
  bool IsValid() const { return m_smb_context != nullptr && !m_reconnect; }
  bool HasOpens() const { return !m_files.empty(); }

private:
  friend CSMB2SessionManager;

  using smb_ctx = struct smb2_context*;
  using smb_cb = smb2_command_cb;
  using smb_data = struct sync_cb_data&;
  typedef std::function<int(smb_ctx, smb_cb, smb_data)> async_func;

  CSMB2Session(DllLibSMB2 *lib, std::string& hostname, std::string& domain, std::string& username
            , std::string& password, std::string& sharename);
  bool Connect(std::string& hostname, std::string& domain, std::string& username
             , std::string& password, std::string& sharename);
  struct file_open* OpenFile(const CURL& url, int mode = O_RDONLY);
  void CloseHandle(struct smb2fh* file);
  int StatPrivate(struct smb2fh* file, struct __stat64* buffer);
  int ProcessAsync(DllLibSMB2 *smb2lib, const std::string& cmd, struct sync_cb_data& cb_data, async_func func);

  mutex_t m_ctx_mutex;                // mutex to smb2_context
  DllLibSMB2 *m_smb2lib;
  struct smb2_context *m_smb_context; // smb2 context

  mutex_t m_open_mutex;             // mutex to m_files
  files_vec_t m_files;              // files opened with session

  uint64_t m_lastAccess;              // the last access time
  int m_lastError;                    // the last error
  bool m_reconnect;                   // session requires reconnect
};



namespace XFILE
{
  class CSMB2File : public IFile
  {
  public:
    CSMB2File();
    virtual ~CSMB2File();
    virtual bool    Open(const CURL& url);
    virtual void    Close();
    virtual int64_t Seek(int64_t iFilePosition, int iWhence = SEEK_SET);
    virtual ssize_t Read(void* lpBuf, size_t uiBufSize);
    virtual int     Truncate(int64_t size);
    virtual bool    Exists(const CURL& url);
    virtual int     Stat(const CURL& url, struct __stat64* buffer);
    virtual int     Stat(struct __stat64* buffer);
    virtual int64_t GetLength();
    virtual int64_t GetPosition();
    virtual int     GetChunkSize();
    virtual int     IoControl(EIoControl request, void* param);

    // write operations
    virtual bool    Delete(const CURL& url);
    virtual bool    OpenForWrite(const CURL& url, bool bOverWrite = false);
    virtual ssize_t Write(const void* lpBuf, size_t uiBufSize);
    virtual bool    Rename(const CURL& url, const CURL& urlnew);

  private:
    void *m_context;
  };

}

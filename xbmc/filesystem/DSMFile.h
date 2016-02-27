#pragma once
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

#include "DllLibDSM.h"
#include "FileItem.h"
#include "IFile.h"
#include "URL.h"

#include "threads/CriticalSection.h"

class CDSMSession
{
public:
  CDSMSession(DllLibDSM *lib);
 ~CDSMSession();

  int     ConnectSession(const CURL &url);
  void    DisconnectSession();
  smb_fd  CreateFileHande(const std::string &file);
  smb_fd  CreateFileHandeForWrite(const std::string &file, bool bOverWrite);
  void    CloseFileHandle(smb_fd handle);

  bool    GetDirectory(const std::string &base, const std::string &folder, CFileItemList &items);
  bool    CreateDirectory(const char *path);
  bool    RemoveDirectory(const char *path);
  bool    DirectoryExists(const char *path);

  bool    FileExists(const char *path);
  bool    RemoveFile(const char *path);
  bool    RenameFile(const char *path, const char *newpath);

  int     Stat(const char *path, struct __stat64* buffer);
  int64_t Seek(smb_fd handle, uint64_t position, int iWhence);
  int64_t Read(smb_fd handle, void *buffer, size_t size);
  ssize_t Write(smb_fd handle, const void *buffer, size_t size);
  int64_t GetPosition(smb_fd handle);

  bool    IsIdle();

private:
  bool    GetShares(const std::string &base, CFileItemList &items);
  bool    ConnectShare(const std::string &sharename);

  CCriticalSection    m_critSect;
  DllLibDSM          *m_dsmlib;
  smb_session        *m_smb_session;
  smb_tid             m_smb_tid;
  int                 m_lastActive;
  time_t              m_timeout_sec;
};

typedef std::shared_ptr<CDSMSession> CDSMSessionPtr;

class CDSMSessionManager
{
public:
  static CDSMSessionPtr CreateSession(const CURL &url);
  static void           ClearOutIdleSessions();
  static void           DisconnectAllSessions();

private:
  static DllLibDSM        *m_dsmlib;
  static CCriticalSection  m_critSect;
  static std::map<std::string, CDSMSessionPtr> m_dsmSessions;
};

namespace XFILE
{
  class CDSMFile : public IFile
  {
  public:
    CDSMFile();
    virtual ~CDSMFile();
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
    virtual int     GetChunkSize() {return 1;};
    virtual int     IoControl(EIoControl request, void* param);

    // write operations
    virtual bool    Delete(const CURL& url);
    virtual bool    OpenForWrite(const CURL& url, bool bOverWrite = false);
    virtual ssize_t Write(const void* lpBuf, size_t uiBufSize);
    virtual bool    Rename(const CURL& url, const CURL& urlnew);

  private:
    bool IsValidFile(const std::string& strFileName);

    std::string     m_file;
    CDSMSessionPtr  m_dsmSession;
    smb_fd          m_smb_fd;
    int64_t         m_fileSize;
  };
}

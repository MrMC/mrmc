/*
 *      Copyright (C) 2016 Team Kodi
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
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <sys/stat.h>

#include "filesystem/SpecialProtocol.h"
#include "platform/darwin/DarwinNSUserDefaults.h"
#include "utils/StringUtils.h"
#include "TVOSFile.h"

using namespace XFILE;


CTVOSFile::CTVOSFile()
{
  m_position = -1;
}


CTVOSFile::~CTVOSFile()
{
  Close();
}

bool CTVOSFile::WantsFile(const CURL& url)
{
  if (!StringUtils::EqualsNoCase(url.GetFileType(), "xml"))
    return false;
  return CDarwinNSUserDefaults::IsKeyFromPath(url.Get());
}

bool CTVOSFile::Open(const CURL& url)
{
  bool ret = CDarwinNSUserDefaults::KeyFromPathExists(url.Get());
  if (ret)
  {
    m_url = url;
    m_position = 0;
  }
  return ret;
}

bool CTVOSFile::OpenForWrite(const CURL& url, bool bOverWrite /* = false */)
{
  if (CDarwinNSUserDefaults::KeyFromPathExists(url.Get()) && !bOverWrite)
      return false; // no overwrite

  bool ret = WantsFile(url);// if we want the file we can write it ...
  if (ret)
  {
    m_url = url;
    m_position = 0;
  }
  return ret;
}

bool CTVOSFile::Delete(const CURL& url)
{
  return CDarwinNSUserDefaults::DeleteKeyFromPath(url.Get(), true);
}

bool CTVOSFile::Exists(const CURL& url)
{
  return CDarwinNSUserDefaults::KeyFromPathExists(url.Get());
}

int CTVOSFile::Stat(const CURL& url, struct __stat64* buffer)
{
  if (buffer != nullptr)
  {
    size_t size = 0;
    // get the size from the data by passing in a nullptr
    if (CDarwinNSUserDefaults::GetKeyDataFromPath(url.Get(), nullptr, size))
    {
      memset(buffer, 0, sizeof(struct __stat64));
      // mimic stat
      // rw for world
      // regular file
      buffer->st_flags = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IWGRP | S_IWOTH | S_IFREG;
      buffer->st_size = size;
      buffer->st_blocks = 1; // we mimic one block
      buffer->st_blksize = size; // with full size
      return 0;
    }
  }
  errno = ENOENT;
  return -1;
}

bool CTVOSFile::Rename(const CURL& url, const CURL& urlnew)
{
  bool ret = false;
  if (Exists(url) && !Exists(urlnew) && WantsFile(urlnew))
  {
    void *lpBuf = nullptr;
    size_t uiBufSize = 0;
    if (CDarwinNSUserDefaults::GetKeyDataFromPath(url.Get(), lpBuf, uiBufSize))// get size from old file
    {
      lpBuf = (void *)new char[uiBufSize];
      if (CDarwinNSUserDefaults::GetKeyDataFromPath(url.Get(), lpBuf, uiBufSize))// read old file
      {
        if (CDarwinNSUserDefaults::SetKeyDataFromPath(urlnew.Get(), lpBuf, uiBufSize, true))// write to new url
        {
          // remove old file
          Delete(url);
          ret = true;
        }
      }
      delete [] (char *)lpBuf;
    }
  }
  return ret;
}

int CTVOSFile::Stat(struct __stat64* buffer)
{
  return Stat(m_url, buffer);
}

ssize_t CTVOSFile::Read(void* lpBuf, size_t uiBufSize)
{
  void *lpBufInternal = nullptr;
  size_t uiBufSizeInternal = 0;
  ssize_t copiedBytes = -1;
  
  if (m_position > 0 && m_position == GetLength())
    return 0;// simulate read 0 bytes on EOF
  
  if (CDarwinNSUserDefaults::GetKeyDataFromPath(m_url.Get(), lpBufInternal, uiBufSizeInternal))// get size from file
  {
    lpBufInternal = (void *)new char[uiBufSize];
    if (CDarwinNSUserDefaults::GetKeyDataFromPath(m_url.Get(), lpBufInternal, uiBufSizeInternal))// read file
    {
      copiedBytes = uiBufSizeInternal > uiBufSize ? uiBufSize : uiBufSizeInternal;
      memcpy(lpBuf, lpBufInternal, copiedBytes);
    }
    delete [] (char *)lpBufInternal;
    m_position = copiedBytes;
  }
  return copiedBytes;
}

ssize_t CTVOSFile::Write(const void* lpBuf, size_t uiBufSize)
{
  if (CDarwinNSUserDefaults::SetKeyDataFromPath(m_url.Get(), lpBuf, uiBufSize, true))// write to file
  {
    m_position = uiBufSize;
    return uiBufSize;
  }
  return -1;
}

int64_t CTVOSFile::Seek(int64_t iFilePosition, int iWhence /*=SEEK_SET*/)
{
  errno = EINVAL;
  return -1;
}

void CTVOSFile::Close()
{
  m_url.Reset();
  m_position = -1;
}

int64_t CTVOSFile::GetPosition()
{
  return 0;
}

int64_t CTVOSFile::GetLength()
{
  struct __stat64 statbuffer;
  if (Stat(&statbuffer) == 0)
  {
    return statbuffer.st_size;
  }
  return 0;
}

int CTVOSFile::GetChunkSize()
{
  return GetLength(); // only full file size can be read from nsuserdefaults...
}

int CTVOSFile::IoControl(EIoControl request, void* param)
{
  if (request == IOCTRL_SEEK_POSSIBLE)
    return 0; // no seek support
  return -1;
}

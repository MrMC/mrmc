#pragma once
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

#include "filesystem/File.h"
#include "filesystem/IFile.h"
#include "URL.h"

namespace XFILE
{
class CTVOSFile : public IFile
{
public:
  CTVOSFile();
  ~CTVOSFile();
  
  bool static WantsFile(const CURL& url);

  virtual bool Open(const CURL& url) override;
  virtual bool Exists(const CURL& url) override;
  virtual int Stat(const CURL& url, struct __stat64* buffer) override;
  virtual int Stat(struct __stat64* buffer) override;
  virtual bool OpenForWrite(const CURL& url, bool bOverWrite = false) override;
  virtual bool Delete(const CURL& url) override;
  virtual bool Rename(const CURL& url, const CURL& urlnew) override;

  virtual ssize_t Read(void* lpBuf, size_t uiBufSize) override;
  virtual ssize_t Write(const void* lpBuf, size_t uiBufSize) override;
  virtual int64_t Seek(int64_t iFilePosition, int iWhence = SEEK_SET) override;
  virtual void Close() override;
  virtual int64_t GetPosition() override;
  virtual int64_t GetLength() override;
  virtual int  GetChunkSize() override;
  virtual int IoControl(EIoControl request, void* param)  override;

protected:

  CURL m_url;
  int64_t m_position;
};
}

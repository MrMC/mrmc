#ifndef __X_FILE_UTILS_
#define __X_FILE_UTILS_

/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#include "linux/PlatformDefs.h"
#include "XHandlePublic.h"

#ifdef TARGET_POSIX
#define XBMC_FILE_SEP '/'
#else
#define XBMC_FILE_SEP '\\'
#endif

HANDLE FindFirstFile(const char*,LPWIN32_FIND_DATA);

int   FindNextFile(HANDLE,LPWIN32_FIND_DATA);
int   FindClose(HANDLE hFindFile);

#define CreateFileA CreateFile
HANDLE CreateFile(LPCTSTR lpFileName, uint32_t dwDesiredAccess, uint32_t dwShareMode,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,  uint32_t dwCreationDisposition,
            uint32_t dwFlagsAndAttributes, HANDLE hTemplateFile);

int   CopyFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, int bFailIfExists);
int   DeleteFile(const char*);
int   MoveFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName);

int   WriteFile(HANDLE hFile, const void * lpBuffer, uint32_t nNumberOfBytesToWrite, uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
int   ReadFile( HANDLE hFile, void* lpBuffer, unsigned int nNumberOfBytesToRead, uint32_t* lpNumberOfBytesRead, void* unsupportedlpOverlapped);
int   FlushFileBuffers(HANDLE hFile);

int   CreateDirectory(LPCTSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
int   RemoveDirectory(LPCTSTR lpPathName);
uint32_t GetCurrentDirectory(uint32_t nBufferLength, char* lpBuffer);

uint32_t SetFilePointer(HANDLE hFile, int32_t lDistanceToMove,
                      int32_t *lpDistanceToMoveHigh, uint32_t dwMoveMethod);
int   SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove,PLARGE_INTEGER lpNewFilePointer, uint32_t dwMoveMethod);
int   SetEndOfFile(HANDLE hFile);

uint32_t SleepEx(uint32_t dwMilliseconds,  int bAlertable);
uint32_t GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation );
uint32_t GetFileSize(HANDLE hFile, uint32_t* lpFileSizeHigh);
int   GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize);
int    _stat64(const char *path, struct __stat64 *buffer);
int    _stati64(const char *path,struct _stati64 *buffer);
int _fstat64(int fd, struct __stat64 *buffer);

uint32_t GetFileAttributes(LPCTSTR lpFileName);

#define ERROR_ALREADY_EXISTS EEXIST

// uses statfs
int GetDiskFreeSpaceEx(
  LPCTSTR lpDirectoryName,
  PULARGE_INTEGER lpFreeBytesAvailable,
  PULARGE_INTEGER lpTotalNumberOfBytes,
  PULARGE_INTEGER lpTotalNumberOfFreeBytes
);

#endif

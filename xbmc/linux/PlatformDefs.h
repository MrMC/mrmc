#pragma once

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

#ifdef TARGET_POSIX

#define LINE_ENDING "\n"

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#if defined(TARGET_DARWIN)
  #include <stdio.h>
  #include <sched.h>
  #include <AvailabilityMacros.h>
  #ifndef __STDC_FORMAT_MACROS
    #define __STDC_FORMAT_MACROS
  #endif

  #include <inttypes.h>
  #include <sys/sysctl.h>
  #include <mach/mach.h>

  #if defined(TARGET_DARWIN_OSX)
    #include <libkern/OSTypes.h>
  #endif

#elif defined(TARGET_FREEBSD)
  #include <stdio.h>
  #include <sys/sysctl.h>
  #include <sys/types.h>
#else
  #include <sys/sysinfo.h>
  #endif
  #include <sys/time.h>
  #include <time.h>
#endif

#include <stdint.h>

#ifndef PRId64
  #if __WORDSIZE == 64
    #define PRId64 "ld"
  #else
    #define PRId64 "lld"
  #endif
#endif

#ifndef PRIu64
  #if __WORDSIZE == 64
  #define PRIu64 "lu"
  #else
  #define PRIu64 "llu"
  #endif
#endif
	
#ifndef PRIx64
  #if __WORDSIZE == 64
  #define PRIx64 "lx"
  #else
  #define PRIx64 "llx"
  #endif
#endif

#ifndef PRIdS
  #define PRIdS "zd"
#endif

#ifndef PRIuS
  #define PRIuS "zu"
#endif

#ifdef TARGET_POSIX

#ifndef INSTALL_PATH
  #define INSTALL_PATH    "/usr/share/xbmc"
#endif

#ifndef BIN_INSTALL_PATH
  #define BIN_INSTALL_PATH "/usr/lib/xbmc"
#endif

#define _fdopen fdopen
#define _vsnprintf vsnprintf
#define _stricmp  strcasecmp
#define stricmp   strcasecmp
#define strcmpi strcasecmp
#define strnicmp  strncasecmp
#define _atoi64(x) atoll(x)
#define ZeroMemory(dst,size) memset(dst, 0, size)

#define __cdecl

#if !defined(TARGET_DARWIN) && !defined(TARGET_FREEBSD)
  #define APIENTRY  __stdcall
#else
  #define APIENTRY
#endif

#define __declspec(X)

struct CXHandle; // forward declaration
typedef CXHandle* HANDLE;

#if defined(TARGET_DARWIN)
typedef uint32_t        ULONG;
#else
typedef unsigned long   ULONG;
#endif

#define INVALID_HANDLE_VALUE     ((HANDLE)~0U)

#if defined(TARGET_DARWIN)
typedef int32_t      HRESULT;
#else
typedef unsigned long HRESULT;
#endif

#ifdef UNICODE
  typedef const wchar_t*  LPCTSTR;
#else
  typedef const char*     LPCTSTR;
#endif


typedef union _LARGE_INTEGER
{
  struct {
    uint32_t LowPart;
    int32_t HighPart;
  } u;
  long long QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

 typedef union _ULARGE_INTEGER {
  struct {
      uint32_t LowPart;
      uint32_t HighPart;
  } u;
  uint64_t QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

// Date / Time

typedef struct _SYSTEMTIME
{
  uint16_t wYear;
  uint16_t wMonth;
  uint16_t wDayOfWeek;
  uint16_t wDay;
  uint16_t wHour;
  uint16_t wMinute;
  uint16_t wSecond;
  uint16_t wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;


typedef struct _TIME_ZONE_INFORMATION {
  long Bias;
  wchar_t StandardName[32];
  SYSTEMTIME StandardDate;
  long StandardBias;
  wchar_t DaylightName[32];
  SYSTEMTIME DaylightDate;
  long DaylightBias;
} TIME_ZONE_INFORMATION, *PTIME_ZONE_INFORMATION, *LPTIME_ZONE_INFORMATION;

#define TIME_ZONE_ID_INVALID    ((uint32_t)0xFFFFFFFF)
#define TIME_ZONE_ID_UNKNOWN    0
#define TIME_ZONE_ID_STANDARD   1
#define TIME_ZONE_ID_DAYLIGHT   2

// Thread
#define THREAD_BASE_PRIORITY_LOWRT  15
#define THREAD_BASE_PRIORITY_MAX    2
#define THREAD_BASE_PRIORITY_MIN   -2
#define THREAD_BASE_PRIORITY_IDLE  -15
#define THREAD_PRIORITY_LOWEST          THREAD_BASE_PRIORITY_MIN
#define THREAD_PRIORITY_BELOW_NORMAL    (THREAD_PRIORITY_LOWEST+1)
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_HIGHEST         THREAD_BASE_PRIORITY_MAX
#define THREAD_PRIORITY_ABOVE_NORMAL    (THREAD_PRIORITY_HIGHEST-1)
#define THREAD_PRIORITY_ERROR_RETURN    (0x7fffffff)
#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT
#define THREAD_PRIORITY_IDLE            THREAD_BASE_PRIORITY_IDLE

// Network
#define SOCKADDR_IN struct sockaddr_in
#define IN_ADDR struct in_addr
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (~0)
#define closesocket(s)  close(s)
#define ioctlsocket(s, f, v) ioctl(s, f, v)

typedef int SOCKET;

// File
#define O_BINARY 0
#define O_TEXT   0
#define _O_TRUNC O_TRUNC
#define _O_RDONLY O_RDONLY
#define _O_WRONLY O_WRONLY

#if defined(TARGET_DARWIN) || defined(TARGET_FREEBSD)
  #define stat64 stat
  #define __stat64 stat
  #define fstat64 fstat
  typedef int64_t off64_t;
  #if defined(TARGET_FREEBSD)
    #define statfs64 statfs
  #endif
#else
  #define __stat64 stat64
#endif

struct _stati64 {
  dev_t st_dev;
  ino_t st_ino;
  unsigned short st_mode;
  short          st_nlink;
  short          st_uid;
  short          st_gid;
  dev_t st_rdev;
  int64_t  st_size;
  time_t _st_atime;
  time_t _st_mtime;
  time_t _st_ctime;
};

typedef struct _FILETIME
{
  uint32_t dwLowDateTime;
  uint32_t dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _WIN32_FIND_DATA
{
    uint32_t     dwFileAttributes;
    FILETIME  ftCreationTime;
    FILETIME  ftLastAccessTime;
    FILETIME  ftLastWriteTime;
    uint32_t     nFileSizeHigh;
    uint32_t     nFileSizeLow;
    uint32_t     dwReserved0;
    uint32_t     dwReserved1;
    char      cFileName[260];
    char      cAlternateFileName[14];
} WIN32_FIND_DATA, *PWIN32_FIND_DATA, *LPWIN32_FIND_DATA;

#define LPWIN32_FIND_DATAA LPWIN32_FIND_DATA

#define FILE_ATTRIBUTE_DIRECTORY           0x00000010

typedef struct _SECURITY_ATTRIBUTES {
  uint32_t nLength;
  void* lpSecurityDescriptor;
  int bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

#define FILE_BEGIN              0
#define FILE_CURRENT            1
#define FILE_END                2

#define _S_IFREG  S_IFREG
#define _S_IFDIR  S_IFDIR
#define MAX_PATH PATH_MAX

#define _stat stat

// Memory
typedef struct _MEMORYSTATUSEX
{
  uint32_t dwLength;
  uint32_t dwMemoryLoad;

  uint64_t ullTotalPhys;
  uint64_t ullAvailPhys;
  uint64_t ullTotalPageFile;
  uint64_t ullAvailPageFile;
  uint64_t ullTotalVirtual;
  uint64_t ullAvailVirtual;
} MEMORYSTATUSEX, *LPMEMORYSTATUSEX;

// Common HRESULT values
#ifndef NOERROR
#define NOERROR           (0L)
#endif
#ifndef S_OK
#define S_OK            (0L)
#endif
#ifndef E_FAIL
#define E_FAIL            (0x80004005L)
#endif
#ifndef E_OUTOFMEMORY
#define E_OUTOFMEMORY         (0x8007000EL)
#endif
#define FAILED(Status)            ((HRESULT)(Status)<0)

// Basic D3D stuff
typedef struct _RECT {
  long left;
  long top;
  long right;
  long bottom;
} RECT, *PRECT;

typedef enum _D3DFORMAT
{
  D3DFMT_A8R8G8B8         = 0x00000006,
  D3DFMT_DXT1         = 0x0000000C,
  D3DFMT_DXT2         = 0x0000000E,
  D3DFMT_DXT4         = 0x0000000F,
  D3DFMT_UNKNOWN          = 0xFFFFFFFF
} D3DFORMAT;


// CreateFile defines
#define FILE_FLAG_WRITE_THROUGH         0x80000000
#define FILE_FLAG_OVERLAPPED            0x40000000
#define FILE_FLAG_NO_BUFFERING          0x20000000
#define FILE_FLAG_RANDOM_ACCESS         0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN       0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE       0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS      0x02000000
#define FILE_FLAG_POSIX_SEMANTICS       0x01000000
#define FILE_FLAG_OPEN_REPARSE_POINT    0x00200000
#define FILE_FLAG_OPEN_NO_RECALL        0x00100000
#define FILE_FLAG_FIRST_PIPE_INSTANCE   0x00080000

#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5

#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_HIDDEN 0x00000002
#define FILE_ATTRIBUTE_SYSTEM 0x00000004
#define FILE_ATTRIBUTE_DIRECTORY  0x00000010

#define FILE_READ_DATA   ( 0x0001 )
#define FILE_WRITE_DATA  ( 0x0002 )
#define FILE_APPEND_DATA ( 0x0004 )

#define GENERIC_READ  FILE_READ_DATA
#define GENERIC_WRITE FILE_WRITE_DATA
#define FILE_SHARE_READ                  0x00000001
#define FILE_SHARE_WRITE                 0x00000002
#define FILE_SHARE_DELETE                0x00000004

#endif

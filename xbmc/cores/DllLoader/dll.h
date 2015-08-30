#ifndef __DLL_H_
#define __DLL_H_

#include "system.h"

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

typedef intptr_t (*FARPROC)(void);

extern "C" void* __stdcall dllLoadLibraryExtended(const char* file, const char* sourcedll);
extern "C" void* __stdcall dllLoadLibraryA(const char* file);
extern "C" void* __stdcall dllLoadLibraryExExtended(const char* lpLibFileName, HANDLE hFile, uint32_t dwFlags, const char* sourcedll);
extern "C" void* __stdcall dllLoadLibraryExA(const char* lpLibFileName, HANDLE hFile, uint32_t dwFlags);
extern "C" int __stdcall dllFreeLibrary(void* hLibModule);
extern "C" FARPROC __stdcall dllGetProcAddress(void* hModule, const char* function);
extern "C" void* __stdcall dllGetModuleHandleA(const char* lpModuleName);
extern "C" uint32_t __stdcall dllGetModuleFileNameA(void* hModule, char* lpFilename, uint32_t nSize);

#endif

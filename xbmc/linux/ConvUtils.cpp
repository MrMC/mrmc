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

#ifdef TARGET_POSIX

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

long long Int32x32To64(long Multiplier, long Multiplicand)
{
  long long result = Multiplier;
  result *= Multiplicand;
  return result;
}

uint32_t GetLastError()
{
  return errno;
}

void SetLastError(uint32_t dwErrCode)
{
  errno = dwErrCode;
}

#endif

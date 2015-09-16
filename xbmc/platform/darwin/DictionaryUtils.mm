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

#import "platform/darwin/DictionaryUtils.h"


//------------------------------------------------------------------------------------------
Boolean GetDictionaryBoolean(CFDictionaryRef theDict, const void* key)
{
  // get a boolean from the dictionary
  Boolean value = false;
  CFBooleanRef boolRef;
  boolRef = (CFBooleanRef)CFDictionaryGetValue(theDict, key);
  if (boolRef != NULL)
    value = CFBooleanGetValue(boolRef);
  return value;
}
//------------------------------------------------------------------------------------------
long GetDictionaryLong(CFDictionaryRef theDict, const void* key)
{
  // get a long from the dictionary
  long value = 0;
  CFNumberRef numRef;
  numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
  if (numRef != NULL)
    CFNumberGetValue(numRef, kCFNumberLongType, &value);
  return value;
}
//------------------------------------------------------------------------------------------
int GetDictionaryInt(CFDictionaryRef theDict, const void* key)
{
  // get a long from the dictionary
  int value = 0;
  CFNumberRef numRef;
  numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
  if (numRef != NULL)
    CFNumberGetValue(numRef, kCFNumberIntType, &value);
  return value;
}
//------------------------------------------------------------------------------------------
float GetDictionaryFloat(CFDictionaryRef theDict, const void* key)
{
  // get a long from the dictionary
  int value = 0;
  CFNumberRef numRef;
  numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
  if (numRef != NULL)
    CFNumberGetValue(numRef, kCFNumberFloatType, &value);
  return value;
}
//------------------------------------------------------------------------------------------
double GetDictionaryDouble(CFDictionaryRef theDict, const void* key)
{
  // get a long from the dictionary
  double value = 0.0;
  CFNumberRef numRef;
  numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
  if (numRef != NULL)
    CFNumberGetValue(numRef, kCFNumberDoubleType, &value);
  return value;
}

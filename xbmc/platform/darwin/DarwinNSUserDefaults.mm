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

#import "platform/darwin/DarwinNSUserDefaults.h"
#import "platform/darwin/DarwinUtils.h"
#import "platform/darwin/NSData+GZIP.h"
#import "utils/URIUtils.h"
#import "filesystem/SpecialProtocol.h"
#import "utils/log.h"

#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#import <Foundation/NSUserDefaults.h>


static bool firstLookup = true;


static bool translatePathIntoKey(const std::string &path, std::string &key)
{
  if (firstLookup)
  {
    NSDictionary<NSString *, id> *dict = [[NSUserDefaults standardUserDefaults] dictionaryRepresentation];
    for( NSString *aKey in [dict allKeys] )
    {
      // do something like a log:
      if ([aKey hasPrefix:@"/userdata/"])
      {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSData *nsdata = [defaults dataForKey:aKey];
        size_t size = [nsdata length];
        CLog::Log(LOGDEBUG, "nsuserdefaults: %s with size %ld", [aKey UTF8String], size);
      }
    }
    firstLookup = false;
  }
  size_t pos;
  std::string translated_key = CSpecialProtocol::TranslatePath(path);
  std::string userDataDir = URIUtils::AddFileToFolder(CDarwinUtils::GetUserHomeDirectory(), "userdata");
  if (translated_key.find(userDataDir) != std::string::npos)
  {
    if ((pos = translated_key.find("/userdata")) != std::string::npos)
    {
      key = translated_key.erase(0, pos);
      return true;
    }
  }

  return false;
}

bool CDarwinNSUserDefaults::Synchronize()
{
  return [[NSUserDefaults standardUserDefaults] synchronize] == YES;
}

bool CDarwinNSUserDefaults::GetKey(const std::string &key, std::string &value)
{
  if (!key.empty())
  {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *nsstring_key = [NSString stringWithUTF8String: key.c_str()];
    NSString *nsstring_value = [defaults stringForKey:nsstring_key];
    if (nsstring_value)
    {
      value = [nsstring_value UTF8String];
      if (!value.empty())
        return true;
    }
  }

  return false;
}

bool CDarwinNSUserDefaults::GetKeyData(const std::string &key, void* lpBuf, size_t &uiBufSize)
{
  if (!key.empty())
  {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *nsstring_key = [NSString stringWithUTF8String: key.c_str()];
    NSData *nsdata = [defaults dataForKey:nsstring_key];
    if (nsdata)
    {
      NSData *decompressed = nsdata;
      if ([nsdata isGzippedData])
      {
        decompressed = [nsdata gunzippedData];
      }

      bool copied = false;
      if (lpBuf != nullptr && decompressed.length <= uiBufSize)
      {
        memcpy(lpBuf, decompressed.bytes, decompressed.length);
        copied = true;
      }
      uiBufSize = [decompressed length];
      
      if (lpBuf != nullptr && !copied)
        return false;
      else
        return true;
    }
  }

  uiBufSize = 0;
  return false;
}

bool CDarwinNSUserDefaults::SetKey(const std::string &key, const std::string &value, bool synchronize)
{
  if (!key.empty() && !value.empty())
  {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    NSString *nsstring_key = [NSString stringWithUTF8String: key.c_str()];
    NSString *nsstring_value = [NSString stringWithUTF8String: value.c_str()];

    [defaults setObject:nsstring_value forKey:nsstring_key];
    if (synchronize)
      return [defaults synchronize] == YES;
    else
      return true;
  }

  return false;
}

bool CDarwinNSUserDefaults::SetKeyData(const std::string &key, const void* lpBuf, size_t uiBufSize, bool synchronize)
{
  if (!key.empty() && lpBuf != nullptr && uiBufSize > 0)
  {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    NSString *nsstring_key = [NSString stringWithUTF8String: key.c_str()];
    NSData *nsdata_value = [NSData dataWithBytes:lpBuf length:uiBufSize];
    NSData *compressed = [nsdata_value gzippedData];
    CLog::Log(LOGDEBUG, "NSUSerDefaults: compressed %s from %ld to %ld", key.c_str(), uiBufSize, [compressed length]);
    
    [defaults setObject:compressed forKey:nsstring_key];
    if (synchronize)
      return [defaults synchronize] == YES;
    else
      return true;
  }
  
  return false;
}

bool CDarwinNSUserDefaults::DeleteKey(const std::string &key, bool synchronize)
{
  if (!key.empty())
  {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *nsstring_key = [NSString stringWithUTF8String: key.c_str()];
    [defaults removeObjectForKey:nsstring_key];
    if (synchronize)
      return [defaults synchronize] == YES;
    else
      return true;
  }

  return false;
}

bool CDarwinNSUserDefaults::KeyExists(const std::string &key)
{
  if (!key.empty())
  {
    NSString *nsstring_key = [NSString stringWithUTF8String: key.c_str()];
    if ([[NSUserDefaults standardUserDefaults] objectForKey:nsstring_key])
      return true;
  }

  return false;
}

bool CDarwinNSUserDefaults::IsKeyFromPath(const std::string &path)
{
  std::string translated_key;
  if (translatePathIntoKey(path, translated_key) && !translated_key.empty())
  {
    CLog::Log(LOGDEBUG, "found key %s", translated_key.c_str());
    return true;
  }

  return false;
}

bool CDarwinNSUserDefaults::GetKeyFromPath(const std::string &path, std::string &value)
{
  std::string translated_key;
  if (translatePathIntoKey(path, translated_key) && !translated_key.empty())
    return CDarwinNSUserDefaults::GetKey(translated_key, value);

  return false;
}

bool CDarwinNSUserDefaults::GetKeyDataFromPath(const std::string &path, void* lpBuf, size_t &uiBufSize)
{
  std::string translated_key;
  if (translatePathIntoKey(path, translated_key) && !translated_key.empty())
    return CDarwinNSUserDefaults::GetKeyData(translated_key, lpBuf, uiBufSize);
  
  return false;
}

bool CDarwinNSUserDefaults::SetKeyFromPath(const std::string &path, const std::string &value, bool synchronize)
{
  std::string translated_key;
  if (translatePathIntoKey(path, translated_key) && !translated_key.empty() && !value.empty())
    return CDarwinNSUserDefaults::SetKey(translated_key, value, synchronize);

  return false;
}

bool CDarwinNSUserDefaults::SetKeyDataFromPath(const std::string &path, const void* lpBuf, size_t uiBufSize, bool synchronize)
{
  std::string translated_key;
  if (translatePathIntoKey(path, translated_key) && !translated_key.empty())
    return CDarwinNSUserDefaults::SetKeyData(translated_key, lpBuf, uiBufSize, synchronize);
  
  return false;

}

bool CDarwinNSUserDefaults::DeleteKeyFromPath(const std::string &path, bool synchronize)
{
  std::string translated_key;
  if (translatePathIntoKey(path, translated_key) && !translated_key.empty())
    return CDarwinNSUserDefaults::DeleteKey(translated_key, synchronize);

  return false;
}

bool CDarwinNSUserDefaults::KeyFromPathExists(const std::string &path)
{
  std::string translated_key;
  if (translatePathIntoKey(path, translated_key) && !translated_key.empty())
    return CDarwinNSUserDefaults::KeyExists(translated_key);

  return false;
}

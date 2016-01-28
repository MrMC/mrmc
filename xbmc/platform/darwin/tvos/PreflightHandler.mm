/*
 *      Copyright (C) 2016 Team MrMC
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

#import "PreflightHandler.h"

#include "URL.h"

#import "filesystem/File.h"
#import "filesystem/TVOSFile.h"
#import "filesystem/posix/PosixFile.h"
#import "platform/darwin/DarwinUtils.h"
#import "platform/darwin/NSLogDebugHelpers.h"

#import <UIKit/UIKit.h>

uint64_t CPreflightHandler::NSUserDefaultsSize()
{
  NSString* libraryDir = [NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0];
  NSString* filepath = [libraryDir stringByAppendingPathComponent:[NSString stringWithFormat:@"/Preferences/%@.plist", [[NSBundle mainBundle] bundleIdentifier]]];
  uint64_t fileSize = [[[NSFileManager defaultManager] attributesOfItemAtPath:filepath error:nil][NSFileSize] unsignedLongLongValue];

  return fileSize;
}

void CPreflightHandler::NSUserDefaultsPurge(const char *prefix)
{
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSDictionary<NSString *, id> *dict = [defaults dictionaryRepresentation];
  for (NSString *aKey in [dict allKeys] )
  {
    if ([aKey hasPrefix:[NSString stringWithUTF8String:prefix]])
    {
      [defaults removeObjectForKey:aKey];
        ILOG(@"nsuserdefaults: removing %s", [aKey UTF8String]);
    }
  }
}

void CPreflightHandler::MigrateUserdataXMLToNSUserDefaults()
{
  // MrMC below 1.3 has an opps bug, NSUserDefaults was keyed to "Caches/userdata"
  // but GetUserHomeDirectory changed to "Caches/home/userdata" so every xml was saved to
  // Caches dir instead of into persistent NSUserDefaults. The whole point of this is to
  // fix this error.

  ILOG(@"MigrateUserdataXMLToNSUserDefaults: "
    "NSUserDefaultsSize(%lld)", NSUserDefaultsSize());

  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

  // if already migrated, we are done.
  NSString *migration_key = @"UserdataMigrated";
  if (![defaults objectForKey:migration_key])
  {
    ILOG(@"MigrateUserdataXMLToNSUserDefaults: migration starting");

    // first we remove any old keys, these are
    // not being used (opps) so we can just delete them
    NSUserDefaultsPurge("Caches/userdata");

    // now search for all xxx.xml files in the
    // user home directory and copy them into NSUserDefaults
    std::string userHome = CDarwinUtils::GetUserHomeDirectory();
    NSString *nsPath = [NSString stringWithUTF8String:userHome.c_str()];

    NSDirectoryEnumerator* enumerator = [[NSFileManager defaultManager] enumeratorAtPath:nsPath];
    while (NSString *file = [enumerator nextObject])
    {
      NSString* fullPath = [nsPath stringByAppendingPathComponent:file];

      // skip the Thumbnails directory, there are no xml files present
      // and it can be very large.
      if ([fullPath containsString:@"Thumbnails"])
        continue;

      // check if it's a directory
      BOOL isDirectory = NO;
      [[NSFileManager defaultManager] fileExistsAtPath:fullPath isDirectory: &isDirectory];
      if (isDirectory == NO)
      {
        // check if the file extension is 'xml'
        if ([[file pathExtension] isEqualToString: @"xml"])
        {
          // log what we are doing
          ILOG(@"MigrateUserdataXMLToNSUserDefaults: "
            "Found -> %s", [fullPath UTF8String]);

          // we cannot use a Cfile for src, it will get mapped into a CTVOSFile
          const CURL srcUrl([fullPath UTF8String]);
          XFILE::CPosixFile srcfile;
          if (srcfile.Open(srcUrl))
          {
            // we could use a CFile here but WTH, let's go direct
            const CURL dtsUrl([fullPath UTF8String]);
            XFILE::CTVOSFile dstfile;
            if (dstfile.OpenForWrite(dtsUrl, true))
            {
              size_t iBufferSize = 128 * 1024;
              XFILE::auto_buffer buffer(iBufferSize);

              while (true)
              {
                // read data
                ssize_t iread = srcfile.Read(buffer.get(), iBufferSize);
                if (iread == 0)
                  break;
                else if (iread < 0)
                {
                  ELOG(@"MigrateUserdataXMLToNSUserDefaults: "
                    "Failed read from file %s", srcUrl.Get().c_str());
                  break;
                }

                // write data and make sure we managed to write it all
                ssize_t iwrite = 0;
                while (iwrite < iread)
                {
                  ssize_t iwrite2 = dstfile.Write(buffer.get() + iwrite, iread - iwrite);
                  if (iwrite2 <= 0)
                    break;
                  iwrite += iwrite2;
                }

                if (iwrite != iread)
                {
                  ELOG(@"MigrateUserdataXMLToNSUserDefaults: "
                    "Failed write to file %s", dtsUrl.Get().c_str());
                  break;
                }
              }
              dstfile.Close();
            }
            srcfile.Close();
            srcfile.Delete(srcUrl);
          }
        }
      }
    }

    // set the migration_key to a known value and
    // write it. we do this for future opps :)
    [defaults setObject:@"1" forKey:migration_key];
    [defaults synchronize];

    ILOG(@"MigrateUserdataXMLToNSUserDefaults: migration finished");
    ILOG(@"MigrateUserdataXMLToNSUserDefaults: "
      "NSUserDefaultsSize(%lld)", NSUserDefaultsSize());
  }
}

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

#include "DynamicDll.h"

#include <libsmbclient.h>

// stupid off_t, the size depends on arch
// but we want smb to work with >4GB files on embedded.
// libsmbclient is diddled in depends to force off_t to 64 bit
// but as the header API must match here, we need to diddle it.
#if defined(TARGET_ANDROID)
  #define SMB_OFF_T off64_t
#else
  #define SMB_OFF_T off_t
#endif


class DllLibSMBInterface
{
public:
  virtual ~DllLibSMBInterface() {}
  
  virtual SMBCCTX*  smbc_new_context(void)=0;
  virtual int       smbc_free_context(SMBCCTX *context, int shutdown_ctx)=0;
  virtual SMBCCTX*  smbc_init_context(SMBCCTX *context)=0;
  virtual int       smbc_init(smbc_get_auth_data_fn fn, int debug)=0;
  virtual SMBCCTX*  smbc_set_context(SMBCCTX *new_context)=0;
  virtual int       smbc_open(const char *furl, int flags, mode_t mode)=0;
  virtual int       smbc_creat(const char *furl, mode_t mode)=0;
  virtual ssize_t   smbc_read(int fd, void *buf, size_t bufsize)=0;
  virtual ssize_t   smbc_write(int fd, void *buf, size_t bufsize)=0;
  virtual SMB_OFF_T smbc_lseek(int fd, SMB_OFF_T offset, int whence)=0;
  virtual int       smbc_close(int fd)=0;
  virtual int       smbc_unlink(const char *furl)=0;
  virtual int       smbc_rename(const char *ourl, const char *nurl)=0;
  virtual int       smbc_opendir(const char *durl)=0;
  virtual int       smbc_closedir(int dh)=0;
  virtual struct smbc_dirent* smbc_readdir(unsigned int dh)=0;
  virtual int       smbc_mkdir(const char *durl, mode_t mode)=0;
  virtual int       smbc_rmdir(const char *durl)=0;
  virtual int       smbc_stat(const char *url, struct stat *st)=0;
  virtual int       smbc_fstat(int fd, struct stat *st)=0;
  virtual int       smbc_getxattr(const char *url, const char *name, const void *value, size_t size)=0;
};

class DllLibSMB : public DllDynamic, DllLibSMBInterface
{
#if defined(TARGET_DARWIN_IOS) && !defined(__x86_64__)
  DECLARE_DLL_WRAPPER(DllLibSMB, "libsmbclient.framework/libsmbclient")
#else
  DECLARE_DLL_WRAPPER(DllLibSMB, DLL_PATH_LIBSMBCLIENT)
#endif
  DEFINE_METHOD0(SMBCCTX*,  smbc_new_context)
  DEFINE_METHOD2(int,       smbc_free_context, (SMBCCTX *p1, int p2))
  DEFINE_METHOD1(SMBCCTX*,  smbc_init_context, (SMBCCTX *p1))
  DEFINE_METHOD2(int,       smbc_init,         (smbc_get_auth_data_fn p1, int p2))
  DEFINE_METHOD1(SMBCCTX*,  smbc_set_context,  (SMBCCTX *p1))
  DEFINE_METHOD3(int,       smbc_open,         (const char *p1, int p2, mode_t p3))
  DEFINE_METHOD2(int,       smbc_creat,        (const char *p1, mode_t p2))
  DEFINE_METHOD3(ssize_t,   smbc_read,         (int p1, void *p2, size_t p3))
  DEFINE_METHOD3(ssize_t,   smbc_write,        (int p1, void *p2, size_t p3))
  DEFINE_METHOD3(SMB_OFF_T, smbc_lseek,        (int p1, SMB_OFF_T p2, int p3))
  DEFINE_METHOD1(int,       smbc_close,        (int p1))
  DEFINE_METHOD1(int,       smbc_unlink,       (const char *p1))
  DEFINE_METHOD2(int,       smbc_rename,       (const char *p1, const char *p2))
  DEFINE_METHOD1(int,       smbc_opendir,      (const char *p1))
  DEFINE_METHOD1(int,       smbc_closedir,     (int p1))
  DEFINE_METHOD1(struct smbc_dirent*, smbc_readdir, (unsigned int p1))
  DEFINE_METHOD2(int,       smbc_mkdir,        (const char *p1, mode_t p2))
  DEFINE_METHOD1(int,       smbc_rmdir,        (const char *p1))
  DEFINE_METHOD2(int,       smbc_stat,         (const char *p1, struct stat *p2))
  DEFINE_METHOD2(int,       smbc_fstat,        (int p1, struct stat *p2))
  DEFINE_METHOD4(int,       smbc_getxattr,     (const char *p1, const char *p2, const void *p3, size_t p4))

  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD_RENAME(smbc_new_context,   smbc_new_context)
    RESOLVE_METHOD_RENAME(smbc_free_context,  smbc_free_context)
    RESOLVE_METHOD_RENAME(smbc_init_context,  smbc_init_context)
    RESOLVE_METHOD_RENAME(smbc_init,          smbc_init)
    RESOLVE_METHOD_RENAME(smbc_set_context,   smbc_set_context)
    RESOLVE_METHOD_RENAME(smbc_open,          smbc_open)
    RESOLVE_METHOD_RENAME(smbc_creat,         smbc_creat)
    RESOLVE_METHOD_RENAME(smbc_read,          smbc_read)
    RESOLVE_METHOD_RENAME(smbc_write,         smbc_write)
    RESOLVE_METHOD_RENAME(smbc_lseek,         smbc_lseek)
    RESOLVE_METHOD_RENAME(smbc_close,         smbc_close)
    RESOLVE_METHOD_RENAME(smbc_unlink,        smbc_unlink)
    RESOLVE_METHOD_RENAME(smbc_rename,        smbc_rename)
    RESOLVE_METHOD_RENAME(smbc_opendir,       smbc_opendir)
    RESOLVE_METHOD_RENAME(smbc_closedir,      smbc_closedir)
    RESOLVE_METHOD_RENAME(smbc_readdir,       smbc_readdir)
    RESOLVE_METHOD_RENAME(smbc_mkdir,         smbc_mkdir)
    RESOLVE_METHOD_RENAME(smbc_rmdir,         smbc_rmdir)
    RESOLVE_METHOD_RENAME(smbc_stat,          smbc_stat)
    RESOLVE_METHOD_RENAME(smbc_fstat,         smbc_fstat)
    RESOLVE_METHOD_RENAME(smbc_getxattr,      smbc_getxattr)
  END_METHOD_RESOLVE()
};

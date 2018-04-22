#pragma once
/*
 *      Copyright (C) 2018 Team MrMC
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

#ifdef __cplusplus
extern "C" {
#endif  
#include <smb2/libsmb2.h>
#ifdef __cplusplus
}
#endif

class DllLibSMB2Interface
{
public:
  virtual ~DllLibSMB2Interface() {}

  virtual struct smb2_context *smb2_init_context(void)=0;
  virtual void smb2_destroy_context(struct smb2_context *smb2)=0;
  virtual t_socket smb2_get_fd(struct smb2_context *smb2)=0;
  virtual int smb2_which_events(struct smb2_context *smb2)=0;
  virtual int smb2_service(struct smb2_context *smb2, int revents)=0;
  virtual void smb2_set_security_mode(struct smb2_context *smb2, uint16_t security_mode)=0;
  virtual void smb2_set_user(struct smb2_context *smb2, const char *user)=0;
  virtual void smb2_set_password(struct smb2_context *smb2, const char *password)=0;
  virtual void smb2_set_domain(struct smb2_context *smb2, const char *domain)=0;
  virtual void smb2_set_workstation(struct smb2_context *smb2, const char *workstation)=0;
  virtual int smb2_connect_share(struct smb2_context *smb2,
    const char *server, const char *share, const char *user)=0;
  virtual int smb2_disconnect_share(struct smb2_context *smb2)=0;
  virtual const char *smb2_get_error(struct smb2_context *smb2)=0;

  virtual int smb2_opendir_async(struct smb2_context *smb2,
    const char *path, smb2_command_cb cb, void *cb_data)=0;
  virtual struct smb2dir *smb2_opendir(struct smb2_context *smb2, const char *path)=0;
  virtual void smb2_closedir(struct smb2_context *smb2, struct smb2dir *smb2dir)=0;
  virtual struct smb2dirent *smb2_readdir(struct smb2_context *smb2, struct smb2dir *smb2dir)=0;

  virtual int smb2_open_async(struct smb2_context *smb2,
    const char *path, int flags, smb2_command_cb cb, void *cb_data)=0;
  virtual struct smb2fh *smb2_open(struct smb2_context *smb2, const char *path, int flags)=0;
  virtual int smb2_close_async(struct smb2_context *smb2,
    struct smb2fh *fh, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_close(struct smb2_context *smb2, struct smb2fh *fh)=0;

  virtual uint32_t smb2_get_max_read_size(struct smb2_context *smb2)=0;
  virtual uint32_t smb2_get_max_write_size(struct smb2_context *smb2)=0;
  virtual int smb2_read_async(struct smb2_context *smb2, struct smb2fh *fh,
    uint8_t *buf, uint32_t count, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_read(struct smb2_context *smb2, struct smb2fh *fh,
    uint8_t *buf, uint32_t count)=0;
  virtual int smb2_write_async(struct smb2_context *smb2, struct smb2fh *fh,
    uint8_t *buf, uint32_t count, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_write(struct smb2_context *smb2,
    struct smb2fh *fh, uint8_t *buf, uint32_t count)=0;
  virtual int smb2_lseek(struct smb2_context *smb2,
    struct smb2fh *fh, int64_t offset, int whence, uint64_t *current_offset)=0;
  virtual int smb2_unlink_async(struct smb2_context *smb2,
    const char *path, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_unlink(struct smb2_context *smb2, const char *path)=0;
  virtual int smb2_rmdir_async(struct smb2_context *smb2,
    const char *path, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_rmdir(struct smb2_context *smb2, const char *path)=0;
  virtual int smb2_mkdir_async(struct smb2_context *smb2,
    const char *path, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_mkdir(struct smb2_context *smb2, const char *path)=0;
  virtual int smb2_fstat_async(struct smb2_context *smb2,
    struct smb2fh *fh, struct smb2_stat_64 *st, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_fstat(struct smb2_context *smb2,
    struct smb2fh *fh, struct smb2_stat_64 *st)=0;
  virtual int smb2_stat_async(struct smb2_context *smb2,
    const char *path, struct smb2_stat_64 *st, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_stat(struct smb2_context *smb2, const char *path, struct smb2_stat_64 *st)=0;
  virtual int smb2_rename_async(struct smb2_context *smb2,
    const char *oldpath, const char *newpath, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_ftruncate_async(struct smb2_context *smb2,
    struct smb2fh *fh, uint64_t length, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_ftruncate(struct smb2_context *smb2, struct smb2fh *fh, uint64_t length)=0;
  virtual int smb2_echo_async(struct smb2_context *smb2, smb2_command_cb cb, void *cb_data)=0;
  virtual int smb2_echo(struct smb2_context *smb2)=0;
};

class DllLibSMB2 : public DllDynamic, DllLibSMB2Interface
{
#if defined(TARGET_DARWIN_IOS) && !defined(__x86_64__)
  DECLARE_DLL_WRAPPER(DllLibSMB2, "libsmb2.framework/libsmb2")
#else
  DECLARE_DLL_WRAPPER(DllLibSMB2, DLL_PATH_LIBSMB2)
#endif
  DEFINE_METHOD0(struct smb2_context*,  smb2_init_context)
  DEFINE_METHOD1(void,                  smb2_destroy_context, (struct smb2_context *p1))
  DEFINE_METHOD1(t_socket,              smb2_get_fd, (struct smb2_context *p1))
  DEFINE_METHOD1(int,                   smb2_which_events, (struct smb2_context *p1))
  DEFINE_METHOD2(int,                   smb2_service, (struct smb2_context *p1, int p2))
  DEFINE_METHOD2(void,                  smb2_set_security_mode, (struct smb2_context *p1, uint16_t p2))
  DEFINE_METHOD2(void,                  smb2_set_user, (struct smb2_context *p1, const char *p2))
  DEFINE_METHOD2(void,                  smb2_set_password, (struct smb2_context *p1, const char *p2))
  DEFINE_METHOD2(void,                  smb2_set_domain, (struct smb2_context *p1, const char *p2))
  DEFINE_METHOD2(void,                  smb2_set_workstation, (struct smb2_context *p1, const char *p2))

  DEFINE_METHOD4(int,                   smb2_connect_share, (struct smb2_context *p1, const char *p2, const char *p3, const char *p4))
  DEFINE_METHOD1(int,                   smb2_disconnect_share, (struct smb2_context *p1))
  DEFINE_METHOD1(const char*,           smb2_get_error, (struct smb2_context *p1))

  DEFINE_METHOD4(int,                   smb2_opendir_async, (struct smb2_context *p1, const char *p2, smb2_command_cb p3, void *p4))
  DEFINE_METHOD2(struct smb2dir*,       smb2_opendir, (struct smb2_context *p1, const char *p2))
  DEFINE_METHOD2(void,                  smb2_closedir, (struct smb2_context *p1, struct smb2dir *p2))
  DEFINE_METHOD2(struct smb2dirent*,    smb2_readdir, (struct smb2_context *p1, struct smb2dir *p2))
  DEFINE_METHOD5(int,                   smb2_open_async, (struct smb2_context *p1, const char *p2, int p3, smb2_command_cb p4, void *p5))
  DEFINE_METHOD3(struct smb2fh*,        smb2_open, (struct smb2_context *p1, const char *p2, int p3))
  DEFINE_METHOD4(int,                   smb2_close_async, (struct smb2_context *p1, struct smb2fh *p2, smb2_command_cb p3, void *p4))
  DEFINE_METHOD2(int,                   smb2_close, (struct smb2_context *p1, struct smb2fh *p2))

  DEFINE_METHOD1(uint32_t,              smb2_get_max_read_size, (struct smb2_context *p1))
  DEFINE_METHOD1(uint32_t,              smb2_get_max_write_size, (struct smb2_context *p1))
  DEFINE_METHOD6(int,                   smb2_read_async, (struct smb2_context *p1, struct smb2fh *p2, uint8_t *p3, uint32_t p4, smb2_command_cb p5, void *p6))
  DEFINE_METHOD4(int,                   smb2_read, (struct smb2_context *p1, struct smb2fh *p2, uint8_t *p3, uint32_t p4))
  DEFINE_METHOD6(int,                   smb2_write_async, (struct smb2_context *p1, struct smb2fh *p2, uint8_t *p3, uint32_t p4, smb2_command_cb p5, void *p6))
  DEFINE_METHOD4(int,                   smb2_write, (struct smb2_context *p1, struct smb2fh *p2, uint8_t *p3, uint32_t p4))
  DEFINE_METHOD5(int,                   smb2_lseek, (struct smb2_context *p1, struct smb2fh *p2, int64_t p3, int p4, uint64_t *p5))
  DEFINE_METHOD4(int,                   smb2_unlink_async, (struct smb2_context *p1, const char *p2, smb2_command_cb p3, void *p4))
  DEFINE_METHOD2(int,                   smb2_unlink, (struct smb2_context *p1, const char *p2))
  DEFINE_METHOD4(int,                   smb2_rmdir_async, (struct smb2_context *p1, const char *p2, smb2_command_cb p3, void *p4))
  DEFINE_METHOD2(int,                   smb2_rmdir, (struct smb2_context *p1, const char *p2))
  DEFINE_METHOD4(int,                   smb2_mkdir_async, (struct smb2_context *p1, const char *p2, smb2_command_cb p3, void *p4))
  DEFINE_METHOD2(int,                   smb2_mkdir, (struct smb2_context *p1, const char *p2))
  DEFINE_METHOD5(int,                   smb2_fstat_async, (struct smb2_context *p1, struct smb2fh *p2, struct smb2_stat_64 *p3, smb2_command_cb p4, void *p5))
  DEFINE_METHOD3(int,                   smb2_fstat, (struct smb2_context *p1, struct smb2fh *p2, struct smb2_stat_64 *p3))
  DEFINE_METHOD5(int,                   smb2_stat_async, (struct smb2_context *p1, const char *p2, struct smb2_stat_64 *p3, smb2_command_cb p4, void *p5))
  DEFINE_METHOD3(int,                   smb2_stat, (struct smb2_context *p1, const char *p2, struct smb2_stat_64 *p3))
  DEFINE_METHOD5(int,                   smb2_rename_async, (struct smb2_context *p1, const char *p2, const char *p3, smb2_command_cb p4, void *p5))
  DEFINE_METHOD5(int,                   smb2_ftruncate_async, (struct smb2_context *p1, struct smb2fh *p2, uint64_t p3, smb2_command_cb p4, void *p5))
  DEFINE_METHOD3(int,                   smb2_ftruncate, (struct smb2_context *p1, struct smb2fh *p2, uint64_t p3))
  DEFINE_METHOD3(int,                   smb2_echo_async, (struct smb2_context *p1, smb2_command_cb p2, void *p3))
  DEFINE_METHOD1(int,                   smb2_echo, (struct smb2_context *p1))

  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD_RENAME(smb2_init_context,        smb2_init_context)
    RESOLVE_METHOD_RENAME(smb2_destroy_context,     smb2_destroy_context)
    RESOLVE_METHOD_RENAME(smb2_get_fd,              smb2_get_fd)
    RESOLVE_METHOD_RENAME(smb2_which_events,        smb2_which_events)
    RESOLVE_METHOD_RENAME(smb2_service,             smb2_service)
    RESOLVE_METHOD_RENAME(smb2_set_security_mode,   smb2_set_security_mode)
    RESOLVE_METHOD_RENAME(smb2_set_user,            smb2_set_user)
    RESOLVE_METHOD_RENAME(smb2_set_password,        smb2_set_password)
    RESOLVE_METHOD_RENAME(smb2_set_domain,          smb2_set_domain)
    RESOLVE_METHOD_RENAME(smb2_set_workstation,     smb2_set_workstation)

    RESOLVE_METHOD_RENAME(smb2_connect_share,       smb2_connect_share)
    RESOLVE_METHOD_RENAME(smb2_disconnect_share,    smb2_disconnect_share)
    RESOLVE_METHOD_RENAME(smb2_get_error,           smb2_get_error)

    RESOLVE_METHOD_RENAME(smb2_opendir_async,       smb2_opendir_async)
    RESOLVE_METHOD_RENAME(smb2_opendir,             smb2_opendir)
    RESOLVE_METHOD_RENAME(smb2_closedir,            smb2_closedir)
    RESOLVE_METHOD_RENAME(smb2_readdir,             smb2_readdir)
    RESOLVE_METHOD_RENAME(smb2_open_async,          smb2_open_async)
    RESOLVE_METHOD_RENAME(smb2_open,                smb2_open)
    RESOLVE_METHOD_RENAME(smb2_close_async,         smb2_close_async)
    RESOLVE_METHOD_RENAME(smb2_close,               smb2_close)

    RESOLVE_METHOD_RENAME(smb2_get_max_read_size,   smb2_get_max_read_size)
    RESOLVE_METHOD_RENAME(smb2_get_max_write_size,  smb2_get_max_write_size)
    RESOLVE_METHOD_RENAME(smb2_read_async,          smb2_read_async)
    RESOLVE_METHOD_RENAME(smb2_read,                smb2_read)
    RESOLVE_METHOD_RENAME(smb2_write_async,         smb2_write_async)
    RESOLVE_METHOD_RENAME(smb2_write,               smb2_write)
    RESOLVE_METHOD_RENAME(smb2_lseek,               smb2_lseek)
    RESOLVE_METHOD_RENAME(smb2_unlink_async,        smb2_unlink_async)
    RESOLVE_METHOD_RENAME(smb2_unlink,              smb2_unlink)
    RESOLVE_METHOD_RENAME(smb2_rmdir_async,         smb2_rmdir_async)
    RESOLVE_METHOD_RENAME(smb2_rmdir,               smb2_rmdir)
    RESOLVE_METHOD_RENAME(smb2_mkdir_async,         smb2_mkdir_async)
    RESOLVE_METHOD_RENAME(smb2_mkdir,               smb2_mkdir)

    RESOLVE_METHOD_RENAME(smb2_fstat_async,         smb2_fstat_async)
    RESOLVE_METHOD_RENAME(smb2_fstat,               smb2_fstat)
    RESOLVE_METHOD_RENAME(smb2_stat_async,          smb2_stat_async)
    RESOLVE_METHOD_RENAME(smb2_stat,                smb2_stat)
    RESOLVE_METHOD_RENAME(smb2_rename_async,        smb2_rename_async)
    RESOLVE_METHOD_RENAME(smb2_ftruncate_async,     smb2_ftruncate_async)
    RESOLVE_METHOD_RENAME(smb2_ftruncate,           smb2_ftruncate)
    RESOLVE_METHOD_RENAME(smb2_echo_async,          smb2_echo_async)
    RESOLVE_METHOD_RENAME(smb2_echo,                smb2_echo)
  END_METHOD_RESOLVE()
};

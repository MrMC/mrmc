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
#include "threads/CriticalSection.h"
#include "threads/SingleLock.h"

#ifdef __cplusplus
extern "C" {
#endif  
#include <bdsm/bdsm.h>
#ifdef __cplusplus
}
#endif

class DllLibDSMInterface
{
public:
  virtual ~DllLibDSMInterface() {}

  // netbios_ns.h
  // netbios name service
                          // used with netbios_ns_discover_callbacks
  virtual const char     *netbios_ns_entry_name(netbios_ns_entry *entry)=0;
  virtual const char     *netbios_ns_entry_group(netbios_ns_entry *entry)=0;
  virtual uint32_t        netbios_ns_entry_ip(netbios_ns_entry *entry)=0;
  virtual char            netbios_ns_entry_type(netbios_ns_entry *entry)=0;
  virtual int             netbios_ns_resolve(const char *name, char type, uint32_t *addr)=0;
  virtual const char     *netbios_ns_inverse(uint32_t ip)=0;
  virtual int             netbios_ns_discover_start(unsigned int broadcast_timeout,
                            netbios_ns_discover_callbacks *callbacks)=0;
  virtual int             netbios_ns_discover_stop()=0;

  // smb_session.h
  // functions to connect and authenticate to an SMB server
  virtual smb_session*    smb_session_new()=0;
  virtual void            smb_session_destroy(smb_session *s)=0;
  virtual int             smb_session_state(smb_session *s)=0;
  virtual void            smb_session_set_creds(smb_session *s, const char *domain,
                            const char *login, const char *password)=0;
  virtual int             smb_session_connect(smb_session *s, const char *hostname,
                            uint32_t ip, int transport)=0;
  virtual int             smb_session_login(smb_session *s)=0;
  virtual int             smb_session_is_guest(smb_session *s)=0;
  virtual const char     *smb_session_server_name(smb_session *s)=0;
  virtual int             smb_session_supports(smb_session *s, int what)=0;

  // smb_share.h
  // list and connect to SMB shares
  virtual size_t          smb_share_get_list(smb_session *s, smb_share_list *list)=0;
  virtual size_t          smb_share_list_count(smb_share_list list)=0;
  virtual const char     *smb_share_list_at(smb_share_list list, size_t index)=0;
  virtual void            smb_share_list_destroy(smb_share_list list)=0;
  virtual smb_tid         smb_tree_connect(smb_session *s, const char *name)=0;
  virtual int             smb_tree_disconnect(smb_session *s, smb_tid tid)=0;

  // smb_stat.h
  virtual smb_stat_list   smb_find(smb_session *s, smb_tid tid, const char *pattern)=0;
  virtual smb_stat        smb_fstat(smb_session *s, smb_tid tid, const char *path)=0;
  virtual smb_stat        smb_stat_fd(smb_session *s, smb_fd fd)=0;
  virtual void            smb_stat_destroy(smb_stat stat)=0;
  virtual size_t          smb_stat_list_count(smb_stat_list list)=0;
  virtual smb_stat        smb_stat_list_at(smb_stat_list list, size_t index)=0;
  virtual void            smb_stat_list_destroy(smb_stat_list list)=0;
  virtual const char     *smb_stat_name(smb_stat info)=0;
  virtual uint64_t        smb_stat_get(smb_stat info, int what)=0;

  // smb_dir.h
  virtual uint32_t        smb_directory_rm(smb_session *s, smb_tid tid, const char *path)=0;
  virtual uint32_t        smb_directory_create(smb_session *s, smb_tid tid, const char *path)=0;

  // smb_file.h
  virtual smb_fd          smb_fopen(smb_session *s, smb_tid tid, const char *path, uint32_t mod)=0;
  virtual void            smb_fclose(smb_session *s, smb_fd fd)=0;
  virtual ssize_t         smb_fread(smb_session *s, smb_fd fd, void *buf, size_t buf_size)=0;
  virtual ssize_t         smb_fwrite(smb_session *s, smb_fd fd, const void *buf, size_t buf_size)=0;
  virtual ssize_t         smb_fseek(smb_session *s, smb_fd fd, ssize_t offset, int whence)=0;
  virtual uint32_t        smb_file_rm(smb_session *s, smb_tid tid, const char *path)=0;
  virtual int             smb_file_mv(smb_session *s, smb_tid tid, const char *old_path, const char *new_path)=0;
};

class DllLibDSM : public DllDynamic, DllLibDSMInterface
{
#if defined(TARGET_DARWIN_IOS) && !defined(__x86_64__)
  DECLARE_DLL_WRAPPER(DllLibDSM, "libdsm.framework/libdsm")
#else
  DECLARE_DLL_WRAPPER(DllLibDSM, DLL_PATH_LIBDSM)
#endif
  DEFINE_METHOD1(const char*,   netbios_ns_entry_name,  (netbios_ns_entry *p1))
  DEFINE_METHOD1(const char*,   netbios_ns_entry_group, (netbios_ns_entry *p1))
  DEFINE_METHOD1(uint32_t,      netbios_ns_entry_ip,    (netbios_ns_entry *p1))
  DEFINE_METHOD1(char,          netbios_ns_entry_type,  (netbios_ns_entry *p1))
  DEFINE_METHOD0(netbios_ns*,   netbios_ns_new)
  DEFINE_METHOD1(void,          netbios_ns_destroy,     (netbios_ns *p1))
  DEFINE_METHOD4(int,           netbios_ns_resolve,     (netbios_ns *p1, const char *p2, char p3, uint32_t *p4))
  DEFINE_METHOD2(const char*,   netbios_ns_inverse,     (netbios_ns *p1, uint32_t p2))
  DEFINE_METHOD3(int,           netbios_ns_discover_start, (netbios_ns *p1, unsigned int p2, netbios_ns_discover_callbacks *p3))
  DEFINE_METHOD1(int,           netbios_ns_discover_stop, (netbios_ns *p1))

  DEFINE_METHOD0(smb_session*,  smb_session_new)
  DEFINE_METHOD1(void,          smb_session_destroy,    (smb_session *p1))
  DEFINE_METHOD1(int,           smb_session_state,      (smb_session *p1))
  DEFINE_METHOD4(void,          smb_session_set_creds,  (smb_session *p1, const char *p2, const char *p3, const char *p4))
  DEFINE_METHOD4(int,           smb_session_connect,    (smb_session *p1, const char *p2, uint32_t p3, int p4))
  DEFINE_METHOD1(int,           smb_session_login,      (smb_session *p1))
  DEFINE_METHOD1(int,           smb_session_is_guest,   (smb_session *p1))
  DEFINE_METHOD1(const char*,   smb_session_server_name,(smb_session *p1))
  DEFINE_METHOD2(int,           smb_session_supports,   (smb_session *p1, int p2))

  DEFINE_METHOD2(size_t,        smb_share_get_list,     (smb_session *p1, smb_share_list *p2))
  DEFINE_METHOD1(size_t,        smb_share_list_count,   (smb_share_list p1))
  DEFINE_METHOD2(const char*,   smb_share_list_at,      (smb_share_list p1, size_t p2))
  DEFINE_METHOD1(void,          smb_share_list_destroy, (smb_share_list p1))
  DEFINE_METHOD2(smb_tid,       smb_tree_connect,       (smb_session *p1, const char *p2))
  DEFINE_METHOD2(int,           smb_tree_disconnect,    (smb_session *p1, smb_tid p2))

  DEFINE_METHOD3(smb_stat_list, smb_find,               (smb_session *p1, smb_tid p2, const char *p3))
  DEFINE_METHOD3(smb_stat,      smb_fstat,              (smb_session *p1, smb_tid p2, const char *p3))
  DEFINE_METHOD2(smb_stat,      smb_stat_fd,            (smb_session *p1, smb_fd p2))
  DEFINE_METHOD1(void,          smb_stat_destroy,       (smb_stat p1))
  DEFINE_METHOD1(size_t,        smb_stat_list_count,    (smb_stat_list p1))
  DEFINE_METHOD2(smb_stat,      smb_stat_list_at,       (smb_stat_list p1, size_t p2))
  DEFINE_METHOD1(void,          smb_stat_list_destroy,  (smb_stat_list p1))
  DEFINE_METHOD1(const char*,   smb_stat_name,          (smb_stat p1))
  DEFINE_METHOD2(uint64_t,      smb_stat_get,           (smb_stat p1, int p2))

  DEFINE_METHOD3(uint32_t,      smb_directory_rm,       (smb_session *p1, smb_tid p2, const char *p3))
  DEFINE_METHOD3(uint32_t,      smb_directory_create,   (smb_session *p1, smb_tid p2, const char *p3))

  DEFINE_METHOD4(smb_fd,        smb_fopen,              (smb_session *p1, smb_tid p2, const char *p3, uint32_t p4))
  DEFINE_METHOD2(void,          smb_fclose,             (smb_session *p1, smb_fd p2))
  DEFINE_METHOD4(ssize_t,       smb_fread,              (smb_session *p1, smb_fd p2, void *p3, size_t p4))
  DEFINE_METHOD4(ssize_t,       smb_fwrite,             (smb_session *p1, smb_fd p2, const void *p3, size_t p4))
  DEFINE_METHOD4(ssize_t,       smb_fseek,              (smb_session *p1, smb_fd p2, ssize_t p3, int p4))
  DEFINE_METHOD3(uint32_t,      smb_file_rm,            (smb_session *p1, smb_tid p2, const char *p3))
  DEFINE_METHOD4(int,           smb_file_mv,            (smb_session *p1, smb_tid p2, const char *p3, const char *p4))

  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD_RENAME(netbios_ns_entry_name,    netbios_ns_entry_name)
    RESOLVE_METHOD_RENAME(netbios_ns_entry_group,   netbios_ns_entry_group)
    RESOLVE_METHOD_RENAME(netbios_ns_entry_ip,      netbios_ns_entry_ip)
    RESOLVE_METHOD_RENAME(netbios_ns_entry_type,    netbios_ns_entry_type)
    RESOLVE_METHOD_RENAME(netbios_ns_new,           netbios_ns_new)
    RESOLVE_METHOD_RENAME(netbios_ns_destroy,       netbios_ns_destroy)
    RESOLVE_METHOD_RENAME(netbios_ns_resolve,       netbios_ns_resolve)
    RESOLVE_METHOD_RENAME(netbios_ns_inverse,       netbios_ns_inverse)
    RESOLVE_METHOD_RENAME(netbios_ns_discover_start,netbios_ns_discover_start)
    RESOLVE_METHOD_RENAME(netbios_ns_discover_stop, netbios_ns_discover_stop)

    RESOLVE_METHOD_RENAME(smb_session_new,          smb_session_new)
    RESOLVE_METHOD_RENAME(smb_session_destroy,      smb_session_destroy)
    RESOLVE_METHOD_RENAME(smb_session_state,        smb_session_state)
    RESOLVE_METHOD_RENAME(smb_session_set_creds,    smb_session_set_creds)
    RESOLVE_METHOD_RENAME(smb_session_connect,      smb_session_connect)
    RESOLVE_METHOD_RENAME(smb_session_login,        smb_session_login)
    RESOLVE_METHOD_RENAME(smb_session_is_guest,     smb_session_is_guest)
    RESOLVE_METHOD_RENAME(smb_session_server_name,  smb_session_server_name)
    RESOLVE_METHOD_RENAME(smb_session_supports,     smb_session_supports)

    RESOLVE_METHOD_RENAME(smb_share_get_list,       smb_share_get_list)
    RESOLVE_METHOD_RENAME(smb_share_list_count,     smb_share_list_count)
    RESOLVE_METHOD_RENAME(smb_share_list_at,        smb_share_list_at)
    RESOLVE_METHOD_RENAME(smb_share_list_destroy,   smb_share_list_destroy)
    RESOLVE_METHOD_RENAME(smb_tree_connect,         smb_tree_connect)
    RESOLVE_METHOD_RENAME(smb_tree_disconnect,      smb_tree_disconnect)

    RESOLVE_METHOD_RENAME(smb_find,                 smb_find)
    RESOLVE_METHOD_RENAME(smb_fstat,                smb_fstat)
    RESOLVE_METHOD_RENAME(smb_stat_fd,              smb_stat_fd)
    RESOLVE_METHOD_RENAME(smb_stat_destroy,         smb_stat_destroy)
    RESOLVE_METHOD_RENAME(smb_stat_list_count,      smb_stat_list_count)
    RESOLVE_METHOD_RENAME(smb_stat_list_at,         smb_stat_list_at)
    RESOLVE_METHOD_RENAME(smb_stat_list_destroy,    smb_stat_list_destroy)
    RESOLVE_METHOD_RENAME(smb_stat_name,            smb_stat_name)
    RESOLVE_METHOD_RENAME(smb_stat_get,             smb_stat_get)

    RESOLVE_METHOD_RENAME(smb_directory_rm,         smb_directory_rm)
    RESOLVE_METHOD_RENAME(smb_directory_create,     smb_directory_create)

    RESOLVE_METHOD_RENAME(smb_fopen,                smb_fopen)
    RESOLVE_METHOD_RENAME(smb_fclose,               smb_fclose)
    RESOLVE_METHOD_RENAME(smb_fread,                smb_fread)
    RESOLVE_METHOD_RENAME(smb_fwrite,               smb_fwrite)
    RESOLVE_METHOD_RENAME(smb_fseek,                smb_fseek)
    RESOLVE_METHOD_RENAME(smb_file_rm,              smb_file_rm)
    RESOLVE_METHOD_RENAME(smb_file_mv,              smb_file_mv)
  END_METHOD_RESOLVE()

public:
  virtual bool Load()
  {
    if (!DllDynamic::Load())
      return false;

    // we do not need a lock here, we are not
    // loaded and ready yet to the outside world.
    m_netbios_ns = netbios_ns_new();
    return true;
  }

  virtual void Unload()
  {
    // no lock needed here either, once we unload, we are gone
    if (m_netbios_ns)
      netbios_ns_destroy(m_netbios_ns), m_netbios_ns = nullptr;
    DllDynamic::Unload();
  }

  // the following two are special, they will start/stop internal thread
  // that is global to libdsm. bad things will happen if called
  // by different threads so they need to be c-sectioned.
  virtual int netbios_ns_resolve(const char *name, char type, uint32_t *addr)
  {
    CSingleLock lock(m_netbios_ns_lock);
    return netbios_ns_resolve(m_netbios_ns, name, type, addr);
  }
  virtual const char *netbios_ns_inverse(uint32_t ip)
  {
    CSingleLock lock(m_netbios_ns_lock);
    return netbios_ns_inverse(m_netbios_ns, ip);
  }

  // the following two will use the same internal thread as the
  // above two. If started, calling the above two will assert :) nice.
  // best we can do now is lock over start/stop life cycle. This can
  // hang threads that use the above two so be quick about it :)
  virtual int netbios_ns_discover_start(unsigned int broadcast_timeout,
    netbios_ns_discover_callbacks *callbacks)
  {
    m_netbios_ns_lock.lock();
    return netbios_ns_discover_start(m_netbios_ns, broadcast_timeout, callbacks);
  }
  virtual int netbios_ns_discover_stop()
  {
    int rtn = netbios_ns_discover_stop(m_netbios_ns);
    // unlock AFTER we stop please.
    m_netbios_ns_lock.unlock();
    return rtn;
  }

private:
  // use a private global netbios_ns object and
  // persist it over lifetime of libdsm Load/Unload
  netbios_ns       *m_netbios_ns;
  CCriticalSection  m_netbios_ns_lock;
};

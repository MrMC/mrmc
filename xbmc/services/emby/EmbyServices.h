#pragma once
/*
 *      Copyright (C) 2017 Team MrMC
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

#include <atomic>
#include <memory>

#include "EmbyClient.h"
#include "URL.h"
#include "filesystem/CurlFile.h"
#include "threads/Thread.h"
#include "threads/CriticalSection.h"
#include "settings/lib/ISettingCallback.h"
#include "services/ServicesManager.h"
#include "interfaces/IAnnouncer.h"
#include "utils/JobManager.h"

namespace SOCKETS
{
  class CUDPSocket;
  class CSocketListener;
}

class CEmbyClient;
typedef std::shared_ptr<CEmbyClient> CEmbyClientPtr;

class CEmbyServices
: public CThread
, public CJobQueue
, public ISettingCallback
, public ANNOUNCEMENT::IAnnouncer
{
  friend class CEmbyServiceJob;

public:
  static CEmbyServices &GetInstance();

  void Start();
  void Stop();
  bool IsActive();
  bool IsEnabled();
  bool HasClients() const;
  void GetClients(std::vector<CEmbyClientPtr> &clients) const;
  CEmbyClientPtr FindClient(const std::string &path);
  CEmbyClientPtr FindClient(const CEmbyClient *client);
  bool ClientIsLocal(std::string path);
  EmbyServerInfo GetEmbyLocalServerInfo(const std::string url);

  // ISettingCallback
  virtual void OnSettingAction(const CSetting *setting) override;
  virtual void OnSettingChanged(const CSetting *setting) override;

  // IAnnouncer callbacks
  virtual void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data) override;

private:
  // private construction, and no assignements; use the provided singleton methods
  CEmbyServices();
  CEmbyServices(const CEmbyServices&);
  virtual ~CEmbyServices();

  void              SetUserSettings();
  void              GetUserSettings();

  // IRunnable entry point for thread
  virtual void      Process() override;

  bool              AuthenticateByName(const CURL& url);
  bool              GetEmbyLocalServers(const std::string &serverURL, const std::string &userId, const std::string &accessToken);

                    // complicated key/token exchange for sign in by pin :)
                    // a) get a random 6 digit pin from emby connect.
                    // b) user enters this pin at https://emby.media/pin
                    // c) we get a "IsConfirmed" pin reply. Now authenticate the pin.
                    // d) authenticating gets us a connectUserId/connectAccessToken (save them)
                    // e) using saved connectUserId/connectAccessToken, get server list.
                    // f) each server has AccessKey. Now using connectUserId and server AccessKey,
                    //    exchange the AccessKey for LocalUserId/AccessToken. Use these
                    //    to access that server end points.
  bool              PostSignInPinCode();
  bool              GetSignInByPinReply();
  bool              AuthenticatePinReply(const std::string &deviceId, const std::string &pin);
  EmbyServerInfoVector GetConnectServerList(const std::string &connectUserId, const std::string &connectAccessToken);
  bool              ExchangeAccessKeyForAccessToken(EmbyServerInfo &connectServerInfo);

  CEmbyClientPtr    GetClient(std::string uuid);
  bool              AddClient(CEmbyClientPtr foundClient);
  bool              RemoveClient(CEmbyClientPtr lostClient);

  std::atomic<bool> m_active;
  CCriticalSection  m_critical;
  CEvent            m_processSleep;

  std::string       m_userId;
  // m_serverURL is tagged 'emby' or 'embys' for http/https access
  // this tag is only used to remember which protocol to use when
  // init'ing the client. Once created, client remembers the correct
  // protocol and we just use an 'emby' tag outside of this class.
  std::string       m_serverURL;
  std::string       m_accessToken;
  std::string       m_signInByPinCode;
  std::string       m_myHomeUser;

  MediaServicesPlayerState m_playState;
  std::atomic<bool> m_hasClients;
  CCriticalSection  m_clients_lock;
  std::vector<CEmbyClientPtr> m_clients;
};

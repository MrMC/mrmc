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

#include "PlexClient.h"
#include "PlexUtils.h"
#include "threads/Thread.h"

namespace easywsclient
{
  class WebSocket;
}

class CPlexClientSync : protected CThread
{
public:
  CPlexClientSync(const bool owned, const std::string &name, const std::string &address, const std::string &deviceId, const std::string &accessToken);
  virtual ~CPlexClientSync();

  void Start();
  void Stop();

protected:
  virtual void Process();
  void         ProcessSyncByPolling();
  void         ProcessSyncByWebSockets();

private:
  CEvent m_processSleep;
  std::atomic<bool> m_stop;

  const bool m_owned;
  const std::string m_name;
  const std::string m_address;
  const std::string m_deviceId;
  const std::string m_accessToken;
  easywsclient::WebSocket *m_websocket;
};

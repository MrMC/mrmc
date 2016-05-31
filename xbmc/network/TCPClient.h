#pragma once
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

#include <string>

class CTCPClient
{
  public:
    enum TCP_RW {
      READ    = 0,
      WRITE   = 1,
    };

    enum {
      FAIL    = 0,
      SUCCESS = 1,
      TIMEOUT = 2,
    };

    CTCPClient();
   ~CTCPClient();

    int         Open(const std::string &address, int port, int timeout_us, bool blocking);
    void        Close();
    int         Read(std::string &data);
    int         Write(const char *data, int size);
    bool        IsOpen()     { return m_sock != -1; }
    std::string GetAddress() { return m_address; }
    int         GetPort()    { return m_port; }
    int         GetSock()    { return m_sock; }
    void        SetTimeout(int timeout_us) { m_timeout_us = timeout_us; }

  protected:
    int         SetNonBlock(bool nonblock = true);
    int         WaitForSocket(TCP_RW direction);

    int         m_port;
    std::string m_address;
    int         m_sock;
    bool        m_blocking;
    int         m_timeout_us;
 };

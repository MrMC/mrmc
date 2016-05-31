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

#include "TCPClient.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

CTCPClient::CTCPClient()
: m_port(-1)
, m_sock(-1)
, m_blocking(false)
{
}

CTCPClient::~CTCPClient()
{
  if (IsOpen())
    Close();
}

int CTCPClient::Open(const std::string &address, int port, int timeout_us, bool blocking)
{
  if (IsOpen())
    Close();

  m_port = port;
  m_address = address;
  m_blocking = blocking;
  m_timeout_us = timeout_us;

  m_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_sock == -1)
    return FAIL;

  if (m_blocking)
  {
    if (SetNonBlock() != SUCCESS)
      return FAIL;
  }

  struct sockaddr_in server = {0};
  server.sin_port = htons(port);
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(address.c_str());

  struct hostent *host = gethostbyname(address.c_str());
  if (!host)
    return FAIL;

  server.sin_addr.s_addr = *reinterpret_cast<in_addr_t*>(host->h_addr);

  if (connect(m_sock, reinterpret_cast<struct sockaddr*>(&server), sizeof(server)) < 0)
  {
    if (errno != EINPROGRESS)
      return FAIL;
  }

  int returnv = WaitForSocket(TCP_RW::WRITE);
  if (returnv == FAIL || returnv == TIMEOUT)
    return returnv;

  int flag = 1;
  if (setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) == -1)
    return FAIL;

  return SUCCESS;
}

void CTCPClient::Close()
{
  if (m_sock != -1)
  {
    if (m_blocking)
      SetNonBlock(false);
    close(m_sock);
    m_sock = -1;
  }
}

int CTCPClient::Read(std::string &data)
{
  if (m_sock == -1)
    return FAIL;

  int returnv = WaitForSocket(TCP_RW::READ);
  if (returnv != SUCCESS)
    return returnv;

  data = "";
  while(1)
  {
    char buff[1024];
    int size = recv(m_sock, buff, 1023, 0);

    if (errno == EAGAIN && size == -1)
      return SUCCESS;
    else if (size == -1)
      return FAIL;
    else if (size == 0 && data.length() == 0)
      return FAIL;
    else if (size == 0)
      return SUCCESS;

    buff[size] = 0x00;
    data += buff;
  }

  return SUCCESS;
}

int CTCPClient::Write(const char *data, int size)
{
  if (m_sock == -1)
    return FAIL;

  int bytestowrite = size;
  int byteswritten = 0;

  while (byteswritten < bytestowrite)
  {
    int returnv = WaitForSocket(TCP_RW::WRITE);

    if (returnv == FAIL || returnv == TIMEOUT)
      return returnv;

    int ret = send(m_sock, data + byteswritten, bytestowrite - byteswritten, 0);
    if (ret == -1)
      return FAIL;

    byteswritten += ret;
  }

  return SUCCESS;
}

int CTCPClient::SetNonBlock(bool nonblock)
{
  int flags = fcntl(m_sock, F_GETFL);
  if (flags == -1)
    return FAIL;

  if (nonblock)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;

  if (fcntl(m_sock, F_SETFL, flags) == -1)
    return FAIL;

  return SUCCESS;
}

int CTCPClient::WaitForSocket(TCP_RW direction)
{
  fd_set rwsock;
  FD_ZERO(&rwsock);
  FD_SET(m_sock, &rwsock);

  struct timeval *tv = NULL;
  struct timeval timeout;
  if (m_timeout_us > 0)
  {
    timeout.tv_sec = m_timeout_us / 1000000;
    timeout.tv_usec = m_timeout_us % 1000000;
    tv = &timeout;
  }

  int returnv;
  if (direction == TCP_RW::WRITE)
    returnv = select(m_sock + 1, NULL, &rwsock, NULL, tv);
  else
    returnv = select(m_sock + 1, &rwsock, NULL, NULL, tv);

  if (returnv == 0)
    return TIMEOUT;
  else if (returnv == -1)
    return FAIL;

  int sockstate;
  socklen_t sockstatelen = sizeof(sockstate);
  returnv = getsockopt(m_sock, SOL_SOCKET, SO_ERROR, &sockstate, &sockstatelen);
  if (returnv == -1)
    return FAIL;
  else if (sockstate)
    return FAIL;

  return SUCCESS;
}

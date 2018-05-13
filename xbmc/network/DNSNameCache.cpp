/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "DNSNameCache.h"
#ifdef HAVE_LIBDSM
#include "filesystem/DSMFile.h"
#endif
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

CDNSNameCache g_DNSCache;

CCriticalSection CDNSNameCache::m_critical;

CDNSNameCache::CDNSNameCache(void)
{}

CDNSNameCache::~CDNSNameCache(void)
{}

bool CDNSNameCache::Lookup(const std::string& strHostName, std::string& strIpAddress)
{
  if (strHostName.empty() && strIpAddress.empty())
    return false;

  // first see if this is already an ip address
  unsigned long address = inet_addr(strHostName.c_str());
  strIpAddress.clear();

  if (address != INADDR_NONE)
  {
    strIpAddress = StringUtils::Format("%lu.%lu.%lu.%lu", (address & 0xFF), (address & 0xFF00) >> 8, (address & 0xFF0000) >> 16, (address & 0xFF000000) >> 24 );
    return true;
  }

  // check if there's a custom entry or if it's already cached
  if(g_DNSCache.GetCached(strHostName, strIpAddress))
    return true;

  CLog::Log(LOGDEBUG, "CDNSNameCache::Lookup, check by getaddrinfo");
  {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *servinfo = nullptr;

    if (getaddrinfo(strHostName.c_str(), NULL, &hints, &servinfo) == 0)
    {
      for(addrinfo *p = servinfo; p != NULL; p = p->ai_next)
      {
        if (p->ai_family == AF_INET)
        {
          struct sockaddr_in* saddr = (struct sockaddr_in*)p->ai_addr;
          strIpAddress = inet_ntoa(saddr->sin_addr);
          CLog::Log(LOGDEBUG, "getaddrinfo: '%s' -> '%s'", strHostName.c_str(), strIpAddress.c_str());
          g_DNSCache.Add(strHostName, strIpAddress);
          freeaddrinfo(servinfo);
          return true;
        }
      }
      freeaddrinfo(servinfo);
    }
  }

  CLog::Log(LOGDEBUG, "CDNSNameCache::Lookup, check by gethostbyname.local");
  // perform dns name lookup with .local appended
  {
    struct hostent *host = gethostbyname(std::string(strHostName + ".local").c_str());
    if (host && host->h_addr_list[0])
    {
      strIpAddress = StringUtils::Format("%d.%d.%d.%d",
        (unsigned char)host->h_addr_list[0][0],
        (unsigned char)host->h_addr_list[0][1],
        (unsigned char)host->h_addr_list[0][2],
        (unsigned char)host->h_addr_list[0][3]);
      g_DNSCache.Add(strHostName, strIpAddress);
      return true;
    }
  }

#ifdef HAVE_LIBDSM
  CLog::Log(LOGDEBUG, "CDNSNameCache::Lookup, check by CDSMSessionManager::HostNameToIP");
  std::string ipaddress = strHostName;
  // HostNameToIP will do the g_DNSCache.Add if found
  if (CDSMSessionManager::HostNameToIP(ipaddress, true))
    return true;
#endif

  CLog::Log(LOGERROR, "Unable to lookup host: '%s'", strHostName.c_str());
  return false;
}

bool CDNSNameCache::GetCached(const std::string& strHostName, std::string& strIpAddress)
{
  CSingleLock lock(m_critical);

  // loop through all DNSname entries and see if strHostName is cached
  for (int i = 0; i < (int)g_DNSCache.m_vecDNSNames.size(); ++i)
  {
    CDNSName& DNSname = g_DNSCache.m_vecDNSNames[i];
    // RFC 4343, Domain Name System (DNS) Case Insensitivity Clarification
    if (StringUtils::EqualsNoCase(DNSname.m_strHostName, strHostName))
    {
      strIpAddress = DNSname.m_strIpAddress;
      return true;
    }
  }

  // not cached
  return false;
}

void CDNSNameCache::Add(const std::string &strHostName, const std::string &strIpAddress)
{
  CDNSName dnsName;

  dnsName.m_strHostName = strHostName;
  dnsName.m_strIpAddress  = strIpAddress;

  CSingleLock lock(m_critical);
  g_DNSCache.m_vecDNSNames.push_back(dnsName);
}


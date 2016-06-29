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

#include <atomic>

#include "PlexClient.h"
#include "PlexUtils.h"

#include "Application.h"
#include "URL.h"
#include "filesystem/CurlFile.h"
#include "network/Network.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Base64.h"

#include <string>
#include <sstream>

static bool IsInSubNet(std::string address, std::string port)
{
  bool rtn = false;
  CNetworkInterface* iface = g_application.getNetwork().GetFirstConnectedInterface();
  in_addr_t localMask = ntohl(inet_addr(iface->GetCurrentNetmask().c_str()));
  in_addr_t testAddress = ntohl(inet_addr(address.c_str()));
  in_addr_t localAddress = ntohl(inet_addr(iface->GetCurrentIPAddress().c_str()));

  in_addr_t temp1 = testAddress & localMask;
  in_addr_t temp2 = localAddress & localMask;
  if (temp1 == temp2)
  {
    // we are on the same subnet
    // now make sure it is a plex server
    std::string url = "https://" + address + ":" + port;
    rtn = CPlexUtils::GetIdentity(url);
  }
  return rtn;
}

CPlexClient::CPlexClient(std::string data, std::string ip)
{
  m_local = true;
  m_alive = true;
  m_owned = true;
  m_scheme = "http";
  m_needUpdate = false;
  int port = 32400;
  std::string s;
  std::istringstream f(data);
  while (std::getline(f, s))
  {
    int pos = s.find(':');
    if (pos > 0)
    {
      std::string substr = s.substr(0, pos);
      std::string name = StringUtils::Trim(substr);
      substr = s.substr(pos + 1);
      std::string val = StringUtils::Trim(substr);
      if (name == "Content-Type")
        m_contentType = val;
      else if (name == "Resource-Identifier")
        m_uuid = val;
      else if (name == "Name")
        m_serverName = val;
      else if (name == "Port")
        port = atoi(val.c_str());
    }
  }

  CURL url;
  url.SetHostName(ip);
  url.SetPort(port);
  url.SetProtocol(m_scheme);

  m_url = url.Get();
}

CPlexClient::CPlexClient(const TiXmlElement* DeviceNode)
{
  m_local = false;
  m_alive = true;
  m_needUpdate = false;
  m_uuid = XMLUtils::GetAttribute(DeviceNode, "clientIdentifier");
  m_owned = XMLUtils::GetAttribute(DeviceNode, "owned");
  m_serverName = XMLUtils::GetAttribute(DeviceNode, "name");
  m_accessToken = XMLUtils::GetAttribute(DeviceNode, "accessToken");
  m_httpsRequired = XMLUtils::GetAttribute(DeviceNode, "httpsRequired");

  std::string owned;
  std::string port;
  std::string address;
  const TiXmlElement* ConnectionNode = DeviceNode->FirstChildElement("Connection");
  while (ConnectionNode)
  {
    port = XMLUtils::GetAttribute(ConnectionNode, "port");
    address = XMLUtils::GetAttribute(ConnectionNode, "address");
    m_scheme = XMLUtils::GetAttribute(ConnectionNode, "protocol");
    if (XMLUtils::GetAttribute(ConnectionNode, "local") == "1" && IsInSubNet(address, port))
    {
      m_local = true;
      break;
    }

    ConnectionNode = ConnectionNode->NextSiblingElement("Connection");
  }

  CURL url;
  url.SetHostName(address);
  url.SetPort(atoi(port.c_str()));
  url.SetProtocol(m_scheme);
  url.SetProtocolOptions("&X-Plex-Token=" + m_accessToken);

  m_url = url.Get();
}

CPlexClient::~CPlexClient()
{
}

std::string CPlexClient::GetUrl()
{
  return m_url;
}

std::string CPlexClient::GetHost()
{
  CURL url(m_url);
  return url.GetHostName();
}

int CPlexClient::GetPort()
{
  CURL url(m_url);
  return url.GetPort();
}

const PlexSectionsContentVector CPlexClient::GetTvContent() const
{
  CSingleLock lock(m_criticalTVShow);
  return m_showSectionsContents;
}

const PlexSectionsContentVector CPlexClient::GetMovieContent() const
{
  CSingleLock lock(m_criticalMovies);
  return m_movieSectionsContents;
}

bool CPlexClient::IsMe(const CURL& url)
{
  CURL real_url(url);
  if (real_url.GetProtocol() == "plex")
    real_url = CURL(Base64::Decode(URIUtils::GetFileName(real_url)));

  if (GetHost() == real_url.GetHostName())
  {
    if (!real_url.GetFileName().empty())
    {
      {
        CSingleLock lock(m_criticalMovies);
        for (const auto &contents : m_movieSectionsContents)
        {
          if (real_url.GetFileName().find(contents.section) != std::string::npos)
            return true;
        }
      }
      {
        CSingleLock lock(m_criticalTVShow);
        for (const auto &contents : m_showSectionsContents)
        {
          if (real_url.GetFileName().find(contents.section) != std::string::npos)
            return true;
        }
      }
    }
  }

  return false;
}

std::string CPlexClient::LookUpUuid(const std::string path) const
{
  std::string uuid;

  CURL url(path);
  {
    CSingleLock lock(m_criticalMovies);
    for (const auto &contents : m_movieSectionsContents)
    {
      if (contents.section == url.GetFileName())
        return m_uuid;
    }
  }
  {
    CSingleLock lock(m_criticalTVShow);
    for (const auto &contents : m_showSectionsContents)
    {
      if (contents.section == url.GetFileName())
        return m_uuid;
    }
  }

  return uuid;
}

bool CPlexClient::ParseSections(PlexSectionParsing parser)
{
  bool rtn = false;
  XFILE::CCurlFile plex;
  plex.SetBufferSize(32768*10);
  plex.SetTimeout(10);

  CURL curl(m_url);
  curl.SetFileName(curl.GetFileName() + "library/sections");
  std::string strResponse;
  if (plex.Get(curl.Get(), strResponse))
  {
#if defined(PLEX_DEBUG_VERBOSE)
    if (parser == PlexSectionParsing::newSection || parser == PlexSectionParsing::checkSection)
      CLog::Log(LOGDEBUG, "CPlexClient::ParseSections %d, %s", parser, strResponse.c_str());
#endif
    if (parser == PlexSectionParsing::updateSection)
    {
      {
        CSingleLock lock(m_criticalMovies);
        m_movieSectionsContents.clear();
      }
      {
        CSingleLock lock(m_criticalTVShow);
        m_showSectionsContents.clear();
      }
      m_needUpdate = false;
    }

    TiXmlDocument xml;
    xml.Parse(strResponse.c_str());

    TiXmlElement* MediaContainer = xml.RootElement();
    if (MediaContainer)
    {
      const TiXmlElement* DirectoryNode = MediaContainer->FirstChildElement("Directory");
      while (DirectoryNode)
      {
        PlexSectionsContent content;
        content.uuid = XMLUtils::GetAttribute(DirectoryNode, "uuid");
        content.path = XMLUtils::GetAttribute(DirectoryNode, "path");
        content.type = XMLUtils::GetAttribute(DirectoryNode, "type");
        content.title = XMLUtils::GetAttribute(DirectoryNode, "title");
        content.updatedAt = XMLUtils::GetAttribute(DirectoryNode, "updatedAt");
        std::string key = XMLUtils::GetAttribute(DirectoryNode, "key");
        content.section = "library/sections/" + key;
        std::string art = XMLUtils::GetAttribute(DirectoryNode, "art");
        if (m_local)
          content.art = art;
        else
          content.art = content.section + "/resources/" + URIUtils::GetFileName(art);
        if (content.type == "movie")
        {
          if (parser == PlexSectionParsing::checkSection)
          {
            CSingleLock lock(m_criticalMovies);
            for (const auto &contents : m_movieSectionsContents)
            {
              if (contents.uuid == content.uuid)
              {
                if (contents.updatedAt != content.updatedAt)
                {
#if defined(PLEX_DEBUG_VERBOSE)
                  CLog::Log(LOGDEBUG, "CPlexClient::ParseSections need update on %s:%s",
                    m_serverName.c_str(), content.title.c_str());
#endif
                  m_needUpdate = true;
                }
              }
            }
          }
          else
          {
            CSingleLock lock(m_criticalMovies);
            m_movieSectionsContents.push_back(content);
          }
        }
        else if (content.type == "show")
        {
          if (parser == PlexSectionParsing::checkSection)
          {
            CSingleLock lock(m_criticalTVShow);
            for (const auto &contents : m_showSectionsContents)
            {
              if (contents.uuid == content.uuid)
              {
                if (contents.updatedAt != content.updatedAt)
                {
#if defined(PLEX_DEBUG_VERBOSE)
                  CLog::Log(LOGDEBUG, "CPlexClient::ParseSections need update on %s:%s",
                    m_serverName.c_str(), content.title.c_str());
#endif
                  m_needUpdate = true;
                }
              }
            }
          }
          else
          {
            CSingleLock lock(m_criticalTVShow);
            m_showSectionsContents.push_back(content);
          }
        }
        DirectoryNode = DirectoryNode->NextSiblingElement("Directory");
      }
      rtn = true;
    }
  }
  else
  {
    // 401's are attempts to access a local server that is also in PMS
    // and these require an access token. Only local servers that are
    // not is PMS can be accessed via GDM.
    if (plex.GetResponseCode() != 401)
      CLog::Log(LOGDEBUG, "CPlexClient::ParseSections failed %s", strResponse.c_str());
    rtn = false;
  }

  return rtn;
}

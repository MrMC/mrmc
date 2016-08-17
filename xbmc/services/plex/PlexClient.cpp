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
#include <memory>
#include <algorithm>

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

static bool IsInSubNet(CURL url)
{
  bool rtn = false;
  CNetworkInterface* iface = g_application.getNetwork().GetFirstConnectedInterface();
  in_addr_t localMask = ntohl(inet_addr(iface->GetCurrentNetmask().c_str()));
  in_addr_t testAddress = ntohl(inet_addr(url.GetHostName().c_str()));
  in_addr_t localAddress = ntohl(inet_addr(iface->GetCurrentIPAddress().c_str()));

  in_addr_t temp1 = testAddress & localMask;
  in_addr_t temp2 = localAddress & localMask;
  if (temp1 == temp2)
  {
    // we are on the same subnet
    // now make sure it is a plex server
    rtn = CPlexUtils::GetIdentity(url, 1);
  }
  return rtn;
}

CPlexClient::CPlexClient()
{
  m_local = true;
  m_owned = true;
  m_presence = true;
  m_protocol = "http";
  m_needUpdate = false;
}

CPlexClient::~CPlexClient()
{
}

bool CPlexClient::Init(std::string data, std::string ip)
{
  m_url = "";

  int port = 32400;
  m_protocol = "http";

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
  url.SetProtocol(m_protocol);
  if (CPlexUtils::GetIdentity(url, 2))
    m_url = url.Get();

  return !m_url.empty();
}

bool CPlexClient::Init(const TiXmlElement* DeviceNode)
{
  m_url = "";
  m_presence = XMLUtils::GetAttribute(DeviceNode, "presence") == "1";
  if (!m_presence)
    return false;

  m_uuid = XMLUtils::GetAttribute(DeviceNode, "clientIdentifier");
  m_owned = XMLUtils::GetAttribute(DeviceNode, "owned");
  m_serverName = XMLUtils::GetAttribute(DeviceNode, "name");
  m_accessToken = XMLUtils::GetAttribute(DeviceNode, "accessToken");
  m_httpsRequired = XMLUtils::GetAttribute(DeviceNode, "httpsRequired");

  std::vector<PlexConnection> connections;
  const TiXmlElement* ConnectionNode = DeviceNode->FirstChildElement("Connection");
  while (ConnectionNode)
  {
    PlexConnection connection;
    connection.port = XMLUtils::GetAttribute(ConnectionNode, "port");
    connection.address = XMLUtils::GetAttribute(ConnectionNode, "address");
    connection.protocol = XMLUtils::GetAttribute(ConnectionNode, "protocol");
    connection.external = XMLUtils::GetAttribute(ConnectionNode, "local") == "0" ? 1 : 0;
    connections.push_back(connection);

    ConnectionNode = ConnectionNode->NextSiblingElement("Connection");
  }

  CURL url;
  if (!connections.empty())
  {
    // sort so that all external=0 are first. These are the local connections.
    std::sort(connections.begin(), connections.end(),
      [] (PlexConnection const& a, PlexConnection const& b) { return a.external < b.external; });

    for (const auto &connection : connections)
    {
      url.SetHostName(connection.address);
      url.SetPort(atoi(connection.port.c_str()));
      url.SetProtocol(connection.protocol);
      url.SetProtocolOptions("&X-Plex-Token=" + m_accessToken);
      int timeout = connection.external ? 5 : 1;
      if (CPlexUtils::GetIdentity(url, timeout))
      {
        CLog::Log(LOGDEBUG, "CPlexClient::Init "
          "serverName(%s), ipAddress(%s), protocol(%s)",
          m_serverName.c_str(), connection.address.c_str(), connection.protocol.c_str());

        m_url = url.Get();
        m_protocol = url.GetProtocol();
        m_local = (connection.external == 0);
        break;
      }
    }
  }

  return !m_url.empty();
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

const std::string CPlexClient::FormatContentTitle(const std::string contentTitle) const
{
  std::string owned = (GetOwned() == "1") ? "O":"S";
  std::string title = StringUtils::Format("Plex(%s) - %s - %s %s",
              owned.c_str(), GetServerName().c_str(), contentTitle.c_str(), GetPresence()? "":"(off-line)");
  return title;
}

std::string CPlexClient::FindSectionTitle(const std::string &path)
{
  CURL real_url(path);
  if (real_url.GetProtocol() == "plex")
    real_url = CURL(Base64::Decode(URIUtils::GetFileName(real_url)));

  if (!real_url.GetFileName().empty())
  {
    {
      CSingleLock lock(m_criticalMovies);
      for (const auto &contents : m_movieSectionsContents)
      {
        if (real_url.GetFileName().find(contents.section) != std::string::npos)
          return contents.title;
      }
    }
    {
      CSingleLock lock(m_criticalTVShow);
      for (const auto &contents : m_showSectionsContents)
      {
        if (real_url.GetFileName().find(contents.section) != std::string::npos)
          return contents.title;
      }
    }
  }

  return "";
}

bool CPlexClient::IsSameClientHostName(const CURL& url)
{
  CURL real_url(url);
  if (real_url.GetProtocol() == "plex")
    real_url = CURL(Base64::Decode(URIUtils::GetFileName(real_url)));

  return GetHost() == real_url.GetHostName();
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
        content.thumb = XMLUtils::GetAttribute(DirectoryNode, "composite");
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
        else
        {
          CLog::Log(LOGDEBUG, "CPlexClient::ParseSections %s found unhandled content type %s",
            m_serverName.c_str(), content.type.c_str());
        }
        DirectoryNode = DirectoryNode->NextSiblingElement("Directory");
      }

      CLog::Log(LOGDEBUG, "CPlexClient::ParseSections %s found %d movie sections",
        m_serverName.c_str(), (int)m_movieSectionsContents.size());
      CLog::Log(LOGDEBUG, "CPlexClient::ParseSections %s found %d shows sections",
        m_serverName.c_str(), (int)m_showSectionsContents.size());

      rtn = true;
    }
    else
    {
      CLog::Log(LOGDEBUG, "CPlexClient::ParseSections no MediaContainer found");
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

void CPlexClient::SetPresence(bool presence)
{
  if (m_presence != presence)
  {
    m_presence = presence;
  }
}

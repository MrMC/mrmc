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

#include "system.h"
#include <cassert>
#include <arpa/inet.h>

#include "DSMDirectory.h"

#include "FileItem.h"
#include "Directory.h"
#include "GUIUserMessages.h"
#include "PasswordManager.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/GUIMessage.h"
#include "guilib/LocalizeStrings.h"
#include "settings/Settings.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "utils/URIUtils.h"

using namespace XFILE;

CDSMDirectory::CDSMDirectory(void)
{
}

CDSMDirectory::~CDSMDirectory(void)
{
}

bool CDSMDirectory::GetDirectory(const CURL& url, CFileItemList &items)
{
  assert(url.IsProtocol("smb"));

  bool rtn = false;
  std::string rootpath = url.Get();
  if (rootpath == "smb://")
  {
    // we are browsing for clients
    // startup ns share discovery
    CDSMSessionManager::NSDiscoverStart();
    std::vector<std::string> serverNames;
    CDSMSessionManager::NSDiscoverServerList(serverNames);
    // serverNames might or might not be populated yet
    // if not CDSMSessionManager/DllLibDSM will post a GUI_MSG_UPDATE_PATH msg
    // when a server shows up and we get called again to provide an updated list.
    if (!serverNames.empty())
    {
      for (auto &serverName : serverNames)
      {
        CFileItemPtr pItem(new CFileItem(serverName));
        std::string path(rootpath);
        path = URIUtils::AddFileToFolder(path, serverName);
        URIUtils::AddSlashAtEnd(path);
        pItem->SetPath(path);
        pItem->m_bIsFolder = true;
        pItem->m_bIsShareOrDrive = true;
        // set the default folder icon
        pItem->FillInDefaultIcon();
        items.Add(pItem);
      }
    }
    rtn = true;
  }
  else
  {
    // try to connect to the smb://<share>... path. if all is good
    // we get a non-null session back. Toss it into a std::unique_ptr
    // and it will take care of itself. Errors creating the session will
    // get reported in passed sessionError.
    ConnectSessionErrors sessionError;
    CDSMSessionPtr session(CDSMSessionManager::CreateSession(url, sessionError));
    if (session)
      rtn = session->GetDirectory(url.GetWithoutFilename().c_str(), url.GetFileName().c_str(), items);

    if (!session || !rtn)
    {
      // if we have a session, error came from GetDirectory, so fetch session error.
      if (session && sessionError == ConnectSessionErrors::NONE)
        sessionError = session->GetSessionError();
      // this is critical to get an user/pass dialog up. session will be null as there was
      // an FAILED_AUTHORIZATION error. So check for it, set up for authorization and
      // (most important) return false so we get called again on main thread for user/pass.
      if (sessionError == ConnectSessionErrors::FAILED_AUTHORIZATION)
      {
        if (m_flags & DIR_FLAG_ALLOW_PROMPT)
          RequireAuthentication(url);
      }
      else
      {
        // anything else is a real error (most common is wrong user/pass)
        std::string errorString;
        errorString = StringUtils::Format(g_localizeStrings.Get(771).c_str(), (int)sessionError);
        if (m_flags & DIR_FLAG_ALLOW_PROMPT)
          SetErrorDialog(257, errorString.c_str());
      }
    }
  }
  return rtn;
}

bool CDSMDirectory::Create(const CURL& url)
{
  ConnectSessionErrors sessionError;
  CDSMSessionPtr session(CDSMSessionManager::CreateSession(url, sessionError));
  if (session)
    return session->CreateDirectory(url.GetFileName().c_str());
  else
  {
    CLog::Log(LOGERROR, "CDSMDirectory: Failed to create %s", url.GetFileName().c_str());
    return false;
  }
}

bool CDSMDirectory::Exists(const CURL& url)
{
  ConnectSessionErrors sessionError;
  CDSMSessionPtr session(CDSMSessionManager::CreateSession(url, sessionError));
  if (session)
    return session->DirectoryExists(url.GetFileName().c_str());
  else
  {
    CLog::Log(LOGERROR, "CDSMDirectory: Failed to create session to check exists");
    return false;
  }
}

bool CDSMDirectory::Remove(const CURL& url)
{
  ConnectSessionErrors sessionError;
  CDSMSessionPtr session(CDSMSessionManager::CreateSession(url, sessionError));
  if (session)
    return session->RemoveDirectory(url.GetFileName().c_str());
  else
  {
    CLog::Log(LOGERROR, "CDSMDirectory: Failed to remove %s", url.GetFileName().c_str());
    return false;
  }
}

bool CDSMDirectory::AuthenticateURL(CURL &url)
{
  if (!CPasswordManager::GetInstance().AuthenticateURL(url))
  {
    // no user/pass match found.
    // 1) is ok
    // 2) hostname missmatch.
    //  is ip and we used host or is host and we used ip
    // first see if this is already an ip address
    unsigned long address = inet_addr(url.GetHostName().c_str());
    if (address == INADDR_NONE)
    {
      // GetHostName is netbios name. flip and try again.
      std::string hostname = url.GetHostName();
      if (CDSMSessionManager::HostNameToIP(hostname))
        url.SetHostName(hostname);
    }
    else
    {
      // GetHostName is ip address. flip and try again.
      const char *netbios_name = CDSMSessionManager::IPAddressToNetBiosName(url.GetHostName());
      if (netbios_name != nullptr)
        url.SetHostName(netbios_name);
    }
    CPasswordManager::GetInstance().AuthenticateURL(url);
  }
  return CPasswordManager::GetInstance().AuthenticateURL(url);
}

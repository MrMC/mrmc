/*
 *      Copyright (C) 2015 MrMC
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

#include <stdlib.h>

// stupid libs that include their config.h
#define PACKAGE _PACKAGE
#undef PACKAGE
#define VERSION _VERSION
#undef VERSION
#define __unix__
#include <ulxmlrpcpp/ulxr_tcpip_connection.h>
#include <ulxmlrpcpp/ulxr_ssl_connection.h>
#include <ulxmlrpcpp/ulxr_http_protocol.h>
#include <ulxmlrpcpp/ulxr_requester.h>
#include <ulxmlrpcpp/ulxr_value.h>
#include <ulxmlrpcpp/ulxr_except.h>
#include <ulxmlrpcpp/ulxr_log4j.h>
#include "zlib.h"
#include "zconf.h"
#undef VERSION
#define _VERSION VERSION
#undef PACKAGE
#define _PACKAGE PACKAGE
#undef __unix__

#include "OpenSubtitlesSearch.h"
#include "SubtitleUtilities.h"

#include "CompileInfo.h"
#include "PasswordManager.h"
#include "Util.h"
#include "filesystem/File.h"
#include "utils/Base64.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "utils/Variant.h"
#include "utils/LangCodeExpander.h"
#include "video/VideoInfoTag.h"

static ulxr::MethodResponse ServerChat(ulxr::MethodCall methodcall)
{
  ulxr::MethodResponse response;
  std::string strServerUrl = "api.opensubtitles.org";
  std::string method = methodcall.getMethodName();
  try
  {
    std::unique_ptr<ulxr::TcpIpConnection> connection(new ulxr::TcpIpConnection(false, ULXR_PCHAR(strServerUrl), 80));
    ulxr::HttpProtocol    protocol(connection.get());
    ulxr::Requester       client(&protocol);
    response = client.call(methodcall, ULXR_PCHAR("/xml-rpc"));
    CLog::Log(LOGDEBUG, "%s - finished -  %s", __PRETTY_FUNCTION__, method.c_str());
  }
  catch(...)
  {
    std::string method = methodcall.getMethodName();
    CLog::Log(LOGDEBUG, "%s - crashed with %s", __PRETTY_FUNCTION__, method.c_str());
  }
  
  return response;
}

COpenSubtitlesSearch::COpenSubtitlesSearch()
  : m_strUser("")
  , m_strPass("")
  , m_authenticated(false)
{
  m_authenticated = CPasswordManager::GetInstance().GetUserPass(ModuleName(), m_strUser, m_strPass);
}

COpenSubtitlesSearch::~COpenSubtitlesSearch()
{
}

std::string COpenSubtitlesSearch::ModuleName()
{
  return "OpenSubtitles";
}

void COpenSubtitlesSearch::ChangeUserPass()
{
  m_authenticated = CPasswordManager::GetInstance().SetUserPass(ModuleName(), m_strUser, m_strPass);
}

bool COpenSubtitlesSearch::LogIn()
{
  if (!m_authenticated)
  {
    m_authenticated = CPasswordManager::GetInstance().SetUserPass(ModuleName(), m_strUser, m_strPass);
  }
  
  std::string strUA = StringUtils::Format("%s_v%i.%i" , CCompileInfo::GetAppName(),
                                          CCompileInfo::GetMajor(),CCompileInfo::GetMinor());
  StringUtils::ToLower(strUA);
  ulxr::MethodCall      methodcall(ULXR_PCHAR("LogIn"));
  methodcall.addParam(ulxr::RpcString(ULXR_PCHAR(m_strUser)));         // username
  methodcall.addParam(ulxr::RpcString(ULXR_PCHAR(m_strPass)));         // password
  methodcall.addParam(ulxr::RpcString(ULXR_PCHAR("eng")));             // language
  methodcall.addParam(ulxr::RpcString(ULXR_PCHAR(strUA)));             // useragent string
  ulxr::MethodResponse response = ServerChat(methodcall);
  ulxr::Struct cap = response.getResult();
  if (response.isOK() && cap.hasMember(ULXR_PCHAR("status")))
  {
    ulxr::RpcString status = cap.getMember(ULXR_PCHAR("status"));
    CLog::Log(LOGDEBUG, "%s - response - %s", __PRETTY_FUNCTION__, status.getString().c_str());
    if (status.getString() == "200 OK")
    {
      ulxr::RpcString token = cap.getMember(ULXR_PCHAR("token"));
      m_strToken = token.getString();
      return true;
    }
  }
  return false;
}

bool COpenSubtitlesSearch::SubtitleSearch(const std::string &path,const std::string strLanguages,
                                          const std::string preferredLanguage,
                                          CFileItemList &subtitlesList,
                                          const std::string &strSearch)
{
  if (LogIn())
  {
    std::string strSize;
    std::string strHash;
    CSubtitleUtilities::SubtitleFileSizeAndHash(path, strSize, strHash);
    CLog::Log(LOGDEBUG, "%s - HASH - %s and Size - %s", __FUNCTION__, strHash.c_str(), strSize.c_str());

    ulxr::Array searchList;
    
    std::string lg;
    std::vector<std::string> languages3;
    std::vector<std::string> languages = StringUtils::Split(strLanguages, ',');
    // convert from English to eng
    for(std::vector<std::string>::iterator it = languages.begin(); it != languages.end(); ++it)
    {
      g_LangCodeExpander.ConvertToISO6392T((*it).c_str(),lg);
      languages3.push_back(lg);
    }
    
    //  hash search
    ulxr::Struct searchHashParam;
    std::string strLang = StringUtils::Join(languages3, ",");
    searchHashParam.addMember(ULXR_PCHAR("sublanguageid"), ulxr::RpcString(strLang));
    searchHashParam.addMember(ULXR_PCHAR("moviehash"),     ulxr::RpcString(strHash));
    searchHashParam.addMember(ULXR_PCHAR("moviebytesize"), ulxr::RpcString(strSize));
    searchList.addItem(searchHashParam);
    
    CVideoInfoTag* tag = g_application.CurrentFileItem().GetVideoInfoTag();
    
    std::string searchString;
    if (!strSearch.empty())
    {
      searchString = strSearch;
    }
    else if (tag->m_iEpisode > -1)
    {
      searchString = StringUtils::Format("%s S%.2dE%.2d",tag->m_strShowTitle.c_str()
                                                        ,tag->m_iSeason
                                                        ,tag->m_iEpisode
                                         );
    }
    else
    {
      if (tag->m_iYear > 0)
      {
        int year = tag->m_iYear;
        std::string title = tag->m_strTitle;
        searchString = StringUtils::Format("%s (%i)",title.c_str(), year);
      }
      else
      {
        std::string strName = g_application.CurrentFileItem().GetMovieName(false);
        
        std::string strTitleAndYear;
        std::string strTitle;
        std::string strYear;
        CUtil::CleanString(strName, strTitle, strTitleAndYear, strYear, false);
        searchString = StringUtils::Format("%s (%s)",strTitle.c_str(), strYear.c_str());
      }
    }

    StringUtils::Replace(searchString, " ", "+");
    ulxr::Struct searchStringParam;
    
    //  title search
    searchStringParam.addMember(ULXR_PCHAR("sublanguageid"), ulxr::RpcString(StringUtils::Join(languages3, ",")));
    searchStringParam.addMember(ULXR_PCHAR("query"),         ulxr::RpcString(searchString));
    searchList.addItem(searchStringParam);
    
    ulxr::MethodCall      methodcall(ULXR_PCHAR("SearchSubtitles"));
    ulxr::RpcString token = m_strToken;
    methodcall.addParam(token);
    methodcall.addParam(searchList);
    ulxr::MethodResponse response = ServerChat(methodcall);
  
    ulxr::Struct cap = response.getResult();
    if (response.isOK() && cap.hasMember(ULXR_PCHAR("status")))
    {
      ulxr::RpcString status = cap.getMember(ULXR_PCHAR("status"));
      CLog::Log(LOGDEBUG, "%s - response - %s", __PRETTY_FUNCTION__, status.getString().c_str());
      if (status.getString() == "200 OK")
      {
        if (cap.hasMember(ULXR_PCHAR("data")))
        {
          ulxr::Array subs = cap.getMember(ULXR_PCHAR("data"));      
          for (unsigned i = 0; i < subs.size(); ++i)
          {
            ulxr::Struct entry = subs.getItem(i);
            CFileItemPtr item(new CFileItem());
            if (entry.hasMember(ULXR_PCHAR("LanguageName")))
            {
              ulxr::RpcString value = entry.getMember(ULXR_PCHAR("LanguageName"));
              item->SetLabel(value.getString());
            }
            if (entry.hasMember(ULXR_PCHAR("SubFileName")))
            {
              ulxr::RpcString value = entry.getMember(ULXR_PCHAR("SubFileName"));
              item->SetLabel2(value.getString());
            }
            if (entry.hasMember(ULXR_PCHAR("SubRating")))
            {
              ulxr::RpcString value = entry.getMember(ULXR_PCHAR("SubRating"));
              item->SetIconImage(StringUtils::Format("%i", atoi(value.getString().c_str())/2));
            }
            if (entry.hasMember(ULXR_PCHAR("ISO639")))
            {
              ulxr::RpcString value = entry.getMember(ULXR_PCHAR("ISO639"));
              item->SetArt("thumb",value.getString());
            }
            if (entry.hasMember(ULXR_PCHAR("MatchedBy")))
            {
              ulxr::RpcString value = entry.getMember(ULXR_PCHAR("MatchedBy"));
              item->SetProperty("sync", value.getString() == "moviehash" ? "true":"false");
            }
            if (entry.hasMember(ULXR_PCHAR("SubHearingImpaired")))
            {
              ulxr::RpcString value = entry.getMember(ULXR_PCHAR("SubHearingImpaired"));
              item->SetProperty("hearing_imp", value.getString() == "1" ? "true":"false");
            }
            if (entry.hasMember(ULXR_PCHAR("ZipDownloadLink")))
            {
              ulxr::RpcString value = entry.getMember(ULXR_PCHAR("ZipDownloadLink"));
              item->SetProperty("ZipDownloadLink", value.getString());
            }
            if (entry.hasMember(ULXR_PCHAR("IDSubtitleFile")))
            {
              ulxr::RpcString value = entry.getMember(ULXR_PCHAR("IDSubtitleFile"));
              item->SetProperty("IDSubtitleFile", value.getString());
            }
            if (entry.hasMember(ULXR_PCHAR("SubFormat")))
            {
              ulxr::RpcString value = entry.getMember(ULXR_PCHAR("SubFormat"));
              item->SetProperty("SubFormat", value.getString());
            }
            subtitlesList.Add(item);
          }
        }
        CLog::Log(LOGDEBUG, "%s - hold", __PRETTY_FUNCTION__);
        return true;
      }
    }
  }
  return false;
}

bool COpenSubtitlesSearch::Download(const CFileItem *subItem,std::vector<std::string> &items)
{
  ulxr::MethodCall      methodcall(ULXR_PCHAR("DownloadSubtitles"));
  ulxr::Array subtitleIDlist;
  ulxr::RpcString ID = subItem->GetProperty("IDSubtitleFile").asString();
  subtitleIDlist.addItem(ID);
  ulxr::RpcString token = m_strToken;
  methodcall.addParam(token);
  methodcall.addParam(subtitleIDlist);
  
  ulxr::MethodResponse response = ServerChat(methodcall);
  ulxr::Struct cap = response.getResult();
  
  if (response.isOK() && cap.hasMember(ULXR_PCHAR("status")))
  {
    ulxr::RpcString status = cap.getMember(ULXR_PCHAR("status"));
    CLog::Log(LOGDEBUG, "%s - response - %s", __PRETTY_FUNCTION__, status.getString().c_str());
    if (status.getString() == "200 OK")
    {
      if (cap.hasMember(ULXR_PCHAR("data")))
      {
        ulxr::Array subs = cap.getMember(ULXR_PCHAR("data"));
        for (unsigned i = 0; i < subs.size(); ++i)
        {
          ulxr::Struct entry = subs.getItem(i);
          ulxr::RpcString data = entry.getMember(ULXR_PCHAR("data"));
          std::string zipdata = data.getString();
          std::string zipdata64Decoded = Base64::Decode(zipdata);
          std::string zipdata64DecodedInflated;
          CSubtitleUtilities::gzipInflate(zipdata64Decoded,zipdata64DecodedInflated);
          XFILE::CFile file;
          std::string destination = StringUtils::Format("special://temp/%s.%s",
                                                        StringUtils::CreateUUID().c_str(),
                                                        subItem->GetProperty("SubFormat").asString().c_str()
                                                        );
          file.OpenForWrite(destination);
          file.Write(zipdata64DecodedInflated.c_str(), zipdata64DecodedInflated.size());
          items.push_back(destination);
        }
        CLog::Log(LOGDEBUG, "%s - OpenSubitles subfile downloaded", __PRETTY_FUNCTION__);
        return true;
      }
    }
  }
  return false;
}

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

#include "CloudUtils.h"
#include "ClientPrivateInfo.h"

#include "URL.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/Base64.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"
#include "filesystem/CurlFile.h"
#include "filesystem/ZipFile.h"
#include "settings/Settings.h"
#include "video/VideoInfoTag.h"

#include <stdlib.h>

CCloudUtils& CCloudUtils::GetInstance()
{
  static CCloudUtils sCloudUtils;
  return sCloudUtils;
}

void testclientinfo(void)
{
  std::string clientInfoString = CClientPrivateInfo::Decrypt();
  CVariant clientInfo(CVariant::VariantTypeArray);
  CJSONVariantParser::Parse(clientInfoString, clientInfo);

  CLog::Log(LOGDEBUG, "testclientinfo %s", clientInfoString.c_str());
}

std::string CCloudUtils::m_dropboxAccessToken;
std::string CCloudUtils::m_dropboxAppID;
std::string CCloudUtils::m_dropboxAppSecret;
std::string CCloudUtils::m_googleAppID;
std::string CCloudUtils::m_googleAppSecret;
std::string CCloudUtils::m_googleAccessToken;
std::string CCloudUtils::m_googleRefreshToken;
void CCloudUtils::ParseAuth2()
{
  std::string clientInfoString = CClientPrivateInfo::Decrypt();
  CVariant clientInfo(CVariant::VariantTypeArray);
  CJSONVariantParser::Parse(clientInfoString, clientInfo);
  for (auto variantItemIt = clientInfo.begin_array(); variantItemIt != clientInfo.end_array(); ++variantItemIt)
  {
    const auto &client = *variantItemIt;
    if (client["client"].asString() == "dropbox")
    {
      m_dropboxAppID = client["client_id"].asString();
      m_dropboxAppSecret = client["client_secret"].asString();
    }
    else if (client["client"].asString() == "gdrive")
    {
      m_googleAppID = client["client_id"].asString();
      m_googleAppSecret = client["client_secret"].asString();
    }
  }
  m_googleAccessToken = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_CLOUDGOOGLETOKEN);
  m_dropboxAccessToken = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_CLOUDDROPBOXTOKEN);
}

std::string CCloudUtils::GetDropboxAppKey()
{
  ParseAuth2();
  return m_dropboxAppID;
}

std::string CCloudUtils::GetGoogleAppKey()
{
  ParseAuth2();
  return m_googleAppID;
}

std::string CCloudUtils::GetDropboxCSRF()
{
  return GenerateRandom16Byte();
}

std::string CCloudUtils::GetAccessToken(std::string service)
{
  ParseAuth2();
  if (service == "dropbox")
    return m_dropboxAccessToken;
  else if (service == "google")
  {
    CheckGoogleTokenValidity();
    return m_googleAccessToken;
  }
  
  return "";
}

bool CCloudUtils::AuthorizeCloud(std::string service, std::string authCode)
{
  ParseAuth2();
  if (service == "dropbox")
  {
    CURL curl("https://api.dropbox.com/1/oauth2/token?grant_type=authorization_code&code=" + authCode);
    curl.SetUserName(m_dropboxAppID);
    curl.SetPassword(m_dropboxAppSecret);
    
    std::string response;
    XFILE::CCurlFile curlfile;
    if (curlfile.Post(curl.Get(), "", response))
    {
      CVariant resultObject;
      if (CJSONVariantParser::Parse(response, resultObject))
      {
        if (resultObject.isObject() || resultObject.isArray())
        {
          m_dropboxAccessToken = resultObject["access_token"].asString();
          CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_CLOUDDROPBOXTOKEN, m_dropboxAccessToken);
          CSettings::GetInstance().Save();
          return true;
        }
      }
    }
  }
  else if (service == "google")
  {
    CURL curl("https://www.googleapis.com/oauth2/v4/token");
    curl.SetProtocolOption("seekable", "0");
   
    std::string data, response;
    data += "redirect_uri=" + CURL::Encode("urn:ietf:wg:oauth:2.0:oob");
    data += "&code=" + CURL::Encode(authCode);
    data += "&client_secret=" + CURL::Encode(m_googleAppSecret);
    data += "&client_id=" + CURL::Encode(m_googleAppID);
    data += "&scope=&grant_type=authorization_code";
    XFILE::CCurlFile curlfile;
    bool ret = curlfile.Post(curl.Get(), data, response);
    if (ret)
    {
      CVariant resultObject;
      if (CJSONVariantParser::Parse(response, resultObject))
      {
        if (resultObject.isObject() || resultObject.isArray())
        {
          m_googleAccessToken = resultObject["access_token"].asString();
          m_googleRefreshToken = resultObject["refresh_token"].asString();
          CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_CLOUDGOOGLETOKEN, m_googleAccessToken);
          CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_CLOUDGOOGLEREFRESHTOKEN, m_googleRefreshToken);
          
          time_t tNow = 0;
          CDateTime now = CDateTime::GetUTCDateTime();
          time_t tExpiry = (time_t)resultObject["expires_in"].asInteger();
          now.GetAsTime(tNow);
          CSettings::GetInstance().SetInt(CSettings::SETTING_SERVICES_CLOUDGOOGLEREFRESHTIME, tExpiry + tNow - 60);
          
          CSettings::GetInstance().Save();
          return true;
        }
      }
    }
  }
  return false;
}

std::string CCloudUtils::GenerateRandom16Byte()
{
  unsigned char buf[16];
  int i;
  srand(time(NULL));
  for (i = 0; i < 16; i++) {
    buf[i] = rand() % 256;
  }
  return Base64::Encode((const char*)buf, sizeof(buf));
}

void CCloudUtils::CheckGoogleTokenValidity()
{
  CDateTime now = CDateTime::GetUTCDateTime();
  time_t tNow = 0;
  time_t tExpiry = (time_t)CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_CLOUDGOOGLEREFRESHTIME);
  now.GetAsTime(tNow);
  if (tNow > tExpiry)
  {
    if (RefreshGoogleToken())
      CLog::Log(LOGDEBUG, "CCloudUtils::RefreshGoogleToken() refreshed");
    else
      CLog::Log(LOGDEBUG, "CCloudUtils::RefreshGoogleToken() failed to refresh, Authorize again");
  }
}

bool CCloudUtils::RefreshGoogleToken()
{
  CURL curl("https://www.googleapis.com/oauth2/v4/token");
  curl.SetProtocolOption("seekable", "0");
  
  std::string refreshToken = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_CLOUDGOOGLEREFRESHTOKEN);
  
  std::string data, response;
  data += "&refresh_token=" + CURL::Encode(refreshToken);
  data += "&client_secret=" + CURL::Encode(m_googleAppSecret);
  data += "&client_id=" + CURL::Encode(m_googleAppID);
  data += "&scope=&grant_type=refresh_token";
  XFILE::CCurlFile curlfile;
  bool ret = curlfile.Post(curl.Get(), data, response);
  if (ret)
  {
    CVariant resultObject;
    if (CJSONVariantParser::Parse(response, resultObject))
    {
      if (resultObject.isObject() || resultObject.isArray())
      {
        m_googleAccessToken = resultObject["access_token"].asString();
        CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_CLOUDGOOGLETOKEN, m_googleAccessToken);
        
        time_t tNow = 0;
        CDateTime now = CDateTime::GetUTCDateTime();
        time_t tExpiry = (time_t)resultObject["expires_in"].asInteger();
        now.GetAsTime(tNow);
        CSettings::GetInstance().SetInt(CSettings::SETTING_SERVICES_CLOUDGOOGLEREFRESHTIME, tExpiry + tNow - 60);
        
        CSettings::GetInstance().Save();
        return true;
      }
    }
  }
  
  return false;
}

bool CCloudUtils::GetURL(CFileItem &item)
{
  std::string path;
  if (item.HasVideoInfoTag()) // Music tag?
  {
    if (!item.GetVideoInfoTag()->m_strFileNameAndPath.empty())
      path = item.GetVideoInfoTag()->m_strFileNameAndPath;
  }
  else
    path = item.GetPath();
  if (StringUtils::StartsWithNoCase(path, "cloud://dropbox"))
  {
    StringUtils::TrimLeft(path,"cloud://dropbox");
    CURL curl("https://api.dropboxapi.com");
    curl.SetFileName("2/files/get_temporary_link");
    // this is key to get back gzip encoded content
    curl.SetProtocolOption("seekable", "0");
    
    CVariant body;
    body["path"] = "/" + path;
    
    std::string data;
    std::string playablePath;
    CJSONVariantWriter::Write(body, data, true);
    
    std::string dropboxAccessToken = CCloudUtils::GetAccessToken("dropbox");
    std::string response;
    // execute the POST request
    XFILE::CCurlFile curlfile;
    curlfile.SetRequestHeader("Cache-Control", "no-cache");
    curlfile.SetRequestHeader("Content-Type", "application/json");
    curlfile.SetRequestHeader("Authorization", "Bearer " + dropboxAccessToken);
    
    if (curlfile.Post(curl.Get(), data, response))
    {
      CVariant reply;
      if (CJSONVariantParser::Parse(response, reply))
      {
        playablePath = reply["link"].asString();
        item.SetPath(playablePath);
        return true;
      }
    }
  }
  return false;
}

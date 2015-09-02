/*
 *      Copyright (C) 2015 MrMC
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

#include "OpenSubtitlesSearch.h"
#include "filesystem/File.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "FileItem.h"
#include "utils/LangCodeExpander.h"

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

COpenSubtitlesSearch::COpenSubtitlesSearch()
{
}

bool COpenSubtitlesSearch::SubtitleFileSizeAndHash(const std::string &path, std::string &strSize, std::string &strHash)
{
  
  const size_t chksum_block_size = 8192;
  XFILE::CFile file;
  size_t i;
  uint64_t hash = 0;
  uint64_t buffer1[chksum_block_size*2];
  uint64_t fileSize ;
  // In natural language it calculates: size + 64k chksum of the first and last 64k
  // (even if they overlap because the file is smaller than 128k).
  file.Open(path, READ_NO_CACHE); //open file
  file.Read(buffer1, chksum_block_size*sizeof(uint64_t)); //read first 64k
  file.Seek(-(int64_t)chksum_block_size*sizeof(uint64_t), SEEK_END); //seek to the end of the file
  file.Read(&buffer1[chksum_block_size], chksum_block_size*sizeof(uint64_t)); //read last 64k

  for (i=0;i<chksum_block_size*2;i++)
    hash += buffer1[i];

  fileSize = file.GetLength();

  hash += fileSize; //add size

  file.Close(); //close file

  strHash = StringUtils::Format("%" PRIx64"", hash);     //format hash
  strSize = StringUtils::Format("%llu", fileSize); // format size
  return true;

}

bool COpenSubtitlesSearch::SubtitleSearch(const std::string &path,const std::string strLanguages,
                                          const std::string preferredLanguage,CFileItemList &subtitlesList)
{

  std::vector<std::string> languages3;
  std::vector<std::string> languages = StringUtils::Split(strLanguages, ',');
  for(std::vector<std::string>::iterator it = languages.begin(); it != languages.end(); ++it)
    languages3.push_back(g_LangCodeExpander.ConvertToISO6392T((*it).c_str()));
  
  if (LogIn())
  {
    std::string strSize;
    std::string strHash;
    SubtitleFileSizeAndHash(path, strSize, strHash);
    CLog::Log(LOGDEBUG, "%s - HASH - %s and Size - %s", __FUNCTION__, strHash.c_str(), strSize.c_str());
    /// Search for subs here
    
    std::vector<xmlrpc_c::value_array> searchParams;
    xmlrpc_c::paramList searchList;
    
    return true;
  }
  return false;
}


bool COpenSubtitlesSearch::LogIn()
{
  std::string const serverUrl("http://api.opensubtitles.org/xml-rpc");
  std::string const methodName("LogIn");
  
  xmlrpc_c::clientSimple myClient;
  xmlrpc_c::value result;
  
  myClient.call(serverUrl, methodName, "ssss", &result, "","","eng","XBMC_Subtitles");
  
  std::map<std::string, xmlrpc_c::value> const resultStruct = xmlrpc_c::value_struct(result);
  std::map<std::string, xmlrpc_c::value>::const_iterator iterStatus = resultStruct.find("status");
  
  std::string retStatus = (std::string)xmlrpc_c::value_string(iterStatus->second);
  
  if (retStatus == "200 OK")
  {
    std::map<std::string, xmlrpc_c::value>::const_iterator iterToken = resultStruct.find("token");
    m_strToken = (std::string)xmlrpc_c::value_string(iterToken->second);
    CLog::Log(LOGDEBUG, "%s - OpenSubitles returned %s", __FUNCTION__, m_strToken.c_str());
    return true;
  }
  return false;
}
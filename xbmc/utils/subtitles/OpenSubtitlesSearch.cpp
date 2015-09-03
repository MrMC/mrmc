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
#include "utils/LangCodeExpander.h"
#include "GUIInfoManager.h"
#include "video/VideoInfoTag.h"
#include "Util.h"
#include "filesystem/CurlFile.h"
#include "URL.h"


#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

#include "zlib.h"
#include "zconf.h"

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";


static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}


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
                                          const std::string preferredLanguage,
                                          std::vector<std::map<std::string, std::string>> &subtitlesList)
{
  if (LogIn())
  {
    std::string strSize;
    std::string strHash;
    SubtitleFileSizeAndHash(path, strSize, strHash);
    CLog::Log(LOGDEBUG, "%s - HASH - %s and Size - %s", __FUNCTION__, strHash.c_str(), strSize.c_str());

    xmlrpc_c::paramList searchList;
    searchList.addc(m_strToken);
    std::vector<std::map<std::string, std::string>> searchParams;
    
    std::string lg;
    std::vector<std::string> languages3;
    std::vector<std::string> languages = StringUtils::Split(strLanguages, ',');
    // convert from English to eng
    for(std::vector<std::string>::iterator it = languages.begin(); it != languages.end(); ++it)
    {
      g_LangCodeExpander.ConvertToISO6392T((*it).c_str(),lg);
      languages3.push_back(lg);
    }
    
//    hash search
    std::map<std::string, std::string> searchHashParam;
    std::string strLang = StringUtils::Join(languages3, ",");
    searchHashParam["sublanguageid"] = StringUtils::Join(languages3, ",");
    searchHashParam["moviehash"] = strHash;
    searchHashParam["moviebytesize"] = strSize;
    searchParams.push_back(searchHashParam);
    
    CVideoInfoTag* tag = g_application.CurrentFileItem().GetVideoInfoTag();
    
    std::string searchString;
    
    if (tag->m_iEpisode > -1)
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
    std::map<std::string, std::string> searchStringParam;
    
//    title search
    searchStringParam["sublanguageid"] = StringUtils::Join(languages3, ",");
    searchStringParam["query"] = searchString;
    searchParams.push_back(searchStringParam);
    searchList.addc(searchParams);
    
    std::string const serverUrl("http://api.opensubtitles.org/xml-rpc");
    std::string const methodName("SearchSubtitles");
    
    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;
    
    myClient.call(serverUrl, methodName, searchList, &result);
    
    std::map<std::string, xmlrpc_c::value> const resultStruct = xmlrpc_c::value_struct(result);
    std::map<std::string, xmlrpc_c::value>::const_iterator iterStatus = resultStruct.find("data");
    std::vector<xmlrpc_c::value> retStatus = xmlrpc_c::value_array(iterStatus->second).cvalue();
    
    std::vector<std::string> itemsNeeded = {"ZipDownloadLink", "IDSubtitleFile", "SubFileName", "SubFormat",
                                 "LanguageName", "SubRating", "ISO639", "MatchedBy", "SubHearingImpaired"
    };
    
    for (std::vector<xmlrpc_c::value>::iterator it = retStatus.begin() ; it != retStatus.end(); ++it)
    {
      std::map<std::string, std::string> subtitle;
      std::map<std::string, xmlrpc_c::value> const subtitleStruct = xmlrpc_c::value_struct(*it);
      for (std::vector<std::string>::iterator is = itemsNeeded.begin() ; is != itemsNeeded.end(); ++is)
      {
        std::map<std::string, xmlrpc_c::value>::const_iterator subItem = subtitleStruct.find(*is);
        subtitle[*is] = (std::string)xmlrpc_c::value_string(subItem->second);
      }
      subtitlesList.push_back(subtitle);
    }
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

bool COpenSubtitlesSearch::Download(const std::string subID,const std::string format,std::vector<std::string> &items)
{
  std::string const serverUrl("http://api.opensubtitles.org/xml-rpc");
  std::string const methodName("DownloadSubtitles");
  
  xmlrpc_c::clientSimple myClient;
  xmlrpc_c::value result;
  xmlrpc_c::paramList searchList;
  searchList.addc(m_strToken);
  std::vector<std::string> IDs;
  IDs.push_back(subID);
  searchList.addc(IDs);
  myClient.call(serverUrl, methodName, searchList, &result);
  
  std::map<std::string, xmlrpc_c::value> const resultStruct = xmlrpc_c::value_struct(result);
  std::map<std::string, xmlrpc_c::value>::const_iterator iterStatus = resultStruct.find("status");
  
  std::string retStatus = (std::string)xmlrpc_c::value_string(iterStatus->second);
  
  if (retStatus == "200 OK")
  {
    std::map<std::string, xmlrpc_c::value>::const_iterator iterStatus = resultStruct.find("data");
    std::vector<xmlrpc_c::value> retStatus = xmlrpc_c::value_array(iterStatus->second).cvalue();
    for (std::vector<xmlrpc_c::value>::iterator it = retStatus.begin() ; it != retStatus.end(); ++it)
    {
      std::map<std::string, xmlrpc_c::value> const subtitleStruct = xmlrpc_c::value_struct(*it);
      std::map<std::string, xmlrpc_c::value>::const_iterator iterStatus = subtitleStruct.find("data");
      std::string zipdata = (std::string)xmlrpc_c::value_string(iterStatus->second);
      std::string zipdata64Decoded = base64_decode(zipdata);
      std::string zipdata64DecodedInflated;
      gzipInflate(zipdata64Decoded,zipdata64DecodedInflated);
      XFILE::CFile file;
      std::string destination = StringUtils::Format("special://temp/%s.%s",
                                                    StringUtils::CreateUUID().c_str(),
                                                    format.c_str()
                                                    );
      file.OpenForWrite(destination);
      file.Write(zipdata64DecodedInflated.c_str(), zipdata64DecodedInflated.size());
      items.push_back(destination);
    }
    CLog::Log(LOGDEBUG, "%s - OpenSubitles subfile downloaded", __FUNCTION__);
    return true;
  }
  return false;
}

// below from http://windrealm.org/tutorials/decompress-gzip-stream.php
bool COpenSubtitlesSearch::gzipInflate( const std::string& compressedBytes, std::string& uncompressedBytes )
{
  if ( compressedBytes.size() == 0 ) {
    uncompressedBytes = compressedBytes ;
    return true ;
  }
  
  uncompressedBytes.clear() ;
  
  unsigned full_length = compressedBytes.size() ;
  unsigned half_length = compressedBytes.size() / 2;
  
  unsigned uncompLength = full_length ;
  char* uncomp = (char*) calloc( sizeof(char), uncompLength );
  
  z_stream strm;
  strm.next_in = (Bytef *) compressedBytes.c_str();
  strm.avail_in = compressedBytes.size() ;
  strm.total_out = 0;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  
  bool done = false ;
  
  if (inflateInit2(&strm, (16+MAX_WBITS)) != Z_OK) {
    free( uncomp );
    return false;
  }
  
  while (!done) {
    // If our output buffer is too small
    if (strm.total_out >= uncompLength ) {
      // Increase size of output buffer
      char* uncomp2 = (char*) calloc( sizeof(char), uncompLength + half_length );
      memcpy( uncomp2, uncomp, uncompLength );
      uncompLength += half_length ;
      free( uncomp );
      uncomp = uncomp2 ;
    }
    
    strm.next_out = (Bytef *) (uncomp + strm.total_out);
    strm.avail_out = uncompLength - strm.total_out;
    
    // Inflate another chunk.
    int err = inflate (&strm, Z_SYNC_FLUSH);
    if (err == Z_STREAM_END) done = true;
    else if (err != Z_OK)  {
      break;
    }
  }
  
  if (inflateEnd (&strm) != Z_OK) {
    free( uncomp );
    return false;
  }
  
  for ( size_t i=0; i<strm.total_out; ++i ) {
    uncompressedBytes += uncomp[ i ];
  }
  free( uncomp );
  return true ;
}

// below from http://www.adp-gmbh.ch/cpp/common/base64.html
std::string COpenSubtitlesSearch::base64_decode(std::string const& encoded_string)
{
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;
  
  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);
      
      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
      
      for (i = 0; (i < 3); i++)
        ret += char_array_3[i];
      i = 0;
    }
  }
  
  if (i) {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;
    
    for (j = 0; j <4; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);
    
    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
    
    for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }
  
  return ret;
}
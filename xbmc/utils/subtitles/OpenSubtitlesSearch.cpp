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
#include "URL.h"
#include "filesystem/CurlFile.h"
#include "filesystem/ZipManager.h"
#include "utils/JSONVariantParser.h"
#include "utils/URIUtils.h"

static std::string restUrl = "https://rest.opensubtitles.org";

CVariant ServerChat(CURL curl)
{
  std::string strUA = StringUtils::Format("mrmc_v%i.%i",CCompileInfo::GetMajor(),CCompileInfo::GetMinor());
  StringUtils::ToLower(strUA);
  CVariant resultObject;
  std::string responseStr;
  XFILE::CCurlFile curlFile;
  curlFile.SetRequestHeader("X-User-Agent", strUA);
  if (curlFile.Get(curl.Get(),responseStr))
  {
      if (CJSONVariantParser::Parse(responseStr, resultObject))
      {
        if (resultObject.isObject() || resultObject.isArray())
          return resultObject;
      }
  }
  return resultObject;
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

bool COpenSubtitlesSearch::SubtitleSearch(const std::string &path,const std::string strLanguages,
                                          const std::string preferredLanguage,
                                          CFileItemList &subtitlesList,
                                          const std::string &strSearch)
{

  if (!m_authenticated)
  {
    CPasswordManager::GetInstance().SetUserPass(ModuleName(), m_strUser, m_strPass);
  }

  std::string lg;
  CURL curl(restUrl);
  CVariant responseJ;
  std::string strSize;
  std::string strHash;
  std::vector<std::string> languages = StringUtils::Split(strLanguages, ',');
  CSubtitleUtilities::SubtitleFileSizeAndHash(path, strSize, strHash);
  CLog::Log(LOGDEBUG, "%s - HASH - %s and Size - %s", __FUNCTION__, strHash.c_str(), strSize.c_str());

  // convert from English to eng
  for(std::vector<std::string>::iterator it = languages.begin(); it != languages.end(); ++it)
  {
    g_LangCodeExpander.ConvertToISO6392B((*it).c_str(),lg);
    std::string filename = StringUtils::Format("search/moviehash-%s/moviebytesize-%s/sublanguageid-%s",strHash.c_str(), strSize.c_str(),lg.c_str());
    curl.SetFileName(filename);
    CVariant response = ServerChat(curl);
    if (response[0].size() > 0)
      responseJ.append(response[0]);
  }

  if (responseJ.size() < 1)
  {
    CVideoInfoTag* tag = g_application.CurrentFileItem().GetVideoInfoTag();

    std::string searchString;
    if (!strSearch.empty())
    {
      searchString = strSearch;
    }
    else if (tag->m_iEpisode > -1)
    {
      searchString = StringUtils::Format("%s/season-%.2d/episode-%.2d",tag->m_strShowTitle.c_str()
                                                        ,tag->m_iSeason
                                                        ,tag->m_iEpisode
                                         );
    }
    else
    {
      if (tag->GetYear() > 0)
      {
        int year = tag->GetYear();
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

    StringUtils::Replace(searchString, " ", "%20");
    StringUtils::Replace(searchString, "'", "");

    // convert from English to eng
    for(std::vector<std::string>::iterator it = languages.begin(); it != languages.end(); ++it)
    {
      g_LangCodeExpander.ConvertToISO6392B((*it).c_str(),lg);
      std::string filename = StringUtils::Format("search/query-%s/sublanguageid-%s",searchString.c_str(),lg.c_str());
      curl.SetFileName(filename);
      CVariant response = ServerChat(curl);
      if (response[0].size() > 0)
        responseJ.append(response[0]);
    }
  }

  if (responseJ.size() > 0 && !responseJ.isNull() && responseJ.isArray())
  {
    for (auto variantIt = responseJ.begin_array(); variantIt != responseJ.end_array(); ++variantIt)
    {
      CFileItemPtr item(new CFileItem());
      const auto varItem = *variantIt;
      if (varItem.isMember("LanguageName"))
      {
        item->SetLabel(varItem["LanguageName"].asString());
      }
      if (varItem.isMember("SubFileName"))
      {
        item->SetLabel2(varItem["SubFileName"].asString());
      }
      if (varItem.isMember("SubRating"))
      {
        item->SetIconImage(StringUtils::Format("%lli", varItem["SubRating"].asInteger()/2));
      }
      if (varItem.isMember("ISO639"))
      {
        item->SetArt("thumb",varItem["ISO639"].asString());
      }
      if (varItem.isMember("MatchedBy"))
      {
        item->SetProperty("sync", varItem["MatchedBy"].asString() == "moviehash" ? "true":"false");
      }
      if (varItem.isMember("SubHearingImpaired"))
      {
        item->SetProperty("hearing_imp", varItem["SubHearingImpaired"].asString() == "1" ? "true":"false");
      }
      if (varItem.isMember("ZipDownloadLink"))
      {
        item->SetProperty("ZipDownloadLink", varItem["ZipDownloadLink"].asString());
      }
      if (varItem.isMember("IDSubtitleFile"))
      {
        item->SetProperty("IDSubtitleFile", varItem["IDSubtitleFile"].asString());
      }
      if (varItem.isMember("SubFormat"))
      {
        item->SetProperty("SubFormat", varItem["SubFormat"].asString());
      }
      if (varItem.isMember("SubFileName"))
      {
        item->SetProperty("SubFileName", varItem["SubFileName"].asString());
      }
      subtitlesList.Add(item);
    }
    return true;
  }
  return false;
}

bool COpenSubtitlesSearch::Download(const CFileItem *subItem,std::vector<std::string> &items)
{
  CURL curl(subItem->GetProperty("ZipDownloadLink").asString());

  CURL zipPath(subItem->GetProperty("ZipDownloadLink").asString());
  zipPath.SetUserName(m_strUser);
  zipPath.SetPassword(m_strPass);
  XFILE::CCurlFile http;
  std::string UUID = StringUtils::CreateUUID();
  std::string destination = StringUtils::Format("special://temp/%s.zip",UUID.c_str());
  if (http.Download(zipPath.Get(), destination))
  {
    CURL pathToUrl(destination);
    std::vector<SZipEntry> entry;
    CURL url = URIUtils::CreateArchivePath("zip", pathToUrl);
    g_ZipManager.GetZipList(url, entry);
    for (std::vector<SZipEntry>::iterator it=entry.begin();it != entry.end();++it)
    {
      if (it->name[strlen(it->name)-1] == '/') // skip dirs
        continue;
      std::string strFilePath(it->name);

      CURL zipPath = URIUtils::CreateArchivePath("zip", pathToUrl, strFilePath);
      std::string sub = zipPath.Get();
      std::string subFormat = StringUtils::Format(".%s",subItem->GetProperty("SubFormat").asString().c_str());
      if (URIUtils::HasExtension(sub, subFormat))
      {
        std::string destinationSub = StringUtils::Format("special://temp/%s%s",UUID.c_str(),subFormat.c_str());
        if (XFILE::CFile::Copy(sub, destinationSub))
          items.push_back(destinationSub);
      }
    }
    XFILE::CFile::Delete(destination);
  }
  return items.size() > 0;
}

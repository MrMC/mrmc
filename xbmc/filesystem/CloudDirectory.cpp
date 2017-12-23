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

#include "CloudDirectory.h"
#include "CloudUtils.h"

#include "Application.h"
#include "DirectoryCache.h"
#include "FileItem.h"
#include "URL.h"
#include "network/Network.h"
#include "network/Socket.h"
#include "filesystem/Directory.h"
#include "filesystem/CurlFile.h"
#include "filesystem/ZipFile.h"
#include "utils/log.h"
#include "utils/Base64.h"
#include "utils/StringUtils.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/Variant.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML.h"

#include "video/VideoDatabase.h"
#include "music/MusicDatabase.h"

using namespace XFILE;

const std::string GOOGLEAPI_ENDPOINT = "https://www.googleapis.com";
const std::string DROPBOXAPI_ENDPOINT = "https://api.dropboxapi.com";

CCloudDirectory::CCloudDirectory()
{
}

CCloudDirectory::~CCloudDirectory()
{
}

bool CCloudDirectory::GetDirectory(const CURL& url, CFileItemList &items)
{
#if CLOUD_DEBUG_VERBOSE
  CLog::Log(LOGDEBUG, "CCloudDirectory::GetDirectory");
#endif
  assert(url.IsProtocol("cloud"));
  bool rtn = false;
  std::string rootpath = url.Get();
  if (rootpath == "cloud://")
  {
    {
      std::string serviceName = "dropbox";
      CFileItemPtr pItem(new CFileItem(serviceName));
      std::string path(rootpath);
      path = URIUtils::AddFileToFolder(path, serviceName);
      URIUtils::AddSlashAtEnd(path);
      pItem->SetPath(path);
      pItem->SetLabel("DropBox");
      pItem->SetLabelPreformated(true);
      pItem->m_bIsFolder = true;
      pItem->m_bIsShareOrDrive = true;
      // set the default folder icon
      pItem->FillInDefaultIcon();
      items.Add(pItem);
    }
//    {
//      std::string serviceName = "gdrive";
//      CFileItemPtr pItem(new CFileItem(serviceName));
//      std::string path(rootpath);
//      path = URIUtils::AddFileToFolder(path, serviceName);
//      URIUtils::AddSlashAtEnd(path);
//      pItem->SetPath(path);
//      pItem->SetLabel("Google Drive");
//      pItem->SetLabelPreformated(true);
//      pItem->m_bIsFolder = true;
//      pItem->m_bIsShareOrDrive = true;
//      // set the default folder icon
//      pItem->FillInDefaultIcon();
//      items.Add(pItem);
//    }
//    {
//      std::string serviceName = "msonedrive";
//      CFileItemPtr pItem(new CFileItem(serviceName));
//      std::string path(rootpath);
//      path = URIUtils::AddFileToFolder(path, serviceName);
//      URIUtils::AddSlashAtEnd(path);
//      pItem->SetPath(path);
//      pItem->SetLabel("Microsoft OneDrive");
//      pItem->SetLabelPreformated(true);
//      pItem->m_bIsFolder = true;
//      pItem->m_bIsShareOrDrive = true;
//      // set the default folder icon
//      pItem->FillInDefaultIcon();
//      items.Add(pItem);
//    }
//    {
//      std::string serviceName = "box";
//      CFileItemPtr pItem(new CFileItem(serviceName));
//      std::string path(rootpath);
//      path = URIUtils::AddFileToFolder(path, serviceName);
//      URIUtils::AddSlashAtEnd(path);
//      pItem->SetPath(path);
//      pItem->SetLabel("Box");
//      pItem->SetLabelPreformated(true);
//      pItem->m_bIsFolder = true;
//      pItem->m_bIsShareOrDrive = true;
//      // set the default folder icon
//      pItem->FillInDefaultIcon();
//      items.Add(pItem);
//    }
//    {
//      std::string serviceName = "owncloud";
//      CFileItemPtr pItem(new CFileItem(serviceName));
//      std::string path(rootpath);
//      path = URIUtils::AddFileToFolder(path, serviceName);
//      URIUtils::AddSlashAtEnd(path);
//      pItem->SetPath(path);
//      pItem->SetLabel("OwnCloud");
//      pItem->SetLabelPreformated(true);
//      pItem->m_bIsFolder = true;
//      pItem->m_bIsShareOrDrive = true;
//      // set the default folder icon
//      pItem->FillInDefaultIcon();
//      items.Add(pItem);
//    }

    return true;
  }

  if (url.GetHostName() == "dropbox")
  {
    CURL curl(DROPBOXAPI_ENDPOINT);
    curl.SetFileName("2/files/list_folder");
    // this is key to get back gzip encoded content
    curl.SetProtocolOption("seekable", "0");

    CVariant body;
    body["path"] = "";
    // root listing does NOT include "/", just empty string
    if (!url.GetFileName().empty())
      body["path"] = "/" + url.GetFileName();
    body["recursive"] = false;
    body["include_deleted"] = false;
    body["include_media_info"] = true;
    body["include_has_explicit_shared_members"] = false;
    std::string data;
    CJSONVariantWriter::Write(body, data, true);

    std::string dropboxAccessToken = CCloudUtils::GetAccessToken("dropbox");
    std::string response;
    // execute the POST request
    XFILE::CCurlFile curlfile;
    curlfile.SetRequestHeader("Cache-Control", "no-cache");
    curlfile.SetRequestHeader("Content-Type", "application/json");
    curlfile.SetRequestHeader("Authorization", "Bearer " + dropboxAccessToken);
    curlfile.SetRequestHeader("Accept-Encoding", "gzip");

    rtn = false;
    if (curlfile.Post(curl.Get(), data, response))
    {
      if (curlfile.GetContentEncoding() == "gzip")
      {
        std::string buffer;
        if (XFILE::CZipFile::DecompressGzip(response, buffer))
          response = std::move(buffer);
        else
          return false;
      }
      CLog::Log(LOGDEBUG, "CCloudDirectory::bullshit %s", response.c_str());
      CVariant reply;
      if (!CJSONVariantParser::Parse(response, reply))
        return false;

      const auto& variantItems = reply["entries"];
      for (auto variantItemIt = variantItems.begin_array(); variantItemIt != variantItems.end_array(); ++variantItemIt)
      {
        if (*variantItemIt == CVariant::VariantTypeNull)
          continue;

        rtn = true;
        const auto objectItem = *variantItemIt;

        std::string id = objectItem["id"].asString();
        std::string tag = objectItem[".tag"].asString();
        std::string name = objectItem["name"].asString();
        std::string path_lower = objectItem["path_lower"].asString();
        std::string path_display = objectItem["path_display"].asString();

        CFileItemPtr item(new CFileItem(name, false));

        std::string path(url.Get());
        path = URIUtils::AddFileToFolder(path, name);
        item->SetPath(path);
        item->SetLabel(name);
        item->SetLabelPreformated(true);
        if (tag == "folder")
        {
          item->m_bIsFolder = true;
        }
        else if (tag == "file")
        {
          item->m_bIsFolder = false;
        }
        item->m_bIsShareOrDrive = false;
        //just set the default folder icon
        item->FillInDefaultIcon();
        items.Add(item);

      }
      if (reply["has_more"].asBoolean())
      {
        std::string next_page_token = reply["cursor"].asString();
      }
    }
    else
    {
      CLog::Log(LOGDEBUG, "CCloudDirectory::bullshit %s", response.c_str());
    }
  }

  std::string strUrl = url.Get();
  std::string section = URIUtils::GetFileName(strUrl);
  items.SetPath(strUrl);
  items.SetLabel("DropBox");

  std::string basePath = strUrl;
  URIUtils::RemoveSlashAtEnd(basePath);
  basePath = URIUtils::GetFileName(basePath);

  return rtn;
}

DIR_CACHE_TYPE CCloudDirectory::GetCacheType(const CURL& url) const
{
  return DIR_CACHE_NEVER;
}


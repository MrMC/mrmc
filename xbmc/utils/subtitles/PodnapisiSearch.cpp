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


#include "PodnapisiSearch.h"
#include "SubtitleUtilities.h"

#include "Util.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "utils/LangCodeExpander.h"
#include "video/VideoInfoTag.h"
#include "utils/URIUtils.h"

#include <libxml/tree.h>
#include <libxml/parser.h>

CPodnapisiSearch::CPodnapisiSearch()
{
}

CPodnapisiSearch::~CPodnapisiSearch()
{
}

std::string CPodnapisiSearch::ModuleName()
{
  return "Podnapisi";
}

void CPodnapisiSearch::ChangeUserPass()
{
}

bool CPodnapisiSearch::LogIn()
{
  return true;
}

bool CPodnapisiSearch::SubtitleSearch(const std::string &path,const std::string strLanguages,
                                      const std::string preferredLanguage,
                                      CFileItemList &subtitlesList,
                                      const std::string &strSearch)
{
  std::string strSize;
  std::string strHash;
  CSubtitleUtilities::CSubtitleUtilities::SubtitleFileSizeAndHash(path, strSize, strHash);
  CLog::Log(LOGDEBUG, "%s - HASH - %s and Size - %s", __FUNCTION__, strHash.c_str(), strSize.c_str());
  
  std::string lg;
  std::vector<std::string> languages3;
  std::vector<std::string> languages = StringUtils::Split(strLanguages, ',');
  // convert from English to en
  for(std::vector<std::string>::iterator it = languages.begin(); it != languages.end(); ++it)
  {
    g_LangCodeExpander.ConvertToISO6391((*it).c_str(),lg);
    if (lg == "sr")
      lg = "sr-latn"; //hack for Serbian Latin, SR is cyrillic only
    languages3.push_back(lg);
  }
  
  std::string strLang = StringUtils::Join(languages3, ",");
  CVideoInfoTag* tag = g_application.CurrentFileItem().GetVideoInfoTag();
  
  std::string searchString;
  
  std::string searchUrl = "https://www.podnapisi.net/ppodnapisi/search?sXML=1&sL=%s&sK=%s&sY=%i&sTS=%i&sTE=%i&sMH=%s";
  
  if (!strSearch.empty())
  {
    std::string strTitleAndYear;
    std::string strTitle;
    std::string strYear;
    CUtil::CleanString(strSearch, strTitle, strTitleAndYear, strYear, false);
    searchString = StringUtils::Format(searchUrl.c_str() ,strLang.c_str()
                                       ,strTitle.c_str()
                                       ,atoi(strYear.c_str())
                                       ,0
                                       ,0
                                       ,strHash.c_str()
                                       );
  }
  else if (tag->m_iEpisode > -1)
  {
    searchString = StringUtils::Format(searchUrl.c_str() ,strLang.c_str()
                                                         ,tag->m_strShowTitle.c_str()
                                                         ,tag->m_iYear
                                                         ,tag->m_iSeason
                                                         ,tag->m_iEpisode
                                                         ,strHash.c_str()
                                       );
  }
  else
  {
    if (tag->m_iYear > 0)
    {
      searchString = StringUtils::Format(searchUrl.c_str() ,strLang.c_str()
                                                           ,tag->m_strTitle.c_str()
                                                           ,tag->m_iYear
                                                           ,0
                                                           ,0
                                                           ,strHash.c_str()
                                         );
    }
    else
    {
      std::string strName = g_application.CurrentFileItem().GetMovieName(false);
      
      std::string strTitleAndYear;
      std::string strTitle;
      std::string strYear;
      CUtil::CleanString(strName, strTitle, strTitleAndYear, strYear, false);
      searchString = StringUtils::Format(searchUrl.c_str() ,strLang.c_str()
                                                           ,strTitle.c_str()
                                                           ,atoi(strYear.c_str())
                                                           ,0
                                                           ,0
                                                           ,strHash.c_str()
                                                           );
    }
  }

  StringUtils::Replace(searchString, " ", "+");
  

  
  xmlDoc *doc = NULL;
  xmlNode *cur = NULL;
  XFILE::CFile file;
  XFILE::auto_buffer buffer;
  if (file.LoadFile(searchString, buffer) < 1)
  {
    return false;
  }
  
  doc = xmlReadMemory(buffer.get(), buffer.length(), NULL, NULL, 0);
  cur = xmlDocGetRootElement(doc);
  if (xmlStrcmp(cur->name, (const xmlChar *) "results"))
  {
    xmlFreeDoc(doc);
    return false;
  }
  cur = cur->xmlChildrenNode;
  while (cur != NULL)
  {
    if ((!xmlStrcmp(cur->name, (const xmlChar *)"subtitle")))
    {
      CFileItemPtr item(new CFileItem());
      xmlNode *cur_node = NULL;
      for (cur_node = cur->children; cur_node; cur_node = cur_node->next)
      {
        if (cur_node->type == XML_ELEMENT_NODE)
        {
          char *key;
          if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"release")))
          {
            if (cur_node->xmlChildrenNode)
              item->SetLabel2((char *)XML_GET_CONTENT(cur_node->xmlChildrenNode));
          }
          else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"release")))
          {
            if (cur_node->xmlChildrenNode)
              item->SetLabel2((char *)XML_GET_CONTENT(cur_node->xmlChildrenNode));
          }
          if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"pid")))
          {
            if (cur_node->xmlChildrenNode)
              item->SetProperty("pid", (char *)XML_GET_CONTENT(cur_node->xmlChildrenNode));
          }
          if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"languageName")))
          {
            if (cur_node->xmlChildrenNode)
            {
              key = (char *)XML_GET_CONTENT(cur_node->xmlChildrenNode);
              if (!strcmp(key,"Serbian (Latin)"))
                key = (char *)"Serbian";
              std::string lng;
              g_LangCodeExpander.ConvertToISO6391(key, lng);
              item->SetArt("thumb",lng);
              item->SetLabel(key);
            }
          }
          if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"rating")))
          {
            if (cur_node->xmlChildrenNode)
            {
              key = (char *)XML_GET_CONTENT(cur_node->xmlChildrenNode);
              item->SetIconImage(StringUtils::Format("%i", (int)(atof(key)*5)));
            }
          }
          if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"flags")))
          {
            char *key;
            if (cur_node->xmlChildrenNode)
            {
              key = (char *)XML_GET_CONTENT(cur_node->xmlChildrenNode);
              item->SetProperty("hearing_imp", strchr(key, 'n') != NULL ? "true":"false");
            }
          }
        }
      }
      subtitlesList.Add(item);
    }
    cur = cur->next;
  }
  xmlFreeDoc(doc);       // free document
  xmlCleanupParser();    // Free globals
  
  return true;
}

bool CPodnapisiSearch::Download(const CFileItem *subItem,std::vector<std::string> &items)
{
  std::string ID = subItem->GetProperty("pid").asString();
  std::string strUrl = StringUtils::Format("http://www.podnapisi.net/subtitles/%s/download?container=zip",ID.c_str());
  std::string zipDestination = StringUtils::Format("special://temp/%s.%s",
                                                StringUtils::CreateUUID().c_str(),
                                                "zip"
                                                );

  XFILE::CFile::Copy(strUrl, zipDestination);
  
  CFileItemList zipItems;
  XFILE::CDirectory::GetDirectory(zipDestination, zipItems,"", XFILE::DIR_FLAG_NO_FILE_DIRS);
  
  for (int i=0; i<zipItems.Size(); i++)
  {
    std::string strSub = zipItems[i]->GetPath();
    std::string strSubExt = URIUtils::GetExtension(strSub);
    std::string subDestination = StringUtils::Format("special://temp/%s.%s",
                                                  StringUtils::CreateUUID().c_str(),
                                                  strSubExt.c_str()
                                                  );
    XFILE::CFile::Copy(strSub, subDestination);
    items.push_back(subDestination);
  }
  XFILE::CFile::Delete(zipDestination);
  return true;
}
/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "system.h"
#include "GUIUserMessages.h"
#include "Application.h"
#include "GUIDialogSubtitles.h"
#include "LangInfo.h"
#include "addons/AddonManager.h"
#include "cores/IPlayer.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "filesystem/AddonsDirectory.h"
#include "filesystem/File.h"
#include "filesystem/SpecialProtocol.h"
#include "filesystem/StackDirectory.h"
#include "guilib/GUIKeyboardFactory.h"
#include "input/Key.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "utils/JobManager.h"
#include "utils/LangCodeExpander.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/subtitles/OpenSubtitlesSearch.h"
#include "utils/subtitles/PodnapisiSearch.h"
#include "URL.h"
#include "Util.h"
#include "video/VideoDatabase.h"
#include "filesystem/Directory.h"
#include "filesystem/CurlFile.h"

using namespace ADDON;
using namespace XFILE;

#define CONTROL_NAMELABEL            100
#define CONTROL_NAMELOGO             110
#define CONTROL_SUBLIST              120
#define CONTROL_SUBSEXIST            130
#define CONTROL_SUBSTATUS            140
#define CONTROL_SERVICELIST          150
#define CONTROL_MANUALSEARCH         160
#define CONTROL_CHANGECREDENTIALS    170
#define CONTROL_FILENAME             180

/*! \brief simple job to retrieve a directory and store a string (language)
 */
class CSubtitlesJob: public CJob
{
public:
  CSubtitlesJob(CSubtitleSearch *currentService, const std::string &strPlayingFile,const std::string &strLanguages,const std::string &preferredLanguage, const std::string &strSearch)
  : m_currentService(currentService),
    m_language(strLanguages),
    m_strPlayingFile(strPlayingFile),
    m_strPrefLanguage(preferredLanguage),
    m_strSearch(strSearch),
    m_search(true)
  {
    m_subtitles = new CFileItemList;
  }
  CSubtitlesJob(CSubtitleSearch *currentService, CFileItem *subItem, const std::vector<std::string> &items)
  : m_currentService(currentService),
    m_subItem(subItem),
    m_items(items),
    m_search(false)
  {
  }
  virtual ~CSubtitlesJob()
  {

  }
  virtual bool DoWork()
  {
    if (!m_strPlayingFile.empty())
      m_currentService->SubtitleSearch(m_strPlayingFile,m_language,m_strPrefLanguage,*m_subtitles,m_strSearch);
    else
      m_currentService->Download(m_subItem,m_items);
    return true;
  }
  virtual bool operator==(const CJob *job) const
  {
    if (strcmp(job->GetType(),GetType()) == 0)
    {
      const CSubtitlesJob* rjob = dynamic_cast<const CSubtitlesJob*>(job);
      if (rjob)
      {
        return m_strPlayingFile == rjob->m_strPlayingFile &&
        m_subItem == rjob->m_subItem;
      }
    }
    return false;
  }
  const CFileItemList *GetSearchItems() const { return m_subtitles; }
  const std::vector<std::string> GetDownloadItems() const { return m_items; }
  const CFileItem GetDownloadFileItem() const { return *m_subItem; }
  const bool GetFunction() const {return m_search;}
private:
  CFileItemList* m_subtitles;
  CSubtitleSearch* m_currentService;
  std::string    m_language;
  std::string    m_strPlayingFile;
  std::string    m_strPrefLanguage;
  std::string    m_strSearch;
  CFileItem *    m_subItem;
  std::vector<std::string> m_items;
  bool           m_search;
};

CGUIDialogSubtitles::CGUIDialogSubtitles(void)
    : CGUIDialog(WINDOW_DIALOG_SUBTITLES, "DialogSubtitles.xml")
    , m_subtitles(new CFileItemList)
    , m_pausedOnRun(false)
    , m_updateSubsList(false)
{
  m_loadType = KEEP_IN_MEMORY;
  m_serviceItems.push_back(new COpenSubtitlesSearch);
  m_serviceItems.push_back(new CPodnapisiSearch);
}

CGUIDialogSubtitles::~CGUIDialogSubtitles(void)
{
  delete m_subtitles;
  m_serviceItems.clear();
}

void CGUIDialogSubtitles::OnJobComplete(unsigned int jobID, bool success, CJob *job)
{
  
  if (((CSubtitlesJob *)job)->GetFunction())
  {
    m_subtitles->Assign(*((CSubtitlesJob *)job)->GetSearchItems());
    OnSearchComplete();
  }
  else
  {
    std::vector<std::string> m_items = ((CSubtitlesJob *)job)->GetDownloadItems();
    CFileItem CFileItem = ((CSubtitlesJob *)job)->GetDownloadFileItem();
    OnDownloadComplete(m_items,&CFileItem);
  }
  CJobQueue::OnJobComplete(jobID, success, job);
}

bool CGUIDialogSubtitles::OnMessage(CGUIMessage& message)
{
  if (message.GetMessage() == GUI_MSG_CLICKED)
  {
    int iControl = message.GetSenderId();
    bool selectAction = (message.GetParam1() == ACTION_SELECT_ITEM ||
                         message.GetParam1() == ACTION_MOUSE_LEFT_CLICK);

    if (selectAction && iControl == CONTROL_SUBLIST)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_SUBLIST);
      OnMessage(msg);

      int item = msg.GetParam1();
      if (item >= 0 && item < m_subtitles->Size())
        Download(item);
      return true;
    }
    else if (selectAction && iControl == CONTROL_SERVICELIST)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_SERVICELIST);
      OnMessage(msg);
      
      int item = msg.GetParam1();

      SetService(item);
      Search();
      return true;
    }
    else if (iControl == CONTROL_MANUALSEARCH)
    {
      //manual search
      if (CGUIKeyboardFactory::ShowAndGetInput(m_strManualSearch, CVariant{g_localizeStrings.Get(24121)}, true))
      {
        Search(m_strManualSearch);
        return true;
      }
    }
    else if (iControl == CONTROL_CHANGECREDENTIALS)
    {
      //change credentials
      ChangeCredentials();
      return true;
    }
  }
  else if (message.GetMessage() == GUI_MSG_WINDOW_DEINIT)
  {
    // Resume the video if the user has requested it
    if (g_application.m_pPlayer->IsPaused() && m_pausedOnRun)
      g_application.m_pPlayer->Pause();

    CGUIDialog::OnMessage(message);

    ClearSubtitles();
    return true;
  }
  return CGUIDialog::OnMessage(message);
}

void CGUIDialogSubtitles::OnInitWindow()
{
  // Pause the video if the user has requested it
  m_pausedOnRun = false;
  if (CSettings::GetInstance().GetBool(CSettings::SETTING_SUBTITLES_PAUSEONSEARCH) && !g_application.m_pPlayer->IsPaused())
  {
    g_application.m_pPlayer->Pause();
    m_pausedOnRun = true;
  }

  CGUIWindow::OnInitWindow();

  std::string file;
  if (g_application.CurrentFileItem().IsMediaServiceBased())
    file = g_application.CurrentFileItem().GetVideoInfoTag()->m_strServiceFile;
  else
    file = g_application.CurrentFileItem().GetPath();
  SET_CONTROL_LABEL(CONTROL_FILENAME, URIUtils::GetFileName(file));

  FillServices();
  Search();
}

void CGUIDialogSubtitles::Process(unsigned int currentTime, CDirtyRegionList &dirtyregions)
{
  if (m_bInvalidated)
  {
    // take copies of our variables to ensure we don't hold the lock for long.
    std::string status;
    CFileItemList subs;
    {
      CSingleLock lock(m_critsection);
      status = m_status;
      subs.Assign(*m_subtitles);
    }
    SET_CONTROL_LABEL(CONTROL_SUBSTATUS, status);

    if (m_updateSubsList)
    {
      CGUIMessage message(GUI_MSG_LABEL_BIND, GetID(), CONTROL_SUBLIST, 0, 0, &subs);
      OnMessage(message);
      if (!subs.IsEmpty())
      {
        // focus subtitles list
        CGUIMessage msg(GUI_MSG_SETFOCUS, GetID(), CONTROL_SUBLIST);
        OnMessage(msg);
      }
      m_updateSubsList = false;
    }
    
    int control = GetFocusedControlID();
    // nothing has focus
    if (!control)
    {
      CGUIMessage msg(GUI_MSG_SETFOCUS, GetID(), m_subtitles->IsEmpty() ?
                      CONTROL_SERVICELIST : CONTROL_SUBLIST);
      OnMessage(msg);
    }
    // subs list is focused but we have no subs
    else if (control == CONTROL_SUBLIST && m_subtitles->IsEmpty())
    {
      CGUIMessage msg(GUI_MSG_SETFOCUS, GetID(), CONTROL_SERVICELIST);
      OnMessage(msg);
    }
  }
  CGUIDialog::Process(currentTime, dirtyregions);
}

void CGUIDialogSubtitles::Search(const std::string &search/*=""*/)
{
  
  UpdateStatus(SEARCHING);
  ClearSubtitles();
  
  const CSetting *setting = CSettings::GetInstance().GetSetting(CSettings::SETTING_SUBTITLES_LANGUAGES);
  std::string strLanguages;
  if (setting)
    strLanguages = setting->ToString();
  
  std::string preferredLanguage = CSettings::GetInstance().GetString(CSettings::SETTING_LOCALE_SUBTITLELANGUAGE);
  
  if(StringUtils::EqualsNoCase(preferredLanguage, "original"))
  {
    SPlayerAudioStreamInfo info;
    std::string strLanguage;
    
    g_application.m_pPlayer->GetAudioStreamInfo(CURRENT_STREAM, info);
    
    if (!g_LangCodeExpander.Lookup(info.language, strLanguage))
      strLanguage = "Unknown";
    
    preferredLanguage = strLanguage;
  }
  else if (StringUtils::EqualsNoCase(preferredLanguage, "default"))
    preferredLanguage = g_langInfo.GetEnglishLanguageName();
  
  std::string strPlayingFile = g_application.CurrentFileItem().GetPath();
  
  
  AddJob(new CSubtitlesJob(m_currentService,strPlayingFile,strLanguages,preferredLanguage,search));

}

void CGUIDialogSubtitles::OnSearchComplete()
{
  CSingleLock lock(m_critsection);
  UpdateStatus(SEARCH_COMPLETE);
  m_updateSubsList = true;
  
  if (!m_subtitles->IsEmpty() && g_application.m_pPlayer->GetSubtitleCount() == 0 &&
      m_LastAutoDownloaded != g_application.CurrentFile() && CSettings::GetInstance().GetBool(CSettings::SETTING_SUBTITLES_DOWNLOADFIRST))
  {
    CLog::Log(LOGDEBUG, "%s - Automatically download first subtitle: %s", __FUNCTION__,
              m_subtitles[0].GetLabel2().c_str());
    m_LastAutoDownloaded = g_application.CurrentFile();
    Download(0);
  }
  
  SetInvalid();
}


void CGUIDialogSubtitles::UpdateStatus(STATUS status)
{
  CSingleLock lock(m_critsection);
  std::string label;
  switch (status)
  {
    case NO_SERVICES:
      label = g_localizeStrings.Get(24114);
      break;
    case SEARCHING:
      label = g_localizeStrings.Get(24107);
      break;
    case SEARCH_COMPLETE:
      if (!m_subtitles->IsEmpty())
        label = StringUtils::Format(g_localizeStrings.Get(24108).c_str(), m_subtitles->Size());
      else
        label = g_localizeStrings.Get(24109);
      break;
    case DOWNLOADING:
      label = g_localizeStrings.Get(24110);
      break;
    default:
      break;
  }
  if (label != m_status)
  {
    m_status = label;
    SetInvalid();
  }
}

void CGUIDialogSubtitles::Download(const int index)
{
  UpdateStatus(DOWNLOADING);
  CFileItem *subItem = new CFileItem(*(*m_subtitles)[index]);;

  std::vector<std::string> items;
  AddJob(new CSubtitlesJob(m_currentService,subItem,items));
  
}

void CGUIDialogSubtitles::OnDownloadComplete(std::vector<std::string> items, CFileItem *subItem)
{
  SUBTITLE_STORAGEMODE storageMode = (SUBTITLE_STORAGEMODE) CSettings::GetInstance().GetInt(CSettings::SETTING_SUBTITLES_STORAGEMODE);
  
  // Get (unstacked) path
  std::string strCurrentFile = g_application.CurrentUnstackedItem().GetPath();
  
  std::string strDownloadPath = "special://temp";
  std::string strDestPath;
  std::vector<std::string> vecFiles;
  
  std::string strCurrentFilePath;
  if (StringUtils::StartsWith(strCurrentFile, "http://"))
  {
    strCurrentFile = "TempSubtitle";
    vecFiles.push_back(strCurrentFile);
  }
  else
  {
    std::string subPath = CSpecialProtocol::TranslatePath("special://subtitles");
    if (!subPath.empty())
      strDownloadPath = subPath;
    
    /* Get item's folder for sub storage, special case for RAR/ZIP items
     TODO: We need some way to avoid special casing this all over the place
     for rar/zip (perhaps modify GetDirectory?)
     */
    if (URIUtils::IsInZIP(strCurrentFile))
      strCurrentFilePath = URIUtils::GetDirectory(CURL(strCurrentFile).GetHostName());
    else
      strCurrentFilePath = URIUtils::GetDirectory(strCurrentFile);
    
    // Handle stacks
    if (g_application.CurrentFileItem().IsStack() && items.size() > 1)
    {
      CStackDirectory::GetPaths(g_application.CurrentFileItem().GetPath(), vecFiles);
      // Make sure (stack) size is the same as the size of the items handed to us, else fallback to single item
      if (items.size() !=  vecFiles.size())
      {
        vecFiles.clear();
        vecFiles.push_back(strCurrentFile);
      }
    }
    else
    {
      vecFiles.push_back(strCurrentFile);
    }
    
    if (storageMode == SUBTITLE_STORAGEMODE_MOVIEPATH &&
        CUtil::SupportsWriteFileOperations(strCurrentFilePath))
    {
      strDestPath = strCurrentFilePath;
    }
  }
  
  // Use fallback?
  if (strDestPath.empty())
    strDestPath = strDownloadPath;
  
  // Extract the language and appropriate extension
  std::string strSubLang;
  g_LangCodeExpander.ConvertToISO6391(subItem->GetLabel(), strSubLang);
  
  
  // Iterate over all items to transfer
  for (unsigned int i = 0; i < vecFiles.size() && i < (unsigned int) items.size(); i++)
  {
    std::string strUrl = items[i];
    std::string strFileName = URIUtils::GetFileName(vecFiles[i]);
    URIUtils::RemoveExtension(strFileName);
    
    // construct subtitle path
    std::string strSubExt = URIUtils::GetExtension(strUrl);
    std::string strSubName = StringUtils::Format("%s.%s%s", strFileName.c_str(), strSubLang.c_str(), strSubExt.c_str());
    
    // Handle URL encoding:
    std::string strDownloadFile = URIUtils::ChangeBasePath(strCurrentFilePath, strSubName, strDownloadPath);
    std::string strDestFile = strDownloadFile;
    
    if (!CFile::Copy(strUrl, strDownloadFile))
    {
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, strSubName, g_localizeStrings.Get(24113));
      CLog::Log(LOGERROR, "%s - Saving of subtitle %s to %s failed", __FUNCTION__, strUrl.c_str(), strDownloadFile.c_str());
    }
    else
    {
      if (strDestPath != strDownloadPath)
      {
        // Handle URL encoding:
        std::string strTryDestFile = URIUtils::ChangeBasePath(strCurrentFilePath, strSubName, strDestPath);
        
        /* Copy the file from temp to our final destination, if that fails fallback to download path
         * (ie. special://subtitles or use special://temp). Note that after the first item strDownloadPath equals strDestpath
         * so that all remaining items (including the .idx below) are copied directly to their final destination and thus all
         * items end up in the same folder
         */
        CLog::Log(LOGDEBUG, "%s - Saving subtitle %s to %s", __FUNCTION__, strDownloadFile.c_str(), strTryDestFile.c_str());
        if (CFile::Copy(strDownloadFile, strTryDestFile))
        {
          CFile::Delete(strDownloadFile);
          strDestFile = strTryDestFile;
          strDownloadPath = strDestPath; // Update download path so all the other items get directly downloaded to our final destination
        }
        else
        {
          CLog::Log(LOGWARNING, "%s - Saving of subtitle %s to %s failed. Falling back to %s", __FUNCTION__, strDownloadFile.c_str(), strTryDestFile.c_str(), strDownloadPath.c_str());
          strDestPath = strDownloadPath; // Copy failed, use fallback for the rest of the items
        }
      }
      else
      {
        CLog::Log(LOGDEBUG, "%s - Saved subtitle %s to %s", __FUNCTION__, strUrl.c_str(), strDownloadFile.c_str());
      }
      
      // for ".sub" subtitles we check if ".idx" counterpart exists and copy that as well
      if (StringUtils::EqualsNoCase(strSubExt, ".sub"))
      {
        strUrl = URIUtils::ReplaceExtension(strUrl, ".idx");
        if(CFile::Exists(strUrl))
        {
          std::string strSubNameIdx = StringUtils::Format("%s.%s.idx", strFileName.c_str(), strSubLang.c_str());
          // Handle URL encoding:
          strDestFile = URIUtils::ChangeBasePath(strCurrentFilePath, strSubNameIdx, strDestPath);
          CFile::Copy(strUrl, strDestFile);
        }
      }
      
      // Set sub for currently playing (stack) item
      if (vecFiles[i] == strCurrentFile)
        SetSubtitles(strDestFile);
      
      CFile::Delete(strUrl);
    }
  }

  // Close the window
  Close();

}
void CGUIDialogSubtitles::ClearSubtitles()
{
  CGUIMessage msg(GUI_MSG_LABEL_RESET, GetID(), CONTROL_SUBLIST);
  OnMessage(msg);
  CSingleLock lock(m_critsection);
  m_subtitles->Clear();
}

void CGUIDialogSubtitles::SetSubtitles(const std::string &subtitle)
{
  if (g_application.m_pPlayer)
  {
    g_application.m_pPlayer->AddSubtitle(subtitle);
  }
}

/// below for future use, if we add more search services
void CGUIDialogSubtitles::FillServices()
{
  if (m_serviceItems.empty())
  {
    UpdateStatus(NO_SERVICES);
    return;
  }
  
  int defaultService;
  //  OpenSubtitles --> 0
  //  Podnapisi -->     1
  const CFileItem &item = g_application.CurrentUnstackedItem();
  if (item.GetVideoContentType() == VIDEODB_CONTENT_TVSHOWS ||
      item.GetVideoContentType() == VIDEODB_CONTENT_EPISODES)
    // Set default service for tv shows
    defaultService = CSettings::GetInstance().GetInt(CSettings::SETTING_SUBTITLES_TV);
  else
    // Set default service for filemode and movies
    defaultService = CSettings::GetInstance().GetInt(CSettings::SETTING_SUBTITLES_MOVIE);
  
  CFileItemList vecItems;
  
  int service = 0;
  for (int i = 0; i < (int)m_serviceItems.size(); i++)
  {
    std::string serviceLabel = m_serviceItems[i]->ModuleName();
    CFileItemPtr item(new CFileItem(serviceLabel));
    vecItems.Add(item);
    if (i == defaultService)
      service = i;
  }
  
//   Bind our services to the UI
  CGUIMessage msg(GUI_MSG_LABEL_BIND, GetID(), CONTROL_SERVICELIST, 0, 0, &vecItems);
  OnMessage(msg);
  
  SetService(service);
}

bool CGUIDialogSubtitles::SetService(const int service)
{
  m_currentService = m_serviceItems[service];
  CGUIMessage msg2(GUI_MSG_ITEM_SELECT, GetID(), CONTROL_SERVICELIST, (int)service);
  OnMessage(msg2);
  
  std::string moduleName = m_currentService->ModuleName();
  
  CLog::Log(LOGDEBUG, "New Service [%s] ", moduleName.c_str());

  
  std::string icon = StringUtils::Format("Subtitles/%s.png", moduleName.c_str());
  SET_CONTROL_FILENAME(CONTROL_NAMELOGO, icon);

  
  SET_CONTROL_LABEL(CONTROL_NAMELABEL, moduleName);
  
  if (g_application.m_pPlayer->GetSubtitleCount() == 0)
    SET_CONTROL_HIDDEN(CONTROL_SUBSEXIST);
  else
    SET_CONTROL_VISIBLE(CONTROL_SUBSEXIST);
  
  return true;
}

void CGUIDialogSubtitles::ChangeCredentials()
{
  m_currentService->ChangeUserPass();
}

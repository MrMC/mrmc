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
#include "utils/LangCodeExpander.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "URL.h"
#include "Util.h"
#include "video/VideoDatabase.h"
#include "filesystem/Directory.h"

using namespace ADDON;
using namespace XFILE;

#define CONTROL_NAMELABEL            100
#define CONTROL_NAMELOGO             110
#define CONTROL_SUBLIST              120
#define CONTROL_SUBSEXIST            130
#define CONTROL_SUBSTATUS            140
#define CONTROL_SERVICELIST          150
#define CONTROL_MANUALSEARCH         160

CGUIDialogSubtitles::CGUIDialogSubtitles(void)
    : CGUIDialog(WINDOW_DIALOG_SUBTITLES, "DialogSubtitles.xml")
    , m_subtitles_searcher (new COpenSubtitlesSearch)
    , m_subtitles(new CFileItemList)
    , m_serviceItems(new CFileItemList)
    , m_pausedOnRun(false)
    , m_updateSubsList(false)
{
  m_loadType = KEEP_IN_MEMORY;
}

CGUIDialogSubtitles::~CGUIDialogSubtitles(void)
{
  delete m_subtitles;
  delete m_serviceItems;
  delete m_subtitles_searcher;
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
        Download(*m_subtitles->Get(item));
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
  }
  else if (message.GetMessage() == GUI_MSG_WINDOW_DEINIT)
  {
    // Resume the video if the user has requested it
    if (g_application.m_pPlayer->IsPaused() && m_pausedOnRun)
      g_application.m_pPlayer->Pause();

    CGUIDialog::OnMessage(message);

    ClearSubtitles();
    ClearServices();
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
  
  std::vector<std::map<std::string, std::string>> subtitlesList;
  
  m_subtitles_searcher->SubtitleSearch(strPlayingFile,strLanguages,preferredLanguage,subtitlesList);
  
  for (std::vector<std::map<std::string, std::string>>::iterator is = subtitlesList.begin() ; is != subtitlesList.end(); ++is)
  {
    CFileItemPtr item(new CFileItem());
    std::map<std::string, std::string> sub = *is;
    item->SetLabel(sub["LanguageName"]);
    item->SetLabel2(sub["SubFileName"]);
    item->SetIconImage(sub["SubRating"]);
    item->SetArt("thumb",sub["ISO639"]);
    item->SetProperty("sync", sub["MatchedBy"] == "moviehash" ? "true":"false");
    item->SetProperty("hearing_imp", sub["SubHearingImpaired"] == "1" ? "true":"false");
    m_subtitles->Add(item);
    
  }
  
  UpdateStatus(SEARCH_COMPLETE);
  m_updateSubsList = true;
  
  if (!m_subtitles->IsEmpty() && g_application.m_pPlayer->GetSubtitleCount() == 0 &&
      m_LastAutoDownloaded != g_application.CurrentFile() && CSettings::GetInstance().GetBool(CSettings::SETTING_SUBTITLES_DOWNLOADFIRST))
  {
    CFileItemPtr item = m_subtitles->Get(0);
    CLog::Log(LOGDEBUG, "%s - Automatically download first subtitle: %s", __FUNCTION__, item->GetLabel2().c_str());
    m_LastAutoDownloaded = g_application.CurrentFile();
    Download(*item);
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

void CGUIDialogSubtitles::Download(const CFileItem &subtitle)
{
  UpdateStatus(DOWNLOADING);

  // subtitle URL should be of the form plugin://<addonid>/?param=foo&param=bar
  // we just append (if not already present) the action=download parameter.
  CURL url(subtitle.GetURL());
  if (url.GetOption("action").empty())
    url.SetOption("action", "download");


}

void CGUIDialogSubtitles::OnDownloadComplete(const CFileItemList *items, const std::string &language)
{

  SUBTITLE_STORAGEMODE storageMode = (SUBTITLE_STORAGEMODE) CSettings::GetInstance().GetInt(CSettings::SETTING_SUBTITLES_STORAGEMODE);

  // Get (unstacked) path
  std::string strCurrentFile = g_application.CurrentUnstackedItem().GetPath();

  std::string strDownloadPath = "special://temp";
  std::string strDestPath;
  std::vector<std::string> vecFiles;

  std::string strCurrentFilePath;
  if (StringUtils::StartsWith(strCurrentFilePath, "http://"))
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
    if (URIUtils::IsInRAR(strCurrentFile) || URIUtils::IsInZIP(strCurrentFile))
      strCurrentFilePath = URIUtils::GetDirectory(CURL(strCurrentFile).GetHostName());
    else
      strCurrentFilePath = URIUtils::GetDirectory(strCurrentFile);

    // Handle stacks
    if (g_application.CurrentFileItem().IsStack() && items->Size() > 1)
    {
      CStackDirectory::GetPaths(g_application.CurrentFileItem().GetPath(), vecFiles);
      // Make sure (stack) size is the same as the size of the items handed to us, else fallback to single item
      if (items->Size() != (int) vecFiles.size())
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
  g_LangCodeExpander.ConvertToISO6391(language, strSubLang);

  // Iterate over all items to transfer
  for (unsigned int i = 0; i < vecFiles.size() && i < (unsigned int) items->Size(); i++)
  {
    std::string strUrl = items->Get(i)->GetPath();
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

void CGUIDialogSubtitles::ClearServices()
{
  CGUIMessage msg(GUI_MSG_LABEL_RESET, GetID(), CONTROL_SERVICELIST);
  OnMessage(msg);
  m_serviceItems->Clear();
  m_currentService.clear();
}

void CGUIDialogSubtitles::SetSubtitles(const std::string &subtitle)
{
  if (g_application.m_pPlayer)
  {
    g_application.m_pPlayer->AddSubtitle(subtitle);
  }
}

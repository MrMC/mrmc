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

#include "GUIUserMessages.h"
#include "GUIWindowMediaSources.h"
#include "utils/FileUtils.h"
#include "Util.h"
#include "PlayListPlayer.h"
#include "GUIPassword.h"
#include "filesystem/MultiPathDirectory.h"
#include "filesystem/VideoDatabaseDirectory.h"
#include "dialogs/GUIDialogOK.h"
#include "PartyModeManager.h"
#include "music/MusicDatabase.h"
#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogYesNo.h"
#include "filesystem/Directory.h"
#include "FileItem.h"
#include "Application.h"
#include "messaging/ApplicationMessenger.h"
#include "profiles/ProfilesManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/MediaSettings.h"
#include "settings/MediaSourceSettings.h"
#include "settings/Settings.h"
#include "services/ServicesManager.h"
#include "input/Key.h"
#include "guilib/LocalizeStrings.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "guilib/GUIKeyboardFactory.h"
#include "video/VideoInfoScanner.h"
#include "video/dialogs/GUIDialogVideoInfo.h"
#include "pvr/recordings/PVRRecording.h"
#include "ContextMenuManager.h"
#include "services/ServicesManager.h"

#include "video/windows/GUIWindowVideoBase.h"

#include <utility>

using namespace XFILE;
using namespace VIDEODATABASEDIRECTORY;
using namespace KODI::MESSAGING;

#define CONTROL_BTNVIEWASICONS     2
#define CONTROL_BTNSORTBY          3
#define CONTROL_BTNSORTASC         4
#define CONTROL_BTNSEARCH          8
#define CONTROL_LABELFILES        12

#define CONTROL_BTN_FILTER        19
#define CONTROL_BTNSHOWMODE       10
#define CONTROL_BTNSHOWALL        14
#define CONTROL_UNLOCK            11

#define CONTROL_FILTER            15
#define CONTROL_BTNPARTYMODE      16
#define CONTROL_LABELEMPTY        18

#define CONTROL_UPDATE_LIBRARY    20

CGUIWindowMediaSources::CGUIWindowMediaSources(void)
    : CGUIMediaWindow(WINDOW_MEDIA_SOURCES, "MyMediaSources.xml")
{
}

CGUIWindowMediaSources::~CGUIWindowMediaSources(void)
{
}

CGUIWindowMediaSources &CGUIWindowMediaSources::GetInstance()
{
  static CGUIWindowMediaSources sWNav;
  return sWNav;
}

bool CGUIWindowMediaSources::OnAction(const CAction &action)
{
  return CGUIMediaWindow::OnAction(action);
}

bool CGUIWindowMediaSources::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
  case GUI_MSG_WINDOW_RESET:
    m_vecItems->SetPath("");
    break;
  case GUI_MSG_WINDOW_DEINIT:
    break;
  case GUI_MSG_WINDOW_INIT:
    break;

  case GUI_MSG_CLICKED:
    break;
  }
  return CGUIMediaWindow::OnMessage(message);
}

bool CGUIWindowMediaSources::Update(const std::string &strDirectory, bool updateFilterPath /* = true */)
{
  if (!CGUIMediaWindow::Update(strDirectory, updateFilterPath))
    return false;

  return true;
}

bool CGUIWindowMediaSources::GetDirectory(const std::string &strDirectory, CFileItemList &items)
{
  items.Clear();
  bool result;

  if (strDirectory.empty() || strDirectory == "mediasources://")
  {
    CFileItemPtr vItem(new CFileItem("Video"));
    vItem->m_bIsFolder = true;
    vItem->m_bIsShareOrDrive = true;
    vItem->SetPath("mediasources://video/");
    vItem->SetLabel(g_localizeStrings.Get(157));
    items.Add(vItem);

    CFileItemPtr mItem(new CFileItem("Music"));
    mItem->m_bIsFolder = true;
    mItem->m_bIsShareOrDrive = true;
    mItem->SetPath("mediasources://music/");
    mItem->SetLabel(g_localizeStrings.Get(2));
    items.Add(mItem);

    CFileItemPtr pItem(new CFileItem("Pictures"));
    pItem->m_bIsFolder = true;
    pItem->m_bIsShareOrDrive = true;
    pItem->SetPath("mediasources://pictures/");
    pItem->SetLabel(g_localizeStrings.Get(1213));
    items.Add(pItem);

//    items.SetLabel(g_localizeStrings.Get(20094));
    items.SetPath("mediasources://");
    result = true;
  }
  else
  {
    if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://video"))
    {
      std::string strParentPath;
      URIUtils::GetParentPath(strDirectory, strParentPath);
      SetHistoryForPath(strParentPath);
      std::vector<std::string> params;
      params.push_back("sources://video/");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=" + strParentPath);
      g_windowManager.ActivateWindow(WINDOW_VIDEO_NAV, params);
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://music/"))
    {
      std::string strParentPath;
      URIUtils::GetParentPath(strDirectory, strParentPath);
      SetHistoryForPath(strParentPath);
      std::vector<std::string> params;
      params.push_back("sources://music/");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=" + strParentPath);
      g_windowManager.ActivateWindow(WINDOW_MUSIC_NAV, params);
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://pictures/"))
    {
      std::string strParentPath;
      URIUtils::GetParentPath(strDirectory, strParentPath);
      SetHistoryForPath(strParentPath);
      std::vector<std::string> params;
      params.push_back("sources://pictures/");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=" + strParentPath);
      g_windowManager.ActivateWindow(WINDOW_PICTURES, params);
    }
    result = true;
  }
  return result;
}


bool CGUIWindowMediaSources::OnClick(int iItem)
{
  return CGUIMediaWindow::OnClick(iItem);
}

std::string CGUIWindowMediaSources::GetStartFolder(const std::string &dir)
{
  return CGUIMediaWindow::GetStartFolder(dir);
}

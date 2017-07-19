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

#include "config.h"

#include "LiteUtils.h"

#include "CompileInfo.h"
#include "Util.h"
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/GUIMessage.h"
#include "guilib/LocalizeStrings.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#if defined(TARGET_ANDROID)
#include "platform/android/activity/AndroidFeatures.h"
#elif defined(TARGET_DARWIN)
#include "platform/darwin/DarwinUtils.h"
#endif


int CLiteUtils::nextReminderTrigger = 0;
bool CLiteUtils::NeedReminding()
{
  if (nextReminderTrigger <= 0)
  {
    nextReminderTrigger = CUtil::GetRandomNumber(3,8);
    return true;
  }
  else
    nextReminderTrigger--;
  return nextReminderTrigger <= 0;
}

int CLiteUtils::GetItemSizeLimit()
{
  // if we are lite, trim to 7 items + ".."
  return 7 + 1;
}

void CLiteUtils::ShowIsLiteDialog(int preTruncateSize)
{
  if (!NeedReminding())
    return;
  std::string line2 = StringUtils::Format(g_localizeStrings.Get(897).c_str(), preTruncateSize, GetItemSizeLimit());
  std::string line3;
  std::string path;
#if defined(TARGET_DARWIN)
  #if defined(TARGET_DARWIN_TVOS)
    path = "https://itunes.apple.com/us/app/mrmc/id1059536415?mt=8&at=11l4L8";
  #elif defined(TARGET_DARWIN_IOS)
    path = "https://itunes.apple.com/us/app/mrmc-touch/id1062986407?mt=8&at=11l4L8";
#endif
  line3 = StringUtils::Format(g_localizeStrings.Get(898).c_str(), "Apple");
#elif defined(TARGET_ANDROID)
  line3 = StringUtils::Format(g_localizeStrings.Get(898).c_str(), CAndroidFeatures::IsAmazonDevice() ? "Amazon":"Google Play");
#endif
  if (!line3.empty())
  {
    CGUIDialogYesNo* pDialog = (CGUIDialogYesNo*)g_windowManager.GetWindow(WINDOW_DIALOG_YES_NO);
    if (!pDialog) return;
    
    pDialog->SetHeading(CVariant{CCompileInfo::GetAppName()});
    pDialog->SetLine(1, CVariant{896});
    pDialog->SetLine(2, CVariant{line2});
    pDialog->SetLine(3, CVariant{line3});
    pDialog->SetChoice(0, "Ok");
    pDialog->SetChoice(1, "Go to store");
    pDialog->Open();
    
    if (pDialog->IsConfirmed())
#if defined(TARGET_DARWIN)
      CDarwinUtils::OpenAppWithOpenURL(path);
#elif defined(TARGET_ANDROID)
      // some android call to open store
#endif    
  }
}

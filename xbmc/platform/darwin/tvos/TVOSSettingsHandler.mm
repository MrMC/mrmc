/*
 *      Copyright (C) 2015 Team MrMC
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

#import "system.h"

#import "TVOSSettingsHandler.h"
#import "settings/Settings.h"
#import "settings/lib/Setting.h"
#import "platform/darwin/tvos/MainController.h"

#include "threads/Atomics.h"
#include "platform/darwin/FocusEngineHandler.h"
#include "settings/AdvancedSettings.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"

static std::atomic<long> sg_singleton_lock_variable {0};
CTVOSInputSettings* CTVOSInputSettings::m_instance = nullptr;

CTVOSInputSettings&
CTVOSInputSettings::GetInstance()
{
  CAtomicSpinLock lock(sg_singleton_lock_variable);
  if (!m_instance)
    m_instance = new CTVOSInputSettings();

  return *m_instance;
}

CTVOSInputSettings::CTVOSInputSettings()
{
}

void CTVOSInputSettings::Initialize()
{
  /*
   const std::string CSettings::SETTING_INPUT_APPLESIRIBACK = "input.applesiriback";
   const std::string CSettings::SETTING_INPUT_APPLESIRIEXPERTMODE = "input.applesiriexpertmode";
   */
  bool enableExpertMode = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIEXPERTMODE);
  [g_xbmcController enableRemoteExpertMode:enableExpertMode];
  bool stopPlaybackOnMenu = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIBACK);
  [g_xbmcController stopPlaybackOnMenu:stopPlaybackOnMenu];

  std::vector<CVariant> exts = CSettings::GetInstance().GetList(CSettings::SETTING_INPUT_APPLESIRIDISABLEOSD);
  id nsstrings = [NSMutableArray new];
  for (auto &ext : exts)
  {
    std::string strExt = ext.asString();
    id nsstr = [NSString stringWithUTF8String:strExt.c_str()];
    [nsstrings addObject:nsstr];
  }
  [g_xbmcController disableOSDExtensions:nsstrings];
  
}

void CTVOSInputSettings::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  const std::string &settingId = setting->GetId();
  if (settingId == CSettings::SETTING_INPUT_APPLESIRIEXPERTMODE)
  {
    bool enableExpertMode = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIEXPERTMODE);
    [g_xbmcController enableRemoteExpertMode:enableExpertMode];
  }
  else if (settingId == CSettings::SETTING_INPUT_APPLESIRIBACK)
  {
    bool stopPlaybackOnMenu = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIBACK);
    [g_xbmcController stopPlaybackOnMenu:stopPlaybackOnMenu];
  }
  else if (settingId == CSettings::SETTING_INPUT_APPLESIRIDISABLEOSD)
  {
    std::vector<CVariant> exts = CSettings::GetInstance().GetList(CSettings::SETTING_INPUT_APPLESIRIDISABLEOSD);
    id nsstrings = [NSMutableArray new];
    for (auto &ext : exts)
    {
      std::string strExt = ext.asString();
      id nsstr = [NSString stringWithUTF8String:strExt.c_str()];
      [nsstrings addObject:nsstr];
    }
    [g_xbmcController disableOSDExtensions:nsstrings];
  }
}

void CTVOSInputSettings::SettingOptionsDisableSiriOSD(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data)
{
  std::vector<std::string> exts = StringUtils::Split(g_advancedSettings.m_videoExtensions, "|");
  std::sort(exts.begin(), exts.end());
  for (std::vector<std::string>::iterator it = exts.begin(); it != exts.end(); ++it)
  {
    std::string ext = *it;
    list.push_back(make_pair(ext,ext));
  }
}

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
  bool enable = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRI);
  [g_xbmcController setSiriRemote:enable];
  bool enableOsdSwipes = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIOSDSWIPE);
  [g_xbmcController setSiriRemoteOSDSwipes:enableOsdSwipes];
  bool enableTimeout = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRITIMEOUTENABLED);
  [g_xbmcController setShouldRemoteIdle:enableTimeout];
  int timeout = CSettings::GetInstance().GetInt(CSettings::SETTING_INPUT_APPLESIRITIMEOUT);
  [g_xbmcController setRemoteIdleTimeout:timeout];
}

void CTVOSInputSettings::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  const std::string &settingId = setting->GetId();
  if (settingId == CSettings::SETTING_INPUT_APPLESIRI)
  {
    bool enable = dynamic_cast<const CSettingBool*>(setting)->GetValue();
    [g_xbmcController setSiriRemote:enable];
  }
  else if (settingId == CSettings::SETTING_INPUT_APPLESIRIOSDSWIPE)
  {
    bool enableOsdSwipes = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIOSDSWIPE);
    [g_xbmcController setSiriRemoteOSDSwipes:enableOsdSwipes];
  }
  else if (settingId == CSettings::SETTING_INPUT_APPLESIRITIMEOUTENABLED)
  {
    bool enableTimeout = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRITIMEOUTENABLED);
    [g_xbmcController setShouldRemoteIdle:enableTimeout];
  }
  else if (settingId == CSettings::SETTING_INPUT_APPLESIRITIMEOUT)
  {
    int timeout = CSettings::GetInstance().GetInt(CSettings::SETTING_INPUT_APPLESIRITIMEOUT);
    [g_xbmcController setRemoteIdleTimeout:timeout];
  }
}

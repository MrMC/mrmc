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
   const std::string CSettings::SETTING_INPUT_APPLESIRIFOCUSEFFECTS = "input.applesirifocuseffects";
   const std::string CSettings::SETTING_INPUT_APPLESIRIFOCUSZOOM = "input.applesirifocuszoom";
   const std::string CSettings::SETTING_INPUT_APPLESIRIFOCUSLIDE = "input.applesirifocusslide";
   const std::string CSettings::SETTING_INPUT_APPLESIRIBACK = "input.applesiriback";
   const std::string CSettings::SETTING_INPUT_APPLESIRITIMEOUT = "input.applesiritimeout";
   const std::string CSettings::SETTING_INPUT_APPLESIRITIMEOUTENABLED = "input.applesiritimeoutenabled";
   const std::string CSettings::SETTING_INPUT_APPLESIRIEXPERTMODE = "input.applesiriexpertmode";
   */
  bool enableTimeout = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRITIMEOUTENABLED);
  [g_xbmcController enableRemoteIdle:enableTimeout];
  int timeout = CSettings::GetInstance().GetInt(CSettings::SETTING_INPUT_APPLESIRITIMEOUT);
  [g_xbmcController setRemoteIdleTimeout:timeout];
  bool enableFocusEffects = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIFOCUSEFFECTS);
  bool enableFocusZoom = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIFOCUSZOOM);
  CFocusEngineHandler::GetInstance().EnableFocusZoom(enableFocusEffects && enableFocusZoom);
  bool enableFocusSlide = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIFOCUSLIDE);
  CFocusEngineHandler::GetInstance().EnableFocusSlide(enableFocusEffects && enableFocusSlide);
  bool enableExpertMode = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIEXPERTMODE);
  [g_xbmcController enableRemoteExpertMode:enableExpertMode];
}

void CTVOSInputSettings::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  const std::string &settingId = setting->GetId();
  if (settingId == CSettings::SETTING_INPUT_APPLESIRITIMEOUTENABLED)
  {
    bool enableTimeout = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRITIMEOUTENABLED);
    [g_xbmcController enableRemoteIdle:enableTimeout];
  }
  else if (settingId == CSettings::SETTING_INPUT_APPLESIRITIMEOUT)
  {
    int timeout = CSettings::GetInstance().GetInt(CSettings::SETTING_INPUT_APPLESIRITIMEOUT);
    [g_xbmcController setRemoteIdleTimeout:timeout];
  }
  else if (settingId == CSettings::SETTING_INPUT_APPLESIRIFOCUSEFFECTS ||
           settingId == CSettings::SETTING_INPUT_APPLESIRIFOCUSZOOM ||
           settingId == CSettings::SETTING_INPUT_APPLESIRIFOCUSLIDE)
  {
    bool enableFocusEffects = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIFOCUSEFFECTS);
    bool enableFocusZoom = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIFOCUSZOOM);
    CFocusEngineHandler::GetInstance().EnableFocusZoom(enableFocusEffects && enableFocusZoom);
    bool enableFocusSlide = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIFOCUSLIDE);
    CFocusEngineHandler::GetInstance().EnableFocusSlide(enableFocusEffects && enableFocusSlide);
  }
  else if (settingId == CSettings::SETTING_INPUT_APPLESIRIEXPERTMODE)
  {
    bool enableExpertMode = CSettings::GetInstance().GetBool(CSettings::SETTING_INPUT_APPLESIRIEXPERTMODE);
    [g_xbmcController enableRemoteExpertMode:enableExpertMode];
  }
}

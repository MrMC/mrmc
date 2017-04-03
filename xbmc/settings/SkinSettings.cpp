/*
 *      Copyright (C) 2013 Team XBMC
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

#include <memory>
#include <string>

#include "SkinSettings.h"
#include "GUIInfoManager.h"
#include "addons/AddonManager.h"
#include "addons/Skin.h"
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GUIWindowManager.h"
#include "settings/Settings.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/XBMCTinyXML.h"

#define XML_SKINSETTINGS  "skinsettings"
#define NEW_SKIN          "skin.opacity"

CSkinSettings::CSkinSettings()
{
  Clear();
}

CSkinSettings::~CSkinSettings()
{ }

CSkinSettings& CSkinSettings::GetInstance()
{
  static CSkinSettings sSkinSettings;
  return sSkinSettings;
}

int CSkinSettings::TranslateString(const std::string &setting)
{
  return g_SkinInfo->TranslateString(setting);
}

const std::string& CSkinSettings::GetString(int setting) const
{
  return g_SkinInfo->GetString(setting);
}

void CSkinSettings::SetString(int setting, const std::string &label)
{
  g_SkinInfo->SetString(setting, label);
}

int CSkinSettings::TranslateBool(const std::string &setting)
{
  return g_SkinInfo->TranslateBool(setting);
}

bool CSkinSettings::GetBool(int setting) const
{
  return g_SkinInfo->GetBool(setting);
}

void CSkinSettings::SetBool(int setting, bool set)
{
  g_SkinInfo->SetBool(setting, set);
}

void CSkinSettings::Reset(const std::string &setting)
{
  g_SkinInfo->Reset(setting);
}

void CSkinSettings::Reset()
{
  g_SkinInfo->Reset();

  g_infoManager.ResetCache();
}

void CSkinSettings::SaveXMLSettings()
{
  g_SkinInfo->SaveSettings();
}

bool CSkinSettings::Load(const TiXmlNode *settings)
{
  if (settings == nullptr)
    return false;

  const TiXmlElement *rootElement = settings->FirstChildElement(XML_SKINSETTINGS);
  
  //return true in the case skinsettings is missing. It just means that
  //it's been migrated and it's not an error
  if (rootElement == nullptr)
  {
    CLog::Log(LOGDEBUG, "CSkinSettings: no <skinsettings> tag found");
    return true;
  }

  CSingleLock lock(m_critical);
  m_settings.clear();
  m_settings = ADDON::CSkinInfo::ParseSettings(rootElement);

  return true;
}

bool CSkinSettings::Save(TiXmlNode *settings) const
{
  if (settings == nullptr)
    return false;

  CSingleLock lock(m_critical);

  if (m_settings.empty())
    return true;

  // add the <skinsettings> tag
  TiXmlElement xmlSettingsElement(XML_SKINSETTINGS);
  TiXmlNode* settingsNode = settings->InsertEndChild(xmlSettingsElement);
  if (settingsNode == nullptr)
  {
    CLog::Log(LOGWARNING, "CSkinSettings: could not create <skinsettings> tag");
    return false;
  }

  TiXmlElement* settingsElement = settingsNode->ToElement();
  for (const auto& setting : m_settings)
  {
    if (!setting->Serialize(settingsElement))
      CLog::Log(LOGWARNING, "CSkinSettings: unable to save setting \"%s\"", setting->name.c_str());
  }

  return true;
}

void CSkinSettings::Clear()
{
  CSingleLock lock(m_critical);
  m_settings.clear();
}

void CSkinSettings::MigrateSettings(const ADDON::SkinPtr& skin)
{
  if (skin == nullptr)
    return;

  CSingleLock lock(m_critical);

  bool settingsMigrated = false;
  const std::string& skinId = skin->ID();
  std::set<ADDON::CSkinSettingPtr> settingsCopy(m_settings.begin(), m_settings.end());
  for (const auto& setting : settingsCopy)
  {
    if (!StringUtils::StartsWith(setting->name, skinId + "."))
      continue;

    std::string settingName = setting->name.substr(skinId.size() + 1);

    if (setting->GetType() == "string")
    {
      int settingNumber = skin->TranslateString(settingName);
      if (settingNumber >= 0)
        skin->SetString(settingNumber, std::dynamic_pointer_cast<ADDON::CSkinSettingString>(setting)->value);
    }
    else if (setting->GetType() == "bool")
    {
      int settingNumber = skin->TranslateBool(settingName);
      if (settingNumber >= 0)
        skin->SetBool(settingNumber, std::dynamic_pointer_cast<ADDON::CSkinSettingBool>(setting)->value);
    }

    m_settings.erase(setting);
    settingsMigrated = true;
  }

  if (settingsMigrated)
  {
    // save the skin's settings
    skin->SaveSettings();

    // save the guisettings.xml
    CSettings::GetInstance().Save();
  }
}

bool CSkinSettings::MigrateToNewSkin(const std::string skin)
{
  ADDON::AddonPtr addon;
  
  // if current skin is same as new skin, return false
  if (skin == NEW_SKIN)
    return false;
  
  // if we dont have new skin installed, return false
  if (!ADDON::CAddonMgr::GetInstance().GetAddon(NEW_SKIN, addon, ADDON::ADDON_SKIN))
    return false;
  
  CGUIDialogYesNo* pDialogYesNo = (CGUIDialogYesNo*)g_windowManager.GetWindow(WINDOW_DIALOG_YES_NO);
  if (!pDialogYesNo)
    return false;
  
  
  std::string surrentSkinName;
  if (ADDON::CAddonMgr::GetInstance().GetAddon(skin, addon, ADDON::ADDON_SKIN))
    surrentSkinName = addon->Name();
  else
    surrentSkinName = "Unknown";
  
  pDialogYesNo->SetHeading(CVariant{20263});
  pDialogYesNo->SetLine(1, CVariant{20264});
  pDialogYesNo->SetLine(2, StringUtils::Format(g_localizeStrings.Get(20265).c_str(), surrentSkinName.c_str()));
  pDialogYesNo->SetLine(3, CVariant{20266});
  pDialogYesNo->Open();
  
  // dont ask again
  CSettings::GetInstance().SetBool(CSettings::SETTING_LOOKANDFEEL_NEWSKINCHECKED, true);
  CSettings::GetInstance().Save();

  if (!pDialogYesNo->IsConfirmed())
    return false;

  if (ADDON::CAddonMgr::GetInstance().GetAddon(NEW_SKIN, addon, ADDON::ADDON_SKIN))
  {
    CSettings::GetInstance().SetString(CSettings::SETTING_LOOKANDFEEL_SKIN, addon->ID());
    CSettings::GetInstance().Save();
    return true;
  }
  
  return false;
}

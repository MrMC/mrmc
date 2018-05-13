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

#include "Application.h"
#include "cores/IPlayer.h"
#include "GUIDialogOSDSettings.h"
#include "GUIUserMessages.h"
#include "guilib/GUIButtonControl.h"
#include "guilib/GUIControlGroupList.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "utils/LangCodeExpander.h"
#include "input/Key.h"

#define SUB_BUTTON                   102
#define AUDIO_BUTTON                 103
#define SUB_GROUP_LIST              9002
#define SUB_BUTTON_TEMPLATE         2000
#define SUB_BUTTON_START            2001
#define SUB_BUTTON_END              (SUB_BUTTON_START + (int)m_subButtons.size() - 1)
#define AUDIO_GROUP_LIST            9003
#define AUDIO_BUTTON_TEMPLATE       3000
#define AUDIO_BUTTON_START          3001
#define AUDIO_BUTTON_END            (AUDIO_BUTTON_START + (int)m_audioButtons.size() - 1)

void COSDButtons::Add(unsigned int button, const std::string &label)
{
  for (const_iterator i = begin(); i != end(); ++i)
    if (i->first == button)
      return; // already have this button
  push_back(std::pair<unsigned int, std::string>(button, label));
}

CGUIDialogOSDSettings::CGUIDialogOSDSettings(void)
    : CGUIDialog(WINDOW_DIALOG_OSD_SETTINGS, "OSDSettings.xml")
{
  m_loadType = KEEP_IN_MEMORY;
}

CGUIDialogOSDSettings::~CGUIDialogOSDSettings(void)
{
}

void CGUIDialogOSDSettings::OnInitWindow()
{
  CGUIDialog::OnInitWindow();
  m_subsEnabled = g_application.m_pPlayer->GetSubtitleVisible();
  SetupButtons();
}

void CGUIDialogOSDSettings::OnDeinitWindow(int nextWindowID)
{
  ClearButtons();
  CGUIDialog::OnDeinitWindow(nextWindowID);
}

void CGUIDialogOSDSettings::FrameMove()
{
  CGUIDialog::FrameMove();
}

bool CGUIDialogOSDSettings::OnAction(const CAction &action)
{
  switch (action.GetID())
  {
//    disable this as the skins are taking care of it,
//    some skins have multiple rows and need to be able to go up
//    case ACTION_MOVE_UP:
    case ACTION_SHOW_OSD_SETTINGS:
      Close();
      return true;
      break;
  }

  return CGUIDialog::OnAction(action);
}

EVENT_RESULT CGUIDialogOSDSettings::OnMouseEvent(const CPoint &point, const CMouseEvent &event)
{
  return CGUIDialog::OnMouseEvent(point, event);
}

bool CGUIDialogOSDSettings::OnMessage(CGUIMessage& message)
{
  if (message.GetMessage() == GUI_MSG_CLICKED)
  { // something has been clicked
    if (message.GetSenderId() >= SUB_BUTTON_START && message.GetSenderId() <= SUB_BUTTON_END)
    {
      int subButton = (int)m_subButtons[message.GetSenderId() - SUB_BUTTON_START].first;
      if (subButton == 0)
      {
        if (g_application.m_pPlayer->GetSubtitleVisible())
        {
          m_subsEnabled = false;
          g_application.m_pPlayer->SetSubtitleVisible(false);
        }
      }
      else if (subButton == 1)
      {
        Close();
        g_windowManager.ActivateWindow(WINDOW_DIALOG_SUBTITLES);
      }
      else
      {
        if (!m_subsEnabled || g_application.m_pPlayer->GetSubtitle() != subButton - m_subsButtonOffset)
        {
          m_subsEnabled = true;
          g_application.m_pPlayer->SetSubtitleVisible(true);
          g_application.m_pPlayer->SetSubtitle(subButton - m_subsButtonOffset);
        }
      }
      UpdateSelectedSubs(subButton);
    }
    else if (message.GetSenderId() >= AUDIO_BUTTON_START && message.GetSenderId() <= AUDIO_BUTTON_END)
    {
      if (m_audioButtons.size() > 1)
      {
        int audioButton = (int)m_audioButtons[message.GetSenderId() - AUDIO_BUTTON_START].first;
        if (g_application.m_pPlayer->GetAudioStream() != audioButton)
        {
          g_application.m_pPlayer->SetAudioStream(audioButton);
          UpdateSelectedAudio(audioButton);
        }
      }
    }
    return true;
  }
  if (message.GetMessage() == GUI_MSG_FOCUSED)
  {
    if (message.GetControlId() == SUB_BUTTON || message.GetControlId() == AUDIO_BUTTON)
    {
      m_subsEnabled = g_application.m_pPlayer->GetSubtitleVisible();
      SetupButtons();
    }
  }
  
  return CGUIDialog::OnMessage(message);
}

void CGUIDialogOSDSettings::SetupButtons()
{
  // disable the template button control
  CGUIButtonControl *pSubButtonTemplate = dynamic_cast<CGUIButtonControl *>(GetFirstFocusableControl(SUB_BUTTON_TEMPLATE));
  if (!pSubButtonTemplate)
    pSubButtonTemplate = dynamic_cast<CGUIButtonControl *>(GetControl(SUB_BUTTON_TEMPLATE));
  if (!pSubButtonTemplate)
    return;
  pSubButtonTemplate->SetVisible(false);
  
  CGUIButtonControl *pAudioButtonTemplate = dynamic_cast<CGUIButtonControl *>(GetFirstFocusableControl(AUDIO_BUTTON_TEMPLATE));
  if (!pAudioButtonTemplate)
    pAudioButtonTemplate = dynamic_cast<CGUIButtonControl *>(GetControl(AUDIO_BUTTON_TEMPLATE));
  if (!pAudioButtonTemplate)
    return;
  pAudioButtonTemplate->SetVisible(false);
  
  m_subsButtonOffset = 1;
  
  ClearButtons();
  
  if (g_application.m_pPlayer->HasPlayer())
  {
    int subtitleCount = g_application.m_pPlayer->GetSubtitleCount();
    
    if (subtitleCount > 0)
    {
      m_subsButtonOffset++;
      if (m_subsEnabled)
        m_subButtons.Add(0, g_localizeStrings.Get(16039));
      else
        m_subButtons.Add(0, "✓ " + g_localizeStrings.Get(16039));
    }
    m_subButtons.Add(1, g_localizeStrings.Get(33003));
    
    for (int iStream=0; iStream < subtitleCount; iStream++)
    {
      SPlayerSubtitleStreamInfo info;
      g_application.m_pPlayer->GetSubtitleStreamInfo(iStream, info);
      
      std::string lang;
      if (info.language.length() > 0)
        g_LangCodeExpander.Lookup(info.language, lang);
      else
        lang = info.name;
      m_subButtons.Add(iStream + m_subsButtonOffset, lang);
    }
    
    int audioCount = g_application.m_pPlayer->GetAudioStreamCount();
    for (int iStream=0; iStream < audioCount; iStream++)
    {
      SPlayerAudioStreamInfo info;
      g_application.m_pPlayer->GetAudioStreamInfo(iStream, info);
      
      std::string lang;
      if (info.language.length() > 0)
      {
         g_LangCodeExpander.Lookup(info.language, lang);
        lang = lang + " - " + info.name;
      }
      else
        lang = info.name;
      m_audioButtons.Add(iStream, lang);
    }
  }
  
  if (m_subButtons.size())
  {
    CGUIControlGroupList* pGroupList = dynamic_cast<CGUIControlGroupList *>(GetControl(SUB_GROUP_LIST));
    // add our buttons
    for (unsigned int i = 0; i < m_subButtons.size(); i++)
    {
      unsigned int selSub = g_application.m_pPlayer->GetSubtitle() + m_subsButtonOffset; // offset for Disable and download
      CGUIButtonControl *pSubButton = new CGUIButtonControl(*pSubButtonTemplate);
      if (pSubButton)
      { // set the button's ID and position
        int id = SUB_BUTTON_START + i;
        pSubButton->SetID(id);
        pSubButton->SetVisible(true);
        if (selSub == i && m_subsEnabled && m_subsButtonOffset > 1)
          pSubButton->SetLabel("✓ " + m_subButtons[i].second);
        else
          pSubButton->SetLabel(m_subButtons[i].second);
        
        if (pGroupList)
        {
          if (!pGroupList->InsertControl(pSubButton, pSubButtonTemplate))
            pGroupList->AddControl(pSubButton);
        }
      }
    }
  }
  if (m_audioButtons.size())
  {
    CGUIControlGroupList* pGroupList = dynamic_cast<CGUIControlGroupList *>(GetControl(AUDIO_GROUP_LIST));
    // add our buttons
    for (unsigned int i = 0; i < m_audioButtons.size(); i++)
    {
      unsigned int selAudio = g_application.m_pPlayer->GetAudioStream();
      CGUIButtonControl *pAudioButton = new CGUIButtonControl(*pAudioButtonTemplate);
      if (pAudioButton)
      { // set the button's ID and position
        int id = AUDIO_BUTTON_START + i;
        pAudioButton->SetID(id);
        pAudioButton->SetVisible(true);
        if (selAudio == i)
          pAudioButton->SetLabel("✓ " + m_audioButtons[i].second);
        else
          pAudioButton->SetLabel(m_audioButtons[i].second);
        
        if (pGroupList)
        {
          if (!pGroupList->InsertControl(pAudioButton, pAudioButtonTemplate))
            pGroupList->AddControl(pAudioButton);
        }
      }
    }
    
  }
}
void CGUIDialogOSDSettings::UpdateSelectedSubs(int selected)
{
  SetupButtons();
  CGUIControlGroupList* pGroupList = dynamic_cast<CGUIControlGroupList *>(GetControl(SUB_GROUP_LIST));
  
  CGUIButtonControl *pButton = (CGUIButtonControl*)pGroupList->GetControl(SUB_BUTTON_START);
  // change first button on/off depending on subtitled being enabled/disabled
  if (m_subsEnabled)
    ((CGUIButtonControl *)pButton)->SetLabel(g_localizeStrings.Get(16039));
  else
    ((CGUIButtonControl *)pButton)->SetLabel("✓ " + g_localizeStrings.Get(16039));

  CGUIButtonControl *pSubSelectButton = (CGUIButtonControl*)pGroupList->GetControl(SUB_BUTTON_START + selected);
  if (pSubSelectButton)
    pSubSelectButton->SetFocus(true);
}

void CGUIDialogOSDSettings::UpdateSelectedAudio(int selected)
{
  SetupButtons();
  CGUIControlGroupList* pGroupList = dynamic_cast<CGUIControlGroupList *>(GetControl(AUDIO_GROUP_LIST));
  CGUIButtonControl *pAudioSelectButton = (CGUIButtonControl*)pGroupList->GetControl(AUDIO_BUTTON_START + selected);
  if (pAudioSelectButton)
    pAudioSelectButton->SetFocus(true);
}

void CGUIDialogOSDSettings::ClearButtons()
{
  for (unsigned int i = 0; i < m_subButtons.size(); i++)
  {
    const CGUIControl *control = GetControl(SUB_BUTTON_START + i);
    if (control)
      RemoveControl(control);
  }
  for (unsigned int i = 0; i < m_audioButtons.size(); i++)
  {
    const CGUIControl *control = GetControl(AUDIO_BUTTON_START + i);
    if (control)
      RemoveControl(control);
  }
  m_subButtons.clear();
  m_audioButtons.clear();
}

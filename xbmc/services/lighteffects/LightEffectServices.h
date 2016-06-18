#pragma once
/*
 *      Copyright (C) 2016 Team MrMC
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

#include <atomic>

#include "threads/Thread.h"
#include "threads/CriticalSection.h"
#include "settings/lib/ISettingCallback.h"
#include "interfaces/IAnnouncer.h"
#include "LightEffectClient.h"

class CLightEffectServices
: public CThread
, public ISettingCallback
, public ANNOUNCEMENT::IAnnouncer
{
public:
  static CLightEffectServices &GetInstance();

  void Start();
  void Stop();
  bool IsActive();

  // ISetting callbacks
  virtual void OnSettingChanged(const CSetting *setting) override;

  // IAnnouncer callbacks
  virtual void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)override;

private:
  // private construction, and no assignements; use the provided singleton methods
  CLightEffectServices();
  CLightEffectServices(const CLightEffectServices&);
  virtual ~CLightEffectServices();

  // IRunnable entry point for thread
  virtual void  Process() override;

  void SetOption(std::string setting);
  void SetAllLightsToStaticRGB();
  void SetBling();
  bool InitConnection();
  void ApplyUserSettings();

  std::atomic<bool> m_active;
  int               m_width;
  int               m_height;
  CCriticalSection  m_critical;
  CLightEffectClient *m_lighteffect;
  bool              m_staticON;
  bool              m_lightsON;
  CEvent            m_blingEvent;

};

#pragma once
/*
 *      Copyright (C) 2018 Team MrMC
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
#include <map>

#include "threads/Thread.h"
#include "threads/CriticalSection.h"
#include "settings/lib/ISettingCallback.h"
#include "interfaces/IAnnouncer.h"

#include "HueClient.h"

class CHueServices
: public CThread
, public ISettingCallback
, public ANNOUNCEMENT::IAnnouncer
{
public:
  static CHueServices &GetInstance();

  void Start();
  void Stop();
  bool IsActive();

  static void SettingOptionsHueStreamGroupsFiller(const CSetting *setting, std::vector< std::pair<std::string, int> > &list, int &current, void *data);
  static void SettingOptionsHueLightsFiller(const CSetting *setting, std::vector< std::pair<std::string, int> > &list, int &current, void *data);
  static void SettingOptionsHueScenesFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data);

  // ISetting callbacks
  virtual void OnSettingAction(const CSetting *setting) override;
  virtual void OnSettingChanged(const CSetting *setting) override;

  // IAnnouncer callbacks
  virtual void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)override;

private:
  // private construction, and no assignements; use the provided singleton methods
  CHueServices();
  CHueServices(const CHueServices&);
  virtual ~CHueServices();

  // IRunnable entry point for thread
  virtual void  Process() override;

  bool SignIn();
  bool SignOut();

  bool InitConnection();
  void ResetConnection(int status);

  void RevertLight(int lightid, bool force=false);
  void SetLight(int lightid, float fx, float fy, float fY);
  void SetLight(int lightid, float fR, float fG, float fB, float fL);
  void DimLight(int lightid, int status);
  void DimScene(int status);

  uint8_t m_oldstatus;
  std::atomic<uint8_t> m_status;
  uint8_t m_dim_mode;
  CVariant m_scene_lights;
  bool m_continuous;
  bool m_forceON;
  bool m_scene_anyon;
  bool m_useStreaming;
  int  m_width;
  int  m_height;
  CCriticalSection  m_critical;
  std::unique_ptr<CHueBridge> m_bridge;
};

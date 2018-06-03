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

#include "HueServices.h"
#include "HueUtils.h"

#include <chrono>
#include <queue>

#include "Application.h"
#include "network/Network.h"
#include "cores/VideoRenderers/RenderManager.h"
#include "cores/VideoRenderers/RenderCapture.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "interfaces/AnnouncementManager.h"
#include "settings/lib/Setting.h"
#include "settings/Settings.h"
#include "guilib/LocalizeStrings.h"
#include "utils/log.h"

#define MODE_IGNORE 0
#define MODE_COLOR 1
#define MODE_DIM 2
#define MODE_STREAM 3

#define STATUS_PLAY 0
#define STATUS_PAUSE 1
#define STATUS_STOP 2

using namespace ANNOUNCEMENT;

// replace these when we hit c++17
/*
static inline int ClampValue(int value, int min, int max)
{
  return std::min(std::max(value, min), max);
}
*/
static inline float ClampValue(float value, float min, float max)
{
  return std::min(std::max(value, min), max);
}

CHueServices::CHueServices()
: CThread("HueServices")
, m_oldstatus(STATUS_STOP)
, m_status(STATUS_STOP)
, m_forceON(false)
, m_useStreaming(false)
, m_width(32)
, m_height(32)
{
  CAnnouncementManager::GetInstance().AddAnnouncer(this);
}

CHueServices::~CHueServices()
{
  CAnnouncementManager::GetInstance().RemoveAnnouncer(this);
  if (IsRunning())
    Stop();
}

CHueServices& CHueServices::GetInstance()
{
  static CHueServices sHueServices;
  return sHueServices;
}

void CHueServices::RevertLight(int lightid, bool force)
{
  if (!force && !m_bridge->getLight(lightid)->isOn() && !m_forceON)
    return;

  CLog::Log(LOGINFO, "Hue - Restoring Light (%d)", lightid);

  uint32_t dur = uint32_t(CSettings::GetInstance().GetNumber(CSettings::SETTING_SERVICES_HUE_DIMDUR) * 1000);
  m_bridge->getLight(lightid)->restoreState(dur);
}

void CHueServices::SetLight(int lightid, float fx, float fy, float fY)
{
  if (!m_bridge->getLight(lightid)->isOn() && !m_forceON)
    return;
  m_bridge->getLight(lightid)->setColorXYB(fx, fy, uint8_t(fY * 255), 0);
}

void CHueServices::SetLight(int lightid, float fR, float fG, float fB, float fL)
{
  if (!m_bridge->getLight(lightid)->isOn() && !m_forceON)
    return;
  m_bridge->getLight(lightid)->setColorRGBL(fR, fG, fB, uint8_t(fL * 255), 0);
}

void CHueServices::DimLight(int lightid, int status)
{
  if (!m_bridge->getLight(lightid)->isOn() && !m_forceON)
    return;

  CLog::Log(LOGINFO, "Hue - Dimming light(%d) status(%d)", lightid, status);

  switch (status)
  {
    case STATUS_PLAY:
      m_bridge->getLight(lightid)->setBrightness(
            uint8_t(CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_DIMBRIGHT) * 255 / 100),
            uint32_t(CSettings::GetInstance().GetNumber(CSettings::SETTING_SERVICES_HUE_DIMDUR) * 1000.0)
            );
      break;
    case STATUS_PAUSE:
      if (CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_HUE_DIMOVERPAUSEDBRIGHT))
        m_bridge->getLight(lightid)->setBrightness(
              uint8_t(CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_DIMPAUSEDBRIGHT) * 255 / 100),
              uint32_t(CSettings::GetInstance().GetNumber(CSettings::SETTING_SERVICES_HUE_DIMDUR) * 1000.0)
              );
      else
        RevertLight(lightid);
      break;
    case STATUS_STOP:
      if (CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_HUE_DIMOVERUNBRIGHT))
        m_bridge->getLight(lightid)->setBrightness(
              uint8_t(CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_DIMUNBRIGHT) * 255 / 100),
              uint32_t(CSettings::GetInstance().GetNumber(CSettings::SETTING_SERVICES_HUE_DIMDUR) * 1000.0)
              );
      else
        RevertLight(lightid);
      break;
  }
}

void CHueServices::Announce(AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  if (flag == Player && !strcmp(sender, "xbmc"))
  {
    if (strcmp(message, "OnPlay") == 0)
      m_status = STATUS_PLAY;
    else if (strcmp(message, "OnPause") == 0)
      m_status = STATUS_PAUSE;
    else if (strcmp(message, "OnStop") == 0)
      m_status = STATUS_STOP;
  }

  if (flag == GUI && !strcmp(sender, "xbmc"))
  {
    if (!strcmp(message, "OnScreensaverActivated"))
      m_status = STATUS_STOP;
  }
}

void CHueServices::Start()
{
  CSingleLock lock(m_critical);
  if (CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_HUE_ENABLE))
  {
    if (IsRunning())
      StopThread();
    CThread::Create();
  }
}

void CHueServices::Stop()
{
  CSingleLock lock(m_critical);
  if (IsRunning())
  {
    StopThread();
  }
}

bool CHueServices::IsActive()
{
  return IsRunning();
}

static CCriticalSection  s_settings_critical;
static std::vector< std::pair<std::string, int> > s_settings_groups;
static std::vector< std::pair<std::string, int> > s_settings_lights;

void CHueServices::SettingOptionsHueStreamGroupsFiller(const CSetting *setting, std::vector< std::pair<std::string, int> > &list, int &current, void *data)
{
  if (s_settings_groups.empty())
  {
    CSingleLock lock(s_settings_critical);

    std::string ip = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_IP);
    std::string username = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_USERNAME);
    std::string clientkey = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_CLIENTKEY);

    s_settings_groups.push_back(std::make_pair(g_localizeStrings.Get(231), 0));
    if (!ip.empty() && !username.empty())
    {
      in_addr_t bridgeip = inet_addr(ip.c_str());
      if (g_application.getNetwork().PingHost(bridgeip, 0, 1000))
      {
        CHueBridge bridge(ip, username, clientkey);
        std::vector< std::pair<std::string, int> > groups = bridge.getStreamGroupsNames();
        s_settings_groups.insert(s_settings_groups.end(), groups.begin(), groups.end());
      }
    }
  }
  list.insert(list.end(), s_settings_groups.begin(), s_settings_groups.end());
}

void CHueServices::SettingOptionsHueLightsFiller(const CSetting *setting, std::vector< std::pair<std::string, int> > &list, int &current, void *data)
{
  if (s_settings_lights.empty())
  {
    CSingleLock lock(s_settings_critical);

    std::string ip = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_IP);
    std::string username = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_USERNAME);
    std::string clientkey = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_CLIENTKEY);

    s_settings_lights.push_back(std::make_pair(g_localizeStrings.Get(231), 0));
    if (!ip.empty() && !username.empty())
    {
      in_addr_t bridgeip = inet_addr(ip.c_str());
      if (g_application.getNetwork().PingHost(bridgeip, 0, 1000))
      {
        CHueBridge bridge(ip, username, clientkey);
        std::vector< std::pair<std::string, int> > lights = bridge.getLightsNames();
        s_settings_lights.insert(s_settings_lights.end(), lights.begin(), lights.end());
      }
    }
  }
  list.insert(list.end(), s_settings_lights.begin(), s_settings_lights.end());
}

void CHueServices::OnSettingAction(const CSetting* setting)
{
  if (setting == nullptr)
    return;

  std::string strSignIn = g_localizeStrings.Get(14208);
  std::string strSignOut = g_localizeStrings.Get(14209);

  const std::string& settingId = setting->GetId();
  if (settingId == CSettings::SETTING_SERVICES_HUE_DISCOVER)
  {
    if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_DISCOVER) == strSignIn)
      SignIn();
    else
      SignOut();
  }
}

void CHueServices::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  const std::string &settingId = setting->GetId();
  if (settingId == CSettings::SETTING_SERVICES_HUE_ENABLE)
  {
    {
      CSingleLock lock(s_settings_critical);
      s_settings_groups.clear();
      s_settings_lights.clear();
    }

    // start or stop the service
    if (static_cast<const CSettingBool*>(setting)->GetValue())
      Start();
    else
      Stop();
  }
  else
  {
    // Reset service if a setting changes
    if (settingId == CSettings::SETTING_SERVICES_HUE_DIMDUR
        || settingId == CSettings::SETTING_SERVICES_HUE_DIMBRIGHT
        || settingId == CSettings::SETTING_SERVICES_HUE_DIMOVERPAUSEDBRIGHT
        || settingId == CSettings::SETTING_SERVICES_HUE_DIMPAUSEDBRIGHT
        || settingId == CSettings::SETTING_SERVICES_HUE_DIMOVERUNBRIGHT
        || settingId == CSettings::SETTING_SERVICES_HUE_DIMUNBRIGHT
        || settingId == CSettings::SETTING_SERVICES_HUE_MINBRIGHT
        || settingId == CSettings::SETTING_SERVICES_HUE_MAXBRIGHT
        || settingId == CSettings::SETTING_SERVICES_HUE_STREAMGROUPID
        || settingId == CSettings::SETTING_SERVICES_HUE_LIGHT1ID
        || settingId == CSettings::SETTING_SERVICES_HUE_LIGHT1MODE
        || settingId == CSettings::SETTING_SERVICES_HUE_LIGHT2ID
        || settingId == CSettings::SETTING_SERVICES_HUE_LIGHT2MODE
        || settingId == CSettings::SETTING_SERVICES_HUE_LIGHT3ID
        || settingId == CSettings::SETTING_SERVICES_HUE_LIGHT3MODE
        || settingId == CSettings::SETTING_SERVICES_HUE_LIGHT4ID
        || settingId == CSettings::SETTING_SERVICES_HUE_LIGHT4MODE
        )
      // start or stop the service
      if (IsActive())
      {
        Stop();
        Start();
      }
  }

  CSettings::GetInstance().Save();
}

void CHueServices::Process()
{
  CRenderCapture *capture = nullptr;
  float fR = 0.0f, fG = 0.0f, fB = 0.0f;
  float fx = 0.0, fy = 0.0, fY = 0.0;
  float minL = 0.0f, maxL = 1.0f, biasC = 0.0f;
  float fu_old = 0.0, fv_old = 0.0;

  while (!m_bStop)
  {
    uint8_t curstatus = m_status;
    if (curstatus != STATUS_STOP)
    {
      // if starting, alloc a rendercapture and start capturing
      if (capture == nullptr)
      {
        capture = g_renderManager.AllocRenderCapture();
        g_renderManager.Capture(capture, m_width, m_height, CAPTUREFLAG_CONTINUOUS);

        if (!InitConnection())
        {
          m_bStop = true;
          continue;
        }

        bool forceOn = CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_HUE_FORCEON);
        bool forceOnAfterSunset = CSettings::GetInstance().GetBool(CSettings::SETTING_SERVICES_HUE_FORCEONAFTERSUNSET);
        m_forceON = (forceOn && (!forceOnAfterSunset || (forceOnAfterSunset && !m_bridge->isDaylight())));

        int streamgroup = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_STREAMGROUPID);
        if (streamgroup > 0
              && !m_bridge->getClientkey().empty()
              && (m_bridge->getGroup(streamgroup)->isAnyOn() || m_forceON)
              && m_bridge->startStreaming(streamgroup)
              )
        {
          for (CVariant::iterator_array it = m_bridge->getStreamingLights().begin_array(); it != m_bridge->getStreamingLights().end_array(); ++it)
          {
            std::shared_ptr<CHueLight> light = m_bridge->getLight(std::stoi(it->asString()));
            light->setMode(MODE_STREAM);
            light->saveState();
          }
        }
        minL = float(CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_MINBRIGHT)) / 100.0f;
        maxL = float(CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_MAXBRIGHT)) / 100.0f;
        biasC = float(((100 - CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_COLORBIAS)) / 5)+1) * 0.0011f;
      }

      capture->GetEvent().WaitMSec(1000);
      if (capture->GetUserState() == CAPTURESTATE_DONE)
      {
        fR = 0.0; fG = 0.0; fB = 0.0;
        int rows = 0;
        //read out the pixels
        unsigned char *pixels = capture->GetPixels();
        for (int y = 0; y < m_height; ++y)
        {
          double rR = 0.0, rG = 0.0, rB = 0.0;
          int row = m_width * y * 4;
          for (int x = 0; x < m_width; ++x)
          {
            int pixel = row + (x * 4);
            rR += pixels[pixel + 2];
            rG += pixels[pixel + 1];
            rB += pixels[pixel];
          }

          // ignore black rows
          rR = ClampValue((rR / (float)(m_width)) / 255.0f, 0.0f, 1.0f);
          rG = ClampValue((rG / (float)(m_width)) / 255.0f, 0.0f, 1.0f);
          rB = ClampValue((rB / (float)(m_width)) / 255.0f, 0.0f, 1.0f);
          double rY = (0.2126 *rR + 0.7152 *rG + 0.0722 *rB);
          if (rY > 0.01)
          {
            fR += rR;
            fG += rG;
            fB += rB;
            ++rows;
          }
        }

        fR /= rows;
        fG /= rows;
        fB /= rows;

        float x, y;
        CHueUtils::rgb2xy(fR, fG, fB, x, y);

        // Skip imperceptible color updates (+ bias)
        float u, v;
        CHueUtils::xy2uv(x, y, u, v);
        double color_dist = sqrt(pow(u - fu_old, 2) + pow(v - fv_old, 2));

        if (color_dist > biasC)
        {
          //CLog::Log(LOGDEBUG, "Hue - Color bias = %f, dist = %f", biasC, color_dist);
          fx = x; fy = y;
          fu_old = u; fv_old = v;
        }

        fY = (0.2126f *fR + 0.7152f *fG + 0.0722f *fB);
        // map luma
        fY = fY * (maxL - minL) + minL;

        for (auto& light : m_bridge->getLights())
        {
          if (light.second->getMode() == MODE_COLOR)
            SetLight(light.first, fx, fy, fY);
        }
        if (m_bridge->isStreaming())
          m_bridge->streamXYB(fx, fy, fY);
      }
      else  // capture->GetUserState() == CAPTURESTATE_DONE
      {
        // Streaming dies after 10 sec; refresh it if we missed a capture
        if (m_bridge->isStreaming())
          m_bridge->streamXYB(fx, fy, fY);
      }
      if (curstatus != m_oldstatus)
      {
        for (auto& light : m_bridge->getLights())
        {
          if (light.second->getMode() == MODE_DIM)
            DimLight(light.first, curstatus);
        }
        m_oldstatus = curstatus;
      }
    }
    else   // STATUS_STOP
    {
      if (curstatus != m_oldstatus)
      {
        if (capture != nullptr)
        {
          g_renderManager.ReleaseRenderCapture(capture);
          capture = nullptr;
        }
        ResetConnection(curstatus);

        m_oldstatus = curstatus;
      }
      usleep(50 * 1000);
    }
  }

  // have to check this in case we go
  // right from playing to death.
  if (capture != nullptr)
  {
    g_renderManager.ReleaseRenderCapture(capture);
    capture = nullptr;
    ResetConnection(STATUS_STOP);
  }
}

bool CHueServices::SignIn()
{
  std::string strSignIn = g_localizeStrings.Get(14208);
  std::string strSignOut = g_localizeStrings.Get(14209);

  std::vector<CHueBridge> bridges = CHueBridge::discover();
  CHueBridge curBridge;

  if (bridges.size() == 0)
  {
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, "Hue Service", g_localizeStrings.Get(14205), 5000, true);
    return false;
  }

  for(auto &bridge : bridges)
  {
    CLog::Log(LOGDEBUG, "Hue: Found bridge at %s", bridge.getIp().c_str());
    if (bridge.getIp() == CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_IP))
    {
      curBridge = bridge;
      break;
    }
  }
  if (curBridge.getIp().empty() && bridges.size() == 1)
    curBridge = bridges[0];

  if (!curBridge.pair())
  {
    CLog::Log(LOGERROR, "Hue: Unable to pair with bridge at %s", curBridge.getIp().c_str());
    return false;
  }

  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_HUE_IP, curBridge.getIp());
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_HUE_USERNAME, curBridge.getUsername());
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_HUE_CLIENTKEY, curBridge.getClientkey());

  // change prompt to 'sign-out'
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_HUE_DISCOVER, strSignOut);
  CSettings::GetInstance().Save();

  CLog::Log(LOGINFO, "Hue: connected to bridge at %s", curBridge.getIp().c_str());

  Start();

  return true;
}

bool CHueServices::SignOut()
{
  std::string strSignIn = g_localizeStrings.Get(14208);
  std::string strSignOut = g_localizeStrings.Get(14209);

  // prompt is 'sign-out'
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_HUE_IP, "");
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_HUE_USERNAME, "");

  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_HUE_DISCOVER, strSignIn);
  CLog::Log(LOGDEBUG, "CHueServices:OnSettingAction sign-out ok");

  Stop();

  return true;
}

bool CHueServices::InitConnection()
{
  std::string ip = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_IP);
  std::string username = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_USERNAME);
  std::string clientkey = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_HUE_CLIENTKEY);

  if (ip.empty() || username.empty())
  {
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, "Hue Service", g_localizeStrings.Get(14205), 5000, true);
    return false;
  }

  in_addr_t bridgeip = inet_addr(ip.c_str());
  if (!g_application.getNetwork().PingHost(bridgeip, 0, 1000))
    return false;

  m_bridge.reset(new CHueBridge(ip, username, clientkey));

  std::map<int, std::shared_ptr<CHueLight>>& lights = m_bridge->getLights();
  if (CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT1ID) > 0)
  {
    int id = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT1ID);
    lights[id]->setMode(CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT1MODE));
    lights[id]->saveState();
    CLog::Log(LOGINFO, "Hue - Light (%d) configured as %d", id, lights[id]->getMode());
  }
  if (CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT2ID) > 0)
  {
    int id = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT2ID);
    lights[id]->setMode(CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT2MODE));
    lights[id]->saveState();
    CLog::Log(LOGINFO, "Hue - Light (%d) configured as %d", id, lights[id]->getMode());
  }
  if (CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT3ID) > 0)
  {
    int id = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT3ID);
    lights[id]->setMode(CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT3MODE));
    lights[id]->saveState();
    CLog::Log(LOGINFO, "Hue - Light (%d) configured as %d", id, lights[id]->getMode());
  }
  if (CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT4ID) > 0)
  {
    int id = CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT4ID);
    lights[id]->setMode(CSettings::GetInstance().GetInt(CSettings::SETTING_SERVICES_HUE_LIGHT4MODE));
    lights[id]->saveState();
    CLog::Log(LOGINFO, "Hue - Light (%d) configured as %d", id, lights[id]->getMode());
  }

  return true;
}

void CHueServices::ResetConnection(int status)
{
  if (m_bridge)
  {
    m_bridge->stopStreaming();
    for (auto& light : m_bridge->getLights())
    {
      if (light.second->getMode() == MODE_DIM)
        DimLight(light.first, status);
      else if (light.second->getMode() == MODE_STREAM)
        RevertLight(light.first, true);
      else if (light.second->getMode() != MODE_IGNORE)
        RevertLight(light.first);
    }
    m_bridge.reset();
  }
}


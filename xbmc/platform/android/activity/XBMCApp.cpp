/*
 *      Copyright (C) 2012-2013 Team XBMC
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

#include "XBMCApp.h"

#include <sstream>

#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/configuration.h>
#include <jni.h>

#include "input/MouseStat.h"
#include "input/XBMC_keysym.h"
#include "input/Key.h"
#include "windowing/XBMC_events.h"
#include <android/log.h>

#include "Application.h"
#include "network/android/NetworkAndroid.h"
#include "settings/AdvancedSettings.h"
#include "platform/MCRuntimeLib.h"
#include "platform/MCRuntimeLibContext.h"
#include "windowing/WinEvents.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/GraphicContext.h"
#include "settings/DisplaySettings.h"
#include "utils/log.h"
#include "messaging/ApplicationMessenger.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/URIUtils.h"
#include "utils/SysfsUtils.h"
#include "AppParamParser.h"
#include <android/bitmap.h>
#include "cores/AudioEngine/AEFactory.h"
#include "cores/VideoRenderers/RenderManager.h"
#include <androidjni/JNIThreading.h>
#include <androidjni/BroadcastReceiver.h>
#include <androidjni/Intent.h>
#include <androidjni/PackageManager.h>
#include <androidjni/Context.h>
#include <androidjni/PowerManager.h>
#include <androidjni/WakeLock.h>
#include <androidjni/Environment.h>
#include <androidjni/File.h>
#include <androidjni/IntentFilter.h>
#include <androidjni/NetworkInfo.h>
#include <androidjni/ConnectivityManager.h>
#include <androidjni/System.h>
#include <androidjni/ApplicationInfo.h>
#include <androidjni/StatFs.h>
#include <androidjni/CharSequence.h>
#include <androidjni/URI.h>
#include <androidjni/Cursor.h>
#include <androidjni/ContentResolver.h>
#include <androidjni/MediaStore.h>
#include <androidjni/Build.h>
#include <androidjni/Window.h>
#include <androidjni/WindowManager.h>
#include <androidjni/KeyEvent.h>
#include <androidjni/SystemProperties.h>
#include <androidjni/Display.h>
#include <androidjni/BitmapFactory.h>
#include <androidjni/SystemClock.h>
#include <androidjni/ComponentName.h>
#include <androidjni/ResolveInfo.h>

#include "platform/android/activity/AndroidKey.h"
#include "AndroidFeatures.h"
#include "GUIInfoManager.h"
#include "guiinfo/GUIInfoLabels.h"
#include "TextureCache.h"
#include "filesystem/SpecialProtocol.h"

#include "CompileInfo.h"
#include "filesystem/VideoDatabaseFile.h"
#include "video/videosync/VideoSyncAndroid.h"
#include "interfaces/AnnouncementManager.h"
#include "windowing/WindowingFactory.h"

#define GIGABYTES       1073741824
#define CAPTURE_QUEUE_MAXDEPTH 1

#define ACTION_XBMC_RESUME "android.intent.XBMC_RESUME"

#define PLAYBACK_STATE_STOPPED  0x0000
#define PLAYBACK_STATE_PLAYING  0x0001
#define PLAYBACK_STATE_VIDEO    0x0100
#define PLAYBACK_STATE_AUDIO    0x0200
#define PLAYBACK_STATE_CANNOT_PAUSE 0x0400

using namespace KODI::MESSAGING;
using namespace ANNOUNCEMENT;
using namespace std;
using namespace jni;

template<class T, void(T::*fn)()>
void* thread_run(void* obj)
{
  (static_cast<T*>(obj)->*fn)();
  return NULL;
}

CXBMCApp* CXBMCApp::m_xbmcappinstance = NULL;
CCriticalSection CXBMCApp::m_AppMutex;

std::unique_ptr<CJNIXBMCMainView> CXBMCApp::m_mainView;
ANativeActivity *CXBMCApp::m_activity = NULL;
CJNIWakeLock *CXBMCApp::m_wakeLock = NULL;
ANativeWindow* CXBMCApp::m_window = NULL;
int CXBMCApp::m_batteryLevel = 0;
bool CXBMCApp::m_hasFocus = false;
bool CXBMCApp::m_hasResumed = false;
bool CXBMCApp::m_audioFocusGranted = false;
int  CXBMCApp::m_lastAudioFocusChange = -1;
bool CXBMCApp::m_wasPlayingVideoWhenPaused = false;
double CXBMCApp::m_wasPlayingVideoWhenPausedTime = 0.0;
bool CXBMCApp::m_wasPlayingWhenTransientLoss = false;
bool CXBMCApp::m_headsetPlugged = false;
bool CXBMCApp::m_hdmiPlugged = true;
bool CXBMCApp::m_hasPIP = false;
CCriticalSection CXBMCApp::m_applicationsMutex;
std::vector<androidPackage> CXBMCApp::m_applications;
std::vector<CActivityResultEvent*> CXBMCApp::m_activityResultEvents;

CCriticalSection CXBMCApp::m_captureMutex;
CCaptureEvent CXBMCApp::m_screenshotEvent;
CCaptureEvent CXBMCApp::m_captureEvent;
std::queue<CJNIImage> CXBMCApp::m_captureQueue;

std::atomic<uint64_t> CXBMCApp::m_vsynctime;
CEvent CXBMCApp::m_vsyncEvent;
CJNIAudioDeviceInfos CXBMCApp::m_audiodevices;

static std::map<int, std::string> type2string = {
  {19, "TYPE_AUX_LINE"},
  { 8, "TYPE_BLUETOOTH_A2DP"},
  { 7, "TYPE_BLUETOOTH_SCO"},
  { 1, "TYPE_BUILTIN_EARPIECE"},
  {15, "TYPE_BUILTIN_MIC"},
  { 2, "TYPE_BUILTIN_SPEAKER"},
  {21, "TYPE_BUS"},
  {13, "TYPE_DOCK"},
  {14, "TYPE_FM"},
  {16, "TYPE_FM_TUNER"},
  { 9, "TYPE_HDMI"},
  {10, "TYPE_HDMI_ARC"},
  {23, "TYPE_HEARING_AID"},
  {20, "TYPE_IP"},
  { 5, "TYPE_LINE_ANALOG"},
  { 6, "TYPE_LINE_DIGITAL"},
  {18, "TYPE_TELEPHONY"},
  {17, "TYPE_TV_TUNER"},
  { 0, "TYPE_UNKNOWN"},
  {12, "TYPE_USB_ACCESSORY"},
  {11, "TYPE_USB_DEVICE"},
  {22, "TYPE_USB_HEADSET"},
  { 4, "TYPE_WIRED_HEADPHONES"},
  { 3, "TYPE_WIRED_HEADSET"},
};

static std::map<int, std::string> encode2string = {
  {15, "ENCODING_AAC_ELD"},
  {11, "ENCODING_AAC_HE_V1"},
  {12, "ENCODING_AAC_HE_V2"},
  {10, "ENCODING_AAC_LC"},
  {16, "ENCODING_AAC_XHE"},
  { 5, "ENCODING_AC3"},
  {17, "ENCODING_AC4"},
  { 1, "ENCODING_DEFAULT"},
  {14, "ENCODING_DOLBY_TRUEHD"},
  { 7, "ENCODING_DTS"},
  { 8, "ENCODING_DTS_HD"},
  { 6, "ENCODING_E_AC3"},
  {18, "ENCODING_E_AC3_JOC"},
  {13, "ENCODING_IEC61937"},
  { 0, "ENCODING_INVALID"},
  { 9, "ENCODING_MP3"},
  { 2, "ENCODING_PCM_16BIT"},
  { 3, "ENCODING_PCM_8BIT"},
  { 4, "ENCODING_PCM_FLOAT"},
};

static std::map<int, std::string> chanmask2string = {
  {0x0000, "CHANNEL_INVALID"},
  {0x00fc, "CHANNEL_OUT_5POINT1"},
  {0x03fc, "CHANNEL_OUT_7POINT1"},
  {0x18fc, "CHANNEL_OUT_7POINT1_SURROUND"},
  {0x0400, "CHANNEL_OUT_BACK_CENTER"},
  {0x0040, "CHANNEL_OUT_BACK_LEFT"},
  {0x0080, "CHANNEL_OUT_BACK_RIGHT"},
  {0x0001, "CHANNEL_OUT_DEFAULT"},
  {0x0010, "CHANNEL_OUT_FRONT_CENTER"},
  {0x0004, "CHANNEL_OUT_FRONT_LEFT"},
  {0x0100, "CHANNEL_OUT_FRONT_LEFT_OF_CENTER"},
  {0x0008, "CHANNEL_OUT_FRONT_RIGHT"},
  {0x0200, "CHANNEL_OUT_FRONT_RIGHT_OF_CENTER"},
  {0x0020, "CHANNEL_OUT_LOW_FREQUENCY"},
  {0x0004, "CHANNEL_OUT_MONO"},
  {0x00cc, "CHANNEL_OUT_QUAD"},
  {0x0800, "CHANNEL_OUT_SIDE_LEFT"},
  {0x1000, "CHANNEL_OUT_SIDE_RIGHT"},
  {0x000c, "CHANNEL_OUT_STEREO"},
  {0x041c, "CHANNEL_OUT_SURROUND"}
};

const char* CXBMCApp::audioencode2string(int i)
{
  return encode2string[i].c_str();
}

void CXBMCApp::LogAudoDevices(const char* stage, const CJNIAudioDeviceInfos& devices)
{
  CLog::Log(LOGDEBUG, ">>> Audio device list: %s", stage);
  for (auto dev : devices)
  {
    CLog::Log(LOGDEBUG, "--- Found device: %s", dev.getProductName().toString().c_str());
    CLog::Log(LOGDEBUG, "    id: %d, type: %s, isSink: %s, isSource: %s", dev.getId(), type2string[dev.getType()].c_str(), dev.isSink() ? "true" : "false", dev.isSource() ? "true" : "false");

    std::ostringstream oss;
    for (auto i : dev.getChannelCounts())
      oss << i << " / ";
    CLog::Log(LOGDEBUG, "    channel counts: %s", oss.str().c_str());

    oss.clear(); oss.str("");
    for (auto i : dev.getChannelIndexMasks())
      oss << i << " / ";
    CLog::Log(LOGDEBUG, "    channel index masks: %s", oss.str().c_str());

    oss.clear(); oss.str("");
    for (auto i : dev.getChannelMasks())
      oss << i << " / ";
    CLog::Log(LOGDEBUG, "    channel masks: %s", oss.str().c_str());

    oss.clear(); oss.str("");
    for (auto i : dev.getEncodings())
      oss << audioencode2string(i) << " / ";
    CLog::Log(LOGDEBUG, "    encodings: %s", oss.str().c_str());

    oss.clear(); oss.str("");
    for (auto i : dev.getSampleRates())
      oss << i << " / ";
    CLog::Log(LOGDEBUG, "    sample rates: %s", oss.str().c_str());
  }
}
CRect CXBMCApp::m_droid2guiRatio(0.0, 0.0, 1.0, 1.0);
CRect CXBMCApp::m_surface_rect;
uint32_t CXBMCApp::m_playback_state = PLAYBACK_STATE_STOPPED;

CXBMCApp::CXBMCApp(ANativeActivity* nativeActivity)
  : CJNIBase()
  , CJNIMainActivity(nativeActivity->clazz)
  , m_videosurfaceInUse(false)

{
  m_xbmcappinstance = this;
  m_activity = nativeActivity;
  if (m_activity == NULL)
  {
    CLog::Log(LOGDEBUG, "CXBMCApp: invalid ANativeActivity instance");
    exit(1);
    return;
  }
  CAnnouncementManager::GetInstance().AddAnnouncer(this);
  m_audioFocusListener.reset(new CJNIXBMCAudioManagerOnAudioFocusChangeListener(this));
  m_broadcastReceiver.reset(new CJNIXBMCBroadcastReceiver(this));
  m_mainView.reset(new CJNIXBMCMainView(this));
  m_firstrun = true;
  m_exiting = false;
  CLog::Log(LOGDEBUG, "CXBMCApp: Created");
}

CXBMCApp::~CXBMCApp()
{
  CAnnouncementManager::GetInstance().RemoveAnnouncer(this);
  m_xbmcappinstance = NULL;
  delete m_wakeLock;
}

void CXBMCApp::Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  if (strcmp(sender, "xbmc") != 0)
    return;

  if (flag & Input)
  {
    if (strcmp(message, "OnInputRequested") == 0)
      CAndroidKey::SetHandleSearchKeys(true);
    else if (strcmp(message, "OnInputFinished") == 0)
      CAndroidKey::SetHandleSearchKeys(false);
  }
  else if (flag & Player)
  {
     if (strcmp(message, "OnPlay") == 0)
      OnPlayBackStarted();
    else if (strcmp(message, "OnPause") == 0)
      OnPlayBackPaused();
    else if (strcmp(message, "OnStop") == 0)
      OnPlayBackStopped();
    else if (strcmp(message, "OnSeek") == 0)
       UpdateSessionState();
    else if (strcmp(message, "OnSpeedChanged") == 0)
       UpdateSessionState();
  }
  else if (flag & GUI)
  {
     if (strcmp(message, "OnVideoResolutionChanged") == 0)
      CalculateGUIRatios();

     //HACK: AFTV send a dummy "pause" mediasession command when switching reso. Mask...
     if (CAndroidFeatures::IsAmazonDevice())
       m_mediaSession->activate(false);
  }
}

void CXBMCApp::onStart()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);

  if (m_firstrun)
  {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&m_thread, &attr, thread_run<CXBMCApp, &CXBMCApp::run>, this);
    pthread_attr_destroy(&attr);

    // Some intent filters MUST be registered in code rather than through the manifest
    CJNIIntentFilter intentFilter;
    intentFilter.addAction("android.intent.action.BATTERY_CHANGED");
    intentFilter.addAction("android.intent.action.SCREEN_ON");
    intentFilter.addAction("android.intent.action.SCREEN_OFF");
    intentFilter.addAction("android.intent.action.HEADSET_PLUG");
    intentFilter.addAction("android.media.action.HDMI_AUDIO_PLUG");
    intentFilter.addAction("android.net.conn.CONNECTIVITY_CHANGE");
    registerReceiver(*m_broadcastReceiver, intentFilter);

    m_mediaSession.reset(new CJNIXBMCMediaSession());
  }
}

void CXBMCApp::onResume()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);

  if (!g_application.IsInScreenSaver())
    EnableWakeLock(true);
  else
    g_application.WakeUpScreenSaverAndDPMS();

  m_audiodevices.clear();
  if (CJNIAudioManager::GetSDKVersion() >= 23)
  {
    CJNIAudioManager audioManager(getSystemService("audio"));
    m_audiodevices = audioManager.getDevices(CJNIAudioManager::GET_DEVICES_OUTPUTS);
    LogAudoDevices("OnResume", m_audiodevices);
  }
  CheckHeadsetPlugged();

  // Clear the applications cache. We could have installed/deinstalled apps
  {
    CSingleLock lock(m_applicationsMutex);
    m_applications.clear();
  }

/*
  if (m_wasPlayingVideoWhenPaused)
  {
    m_wasPlayingVideoWhenPaused = false;
    if (!g_application.LastProgressTrackingItem().GetPath().empty())
    {
      CFileItem *fileitem = new CFileItem(g_application.LastProgressTrackingItem());
      if (!fileitem->IsLiveTV())
      {
        // m_lStartOffset always gets multiplied by 75, magic numbers :)
        fileitem->m_lStartOffset = m_wasPlayingVideoWhenPausedTime * 75;
      }
      CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_PLAY, 0, 0, static_cast<void*>(fileitem));
      //CLog::Log(LOGDEBUG, "CXBMCApp::onResume - m_wasPlayingVideoWhenPausedTime [%f], fileitem [%s]", m_wasPlayingVideoWhenPausedTime, fileitem->GetPath().c_str());
    }
  }
*/
  m_hasResumed = true;
}

void CXBMCApp::onPause()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);

    /*
  if (g_application.m_pPlayer->IsPlaying())
  {
    if (g_application.m_pPlayer->IsPlayingVideo())
    {
      m_wasPlayingVideoWhenPaused = true;
      // get the current playing time but backup a little, it looks better
      m_wasPlayingVideoWhenPausedTime = g_application.GetTime() - 1.50;
      CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_STOP)));
    }
    else
      registerMediaButtonEventReceiver();
  }
    */

  EnableWakeLock(false);
  m_hasResumed = false;
}

void CXBMCApp::onStop()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);

  if ((m_playback_state & PLAYBACK_STATE_PLAYING) && (m_playback_state & PLAYBACK_STATE_VIDEO))
  {
    if (m_playback_state & PLAYBACK_STATE_CANNOT_PAUSE)
      CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_STOP)));
    else
      CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PAUSE)));
  }
}

void CXBMCApp::onDestroy()
{
  CLog::Log(LOGDEBUG, "%s", __PRETTY_FUNCTION__);

  unregisterReceiver(*m_broadcastReceiver);

  if (m_mediaSession)
    m_mediaSession.release();

  // If android is forcing us to stop, ask XBMC to exit then wait until it's
  // been destroyed.
  if (!m_exiting)
  {
    CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
    pthread_join(m_thread, NULL);
    android_printf(" => XBMC finished");
  }
}

void CXBMCApp::onSaveState(void **data, size_t *size)
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);
  // no need to save anything as XBMC is running in its own thread
}

void CXBMCApp::onConfigurationChanged()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);
}

void CXBMCApp::onLowMemory()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);
  // can't do much as we don't want to close completely
}

void CXBMCApp::onCreateWindow(ANativeWindow* window)
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);
}

void CXBMCApp::onResizeWindow()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);
  m_window = NULL;
  // no need to do anything because we are fixed in fullscreen landscape mode
}

void CXBMCApp::onDestroyWindow()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);
}

void CXBMCApp::onGainFocus()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);
  m_hasFocus = true;
  g_application.WakeUpScreenSaverAndDPMS();
}

void CXBMCApp::onLostFocus()
{
  CLog::Log(LOGDEBUG, "%s: ", __PRETTY_FUNCTION__);
  m_hasFocus = false;
}

bool CXBMCApp::EnableWakeLock(bool on)
{
  CLog::Log(LOGDEBUG, "%s: %s", __PRETTY_FUNCTION__, on ? "true" : "false");
  if (!m_wakeLock)
  {
    std::string appName = CCompileInfo::GetAppName();
    StringUtils::ToLower(appName);
    std::string className = CCompileInfo::GetPackage();
    StringUtils::ToLower(className);
    // SCREEN_BRIGHT_WAKE_LOCK is marked as deprecated but there is no real alternatives for now
    m_wakeLock = new CJNIWakeLock(CJNIPowerManager(getSystemService("power")).newWakeLock(CJNIPowerManager::SCREEN_BRIGHT_WAKE_LOCK, className.c_str()));
    if (m_wakeLock)
      m_wakeLock->setReferenceCounted(false);
    else
      return false;
  }

  if (on)
  {
    if (!m_wakeLock->isHeld())
      m_wakeLock->acquire();
  }
  else
  {
    if (m_wakeLock->isHeld())
      m_wakeLock->release();
  }

  return true;
}

bool CXBMCApp::ResetSystemIdleTimer()
{
  if (!m_wakeLock->isHeld())
  {
    m_wakeLock->acquire();
    return true;
  }

  return false;
}

bool CXBMCApp::AcquireAudioFocus()
{
  if (!m_xbmcappinstance)
    return false;

  if (m_audioFocusGranted)
    return true;

  CJNIAudioManager audioManager(getSystemService("audio"));
  if (!audioManager)
  {
    CLog::Log(LOGDEBUG, "Cannot get audiomanger");
    return false;
  }

  // Request audio focus for playback
  int result = audioManager.requestAudioFocus(*m_audioFocusListener,
    // Use the music stream.
    CJNIAudioManager::STREAM_MUSIC,
    // Request permanent focus.
    CJNIAudioManager::AUDIOFOCUS_GAIN);
  // A successful focus change request returns AUDIOFOCUS_REQUEST_GRANTED
  if (result != CJNIAudioManager::AUDIOFOCUS_REQUEST_GRANTED)
  {
    CLog::Log(LOGDEBUG, "Audio Focus request failed");
    return false;
  }
  //m_audioFocusGranted = true;
  return true;
}

bool CXBMCApp::ReleaseAudioFocus()
{
  if (!m_xbmcappinstance)
    return false;

  if (!m_audioFocusGranted)
    return true;

  CJNIAudioManager audioManager(getSystemService("audio"));
  if (!audioManager)
  {
    CLog::Log(LOGDEBUG, "Cannot get audiomanger");
    return false;
  }

  // Release audio focus after playback
  int result = audioManager.abandonAudioFocus(*m_audioFocusListener);
  // A successful focus change request returns AUDIOFOCUS_REQUEST_GRANTED
  if (result != CJNIAudioManager::AUDIOFOCUS_REQUEST_GRANTED)
  {
    CLog::Log(LOGDEBUG, "Audio Focus abandon failed");
    return false;
  }
  //m_audioFocusGranted = false;
  return true;
}

void CXBMCApp::CheckHeadsetPlugged()
{
  bool oldstate = m_headsetPlugged;

  CLog::Log(LOGDEBUG, "CXBMCApp::CheckHeadsetPlugged");
  CJNIAudioManager audioManager(getSystemService("audio"));
  m_headsetPlugged = audioManager.isWiredHeadsetOn() || audioManager.isBluetoothA2dpOn();

  if (!m_audiodevices.empty())
  {
    for (auto dev : m_audiodevices)
    {
      if (dev.getType() == CJNIAudioDeviceInfo::TYPE_DOCK && dev.isSink() && StringUtils::CompareNoCase(dev.getProductName().toString(), "SHIELD Android TV") == 0)
      {
        // SHIELD specifics: Gamepad headphone is inserted
        m_headsetPlugged = true;
        CLog::Log(LOGINFO, "SHIELD: Wifi direct headset inserted");
      }
    }
  }

  if (m_headsetPlugged != oldstate)
    CAEFactory::DeviceChange();
}

void CXBMCApp::RequestPictureInPictureMode()
{
//  enterPictureInPictureMode();
//  CLog::Log(LOGDEBUG, "Entering PIP mode");
}

bool CXBMCApp::IsHeadsetPlugged()
{
  return m_headsetPlugged;
}

bool CXBMCApp::IsHDMIPlugged()
{
  return m_hdmiPlugged;
}

void CXBMCApp::run()
{
  std::string threadname = "MCRuntimeLib";
  pthread_setname_np(pthread_self(), threadname.c_str());

  // Wait for Main surface
  if (!*CXBMCApp::GetNativeWindow(30000))
    return;

  int status = 0;

  SetupEnv();
  MCRuntimeLib::Context context;

  CJNIIntent startIntent = getIntent();

  android_printf("%s Started with action: %s\n", CCompileInfo::GetAppName(), startIntent.getAction().c_str());

  std::string filenameToPlay = GetFilenameFromIntent(startIntent);
  if (!filenameToPlay.empty())
  {
    int argc = 2;
    const char** argv = (const char**) malloc(argc*sizeof(char*));

    std::string exe_name(CCompileInfo::GetAppName());
    argv[0] = exe_name.c_str();
    argv[1] = filenameToPlay.c_str();

    CAppParamParser appParamParser;
    appParamParser.Parse((const char **)argv, argc);

    free(argv);
  }

  m_firstrun=false;
  android_printf(" => running MCRuntimeLib...");
  try
  {
    //nice(10);
    status = MCRuntimeLib_Run(true);
    android_printf(" => App_Run finished with %d", status);
  }
  catch(...)
  {
    android_printf("ERROR: Exception caught on main loop. Exiting");
  }

  // Pass the return code to Java
  set_field(m_object, "mExitCode", status);

  // If we are have not been force by Android to exit, notify its finish routine.
  // This will cause android to run through its teardown events, it calls:
  // onPause(), onLostFocus(), onDestroyWindow(), onStop(), onDestroy().
  ANativeActivity_finish(m_activity);
  m_exiting=true;
}

void CXBMCApp::XBMC_SetupDisplay()
{
  android_printf("XBMC_SetupDisplay()");
  CApplicationMessenger::GetInstance().PostMsg(TMSG_DISPLAY_SETUP);
}

void CXBMCApp::XBMC_DestroyDisplay()
{
  android_printf("XBMC_DestroyDisplay()");
  CApplicationMessenger::GetInstance().PostMsg(TMSG_DISPLAY_DESTROY);
}

int CXBMCApp::SetBuffersGeometry(int width, int height, int format)
{
  if (m_window)
    return ANativeWindow_setBuffersGeometry(m_window, width, height, format);
  return 0;
}

#include "threads/Event.h"
#include <time.h>

void CXBMCApp::SetRefreshRateCallback(CVariant* rateVariant)
{
  float rate = rateVariant->asFloat();
  delete rateVariant;

  CJNIWindow window = CXBMCApp::get()->getWindow();
  if (window)
  {
    CJNIWindowManagerLayoutParams params = window.getAttributes();
    if (params.getpreferredRefreshRate() != rate)
    {
      params.setpreferredRefreshRate(rate);
      if (params.getpreferredRefreshRate() > 0.0)
        window.setAttributes(params);
    }
  }
}

void CXBMCApp::SetDisplayModeIdCallback(CVariant* rateVariant)
{
  int modeId = rateVariant->asInteger();
  delete rateVariant;

  CJNIWindow window = CXBMCApp::get()->getWindow();
  if (window)
  {
    CJNIWindowManagerLayoutParams params = window.getAttributes();
    if (params.getpreferredDisplayModeId() != modeId)
    {
      params.setpreferredDisplayModeId(modeId);
      window.setAttributes(params);
    }
  }
}

void CXBMCApp::SetRefreshRate(float rate)
{
  if (rate < 1.0)
    return;

  CVariant *variant = new CVariant(rate);
  runNativeOnUiThread(SetRefreshRateCallback, variant);
}

void CXBMCApp::SetDisplayModeId(int modeId)
{
  if (modeId < 0)
    return;

  CVariant *variant = new CVariant(modeId);
  runNativeOnUiThread(SetDisplayModeIdCallback, variant);
}

int CXBMCApp::android_printf(const char *format, ...)
{
  // For use before CLog is setup by XBMC_Run()
  va_list args;
  va_start(args, format);
  int result = __android_log_vprint(ANDROID_LOG_DEBUG, CCompileInfo::GetAppName(), format, args);
  va_end(args);
  return result;
}

void CXBMCApp::BringToFront()
{
  if (!m_hasResumed)
  {
    CLog::Log(LOGERROR, "CXBMCApp::BringToFront");
    StartActivity(getPackageName());
  }
}

int CXBMCApp::GetDPI()
{
  if (m_activity == nullptr || m_activity->assetManager == nullptr)
    return 0;

  // grab DPI from the current configuration - this is approximate
  // but should be close enough for what we need
  AConfiguration *config = AConfiguration_new();
  AConfiguration_fromAssetManager(config, m_activity->assetManager);
  int dpi = AConfiguration_getDensity(config);
  AConfiguration_delete(config);

  return dpi;
}

bool CXBMCApp::IsNightMode()
{
  if (m_activity == nullptr || m_activity->assetManager == nullptr)
    return false;

  AConfiguration *config = AConfiguration_new();
  AConfiguration_fromAssetManager(config, m_activity->assetManager);
  int nm = AConfiguration_getUiModeNight(config);
  AConfiguration_delete(config);

  return (nm == ACONFIGURATION_UI_MODE_NIGHT_YES);
}

CPointInt CXBMCApp::GetMaxDisplayResolution()
{
  // Find larger possible resolution
  RESOLUTION_INFO res_info = CDisplaySettings::GetInstance().GetResolutionInfo(g_graphicsContext.GetVideoResolution());
  for (unsigned int i=0; i<CDisplaySettings::GetInstance().ResolutionInfoSize(); ++i)
  {
    RESOLUTION_INFO res = CDisplaySettings::GetInstance().GetResolutionInfo(i);
    if (res.iWidth > res_info.iWidth || res.iHeight > res_info.iHeight)
      res_info = res;
  }

  // Android might go even higher via surface
  std::string displaySize = CJNISystemProperties::get("sys.display-size", "");
  if (!displaySize.empty())
  {
    std::vector<std::string> aSize = StringUtils::Split(displaySize, "x");
    if (aSize.size() == 2)
    {
      res_info.iWidth = StringUtils::IsInteger(aSize[0]) ? atoi(aSize[0].c_str()) : 0;
      res_info.iHeight = StringUtils::IsInteger(aSize[1]) ? atoi(aSize[1].c_str()) : 0;
    }
  }

  return CPointInt(res_info.iWidth, res_info.iHeight);
}

CRect CXBMCApp::GetSurfaceRect()
{
  CSingleLock lock(m_AppMutex);

  return m_surface_rect;
}

CRect CXBMCApp::MapRenderToDroid(const CRect& srcRect)
{
  CSingleLock lock(m_AppMutex);

  return CRect(srcRect.x1 / m_droid2guiRatio.x2, srcRect.y1 / m_droid2guiRatio.y2, srcRect.x2 / m_droid2guiRatio.x2, srcRect.y2 / m_droid2guiRatio.y2);
}

CPoint CXBMCApp::MapDroidToGui(const CPoint& src)
{
  CSingleLock lock(m_AppMutex);

  return CPoint((src.x - m_droid2guiRatio.x1) * m_droid2guiRatio.x2, (src.y - m_droid2guiRatio.y1) * m_droid2guiRatio.y2);
}

void CXBMCApp::UpdateSessionMetadata()
{
  CJNIMediaMetadataBuilder builder;
  builder
      .putString(CJNIMediaMetadata::METADATA_KEY_DISPLAY_TITLE, g_infoManager.GetLabel(PLAYER_TITLE))
      .putString(CJNIMediaMetadata::METADATA_KEY_TITLE, g_infoManager.GetLabel(PLAYER_TITLE))
      .putLong(CJNIMediaMetadata::METADATA_KEY_DURATION, g_application.m_pPlayer->GetTotalTime())
//      .putString(CJNIMediaMetadata::METADATA_KEY_ART_URI, thumb)
//      .putString(CJNIMediaMetadata::METADATA_KEY_DISPLAY_ICON_URI, thumb)
//      .putString(CJNIMediaMetadata::METADATA_KEY_ALBUM_ART_URI, thumb)
      ;

  std::string thumb;
  if (m_playback_state & PLAYBACK_STATE_VIDEO)
  {
    builder
        .putString(CJNIMediaMetadata::METADATA_KEY_DISPLAY_SUBTITLE, g_infoManager.GetLabel(VIDEOPLAYER_TAGLINE))
        .putString(CJNIMediaMetadata::METADATA_KEY_ARTIST, g_infoManager.GetLabel(VIDEOPLAYER_DIRECTOR))
        ;
    thumb = g_infoManager.GetImage(VIDEOPLAYER_COVER, -1);
  }
  else if (m_playback_state & PLAYBACK_STATE_AUDIO)
  {
    builder
        .putString(CJNIMediaMetadata::METADATA_KEY_DISPLAY_SUBTITLE, g_infoManager.GetLabel(MUSICPLAYER_ARTIST))
        .putString(CJNIMediaMetadata::METADATA_KEY_ARTIST, g_infoManager.GetLabel(MUSICPLAYER_ARTIST))
        ;
    thumb = g_infoManager.GetImage(MUSICPLAYER_COVER, -1);
  }
  bool needrecaching = false;
  std::string cachefile = CTextureCache::GetInstance().CheckCachedImage(thumb, needrecaching);
  if (!cachefile.empty())
  {
    std::string actualfile = CSpecialProtocol::TranslatePath(cachefile);
    CJNIBitmap bmp = CJNIBitmapFactory::decodeFile(actualfile);
    if (bmp)
      builder.putBitmap(CJNIMediaMetadata::METADATA_KEY_ART, bmp);
  }
  m_mediaSession->updateMetadata(builder.build());
}

void CXBMCApp::UpdateSessionState()
{
  CJNIPlaybackStateBuilder builder;
  int state = CJNIPlaybackState::STATE_NONE;
  int64_t pos = 0;
  float speed = 0.0;
  if (m_playback_state != PLAYBACK_STATE_STOPPED)
  {
    if (g_application.m_pPlayer->HasVideo())
      m_playback_state |= PLAYBACK_STATE_VIDEO;
    else
      m_playback_state &= ~PLAYBACK_STATE_VIDEO;
    if (g_application.m_pPlayer->HasAudio())
      m_playback_state |= PLAYBACK_STATE_AUDIO;
    else
      m_playback_state &= ~PLAYBACK_STATE_AUDIO;
    pos = g_application.m_pPlayer->GetTime();
    speed = g_application.m_pPlayer->GetPlaySpeed();
    if (m_playback_state & PLAYBACK_STATE_PLAYING)
      state = CJNIPlaybackState::STATE_PLAYING;
    else
      state = CJNIPlaybackState::STATE_PAUSED;
  }
  else
    state = CJNIPlaybackState::STATE_STOPPED;
  builder
      .setState(state, pos, speed, CJNISystemClock::elapsedRealtime())
      .setActions(0xffffffffffffffff)
      ;
  m_mediaSession->updatePlaybackState(builder.build());
}

void CXBMCApp::OnPlayBackStarted()
{
  CLog::Log(LOGDEBUG, "%s", __PRETTY_FUNCTION__);

  if (getPackageName() != CCompileInfo::GetPackage())
    CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);

  AcquireAudioFocus();

  m_playback_state = PLAYBACK_STATE_PLAYING;
  if (g_application.m_pPlayer->HasVideo())
    m_playback_state |= PLAYBACK_STATE_VIDEO;
  if (g_application.m_pPlayer->HasAudio())
    m_playback_state |= PLAYBACK_STATE_AUDIO;
  if (!g_application.m_pPlayer->CanPause())
    m_playback_state |= PLAYBACK_STATE_CANNOT_PAUSE;

  UpdateSessionMetadata();
  m_mediaSession->activate(true);
  UpdateSessionState();

  CJNIIntent intent(ACTION_XBMC_RESUME, CJNIURI::EMPTY, *this, get_class(CJNIContext::get_raw()));
  m_mediaSession->updateIntent(intent);
}

void CXBMCApp::OnPlayBackPaused()
{
  CLog::Log(LOGDEBUG, "%s", __PRETTY_FUNCTION__);

  ReleaseAudioFocus();
  m_playback_state &= ~PLAYBACK_STATE_PLAYING;
  UpdateSessionState();
}

void CXBMCApp::OnPlayBackStopped()
{
  CLog::Log(LOGDEBUG, "%s", __PRETTY_FUNCTION__);

  m_playback_state = PLAYBACK_STATE_STOPPED;
  UpdateSessionState();

  ReleaseAudioFocus();
  m_mediaSession->activate(false);
}

void CXBMCApp::ProcessSlow()
{
  if ((m_playback_state & PLAYBACK_STATE_PLAYING) && m_mediaSession->isActive())
    UpdateSessionState();
}

std::vector<androidPackage> CXBMCApp::GetApplications()
{
  CSingleLock lock(m_applicationsMutex);
  if (m_applications.empty())
  {
    std::map<std::string, androidPackage> applications;
    CJNIIntent main(CJNIIntent::ACTION_MAIN, CJNIURI());

    if (CAndroidFeatures::IsLeanback())  // First try leanback
    {
      main.addCategory(CJNIIntent::CATEGORY_LEANBACK_LAUNCHER);

      CJNIList<CJNIResolveInfo> launchables = GetPackageManager().queryIntentActivities(main, 0);
      int numPackages = launchables.size();
      for (int i = 0; i < numPackages; i++)
      {
        CJNIResolveInfo launchable = launchables.get(i);
        CJNIActivityInfo activity = launchable.activityInfo;

        androidPackage newPackage;
        newPackage.packageName = activity.applicationInfo.packageName;
        newPackage.className = activity.name;
        newPackage.packageLabel = launchable.loadLabel(GetPackageManager()).toString();
        newPackage.icon = activity.applicationInfo.icon;
        applications.insert(std::make_pair(newPackage.packageName, newPackage));
      }
    }

    main.removeCategory(CJNIIntent::CATEGORY_LEANBACK_LAUNCHER);
    main.addCategory(CJNIIntent::CATEGORY_LAUNCHER);

    CJNIList<CJNIResolveInfo> launchables = GetPackageManager().queryIntentActivities(main, 0);
    int numPackages = launchables.size();
    for (int i = 0; i < numPackages; i++)
    {
      CJNIResolveInfo launchable = launchables.get(i);
      CJNIActivityInfo activity = launchable.activityInfo;

      if (applications.find(activity.applicationInfo.packageName) == applications.end())
      {
        androidPackage newPackage;
        newPackage.packageName = activity.applicationInfo.packageName;
        newPackage.className = activity.name;
        newPackage.packageLabel = launchable.loadLabel(GetPackageManager()).toString();
        newPackage.icon = activity.applicationInfo.icon;
        applications.insert(std::make_pair(newPackage.packageName, newPackage));
      }
    }

    for(auto it : applications)
      m_applications.push_back(it.second);
  }

  return m_applications;
}

bool CXBMCApp::HasLaunchIntent(const string &package)
{
  return (GetPackageManager().getLaunchIntentForPackage(package) != NULL);
}

bool CXBMCApp::StartAppActivity(const std::string &package, const std::string &cls)
{
  CJNIComponentName name(package, cls);
  CJNIIntent newIntent(CJNIIntent::ACTION_MAIN);

  newIntent.addCategory(CJNIIntent::CATEGORY_LAUNCHER);
  newIntent.setFlags(CJNIIntent::FLAG_ACTIVITY_NEW_TASK | CJNIIntent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
  newIntent.setComponent(name);

  startActivity(newIntent);
  if (xbmc_jnienv()->ExceptionCheck())
  {
    CLog::Log(LOGERROR, "CXBMCApp::StartActivity - ExceptionOccurred launching %s", package.c_str());
    xbmc_jnienv()->ExceptionClear();
    return false;
  }

  return true;
}

// Note intent, dataType, dataURI all default to ""
bool CXBMCApp::StartActivity(const string &package, const string &intent, const string &dataType, const string &dataURI)
{
  if (package.find('/') != std::string::npos)
  {
    std::vector<std::string> split = StringUtils::Split(package, '/');
    return StartAppActivity(split[0], split[1]);
  }

  CJNIIntent newIntent;
  if (intent.empty())
  {
    if (CAndroidFeatures::IsLeanback())
      newIntent = GetPackageManager().getLeanbackLaunchIntentForPackage(package);
    if (!newIntent)
      newIntent = GetPackageManager().getLaunchIntentForPackage(package);
  }
  else
    newIntent = CJNIIntent(intent);

  if (!newIntent)
    return false;

  if (!dataURI.empty())
  {
    CJNIURI jniURI = CJNIURI::parse(dataURI);

    if (!jniURI)
      return false;

    newIntent.setDataAndType(jniURI, dataType);
  }

  newIntent.setPackage(package);
  startActivity(newIntent);
  if (xbmc_jnienv()->ExceptionCheck())
  {
    CLog::Log(LOGERROR, "CXBMCApp::StartActivity - ExceptionOccurred launching %s", package.c_str());
    xbmc_jnienv()->ExceptionClear();
    return false;
  }

  return true;
}

int CXBMCApp::GetBatteryLevel()
{
  return m_batteryLevel;
}

bool CXBMCApp::GetExternalStorage(std::string &path, const std::string &type /* = "" */)
{
  std::string sType;
  std::string mountedState;
  bool mounted = false;

  if(type == "files" || type.empty())
  {
    CJNIFile external = CJNIEnvironment::getExternalStorageDirectory();
    if (external)
      path = external.getAbsolutePath();
  }
  else
  {
    if (type == "music")
      sType = "Music"; // Environment.DIRECTORY_MUSIC
    else if (type == "videos")
      sType = "Movies"; // Environment.DIRECTORY_MOVIES
    else if (type == "pictures")
      sType = "Pictures"; // Environment.DIRECTORY_PICTURES
    else if (type == "photos")
      sType = "DCIM"; // Environment.DIRECTORY_DCIM
    else if (type == "downloads")
      sType = "Download"; // Environment.DIRECTORY_DOWNLOADS
    if (!sType.empty())
    {
      CJNIFile external = CJNIEnvironment::getExternalStoragePublicDirectory(sType);
      if (external)
        path = external.getAbsolutePath();
    }
  }
  mountedState = CJNIEnvironment::getExternalStorageState();
  mounted = (mountedState == "mounted" || mountedState == "mounted_ro");
  return mounted && !path.empty();
}

bool CXBMCApp::GetStorageUsage(const std::string &path, std::string &usage)
{
#define PATH_MAXLEN 50

  if (path.empty())
  {
    std::ostringstream fmt;
    fmt.width(PATH_MAXLEN);  fmt << std::left  << "Filesystem";
    fmt.width(12);  fmt << std::right << "Size";
    fmt.width(12);  fmt << "Used";
    fmt.width(12);  fmt << "Avail";
    fmt.width(12);  fmt << "Use %";

    usage = fmt.str();
    return false;
  }

  CJNIStatFs fileStat(path);
  if (!fileStat)
  {
    CLog::Log(LOGERROR, "CXBMCApp::GetStorageUsage cannot stat %s", path.c_str());
    return false;
  }

  int blockSize = fileStat.getBlockSize();
  int blockCount = fileStat.getBlockCount();
  int freeBlocks = fileStat.getFreeBlocks();

  if (blockSize <= 0 || blockCount <= 0 || freeBlocks < 0)
    return false;

  float totalSize = (float)blockSize * blockCount / GIGABYTES;
  float freeSize = (float)blockSize * freeBlocks / GIGABYTES;
  float usedSize = totalSize - freeSize;
  float usedPercentage = usedSize / totalSize * 100;

  std::ostringstream fmt;
  fmt << std::fixed;
  fmt.precision(1);
  fmt.width(PATH_MAXLEN);  fmt << std::left  << (path.size() < PATH_MAXLEN-1 ? path : StringUtils::Left(path, PATH_MAXLEN-4) + "...");
  fmt.width(12);  fmt << std::right << totalSize << "G"; // size in GB
  fmt.width(12);  fmt << usedSize << "G"; // used in GB
  fmt.width(12);  fmt << freeSize << "G"; // free
  fmt.precision(0);
  fmt.width(12);  fmt << usedPercentage << "%"; // percentage used

  usage = fmt.str();
  return true;
}

// Used in Application.cpp to figure out volume steps
int CXBMCApp::GetMaxSystemVolume()
{
  JNIEnv* env = xbmc_jnienv();
  static int maxVolume = -1;
  if (maxVolume == -1)
  {
    maxVolume = GetMaxSystemVolume(env);
  }
  //CLog::Log(LOGDEBUG, "CXBMCApp::GetMaxSystemVolume: %i",maxVolume);
  return maxVolume;
}

int CXBMCApp::GetMaxSystemVolume(JNIEnv *env)
{
  CJNIAudioManager audioManager(getSystemService("audio"));
  if (audioManager)
    return audioManager.getStreamMaxVolume();
  CLog::Log(LOGDEBUG, "CXBMCApp::SetSystemVolume: Could not get Audio Manager");
  return 0;
}

float CXBMCApp::GetSystemVolume()
{
  CJNIAudioManager audioManager(getSystemService("audio"));
  if (audioManager)
    return (float)audioManager.getStreamVolume() / GetMaxSystemVolume();
  else
  {
    CLog::Log(LOGDEBUG, "CXBMCApp::GetSystemVolume: Could not get Audio Manager");
    return 0;
  }
}

void CXBMCApp::SetSystemVolume(float percent)
{
  CJNIAudioManager audioManager(getSystemService("audio"));
  int maxVolume = (int)(GetMaxSystemVolume() * percent);
  if (audioManager)
    audioManager.setStreamVolume(maxVolume);
  else
    CLog::Log(LOGDEBUG, "CXBMCApp::SetSystemVolume: Could not get Audio Manager");
}

void CXBMCApp::onReceive(CJNIIntent intent)
{
  std::string action = intent.getAction();
  CLog::Log(LOGDEBUG, "CXBMCApp::onReceive Got intent. Action: %s", action.c_str());
  if (action == "android.intent.action.BATTERY_CHANGED")
    m_batteryLevel = intent.getIntExtra("level",-1);
  else if (action == "android.intent.action.DREAMING_STOPPED" || action == "android.intent.action.SCREEN_ON")
  {
    if (HasFocus())
      g_application.WakeUpScreenSaverAndDPMS();
  }
  else if (action == "android.intent.action.SCREEN_OFF")
  {
    if (m_playback_state & PLAYBACK_STATE_VIDEO)
      CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_STOP)));
  }
  else if (action == "android.intent.action.HEADSET_PLUG" || action == "android.bluetooth.a2dp.profile.action.CONNECTION_STATE_CHANGED")
  {
    m_audiodevices.clear();
    if (CJNIAudioManager::GetSDKVersion() >= 23)
    {
      CJNIAudioManager audioManager(getSystemService("audio"));
      m_audiodevices = audioManager.getDevices(CJNIAudioManager::GET_DEVICES_OUTPUTS);
      LogAudoDevices("Connectivity changed", m_audiodevices);
    }
    CheckHeadsetPlugged();
  }
  else if (action == "android.net.conn.CONNECTIVITY_CHANGE")
  {
    if (g_application.IsAppInitialized())
    {
      CNetwork& net = g_application.getNetwork();
      CNetworkAndroid* netdroid = static_cast<CNetworkAndroid*>(&net);
      netdroid->RetrieveInterfaces();
    }
  }
  else if (action == "android.media.action.HDMI_AUDIO_PLUG")
  {
    bool newstate;
    newstate = (intent.getIntExtra("android.media.extra.AUDIO_PLUG_STATE", 0) != 0);

    if (newstate != m_hdmiPlugged)
    {
      CLog::Log(LOGDEBUG, "-- HDMI state: %s",  newstate ? "on" : "off");
      std::ostringstream oss;
      if (intent.hasExtra("android.media.extra.ENCODINGS"))
      {
        for (auto i : intent.getIntArrayExtra("android.media.extra.ENCODINGS"))
          oss << encode2string[i].c_str() << " / ";
        CLog::Log(LOGDEBUG, "    encodings: %s", oss.str().c_str());
      }

      m_hdmiPlugged = newstate;
      CAEFactory::DeviceChange();

      //HACK: AFTV send a dummy "pause" mediasession command when switching reso. Reenable...
      if (CAndroidFeatures::IsAmazonDevice())
      {
        if (newstate && m_playback_state & PLAYBACK_STATE_PLAYING)
          m_mediaSession->activate(true);
      }

    }
  }
  else if (action == "android.intent.action.MEDIA_BUTTON")
  {
    if (m_playback_state == PLAYBACK_STATE_STOPPED)
    {
      CLog::Log(LOGINFO, "Ignore MEDIA_BUTTON intent: no media playing");
      return;
    }
    CJNIKeyEvent keyevt = (CJNIKeyEvent)intent.getParcelableExtra(CJNIIntent::EXTRA_KEY_EVENT);

    int keycode = keyevt.getKeyCode();
    bool up = (keyevt.getAction() == CJNIKeyEvent::ACTION_UP);

    CLog::Log(LOGINFO, "Got MEDIA_BUTTON intent: %d, up:%s", keycode, up ? "true" : "false");
    if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_RECORD)
      CAndroidKey::XBMC_Key(keycode, XBMCK_RECORD, 0, 0, up);
    else if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_EJECT)
      CAndroidKey::XBMC_Key(keycode, XBMCK_EJECT, 0, 0, up);
    else if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_FAST_FORWARD)
      CAndroidKey::XBMC_Key(keycode, XBMCK_MEDIA_FASTFORWARD, 0, 0, up);
    else if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_NEXT)
      CAndroidKey::XBMC_Key(keycode, XBMCK_MEDIA_NEXT_TRACK, 0, 0, up);
    else if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_PAUSE)
      CAndroidKey::XBMC_Key(keycode, XBMCK_MEDIA_PLAY_PAUSE, 0, 0, up);
    else if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_PLAY)
      CAndroidKey::XBMC_Key(keycode, XBMCK_MEDIA_PLAY_PAUSE, 0, 0, up);
    else if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_PLAY_PAUSE)
      CAndroidKey::XBMC_Key(keycode, XBMCK_MEDIA_PLAY_PAUSE, 0, 0, up);
    else if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_PREVIOUS)
      CAndroidKey::XBMC_Key(keycode, XBMCK_MEDIA_PREV_TRACK, 0, 0, up);
    else if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_REWIND)
      CAndroidKey::XBMC_Key(keycode, XBMCK_MEDIA_REWIND, 0, 0, up);
    else if (keycode == CJNIKeyEvent::KEYCODE_MEDIA_STOP)
      CAndroidKey::XBMC_Key(keycode, XBMCK_MEDIA_STOP, 0, 0, up);
  }
}

void CXBMCApp::onNewIntent(CJNIIntent intent)
{
  std::string action = intent.getAction();
  CLog::Log(LOGDEBUG, "Got Intent: %s", action.c_str());
  std::string targetFile = GetFilenameFromIntent(intent);
  CLog::Log(LOGDEBUG, "-- targetFile: %s", targetFile.c_str());
  if (action == "android.intent.action.VIEW")
  {
    CURL targeturl(targetFile);
    bool resume = false;
    if (targeturl.GetOption("resume") == "true")
      resume = true;

    CFileItem* item = new CFileItem(targeturl.GetWithoutOptions(), false);
    if (item->IsVideoDb())
    {
      *(item->GetVideoInfoTag()) = XFILE::CVideoDatabaseFile::GetVideoTag(CURL(item->GetPath()));
      item->SetPath(item->GetVideoInfoTag()->m_strFileNameAndPath);
      if (resume)
        item->m_lStartOffset = STARTOFFSET_RESUME;
    }
    CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_PLAY, 0, 0, static_cast<void*>(item));
  }
  else if (action == "android.intent.action.GET_CONTENT")
  {
    CURL targeturl(targetFile);
    if (targeturl.IsProtocol("videodb")
        || (targeturl.IsProtocol("special") && targetFile.find("playlists/video") != std::string::npos)
        || (targeturl.IsProtocol("special") && targetFile.find("playlists/mixed") != std::string::npos)
        )
    {
      std::vector<std::string> params;
      params.push_back(targeturl.Get());
      params.push_back("return");
      CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTIVATE_WINDOW, WINDOW_VIDEO_NAV, 0, nullptr, "", params);
    }
    else if (targeturl.IsProtocol("musicdb")
             || (targeturl.IsProtocol("special") && targetFile.find("playlists/music") != std::string::npos))
    {
      std::vector<std::string> params;
      params.push_back(targeturl.Get());
      params.push_back("return");
      CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTIVATE_WINDOW, WINDOW_MUSIC_NAV, 0, nullptr, "", params);
    }
  }
  else if (action == ACTION_XBMC_RESUME)
  {
    if (m_playback_state != PLAYBACK_STATE_STOPPED)
    {
      if (!(m_playback_state & PLAYBACK_STATE_PLAYING))
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PAUSE)));
    }
  }
}

void CXBMCApp::onActivityResult(int requestCode, int resultCode, CJNIIntent resultData)
{
  for (auto it = m_activityResultEvents.begin(); it != m_activityResultEvents.end(); ++it)
  {
    if ((*it)->GetRequestCode() == requestCode)
    {
      m_activityResultEvents.erase(it);
      (*it)->SetResultCode(resultCode);
      (*it)->SetResultData(resultData);
      (*it)->Set();
      break;
    }
  }
}

bool CXBMCApp::GetCapture(CJNIImage& img)
{
  CSingleLock lock(m_captureMutex);
  if (m_captureQueue.empty())
    return false;

  img = m_captureQueue.front();
  m_captureQueue.pop();
  return true;
}

void CXBMCApp::TakeScreenshot()
{
  takeScreenshot();
}

void CXBMCApp::StopCapture()
{
  CSingleLock lock(m_captureMutex);
  while (!m_captureQueue.empty())
  {
    CJNIImage img = m_captureQueue.front();
    img.close();
    m_captureQueue.pop();
  }
  CJNIMainActivity::stopCapture();
}

void CXBMCApp::onCaptureAvailable(CJNIImage image)
{
  CSingleLock lock(m_captureMutex);

  m_captureQueue.push(image);
  if (m_captureQueue.size() > CAPTURE_QUEUE_MAXDEPTH)
  {
    CJNIImage img = m_captureQueue.front();
    img.close();
    m_captureQueue.pop();
  }
  m_captureEvent.Set();
}

void CXBMCApp::onScreenshotAvailable(CJNIImage image)
{
  CSingleLock lock(m_captureMutex);

  m_screenshotEvent.SetImage(image);
  m_screenshotEvent.Set();
}

void CXBMCApp::onMultiWindowModeChanged(bool isInMultiWindowMode)
{
  CLog::Log(LOGDEBUG, "%s: %s", __PRETTY_FUNCTION__, isInMultiWindowMode ? "true" : "false");
}

void CXBMCApp::onPictureInPictureModeChanged(bool isInPictureInPictureMode)
{
  CLog::Log(LOGDEBUG, "%s: %s", __PRETTY_FUNCTION__, isInPictureInPictureMode ? "true" : "false");
  m_hasPIP = isInPictureInPictureMode;

}

void CXBMCApp::onUserLeaveHint()
{
  if ((m_playback_state & PLAYBACK_STATE_PLAYING) && (m_playback_state & PLAYBACK_STATE_VIDEO))
  {
    if (CJNIBase::GetSDKVersion() >= 24)
      RequestPictureInPictureMode();
  }
}

int CXBMCApp::WaitForActivityResult(const CJNIIntent &intent, int requestCode, CJNIIntent &result)
{
  int ret = 0;
  CActivityResultEvent* event = new CActivityResultEvent(requestCode);
  m_activityResultEvents.push_back(event);
  startActivityForResult(intent, requestCode);
  if (event->Wait())
  {
    result = event->GetResultData();
    ret = event->GetResultCode();
  }
  delete event;
  return ret;
}

bool CXBMCApp::WaitForScreenshot(CJNIImage& image)
{
  bool ret = false;
  if (m_screenshotEvent.WaitMSec(2000))
  {
    image = m_screenshotEvent.GetImage();
    ret = true;
  }
  m_screenshotEvent.Reset();
  return ret;
}

bool CXBMCApp::WaitForCapture(unsigned int timeoutMs)
{
  bool ret = m_captureEvent.WaitMSec(timeoutMs);
  m_captureEvent.Reset();
  return ret;
}

void CXBMCApp::onVolumeChanged(int volume)
{
  // System volume was used; Reset Kodi volume to 100% if it'not, already
  if (g_application.GetVolume(false) != 1.0)
    CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(
                                                 new CAction(ACTION_VOLUME_SET, static_cast<float>(CXBMCApp::GetMaxSystemVolume()))));
}

void CXBMCApp::onAudioFocusChange(int focusChange)
{
  CLog::Log(LOGDEBUG, "CXBMCApp::onAudioFocusChange: %d", focusChange);
  if (focusChange == CJNIAudioManager::AUDIOFOCUS_GAIN)
  {
    m_audioFocusGranted = true;
    if (m_lastAudioFocusChange == CJNIAudioManager::AUDIOFOCUS_LOSS ||
        m_lastAudioFocusChange == CJNIAudioManager::AUDIOFOCUS_LOSS_TRANSIENT ||
        m_lastAudioFocusChange == CJNIAudioManager::AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK
       )
    {
      if (m_wasPlayingWhenTransientLoss && m_playback_state != PLAYBACK_STATE_STOPPED)
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PAUSE)));
      m_wasPlayingWhenTransientLoss = false;
    }
  }
  else if (focusChange == CJNIAudioManager::AUDIOFOCUS_LOSS ||
           focusChange == CJNIAudioManager::AUDIOFOCUS_LOSS_TRANSIENT ||
           focusChange == CJNIAudioManager::AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK
          )
  {
    m_audioFocusGranted = false;
    if ((m_playback_state & PLAYBACK_STATE_PLAYING))
    {
      if (m_playback_state & PLAYBACK_STATE_CANNOT_PAUSE)
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_STOP)));
      else
      {
        CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_PAUSE)));
        m_wasPlayingWhenTransientLoss = true;
      }
    }
  }
  m_lastAudioFocusChange = focusChange;
}

void CXBMCApp::doFrame(int64_t frameTimeNanos)
{
  m_vsynctime = frameTimeNanos;
  //CLog::Log(LOGDEBUG, "vsync: %lld", frameTimeNanos);
  m_vsyncEvent.Set();
}

bool CXBMCApp::WaitVSync(unsigned int milliSeconds)
{
  return m_vsyncEvent.WaitMSec(milliSeconds);
}

void CXBMCApp::SetupEnv()
{
  setenv("XBMC_ANDROID_SYSTEM_LIBS", CJNISystem::getProperty("java.library.path").c_str(), 0);
  setenv("XBMC_ANDROID_LIBS", getApplicationInfo().nativeLibraryDir.c_str(), 0);
  setenv("XBMC_ANDROID_APK", getPackageResourcePath().c_str(), 0);

  std::string appName = CCompileInfo::GetAppName();
  StringUtils::ToLower(appName);
  std::string className = "org.xbmc." + appName;

  std::string xbmcHome = CJNISystem::getProperty("xbmc.home", "");
  if (xbmcHome.empty())
  {
    std::string cacheDir = getCacheDir().getAbsolutePath();
    setenv("MRMC_BIN_HOME", (cacheDir + "/apk/assets").c_str(), 0);
    setenv("MRMC_HOME", (cacheDir + "/apk/assets").c_str(), 0);
  }
  else
  {
    setenv("MRMC_BIN_HOME", (xbmcHome + "/assets").c_str(), 0);
    setenv("MRMC_HOME", (xbmcHome + "/assets").c_str(), 0);
  }

  std::string externalDir = CJNISystem::getProperty("xbmc.data", "");
  if (externalDir.empty())
  {
    CJNIFile androidPath = getExternalFilesDir("");
    if (!androidPath)
      androidPath = getDir(className.c_str(), 1);

    if (androidPath)
      externalDir = androidPath.getAbsolutePath();
  }

  if (!externalDir.empty())
    setenv("HOME", externalDir.c_str(), 0);
  else
    setenv("HOME", getenv("MRMC_TEMP"), 0);

  std::string apkPath = getenv("XBMC_ANDROID_APK");
  apkPath += "/assets/python2.7";
  setenv("PYTHONHOME", apkPath.c_str(), 1);
  setenv("PYTHONPATH", "", 1);
  setenv("PYTHONOPTIMIZE","", 1);
  setenv("PYTHONNOUSERSITE", "1", 1);
}

std::string CXBMCApp::GetFilenameFromIntent(const CJNIIntent &intent)
{
    std::string ret;
    if (!intent)
      return ret;
    CJNIURI data = intent.getData();
    if (!data)
      return ret;
    std::string scheme = data.getScheme();
    StringUtils::ToLower(scheme);
    if (scheme == "content")
    {
      std::vector<std::string> filePathColumn;
      filePathColumn.push_back(CJNIMediaStoreMediaColumns::DATA);
      CJNICursor cursor = getContentResolver().query(data, filePathColumn, std::string(), std::vector<std::string>(), std::string());
      if(cursor.moveToFirst())
      {
        int columnIndex = cursor.getColumnIndex(filePathColumn[0]);
        ret = cursor.getString(columnIndex);
      }
      cursor.close();
    }
    else if(scheme == "file")
      ret = data.getPath();
    else
      ret = data.toString();
  return ret;
}

const ANativeWindow** CXBMCApp::GetNativeWindow(int timeout)
{
  if (m_window)
    return (const ANativeWindow**)&m_window;

  if (m_mainView)
    m_mainView->waitForSurface(timeout);

  return (const ANativeWindow**)&m_window;
}

void CXBMCApp::CalculateGUIRatios()
{
  m_droid2guiRatio = CRect(0.0, 0.0, 1.0, 1.0);

  if(!m_xbmcappinstance || !m_surface_rect.Width() || !m_surface_rect.Height())
    return;

  RESOLUTION_INFO res_info = g_graphicsContext.GetResInfo();
  float curRatio = (float)res_info.iWidth / res_info.iHeight;
  float newRatio = (float)m_surface_rect.Width() / m_surface_rect.Height();

  res_info.fPixelRatio = newRatio / curRatio;
  g_graphicsContext.SetResInfo(g_graphicsContext.GetVideoResolution(), res_info);

  CRect gui = CRect(0, 0, res_info.iWidth, res_info.iHeight);
  m_droid2guiRatio.x1 = m_surface_rect.x1;
  m_droid2guiRatio.y1 = m_surface_rect.y1;
  m_droid2guiRatio.x2 = gui.Width() / (double)m_surface_rect.Width();
  m_droid2guiRatio.y2 = gui.Height() / (double)m_surface_rect.Height();

  CLog::Log(LOGDEBUG, "%s(gui scaling) - %f, %f", __PRETTY_FUNCTION__, m_droid2guiRatio.x2, m_droid2guiRatio.y2);
}

void CXBMCApp::surfaceChanged(CJNISurfaceHolder holder, int format, int width, int height)
{
  CLog::Log(LOGDEBUG, "%s: %d x %d", __PRETTY_FUNCTION__, width, height);
}

void CXBMCApp::surfaceCreated(CJNISurfaceHolder holder)
{
  m_window = ANativeWindow_fromSurface(xbmc_jnienv(), holder.getSurface().get_raw());
  if (m_window == NULL)
  {
    CLog::Log(LOGDEBUG, " => invalid ANativeWindow object");
    return;
  }
  XBMC_SetupDisplay();
}

void CXBMCApp::surfaceDestroyed(CJNISurfaceHolder holder)
{
  // If we have exited XBMC, it no longer exists.
  if (!m_exiting)
  {
    XBMC_DestroyDisplay();
    m_window = NULL;
  }
}

void CXBMCApp::onLayoutChange(int left, int top, int width, int height)
{
  CSingleLock lock(m_AppMutex);

  // Ignore left/top. We are zero-based
  m_surface_rect.x1 = 0;
  m_surface_rect.y1 = 0;
  m_surface_rect.x2 = width;
  m_surface_rect.y2 = height;

  CLog::Log(LOGDEBUG, "%s: %f + %f - %f x %f", __PRETTY_FUNCTION__, m_surface_rect.x1, m_surface_rect.y1, m_surface_rect.Width(), m_surface_rect.Height());

  if (g_application.GetRenderGUI())
    CalculateGUIRatios();
}

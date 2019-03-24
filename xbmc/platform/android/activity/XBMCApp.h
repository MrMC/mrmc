#pragma once
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

#include "system.h"

#include <math.h>
#include <pthread.h>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <memory>

#include <android/native_activity.h>

#include "IActivityHandler.h"
#include "IInputHandler.h"

#include "platform/MCRuntimeLib.h"
#include <androidjni/Activity.h>
#include <androidjni/BroadcastReceiver.h>
#include <androidjni/AudioManager.h>
#include <androidjni/AudioDeviceInfo.h>
#include <androidjni/Image.h>
#include <androidjni/SurfaceHolder.h>

#include "threads/Event.h"
#include "interfaces/IAnnouncer.h"
#include "guilib/Geometry.h"

#include "JNIMainActivity.h"
#include "JNIXBMCAudioManagerOnAudioFocusChangeListener.h"
#include "JNIXBMCMainView.h"
#include "JNIXBMCMediaSession.h"
#include "JNIXBMCBroadcastReceiver.h"

// forward delares
class CJNIWakeLock;
class CAESinkAUDIOTRACK;
class CVariant;
class CVideoSyncAndroid;
typedef struct _JNIEnv JNIEnv;

struct androidIcon
{
  unsigned int width;
  unsigned int height;
  void *pixels;
};

struct androidPackage
{
  std::string packageName;
  std::string className;
  std::string packageLabel;
  int icon;
};

class CActivityResultEvent : public CEvent
{
public:
  CActivityResultEvent(int requestcode)
    : m_requestcode(requestcode)
  {}
  int GetRequestCode() const { return m_requestcode; }
  int GetResultCode() const { return m_resultcode; }
  void SetResultCode(int resultcode) { m_resultcode = resultcode; }
  CJNIIntent GetResultData() const { return m_resultdata; }
  void SetResultData(const CJNIIntent &resultdata) { m_resultdata = resultdata; }

protected:
  int m_requestcode;
  CJNIIntent m_resultdata;
  int m_resultcode;
};

class CCaptureEvent : public CEvent
{
public:
  CCaptureEvent() {}
  jni::CJNIImage GetImage() const { return m_image; }
  void SetImage(const jni::CJNIImage &image) { m_image = image; }

protected:
  jni::CJNIImage m_image;
};

class CXBMCApp
    : public IActivityHandler
    , public CJNIMainActivity
    , public CJNIBroadcastReceiver
    , public CJNIAudioManagerAudioFocusChangeListener
    , public ANNOUNCEMENT::IAnnouncer
    , public CJNISurfaceHolderCallback
{
public:
  CXBMCApp(ANativeActivity *nativeActivity);
  virtual ~CXBMCApp();
  static CXBMCApp* get() { return m_xbmcappinstance; }

  static const char* audioencode2string(int i);

  // IAnnouncer IF
  virtual void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data);

  virtual void onReceive(CJNIIntent intent);
  virtual void onNewIntent(CJNIIntent intent);
  virtual void onActivityResult(int requestCode, int resultCode, CJNIIntent resultData);
  virtual void onCaptureAvailable(jni::CJNIImage image);
  virtual void onScreenshotAvailable(jni::CJNIImage image);
  virtual void onVolumeChanged(int volume);
  virtual void onAudioFocusChange(int focusChange);
  virtual void doFrame(int64_t frameTimeNanos);
//  virtual void onAudioDeviceAdded(CJNIAudioDeviceInfos devices);
//  virtual void onAudioDeviceRemoved(CJNIAudioDeviceInfos devices);
  virtual void onVisibleBehindCanceled() {}
  virtual void onMultiWindowModeChanged(bool isInMultiWindowMode);
  virtual void onPictureInPictureModeChanged(bool isInPictureInPictureMode);
  virtual void onUserLeaveHint();

  // CJNISurfaceHolderCallback interface
  virtual void surfaceChanged(CJNISurfaceHolder holder, int format, int width, int height) override;
  virtual void surfaceCreated(CJNISurfaceHolder holder) override;
  virtual void surfaceDestroyed(CJNISurfaceHolder holder) override;

  bool isValid() { return m_activity != NULL; }
  void Deinitialize(int status);

  void onStart();
  void onResume();
  void onPause();
  void onStop();
  void onDestroy();

  void onSaveState(void **data, size_t *size);
  void onConfigurationChanged();
  void onLowMemory();

  void onCreateWindow(ANativeWindow* window);
  void onResizeWindow();
  void onDestroyWindow();
  void onGainFocus();
  void onLostFocus();


  static const ANativeWindow** GetNativeWindow(int timeout);
  static int SetBuffersGeometry(int width, int height, int format);
  static int android_printf(const char *format, ...);
  void BringToFront();
  void Minimize();

  static int GetBatteryLevel();
  bool EnableWakeLock(bool on);
  bool ResetSystemIdleTimer();
  static bool HasFocus() { return m_hasFocus; }
  static bool IsResumed() { return m_hasResumed; }
  static CJNIAudioDeviceInfos GetAudioDeviceInfos() { return m_audiodevices; }
  void CheckHeadsetPlugged();
  static bool IsHeadsetPlugged();
  static bool IsHDMIPlugged();

  bool StartAppActivity(const std::string &package, const std::string &cls);
  bool StartActivity(const std::string &package, const std::string &intent = std::string(), const std::string &dataType = std::string(), const std::string &dataURI = std::string());
  std::vector <androidPackage> GetApplications();

  int GetMaxSystemVolume();
  float GetSystemVolume();
  void SetSystemVolume(float percent);

  void SetRefreshRate(float rate);
  void SetDisplayModeId(int mode, float rate);
  float GetRefreshRate() { return m_refreshrate; }
  int GetDPI();
  static bool IsNightMode();
  static CPointInt GetMaxDisplayResolution();
  static CRect GetSurfaceRect();
  static CRect MapRenderToDroid(const CRect& srcRect);
  static CPoint MapDroidToGui(const CPoint& src);

  int WaitForActivityResult(const CJNIIntent &intent, int requestCode, CJNIIntent& result);
  bool WaitForScreenshot(jni::CJNIImage& image);
  bool WaitForCapture(unsigned int timeoutMs);
  bool GetCapture(jni::CJNIImage& img);
  void TakeScreenshot();
  void StopCapture();

  // Playback callbacks
  void UpdateSessionMetadata();
  void UpdateSessionState();
  void OnPlayBackStarted();
  void OnPlayBackPaused();
  void OnPlayBackStopped();

  //PIP
  void RequestPictureInPictureMode();

  // Application slow ping
  void ProcessSlow();

  static bool WaitVSync(unsigned int milliSeconds);
  static uint64_t GetVsyncTime() { return m_vsynctime; }

  bool getVideosurfaceInUse();
  void setVideosurfaceInUse(bool videosurfaceInUse);

  void onLayoutChange(int left, int top, int width, int height);

protected:
  static void LogAudoDevices(const char* stage, const CJNIAudioDeviceInfos& devices);

  // limit who can access Volume
  friend class CAESinkAUDIOTRACK;

  int GetMaxSystemVolume(JNIEnv *env);
  bool AcquireAudioFocus();
  bool ReleaseAudioFocus();

private:
  static CXBMCApp* m_xbmcappinstance;
  static CCriticalSection m_AppMutex;

  std::unique_ptr<CJNIXBMCAudioManagerOnAudioFocusChangeListener> m_audioFocusListener;
  std::unique_ptr<jni::CJNIXBMCBroadcastReceiver> m_broadcastReceiver;
  static std::unique_ptr<CJNIXBMCMainView> m_mainView;
  std::unique_ptr<jni::CJNIXBMCMediaSession> m_mediaSession;
  bool HasLaunchIntent(const std::string &package);
  std::string GetFilenameFromIntent(const CJNIIntent &intent);
  void stop();
  static void SetRefreshRateCallback(CVariant *rate);
  static void SetDisplayModeIdCallback(CVariant *mode);
  static ANativeActivity *m_activity;
  static CJNIWakeLock *m_wakeLock;
  static int m_batteryLevel;
  static bool m_hasFocus;
  static bool m_hasResumed;
  static bool m_audioFocusGranted;
  static int  m_lastAudioFocusChange;
  static bool m_wasPlayingVideoWhenPaused;
  static double m_wasPlayingVideoWhenPausedTime;
  static bool m_wasPlayingWhenTransientLoss;
  static bool m_headsetPlugged;
  static bool m_hdmiPlugged;
  static bool m_hasPIP;
  bool m_videosurfaceInUse;
  bool m_firstActivityRun;
  bool m_exiting;
  pthread_t m_thread;
  static CCriticalSection m_applicationsMutex;
  static std::vector<androidPackage> m_applications;
  static std::vector<CActivityResultEvent*> m_activityResultEvents;

  static CCriticalSection m_captureMutex;
  static CCaptureEvent m_screenshotEvent;
  static CCaptureEvent m_captureEvent;
  static std::queue<jni::CJNIImage> m_captureQueue;

  static ANativeWindow* m_window;
  static CJNIAudioDeviceInfos m_audiodevices;

  static std::atomic<uint64_t> m_vsynctime;
  static CEvent m_vsyncEvent;
  float m_refreshrate = 0.0;

  static void CalculateGUIRatios();
  static CRect m_droid2guiRatio;

  static CRect m_surface_rect;
  static uint32_t m_playback_state;
};

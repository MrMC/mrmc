#pragma once
/*
 *      Copyright (C) 2015 Team XBMC
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

#include <androidjni/Activity.h>
#include <androidjni/Surface.h>
#include <androidjni/Intent.h>
#include <androidjni/AudioDeviceInfo.h>
#include <androidjni/Image.h>
#include <androidjni/Rect.h>

class CJNIMainActivity : public CJNIActivity
{
public:
  CJNIMainActivity(const jobject& clazz);
  ~CJNIMainActivity();

  static CJNIMainActivity* GetAppInstance() { return m_appInstance; }

  static void _onNewIntent(JNIEnv *env, jobject context, jobject intent);
  static void _onActivityResult(JNIEnv *env, jobject context, jint requestCode, jint resultCode, jobject resultData);
  static void _onVolumeChanged(JNIEnv *env, jobject context, jint volume);
  static void _doFrame(JNIEnv *env, jobject context, jlong frameTimeNanos);
//  static void _onAudioDeviceAdded(JNIEnv *env, jobject context, jobjectArray devices);
//  static void _onAudioDeviceRemoved(JNIEnv *env, jobject context, jobjectArray devices);
  static void _onCaptureAvailable(JNIEnv *env, jobject context, jobject image);
  static void _onScreenshotAvailable(JNIEnv *env, jobject context, jobject image);
  static void _onVisibleBehindCanceled(JNIEnv *env, jobject context);
  static void _onMultiWindowModeChanged(JNIEnv *env, jobject context, jboolean isInMultiWindowMode);
  static void _onPictureInPictureModeChanged(JNIEnv *env, jobject context, jboolean isInPictureInPictureMode);
  static void _onUserLeaveHint(JNIEnv *env, jobject context);

  static void _callNative(JNIEnv *env, jobject context, jlong funcAddr, jlong variantAddr);
  void runNativeOnUiThread(void (*callback)(CVariant *), CVariant *variant);
  void registerMediaButtonEventReceiver();
  void unregisterMediaButtonEventReceiver();
  void screenOn();
  void startCrashHandler();
  void uploadLog();

  void takeScreenshot();
  void startProjection();
  void startCapture(int width, int height);
  void stopCapture();

  void openAmazonStore();
  void openGooglePlayStore();
  void openYouTubeVideo(const std::string key);

private:
  static CJNIMainActivity *m_appInstance;

protected:
  virtual void onNewIntent(CJNIIntent intent)=0;
  virtual void onActivityResult(int requestCode, int resultCode, CJNIIntent resultData)=0;
  virtual void onCaptureAvailable(jni::CJNIImage image)=0;
  virtual void onScreenshotAvailable(jni::CJNIImage image)=0;
  virtual void onVolumeChanged(int volume)=0;
  virtual void doFrame(int64_t frameTimeNanos)=0;
//  virtual void onAudioDeviceAdded(CJNIAudioDeviceInfos devices)=0;
//  virtual void onAudioDeviceRemoved(CJNIAudioDeviceInfos devices)=0;
  virtual void onVisibleBehindCanceled() = 0;
  virtual void onMultiWindowModeChanged(bool isInMultiWindowMode) = 0;
  virtual void onPictureInPictureModeChanged(bool isInPictureInPictureMode) = 0;
  virtual void onUserLeaveHint() = 0;
};

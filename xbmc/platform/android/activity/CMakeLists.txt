cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

include_directories ( SYSTEM ${ANDROID_NDK}/sources/android/native_app_glue ${ANDROID_NDK}/sources/android/cpufeatures )

set (my_SOURCES
  android_main.cpp
  android_onload.cpp
  AndroidFeatures.cpp
  AndroidKey.cpp
  AndroidTouch.cpp
  AndroidMouse.cpp
  AndroidJoyStick.cpp
  GraphicBuffer.cpp
  EventLoop.cpp
  XBMCApp.cpp
  JNIMainActivity.cpp
  JNIXBMCMainView.cpp
  JNIXBMCVideoView.cpp
  JNIXBMCAudioManagerOnAudioFocusChangeListener.cpp
  JNIXBMCSurfaceTextureOnFrameAvailableListener.cpp
  JNIXBMCMediaSession.cpp
  JNIXBMCBroadcastReceiver.cpp
  JNIXBMCFile.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  activity
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

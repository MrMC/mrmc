cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  GUIMediaWindow.cpp
  GUIWindowDebugInfo.cpp
  GUIWindowFileManager.cpp
  GUIWindowHome.cpp
  GUIWindowLoginScreen.cpp
  GUIWindowMediaSources.cpp
  GUIWindowPointer.cpp
  GUIWindowScreensaver.cpp
  GUIWindowScreensaverDim.cpp
  GUIWindowSplash.cpp
  GUIWindowStartup.cpp
  GUIWindowSystemInfo.cpp
  GUIWindowWeather.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  windows
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

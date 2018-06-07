cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  GUIViewStatePVR.cpp
  GUIWindowPVRBase.cpp
  GUIWindowPVRChannels.cpp
  GUIWindowPVRGuide.cpp
  GUIWindowPVRRecordings.cpp
  GUIWindowPVRSearch.cpp
  GUIWindowPVRTimers.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  pvrwindows
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

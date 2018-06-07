cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  GUIDialogPVRChannelManager.cpp
  GUIDialogPVRChannelsOSD.cpp
  GUIDialogPVRGroupManager.cpp
  GUIDialogPVRGuideInfo.cpp
  GUIDialogPVRGuideOSD.cpp
  GUIDialogPVRGuideSearch.cpp
  GUIDialogPVRRadioRDSInfo.cpp
  GUIDialogPVRRecordingInfo.cpp
  GUIDialogPVRTimerSettings.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  pvrdialogs
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  AdvancedSettings.cpp
  DiscSettings.cpp
  DisplaySettings.cpp
  MediaSettings.cpp
  MediaSourceSettings.cpp
  SettingAddon.cpp
  SettingConditions.cpp
  SettingControl.cpp
  SettingCreator.cpp
  SettingPath.cpp
  Settings.cpp
  SettingUtils.cpp
  SkinSettings.cpp
  VideoSettings.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  settings
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

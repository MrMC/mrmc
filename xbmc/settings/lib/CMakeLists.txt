cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  ISetting.cpp
  ISettingControl.cpp
  Setting.cpp
  SettingCategoryAccess.cpp
  SettingConditions.cpp
  SettingDependency.cpp
  SettingSection.cpp
  SettingsManager.cpp
  SettingRequirement.cpp
  SettingUpdate.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  settings_lib
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

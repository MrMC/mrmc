cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  PVRGUIInfo.cpp
  PVRManager.cpp
  PVRDatabase.cpp
  PVRActionListener.cpp
  PVRSettings.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  pvr
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

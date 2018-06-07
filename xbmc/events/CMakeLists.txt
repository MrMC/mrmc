cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  AddonEvent.cpp
  AddonManagementEvent.cpp
  BaseEvent.cpp
  EventLog.cpp
  MediaLibraryEvent.cpp
  windows/GUIViewStateEventLog.cpp
  windows/GUIWindowEventLog.cpp
  )

file(GLOB_RECURSE my_HEADERS *.h)

add_library(
  events
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  WinSystem.cpp
  WinEvents.cpp

  WinEventsSDL.cpp
  WinEventsLinux.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  windowing
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

include_directories( ${rtdir}/xbmc/addons/include ${rtdir}/xbmc/addons/library.xbmc.addon )

set (my_SOURCES
  libXBMCcodec.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  XBMCcodec
  SHARED

  ${my_SOURCES}
  ${my_HEADERS}
  )

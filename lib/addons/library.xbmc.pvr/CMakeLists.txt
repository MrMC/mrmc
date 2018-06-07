cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

include_directories( ${rtdir}/xbmc/addons/include ${rtdir}/xbmc/addons/library.xbmc.addon ${rtdir}/xbmc/cores/dvdplayer/DVDDemuxers )

set (my_SOURCES
  libXBMCpvr.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  XBMCpvr
  SHARED

  ${my_SOURCES}
  ${my_HEADERS}
  )


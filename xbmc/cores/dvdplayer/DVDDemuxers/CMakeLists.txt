cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  DVDDemux.cpp
  DVDDemuxBXA.cpp
  DVDDemuxCDDA.cpp
  DVDDemuxFFmpeg.cpp
  DVDDemuxPVRClient.cpp
  DVDDemuxShoutcast.cpp
  DVDDemuxUtils.cpp
  DVDDemuxVobsub.cpp
  DVDDemuxCC.cpp
  DVDFactoryDemuxer.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  dvddemuxers
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

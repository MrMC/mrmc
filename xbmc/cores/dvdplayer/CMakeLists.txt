cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

add_compile_options( -D__STDC_FORMAT_MACROS )

set (my_SOURCES
  DVDAudio.cpp
  DVDClock.cpp
  DVDDemuxSPU.cpp
  DVDFileInfo.cpp
  DVDMessage.cpp
  DVDMessageQueue.cpp
  DVDOverlayContainer.cpp
  DVDOverlayRenderer.cpp
  DVDPlayer.cpp
  DVDPlayerAudio.cpp
  DVDPlayerRadioRDS.cpp
  DVDPlayerSubtitle.cpp
  DVDPlayerTeletext.cpp
  DVDPlayerVideo.cpp
  DVDStreamInfo.cpp
  DVDTSCorrection.cpp
  Edl.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  dvdplayer
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )


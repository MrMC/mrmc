cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  DVDOverlayCodec.cpp
  DVDOverlayCodecFFmpeg.cpp
  DVDOverlayCodecSSA.cpp
  DVDOverlayCodecText.cpp
  DVDOverlayCodecTX3G.cpp
  contrib/cc_decoder.c
  contrib/cc_decoder708.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  overlaycodecs
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

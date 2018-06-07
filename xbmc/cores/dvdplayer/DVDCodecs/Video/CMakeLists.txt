cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  DVDVideoCodec.cpp
  DVDVideoCodecFFmpeg.cpp
  DVDVideoCodecLibMpeg2.cpp
  DVDVideoPPFFmpeg.cpp
  DVDVideoCodecAndroidMediaCodec.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  videocodecs
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

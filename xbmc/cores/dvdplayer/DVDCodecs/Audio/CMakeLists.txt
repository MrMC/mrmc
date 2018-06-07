cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

add_compile_options( -DHAVE_MMX )

set (my_SOURCES
  DVDAudioCodecFFmpeg.cpp
  DVDAudioCodecPassthrough.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  audiocodecs
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

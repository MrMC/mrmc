cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  CDDARipJob.cpp
  CDDARipper.cpp
  Encoder.cpp
  EncoderFFmpeg.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  cdrip
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

include_directories ( SYSTEM ${ANDROID_NDK}/sources/android/native_app_glue ${ANDROID_NDK}/sources/android/cpufeatures )

set (my_SOURCES
  XBMCService.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  android_service
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

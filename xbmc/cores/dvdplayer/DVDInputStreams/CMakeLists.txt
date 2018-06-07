cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

add_compile_options( -D__STDC_FORMAT_MACROS -DENABLE_DVDINPUTSTREAM_STACK )

set (my_SOURCES
  DVDFactoryInputStream.cpp
  DVDInputStream.cpp
  DVDInputStreamBluray.cpp
  DVDInputStreamFFmpeg.cpp
  DVDInputStreamFile.cpp
  DVDInputStreamHttp.cpp
  DVDInputStreamMemory.cpp
  DVDInputStreamNavigator.cpp
  DVDInputStreamRTMP.cpp
  DVDInputStreamPVRManager.cpp
  DVDInputStreamStack.cpp
  DVDStateSerializer.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  dvdinputstreams
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

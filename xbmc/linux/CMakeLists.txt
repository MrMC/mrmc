cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  ConvUtils.cpp
  DBusUtil.cpp
  DBusMessage.cpp
  DBusReserve.cpp
  FDEventMonitor.cpp
  LinuxResourceCounter.cpp
  LinuxTimezone.cpp
  PosixMountProvider.cpp
  XFileUtils.cpp
  XHandle.cpp
  XMemUtils.cpp
  XTimeUtils.cpp
  RBP.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  linux
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

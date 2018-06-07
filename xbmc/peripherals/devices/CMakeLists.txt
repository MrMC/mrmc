cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  Peripheral.cpp
  PeripheralBluetooth.cpp
  PeripheralDisk.cpp
  PeripheralHID.cpp
  PeripheralImon.cpp
  PeripheralNIC.cpp
  PeripheralNyxboard.cpp
  PeripheralTuner.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  peripheral-devices
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

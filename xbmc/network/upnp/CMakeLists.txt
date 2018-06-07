cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

include_directories (
  ${rtdir}/lib/libUPnP
  ${rtdir}/lib/libUPnP/Platinum/Source/Core
  ${rtdir}/lib/libUPnP/Platinum/Source/Platinum
  ${rtdir}/lib/libUPnP/Platinum/Source/Devices/MediaConnect
  ${rtdir}/lib/libUPnP/Platinum/Source/Devices/MediaRenderer
  ${rtdir}/lib/libUPnP/Platinum/Source/Devices/MediaServer
  ${rtdir}/lib/libUPnP/Platinum/Source/Extras
  ${rtdir}/lib/libUPnP/Neptune/Source/System/Posix
  ${rtdir}/lib/libUPnP/Neptune/Source/Core
  )

set (my_SOURCES
  UPnP.cpp
  UPnPInternal.cpp
  UPnPPlayer.cpp
  UPnPRenderer.cpp
  UPnPServer.cpp
  UPnPSettings.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  upnp
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  cddb.cpp
  DNSNameCache.cpp
  EventClient.cpp
  EventPacket.cpp
  EventServer.cpp
  GUIDialogAccessPoints.cpp
  GUIDialogNetworkSetup.cpp
  Network.cpp
  NetworkServices.cpp
  Socket.cpp
  TCPClient.cpp
  TCPServer.cpp
  UdpClient.cpp
  WakeOnAccess.cpp
  WebServer.cpp
  ZeroconfBrowser.cpp
  Zeroconf.cpp

  AirPlayServer.cpp
  AirTunesServer.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  network
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

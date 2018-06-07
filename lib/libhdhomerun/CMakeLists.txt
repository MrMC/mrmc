cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  hdhomerun_channels.c
  hdhomerun_channelscan.c
  hdhomerun_control.c
  hdhomerun_debug.c
  hdhomerun_device.c
  hdhomerun_dhcp.c
  hdhomerun_discover.c
  hdhomerun_pkt.c
  hdhomerun_video.c
  )

file(GLOB my_HEADERS *.h)

add_library(
  hdhomerun
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  ServicesManager.cpp
  emby/EmbyUtils.cpp
  emby/EmbyClient.cpp
  emby/EmbyClientSync.cpp
  emby/EmbyViewCache.cpp
  emby/EmbyServices.cpp
  plex/PlexUtils.cpp
  plex/PlexClient.cpp
  plex/PlexClientSync.cpp
  plex/PlexServices.cpp
  trakt/TraktServices.cpp
  lighteffects/LightEffectClient.cpp
  lighteffects/LightEffectServices.cpp
  )

file(GLOB_RECURSE my_HEADERS *.h)

add_library(
  services
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

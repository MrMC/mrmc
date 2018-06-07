cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  PlayListB4S.cpp
  PlayList.cpp
  PlayListFactory.cpp
  PlayListM3U.cpp
  PlayListPLS.cpp
  PlayListURL.cpp
  PlayListWPL.cpp
  PlayListXML.cpp
  SmartPlayList.cpp
  SmartPlaylistFileItemListModifier.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  playlists
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

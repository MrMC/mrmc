cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  Album.cpp
  Artist.cpp
  GUIViewStateMusic.cpp
  MusicDatabase.cpp
  MusicDbUrl.cpp
  MusicInfoLoader.cpp
  MusicThumbLoader.cpp
  Song.cpp
  CueInfoLoader.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  music
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

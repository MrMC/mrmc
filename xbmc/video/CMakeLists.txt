cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  Bookmark.cpp
  GUIViewStateVideo.cpp
  PlayerController.cpp
  Teletext.cpp
  VideoDatabase.cpp
  VideoDbUrl.cpp
  VideoInfoDownloader.cpp
  VideoInfoScanner.cpp
  VideoInfoTag.cpp
  VideoLibraryQueue.cpp
  VideoReferenceClock.cpp
  VideoThumbLoader.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  video
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

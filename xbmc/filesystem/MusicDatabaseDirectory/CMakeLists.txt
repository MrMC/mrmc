cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  DirectoryNode.cpp
  DirectoryNodeAlbum.cpp
  DirectoryNodeAlbumCompilations.cpp
  DirectoryNodeAlbumCompilationsSongs.cpp
  DirectoryNodeAlbumRecentlyAdded.cpp
  DirectoryNodeAlbumRecentlyAddedSong.cpp
  DirectoryNodeAlbumRecentlyPlayed.cpp
  DirectoryNodeAlbumRecentlyPlayedSong.cpp
  DirectoryNodeAlbumTop100.cpp
  DirectoryNodeAlbumTop100Song.cpp
  DirectoryNodeArtist.cpp
  DirectoryNodeGrouped.cpp
  DirectoryNodeOverview.cpp
  DirectoryNodeRoot.cpp
  DirectoryNodeSingles.cpp
  DirectoryNodeSong.cpp
  DirectoryNodeSongTop100.cpp
  DirectoryNodeTop100.cpp
  DirectoryNodeYearAlbum.cpp
  DirectoryNodeYearSong.cpp
  QueryParams.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  musicdatabasedirectory
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

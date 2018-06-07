cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  DirectoryNode.cpp
  DirectoryNodeEpisodes.cpp
  DirectoryNodeGrouped.cpp
  DirectoryNodeInProgressTvShows.cpp
  DirectoryNodeInProgressMovies.cpp
  DirectoryNodeMoviesOverview.cpp
  DirectoryNodeMusicVideosOverview.cpp
  DirectoryNodeOverview.cpp
  DirectoryNodeRecentlyAddedEpisodes.cpp
  DirectoryNodeRecentlyAddedMovies.cpp
  DirectoryNodeRecentlyAddedMusicVideos.cpp
  DirectoryNodeRoot.cpp
  DirectoryNodeSeasons.cpp
  DirectoryNodeTitleMovies.cpp
  DirectoryNodeTitleMusicVideos.cpp
  DirectoryNodeTitleTvShows.cpp
  DirectoryNodeTvShowsOverview.cpp
  QueryParams.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  videodatabasedirectory
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

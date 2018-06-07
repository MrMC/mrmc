cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  VideoLibraryCleaningJob.cpp
  VideoLibraryJob.cpp
  VideoLibraryMarkWatchedJob.cpp
  VideoLibraryProgressJob.cpp
  VideoLibraryRefreshingJob.cpp
  VideoLibraryScanningJob.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  video-jobs
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

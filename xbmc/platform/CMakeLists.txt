cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  MCRuntimeLibStartupLogger.cpp
  MCRuntimeLib.cpp
  MCRuntimeLibContext.cpp

  posix/main.cpp

  )

file(GLOB my_HEADERS *.h)

add_library(
  main
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

add_compile_options( -DAPI_DEBUG )

set (my_SOURCES
  DllLoaderContainer.cpp
  LibraryLoader.cpp
  SoLoader.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  dllloader
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

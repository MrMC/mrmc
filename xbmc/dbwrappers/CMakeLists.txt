cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  Database.cpp
  DatabaseQuery.cpp
  dataset.cpp
  mysqldataset.cpp
  qry_dat.cpp
  sqlitedataset.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  dbwrappers
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

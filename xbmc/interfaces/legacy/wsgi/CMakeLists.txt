cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

add_compile_options( -DSWIGRUNTIME_DEBUG -DSTATIC_LINKED )

set (my_SOURCES
  WsgiErrorStream.cpp
  WsgiInputStream.cpp
  WsgiResponse.cpp
  WsgiResponseBody.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  legacy-wsgi
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  HTTPFileHandler.cpp
  HTTPImageHandler.cpp
  HTTPImageTransformationHandler.cpp
  HTTPJsonRpcHandler.cpp
  HTTPVfsHandler.cpp
  HTTPWebinterfaceAddonsHandler.cpp
  HTTPWebinterfaceHandler.cpp
  IHTTPRequestHandler.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  httprequesthandlers
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

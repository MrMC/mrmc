cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  HueServices.cpp
  HueClient.cpp
  HueUtils.cpp
  providers/HueProviderRenderCapture.cpp
  providers/HueProviderAndroidProjection.cpp
  )

file(GLOB_RECURSE my_HEADERS *.h)

add_library(
  services_hue
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

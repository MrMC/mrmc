cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  AddonBuiltins.cpp
  ApplicationBuiltins.cpp
  Builtins.cpp
  CECBuiltins.cpp
  GUIBuiltins.cpp
  GUIContainerBuiltins.cpp
  GUIControlBuiltins.cpp
  LibraryBuiltins.cpp
  OpticalBuiltins.cpp
  PictureBuiltins.cpp
  PlayerBuiltins.cpp
  ProfileBuiltins.cpp
  PVRBuiltins.cpp
  SkinBuiltins.cpp
  SystemBuiltins.cpp
  WeatherBuiltins.cpp

  AndroidBuiltins.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  builtins
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  AddonsOperations.cpp
  ApplicationOperations.cpp
  AudioLibrary.cpp
  FavouritesOperations.cpp
  FileItemHandler.cpp
  FileOperations.cpp
  GUIOperations.cpp
  InputOperations.cpp
  JSONRPC.cpp
  JSONServiceDescription.cpp
  PlayerOperations.cpp
  PlaylistOperations.cpp
  ProfilesOperations.cpp
  PVROperations.cpp
  SettingsOperations.cpp
  SystemOperations.cpp
  TextureOperations.cpp
  VideoLibrary.cpp
  XBMCOperations.cpp
  CloudOperations.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  json-rpc
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

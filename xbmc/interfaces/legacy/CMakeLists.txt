cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

add_compile_options( -DSWIGRUNTIME_DEBUG -DSTATIC_LINKED )

set (my_SOURCES
  Addon.cpp
  AddonCallback.cpp
  AddonClass.cpp
  AddonUtils.cpp
  CallbackFunction.cpp
  CallbackHandler.cpp
  Control.cpp
  Dialog.cpp
  File.cpp
  InfoTagMusic.cpp
  InfoTagRadioRDS.cpp
  InfoTagVideo.cpp
  Keyboard.cpp
  LanguageHook.cpp
  ListItem.cpp
  ModuleXbmc.cpp
  ModuleXbmcgui.cpp
  ModuleXbmcplugin.cpp
  ModuleXbmcvfs.cpp
  Monitor.cpp
  Player.cpp
  PlayList.cpp
  String.cpp
  Window.cpp
  WindowDialog.cpp
  WindowDialogMixin.cpp
  WindowXML.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  legacy
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  ButtonTranslator.cpp
  InertialScrollingHandler.cpp
  InputCodingTableBaiduPY.cpp
  InputCodingTableBasePY.cpp
  InputCodingTableFactory.cpp
  InputCodingTableKorean.cpp
  KeyboardLayout.cpp
  KeyboardLayoutManager.cpp
  KeyboardStat.cpp
  MouseStat.cpp
  SDLJoystick.cpp
  XBMC_keytable.cpp
  InputManager.cpp
  Key.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  input
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

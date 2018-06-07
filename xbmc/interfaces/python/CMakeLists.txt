cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

include_directories ( ${DEPENDS_DIR}/include/python2.7 )

set (my_SOURCES
  AddonPythonInvoker.cpp
  CallbackHandler.cpp
  ContextItemAddonInvoker.cpp
  LanguageHook.cpp
  PythonInvoker.cpp
  XBPython.cpp
  swig.cpp
  PyContext.cpp

  ${rtdir}/xbmc/interfaces/python/generated/AddonModuleXbmc.cpp
  ${rtdir}/xbmc/interfaces/python/generated/AddonModuleXbmcgui.cpp
  ${rtdir}/xbmc/interfaces/python/generated/AddonModuleXbmcplugin.cpp
  ${rtdir}/xbmc/interfaces/python/generated/AddonModuleXbmcaddon.cpp
  ${rtdir}/xbmc/interfaces/python/generated/AddonModuleXbmcvfs.cpp
  ${rtdir}/xbmc/interfaces/python/generated/AddonModuleXbmcwsgi.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  python_binding
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

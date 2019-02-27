cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  BaseRenderer.cpp
  OverlayRenderer.cpp
  OverlayRendererUtil.cpp
  OverlayRendererGUI.cpp
  RenderCapture.cpp
  RenderManager.cpp
  RenderFlags.cpp
  LinuxRendererGLES.cpp
  OverlayRendererGL.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  videorenderer
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

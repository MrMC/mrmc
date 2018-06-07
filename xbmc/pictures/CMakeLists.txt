cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  GUIDialogPictureInfo.cpp
  GUIViewStatePictures.cpp
  GUIWindowPictures.cpp
  GUIWindowSlideShow.cpp
  Picture.cpp
  PictureInfoLoader.cpp
  PictureInfoTag.cpp
  PictureScalingAlgorithm.cpp
  PictureThumbLoader.cpp
  SlideShowPicture.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  pictures
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

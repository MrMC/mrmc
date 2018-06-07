cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  GUIDialogBoxBase.cpp
  GUIDialogBusy.cpp
  GUIDialogButtonMenu.cpp
  GUIDialogCache.cpp
  GUIDialogContextMenu.cpp
  GUIDialogExtendedProgressBar.cpp
  GUIDialogFavourites.cpp
  GUIDialogFileBrowser.cpp
  GUIDialogGamepad.cpp
  GUIDialogKaiToast.cpp
  GUIDialogKeyboardGeneric.cpp
  GUIDialogMediaFilter.cpp
  GUIDialogMediaSource.cpp
  GUIDialogMuteBug.cpp
  GUIDialogNumeric.cpp
  GUIDialogOK.cpp
  GUIDialogPlayEject.cpp
  GUIDialogPlayerControls.cpp
  GUIDialogProgress.cpp
  GUIDialogSeekBar.cpp
  GUIDialogSelect.cpp
  GUIDialogSimpleMenu.cpp
  GUIDialogSlider.cpp
  GUIDialogSmartPlaylistEditor.cpp
  GUIDialogSmartPlaylistRule.cpp
  GUIDialogSubMenu.cpp
  GUIDialogTextViewer.cpp
  GUIDialogVolumeBar.cpp
  GUIDialogYesNo.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  dialogs
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

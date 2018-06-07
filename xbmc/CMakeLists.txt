cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  ApplicationPlayer.cpp
  AppParamParser.cpp
  Autorun.cpp
  AutoSwitch.cpp
  BackgroundInfoLoader.cpp
  ContextMenuItem.cpp
  ContextMenuManager.cpp
  ClientPrivateInfo.cpp
  CompileInfo.cpp
  CueDocument.cpp
  DatabaseManager.cpp
  DbUrl.cpp
  DynamicDll.cpp
  FileItem.cpp
  FileItemListModification.cpp
  GUIInfoManager.cpp
  GUILargeTextureManager.cpp
  GUIPassword.cpp
  LangInfo.cpp
  MediaSource.cpp
  NfoFile.cpp
  PasswordManager.cpp
  PlayListPlayer.cpp
  PartyModeManager.cpp
  SectionLoader.cpp
  SystemGlobals.cpp
  TextureCache.cpp
  TextureCacheJob.cpp
  TextureDatabase.cpp
  ThumbLoader.cpp
  ThumbnailCache.cpp
  URL.cpp
  Util.cpp
  XBApplicationEx.cpp
  XBDateTime.cpp
)

file(GLOB my_HEADERS *.h)

add_library(
        xbmc
        STATIC

        ${my_SOURCES}
        ${my_HEADERS}
        )

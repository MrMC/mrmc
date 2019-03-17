cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  Addon.cpp
  AddonCallbacks.cpp
  AddonCallbacksAddon.cpp
  AddonCallbacksCodec.cpp
  AddonCallbacksGUI.cpp
  AddonCallbacksPVR.cpp
  AddonDatabase.cpp
  AddonInstaller.cpp
  AddonManager.cpp
  AddonStatusHandler.cpp
  AddonVersion.cpp
  AudioEncoder.cpp
  AudioDecoder.cpp
  ContextMenuAddon.cpp
  GUIDialogAddonInfo.cpp
  GUIDialogAddonSettings.cpp
  GUIViewStateAddonBrowser.cpp
  GUIWindowAddonBrowser.cpp
  ImageResource.cpp
  LanguageResource.cpp
  PluginSource.cpp
  Repository.cpp
  RepositoryUpdater.cpp
  Scraper.cpp
  ScreenSaver.cpp
  Service.cpp
  Skin.cpp
  UISoundsResource.cpp
  Visualisation.cpp
  Webinterface.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  addons
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

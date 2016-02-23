SRCS  = Addon.cpp
SRCS += AddonCallbacks.cpp
SRCS += AddonCallbacksAddon.cpp
SRCS += AddonCallbacksAudioDSP.cpp
SRCS += AddonCallbacksAudioEngine.cpp
SRCS += AddonCallbacksCodec.cpp
SRCS += AddonCallbacksGUI.cpp
SRCS += AddonCallbacksPVR.cpp
SRCS += AddonDatabase.cpp
SRCS += AddonInstaller.cpp
SRCS += AddonManager.cpp
SRCS += AddonStatusHandler.cpp
SRCS += AddonVersion.cpp
SRCS += AudioEncoder.cpp
SRCS += AudioDecoder.cpp
SRCS += ContextMenuAddon.cpp
SRCS += GUIDialogAddonInfo.cpp
SRCS += GUIDialogAddonSettings.cpp
SRCS += GUIViewStateAddonBrowser.cpp
SRCS += GUIWindowAddonBrowser.cpp
SRCS += ImageResource.cpp
SRCS += LanguageResource.cpp
SRCS += PluginSource.cpp
SRCS += Repository.cpp
SRCS += RepositoryUpdater.cpp
SRCS += Scraper.cpp
SRCS += ScreenSaver.cpp
SRCS += Service.cpp
SRCS += Skin.cpp
SRCS += UISoundsResource.cpp
SRCS += Webinterface.cpp

LIB   = addons.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))


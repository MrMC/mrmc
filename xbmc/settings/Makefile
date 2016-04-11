SRCS  = AdvancedSettings.cpp
SRCS += AudioDSPSettings.cpp
SRCS += DisplaySettings.cpp
SRCS += MediaSettings.cpp
SRCS += MediaSourceSettings.cpp
SRCS += SettingAddon.cpp
SRCS += SettingConditions.cpp
SRCS += SettingControl.cpp
SRCS += SettingCreator.cpp
SRCS += SettingPath.cpp
SRCS += Settings.cpp
SRCS += SettingUtils.cpp
SRCS += SkinSettings.cpp
SRCS += VideoSettings.cpp

LIB   = settings.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

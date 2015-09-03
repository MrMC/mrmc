SRCS  = ISetting.cpp
SRCS += ISettingControl.cpp
SRCS += Setting.cpp
SRCS += SettingCategoryAccess.cpp
SRCS += SettingConditions.cpp
SRCS += SettingDependency.cpp
SRCS += SettingSection.cpp
SRCS += SettingsManager.cpp
SRCS += SettingRequirement.cpp
SRCS += SettingUpdate.cpp

LIB   = settings_lib.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

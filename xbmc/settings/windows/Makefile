SRCS  = GUIControlSettings.cpp
SRCS += GUIWindowSettings.cpp
SRCS += GUIWindowSettingsCategory.cpp
SRCS += GUIWindowSettingsScreenCalibration.cpp
SRCS += GUIWindowTestPattern.cpp

LIB   = settings_windows.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

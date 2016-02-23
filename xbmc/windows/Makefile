SRCS  = GUIMediaWindow.cpp
SRCS += GUIWindowDebugInfo.cpp
SRCS += GUIWindowFileManager.cpp
SRCS += GUIWindowHome.cpp
SRCS += GUIWindowLoginScreen.cpp
SRCS += GUIWindowPointer.cpp
SRCS += GUIWindowScreensaver.cpp
SRCS += GUIWindowScreensaverDim.cpp
SRCS += GUIWindowSplash.cpp
SRCS += GUIWindowStartup.cpp
SRCS += GUIWindowSystemInfo.cpp
SRCS += GUIWindowWeather.cpp

LIB   = windows.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

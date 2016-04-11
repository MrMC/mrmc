SRCS  = VideoSyncGLX.cpp
SRCS += VideoSyncOsx.cpp
SRCS += VideoSyncIos.cpp
SRCS += VideoSyncDRM.cpp
SRCS += VideoSyncPi.cpp
SRCS += VideoSyncAndroid.cpp

LIB   = videosync.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
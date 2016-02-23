SRCS  = PVRChannel.cpp
SRCS += PVRChannelGroup.cpp
SRCS += PVRChannelGroupInternal.cpp
SRCS += PVRChannelGroups.cpp
SRCS += PVRChannelGroupsContainer.cpp
SRCS += PVRRadioRDSInfoTag.cpp

LIB   = pvrchannels.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

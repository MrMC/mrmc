SRCS  = MusicInfoTag.cpp
SRCS += MusicInfoTagLoaderCDDA.cpp
SRCS += MusicInfoTagLoaderDatabase.cpp
SRCS += MusicInfoTagLoaderFactory.cpp
SRCS += MusicInfoTagLoaderFFmpeg.cpp
SRCS += MusicInfoTagLoaderShn.cpp
SRCS += TagLoaderTagLib.cpp
SRCS += TagLibVFSStream.cpp
SRCS += ReplayGain.cpp

LIB   = musictags.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

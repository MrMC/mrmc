CXXFLAGS += -D__STDC_FORMAT_MACROS

SRCS  = DVDFactoryInputStream.cpp
SRCS += DVDInputStream.cpp
SRCS += DVDInputStreamFFmpeg.cpp
SRCS += DVDInputStreamFile.cpp
SRCS += DVDInputStreamHttp.cpp
SRCS += DVDInputStreamMemory.cpp
SRCS += DVDInputStreamRTMP.cpp
SRCS += DVDInputStreamPVRManager.cpp

LIB   = dvdinputstreams.a

include ../../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

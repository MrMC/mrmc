SRCS  = DPMSSupport.cpp
SRCS += PowerManager.cpp
SRCS += IPowerSyscall.cpp

LIB   = powermanagement.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

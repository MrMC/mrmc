SRCS  = ConsoleDeviceKitPowerSyscall.cpp
SRCS += ConsoleUPowerSyscall.cpp
SRCS += UPowerSyscall.cpp
SRCS += LogindUPowerSyscall.cpp

LIB   = powermanagement_linux.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

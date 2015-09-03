.SUFFIXES : .m .mm

SRCS  = WinEventsOSX.mm
SRCS += WinSystemOSX.mm
SRCS += WinSystemOSXGL.mm
     
LIB   = windowing_osx.a

include ../../../Makefile.include
-include $(patsubst %.mm,%.P,$(SRCS))

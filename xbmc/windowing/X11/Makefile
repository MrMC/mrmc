SRCS  = WinSystemX11.cpp
SRCS += WinSystemX11GL.cpp
SRCS += WinSystemX11GLES.cpp
SRCS += XRandR.cpp
     
LIB   = windowing_X11.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

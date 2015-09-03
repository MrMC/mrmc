SRCS  = YUV2RGBShader.cpp
SRCS += VideoFilterShader.cpp
SRCS += ConvolutionKernels.cpp

LIB   = videoshaders.a

include ../../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

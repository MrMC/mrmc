SRCS  = AutorunMediaJob.cpp
SRCS += cdioSupport.cpp
SRCS += DetectDVDType.cpp
SRCS += IoSupport.cpp
SRCS += MediaManager.cpp
     
LIB   = storage.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

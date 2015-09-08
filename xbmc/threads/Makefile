SRCS  = Atomics.cpp
SRCS += Event.cpp
SRCS += Thread.cpp
SRCS += Timer.cpp
SRCS += SystemClock.cpp
SRCS += platform/Implementation.cpp

LIB   = threads.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

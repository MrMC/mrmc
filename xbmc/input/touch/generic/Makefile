SRCS  = GenericTouchActionHandler.cpp
SRCS += GenericTouchInputHandler.cpp
SRCS += GenericTouchPinchDetector.cpp
SRCS += GenericTouchRotateDetector.cpp
SRCS += GenericTouchSwipeDetector.cpp

LIB   = input_touch_generic.a

include ../../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

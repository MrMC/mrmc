SRCS  = AddonEvent.cpp
SRCS += AddonManagementEvent.cpp
SRCS += BaseEvent.cpp
SRCS += EventLog.cpp
SRCS += MediaLibraryEvent.cpp
SRCS += windows/GUIViewStateEventLog.cpp
SRCS += windows/GUIWindowEventLog.cpp

LIB   = events.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))


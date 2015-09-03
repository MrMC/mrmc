SRCS  = EpgInfoTag.cpp
SRCS += EpgSearchFilter.cpp
SRCS += Epg.cpp
SRCS += EpgContainer.cpp
SRCS += EpgDatabase.cpp
SRCS += GUIEPGGridContainer.cpp

LIB   = epg.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

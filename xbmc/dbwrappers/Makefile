SRCS  = Database.cpp
SRCS += DatabaseQuery.cpp
SRCS += dataset.cpp
SRCS += mysqldataset.cpp
SRCS += qry_dat.cpp
SRCS += sqlitedataset.cpp

LIB   = dbwrappers.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

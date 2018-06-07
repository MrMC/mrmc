include /var/lib/cloud9/w1/mrmc/tools/depends/Makefile.include

SRCS       = XBMCService.cpp

LIB        = service.a

$(LIB): $(OBJS)

include /var/lib/cloud9/w1/mrmc/Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

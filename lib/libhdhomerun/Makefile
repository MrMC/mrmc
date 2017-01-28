SRCS = hdhomerun_channels.c
SRCS+= hdhomerun_channelscan.c
SRCS+= hdhomerun_control.c
SRCS+= hdhomerun_debug.c
SRCS+= hdhomerun_device.c
SRCS+= hdhomerun_dhcp.c
SRCS+= hdhomerun_discover.c
SRCS+= hdhomerun_pkt.c
SRCS+= hdhomerun_video.c

LIB = hdhomerun.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

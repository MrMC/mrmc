SRCS  = PlayListB4S.cpp
SRCS += PlayList.cpp
SRCS += PlayListFactory.cpp
SRCS += PlayListM3U.cpp
SRCS += PlayListPLS.cpp
SRCS += PlayListURL.cpp
SRCS += PlayListWPL.cpp
SRCS += PlayListXML.cpp
SRCS += SmartPlayList.cpp
SRCS += SmartPlaylistFileItemListModifier.cpp

LIB   = playlists.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

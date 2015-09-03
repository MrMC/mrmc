SRCS  = MusicAlbumInfo.cpp
SRCS += MusicArtistInfo.cpp
SRCS += MusicInfoScanner.cpp
SRCS += MusicInfoScraper.cpp

LIB   = musicscanner.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

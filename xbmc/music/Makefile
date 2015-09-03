SRCS  = Album.cpp
SRCS += Artist.cpp
SRCS += GUIViewStateMusic.cpp
SRCS += MusicDatabase.cpp
SRCS += MusicDbUrl.cpp
SRCS += MusicInfoLoader.cpp
SRCS += MusicThumbLoader.cpp
SRCS += Song.cpp
SRCS += CueInfoLoader.cpp

LIB   = music.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

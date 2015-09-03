SRCS  = DirectoryNode.cpp
SRCS += DirectoryNodeAlbum.cpp
SRCS += DirectoryNodeAlbumCompilations.cpp
SRCS += DirectoryNodeAlbumCompilationsSongs.cpp
SRCS += DirectoryNodeAlbumRecentlyAdded.cpp
SRCS += DirectoryNodeAlbumRecentlyAddedSong.cpp
SRCS += DirectoryNodeAlbumRecentlyPlayed.cpp
SRCS += DirectoryNodeAlbumRecentlyPlayedSong.cpp
SRCS += DirectoryNodeAlbumTop100.cpp
SRCS += DirectoryNodeAlbumTop100Song.cpp
SRCS += DirectoryNodeArtist.cpp
SRCS += DirectoryNodeGrouped.cpp
SRCS += DirectoryNodeOverview.cpp
SRCS += DirectoryNodeRoot.cpp
SRCS += DirectoryNodeSingles.cpp
SRCS += DirectoryNodeSong.cpp
SRCS += DirectoryNodeSongTop100.cpp
SRCS += DirectoryNodeTop100.cpp
SRCS += DirectoryNodeYearAlbum.cpp
SRCS += DirectoryNodeYearSong.cpp
SRCS += QueryParams.cpp

LIB   = musicdatabasedirectory.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

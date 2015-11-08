SRCS  = Bookmark.cpp
SRCS += GUIViewStateVideo.cpp
SRCS += PlayerController.cpp
SRCS += Teletext.cpp
SRCS += VideoDatabase.cpp
SRCS += VideoDbUrl.cpp
SRCS += VideoInfoDownloader.cpp
SRCS += VideoInfoScanner.cpp
SRCS += VideoInfoTag.cpp
SRCS += VideoLibraryQueue.cpp
SRCS += VideoReferenceClock.cpp
SRCS += VideoThumbLoader.cpp
     
LIB   = video.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

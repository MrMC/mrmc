SRCS  = VideoLibraryCleaningJob.cpp
SRCS += VideoLibraryJob.cpp
SRCS += VideoLibraryMarkWatchedJob.cpp
SRCS += VideoLibraryProgressJob.cpp
SRCS += VideoLibraryRefreshingJob.cpp
SRCS += VideoLibraryScanningJob.cpp
     
LIB   = video-jobs.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

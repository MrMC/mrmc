SRCS = src/dir_posix.c
SRCS+= src/file_posix.c
SRCS+= src/file.c
SRCS+= src/filesystem.c
SRCS+= src/highlight.c
SRCS+= src/libdvd.c
SRCS+= src/logging.c
SRCS+= src/navigation.c
SRCS+= src/read_cache.c
SRCS+= src/searching.c
SRCS+= src/settings.c
SRCS+= src/strutl.c
SRCS+= src/reader/bitreader.c
SRCS+= src/reader/dvd_input.c
SRCS+= src/reader/dvd_iso9660.c
SRCS+= src/reader/dvd_reader.c
SRCS+= src/reader/dvd_udf.c
SRCS+= src/reader/ifo_print.c
SRCS+= src/reader/ifo_read.c
SRCS+= src/reader/md5.c
SRCS+= src/reader/nav_print.c
SRCS+= src/reader/nav_read.c
SRCS+= src/vm/decoder.c
SRCS+= src/vm/getset.c
SRCS+= src/vm/play.c
SRCS+= src/vm/vm.c
SRCS+= src/vm/vmcmd.c
SRCS+= src/vm/vmget.c

LIB = libdvd.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

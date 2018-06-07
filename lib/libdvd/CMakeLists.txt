cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  src/dir_posix.c
  src/file_posix.c
  src/file.c
  src/filesystem.c
  src/highlight.c
  src/libdvd.c
  src/logging.c
  src/navigation.c
  src/read_cache.c
  src/searching.c
  src/settings.c
  src/strutl.c
  src/reader/bitreader.c
  src/reader/dvd_input.c
  src/reader/dvd_iso9660.c
  src/reader/dvd_reader.c
  src/reader/dvd_udf.c
  src/reader/ifo_print.c
  src/reader/ifo_read.c
  src/reader/md5.c
  src/reader/nav_print.c
  src/reader/nav_read.c
  src/vm/decoder.c
  src/vm/getset.c
  src/vm/play.c
  src/vm/vm.c
  src/vm/vmcmd.c
  src/vm/vmget.c
  )

file(GLOB my_HEADERS *.h)

add_library(
  libdvd
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

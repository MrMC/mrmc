cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

add_compile_options( -DRAR_SILENT )

set (my_SOURCES
  archive.cpp
  arcread.cpp
  cmddata.cpp
  consio.cpp
  crc.cpp
  crypt.cpp
  encname.cpp
  errhnd.cpp
  extinfo.cpp
  extract.cpp
  filcreat.cpp
  file.cpp
  filefn.cpp
  filestr.cpp
  find.cpp
  getbits.cpp
  global.cpp
  int64.cpp
  isnt.cpp
  log.cpp
  match.cpp
  options.cpp
  pathfn.cpp
  rar.cpp
  rarvm.cpp
  rawread.cpp
  rdwrfn.cpp
  recvol.cpp
  resource.cpp
  rijndael.cpp
  rs.cpp
  savepos.cpp
  scantree.cpp
  sha1.cpp
  strfn.cpp
  strlist.cpp
  system.cpp
  timefn.cpp
  ulinks.cpp
  unicode.cpp
  unpack.cpp
  volume.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  UnrarXLib
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

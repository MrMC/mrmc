cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  DVDFactorySubtitle.cpp
  DVDSubtitleLineCollection.cpp
  DVDSubtitleParserMicroDVD.cpp
  DVDSubtitleParserMPL2.cpp
  DVDSubtitleParserSami.cpp
  DVDSubtitleParserSSA.cpp
  DVDSubtitleParserSubrip.cpp
  DVDSubtitleParserVplayer.cpp
  DVDSubtitleTagMicroDVD.cpp
  DVDSubtitleTagSami.cpp
  DVDSubtitlesLibass.cpp
  DVDSubtitleStream.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  dvdsubtitles
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

include_directories( SYSTEM  ${DEPENDS_DIR}/include/libxml2 )

set (my_SOURCES
  ActorProtocol.cpp
  AlarmClock.cpp
  AliasShortcutUtils.cpp
  Archive.cpp
  AsyncFileCopy.cpp
  auto_buffer.cpp
  Base64.cpp
  Base64URL.cpp
  BitstreamConverter.cpp
  BitstreamReader.cpp
  BitstreamStats.cpp
  BitstreamWriter.cpp
  BooleanLogic.cpp
  CharsetConverter.cpp
  CharsetDetection.cpp
  CPUInfo.cpp
  Crc32.cpp
  CryptThreading.cpp
  DatabaseUtils.cpp
  DolbyFrameParser.cpp
  EndianSwap.cpp
  Environment.cpp
  Fanart.cpp
  exif/libexif.cpp
  exif/IptcParse.cpp
  exif/ExifParse.cpp
  exif/JpegParse.cpp
  FileOperationJob.cpp
  FileUtils.cpp
  fstrcmp.c
  GLUtils.cpp
  GroupUtils.cpp
  HTMLUtil.cpp
  HttpHeader.cpp
  HttpParser.cpp
  HttpRangeUtils.cpp
  HttpResponse.cpp
  InfoLoader.cpp
  JobManager.cpp
  JSONVariantParser.cpp
  JSONVariantWriter.cpp
  LabelFormatter.cpp
  LangCodeExpander.cpp
  LegacyPathTranslation.cpp
  LiteUtils.cpp
  Locale.cpp
  log.cpp
  md5.cpp
  MemoryBitstream.cpp
  Mime.cpp
  Observer.cpp
  posix/PosixInterfaceForCLog.cpp
  POUtils.cpp
  ProgressJob.cpp
  HomeShelfJob.cpp
  RegExp.cpp
  rfft.cpp
  RingBuffer.cpp
  RssManager.cpp
  RssReader.cpp
  SaveFileStateJob.cpp
  ScraperParser.cpp
  ScraperUrl.cpp
  Screenshot.cpp
  SeekHandler.cpp
  SortUtils.cpp
  Speed.cpp
  Splash.cpp
  Stopwatch.cpp
  StreamDetails.cpp
  StreamUtils.cpp
  StringUtils.cpp
  StringValidation.cpp
  subtitles/OpenSubtitlesSearch.cpp
  subtitles/PodnapisiSearch.cpp
  subtitles/SubtitleUtilities.cpp
  SysfsUtils.cpp
  SystemInfo.cpp
  Temperature.cpp
  TextSearch.cpp
  TimeUtils.cpp
  URIUtils.cpp
  UrlOptions.cpp
  Variant.cpp
  Vector.cpp
  Weather.cpp
  XBMCTinyXML.cpp
  XMLUtils.cpp
  Utf8Utils.cpp
  XSLTUtils.cpp
  )

file(GLOB my_HEADERS *.h)

add_library(
  utils
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

add_compile_options( -D__STDC_FORMAT_MACROS )

include_directories (
  ${rtdir}/lib/libUPnP
  ${rtdir}/lib/libUPnP/Platinum/Source/Core
  ${rtdir}/lib/libUPnP/Platinum/Source/Platinum
  ${rtdir}/lib/libUPnP/Platinum/Source/Devices/MediaConnect
  ${rtdir}/lib/libUPnP/Platinum/Source/Devices/MediaRenderer
  ${rtdir}/lib/libUPnP/Platinum/Source/Devices/MediaServer
  ${rtdir}/lib/libUPnP/Platinum/Source/Extras
  ${rtdir}/lib/libUPnP/Neptune/Source/System/Posix
  ${rtdir}/lib/libUPnP/Neptune/Source/Core
  )

set (my_SOURCES
  AddonsDirectory.cpp
  CacheStrategy.cpp
  CircularCache.cpp
  CDDADirectory.cpp
  CDDAFile.cpp
  CurlFile.cpp
  CloudDirectory.cpp
  CloudUtils.cpp
  DAVCommon.cpp
  DAVDirectory.cpp
  DAVFile.cpp
  Directory.cpp
  DirectoryCache.cpp
  DirectoryFactory.cpp
  DirectoryHistory.cpp
  DllLibCurl.cpp
  EmbyDirectory.cpp
  EventsDirectory.cpp
  FavouritesDirectory.cpp
  File.cpp
  FileCache.cpp
  FileDirectoryFactory.cpp
  FileFactory.cpp
  FileReaderFile.cpp
  FTPDirectory.cpp
  FTPParse.cpp
  HDHomeRunDirectory.cpp
  HDHomeRunFile.cpp
  HTTPDirectory.cpp
  HTTPFile.cpp
  IDirectory.cpp
  IFile.cpp
  ImageFile.cpp
  iso9660.cpp
  ISO9660Directory.cpp
  ISOFile.cpp
  LibraryDirectory.cpp
  MediaDirectory.cpp
  MemBufferCache.cpp
  MultiPathDirectory.cpp
  MultiPathFile.cpp
  MusicDatabaseDirectory.cpp
  MusicDatabaseFile.cpp
  MusicFileDirectory.cpp
  MusicSearchDirectory.cpp
  OverrideDirectory.cpp
  OverrideFile.cpp
  PlaylistDirectory.cpp
  PlaylistFileDirectory.cpp
  PipeFile.cpp
  PipesManager.cpp
  PluginDirectory.cpp
  PlexDirectory.cpp
  posix/PosixDirectory.cpp
  posix/PosixFile.cpp
  PVRFile.cpp
  PVRDirectory.cpp
  RarFile.cpp
  RarDirectory.cpp
  RarManager.cpp
  ResourceDirectory.cpp
  ResourceFile.cpp
  RSSDirectory.cpp
  SAPDirectory.cpp
  SAPFile.cpp
  SFTPDirectory.cpp
  SFTPFile.cpp
  ServicesDirectory.cpp
  ShoutcastFile.cpp
  SmartPlaylistDirectory.cpp
  SourcesDirectory.cpp
  SpecialProtocol.cpp
  SpecialProtocolDirectory.cpp
  SpecialProtocolFile.cpp
  StackDirectory.cpp
  udf25.cpp
  UDFDirectory.cpp
  UDFFile.cpp
  VideoDatabaseDirectory.cpp
  VideoDatabaseFile.cpp
  VirtualDirectory.cpp
  XbtDirectory.cpp
  XbtFile.cpp
  XbtManager.cpp
  ZeroconfDirectory.cpp
  ZipDirectory.cpp
  ZipFile.cpp
  ZipManager.cpp

  APKDirectory.cpp
  APKFile.cpp
  AndroidAppFile.cpp
  AndroidAppDirectory.cpp
  AndroidSettingFile.cpp
  AndroidSettingDirectory.cpp
  AndroidContentDirectory.cpp

  SMBFile.cpp
  SMBDirectory.cpp

  DSMDirectory.cpp
  DSMFile.cpp

  SMB2Directory.cpp
  SMB2File.cpp

  NFSFile.cpp
  NFSDirectory.cpp

  BlurayDirectory.cpp
  BlurayFile.cpp

  NptXbmcFile.cpp
  UPnPDirectory.cpp
  UPnPFile.cpp
  )

file(GLOB_RECURSE my_HEADERS *.h)

add_library(
  filesystem
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

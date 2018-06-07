cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

add_compile_options( -D__STDC_LIMIT_MACROS )

set (my_SOURCES
  AEFactory.cpp
  AESinkFactory.cpp
  AEResampleFactory.cpp

  Engines/ActiveAE/ActiveAE.cpp
  Engines/ActiveAE/ActiveAESink.cpp
  Engines/ActiveAE/ActiveAEStream.cpp
  Engines/ActiveAE/ActiveAESound.cpp
  Engines/ActiveAE/ActiveAEResampleFFMPEG.cpp
  Engines/ActiveAE/ActiveAEBuffer.cpp
  Engines/ActiveAE/ActiveAEFilter.cpp

  Sinks/AESinkNULL.cpp
  Sinks/AESinkAUDIOTRACK.cpp
  DSPAddons/ActiveAEDSP.cpp
  DSPAddons/ActiveAEDSPMode.cpp
  DSPAddons/ActiveAEDSPAddon.cpp
  DSPAddons/ActiveAEDSPDatabase.cpp
  DSPAddons/ActiveAEDSPProcess.cpp

  Utils/AEChannelInfo.cpp
  Utils/AEBuffer.cpp
  Utils/AEUtil.cpp
  Utils/AEStreamInfo.cpp
  Utils/AEPackIEC61937.cpp
  Utils/AEBitstreamPacker.cpp
  Utils/AEELDParser.cpp
  Utils/AEDeviceInfo.cpp
  Utils/AELimiter.cpp

  Encoders/AEEncoderFFmpeg.cpp
  )

file(GLOB_RECURSE  my_HEADERS *.h)

add_library(
  audioengine
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )

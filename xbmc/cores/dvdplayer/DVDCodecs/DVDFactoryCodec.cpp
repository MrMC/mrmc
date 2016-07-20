/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "system.h"
#include "utils/log.h"

#include "DVDFactoryCodec.h"
#include "Video/DVDVideoCodec.h"
#include "Audio/DVDAudioCodec.h"
#include "Overlay/DVDOverlayCodec.h"
#include "cores/dvdplayer/DVDCodecs/DVDCodecs.h"

#if defined(TARGET_DARWIN_OSX)
  #include "Video/DVDVideoCodecVDA.h"
#endif
#if defined(TARGET_DARWIN_IOS)
  #if !defined(TARGET_DARWIN_TVOS)
    #include "Video/DVDVideoCodecVideoToolBox.h"
	#include "utils/SystemInfo.h"
  #endif
  #include "Video/DVDVideoCodecAVFoundation.h"
#endif
#include "Video/DVDVideoCodecFFmpeg.h"
#include "Video/DVDVideoCodecOpenMax.h"
#include "Video/DVDVideoCodecLibMpeg2.h"
#if defined(HAS_IMXVPU)
  #include "Video/DVDVideoCodecIMX.h"
#endif
#include "Video/MMALCodec.h"
#include "Video/DVDVideoCodecStageFright.h"
#if defined(HAS_LIBAMCODEC)
  #include "utils/AMLUtils.h"
  #include "Video/DVDVideoCodecAmlogic.h"
#endif
#if defined(TARGET_ANDROID)
  #include "Video/DVDVideoCodecAndroidMediaCodec.h"
  #include "platform/android/activity/AndroidFeatures.h"
#endif
#include "Audio/DVDAudioCodecFFmpeg.h"
#include "Audio/DVDAudioCodecPassthrough.h"
#if defined(TARGET_DARWIN)
  #include "Audio/DVDAudioCodecAudioConverter.h"
#endif
#include "Overlay/DVDOverlayCodecSSA.h"
#include "Overlay/DVDOverlayCodecText.h"
#include "Overlay/DVDOverlayCodecTX3G.h"
#include "Overlay/DVDOverlayCodecFFmpeg.h"


#include "DVDStreamInfo.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/VideoSettings.h"
#include "utils/StringUtils.h"

CDVDVideoCodec* CDVDFactoryCodec::OpenCodec(CDVDVideoCodec* pCodec, CDVDStreamInfo &hints, CDVDCodecOptions &options )
{
  try
  {
    CLog::Log(LOGDEBUG, "FactoryCodec - Video: %s - Opening", pCodec->GetName());
    if( pCodec->Open( hints, options ) )
    {
      CLog::Log(LOGDEBUG, "FactoryCodec - Video: %s - Opened", pCodec->GetName());
      return pCodec;
    }

    CLog::Log(LOGDEBUG, "FactoryCodec - Video: %s - Failed", pCodec->GetName());
    pCodec->Dispose();
    delete pCodec;
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "FactoryCodec - Video: Failed with exception");
  }
  return nullptr;
}

CDVDAudioCodec* CDVDFactoryCodec::OpenCodec(CDVDAudioCodec* pCodec, CDVDStreamInfo &hints, CDVDCodecOptions &options )
{
  try
  {
    CLog::Log(LOGDEBUG, "FactoryCodec - Audio: %s - Opening", pCodec->GetName());
    if( pCodec->Open( hints, options ) )
    {
      CLog::Log(LOGDEBUG, "FactoryCodec - Audio: %s - Opened", pCodec->GetName());
      return pCodec;
    }

    CLog::Log(LOGDEBUG, "FactoryCodec - Audio: %s - Failed", pCodec->GetName());
    pCodec->Dispose();
    delete pCodec;
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "FactoryCodec - Audio: Failed with exception");
  }
  return nullptr;
}

CDVDOverlayCodec* CDVDFactoryCodec::OpenCodec(CDVDOverlayCodec* pCodec, CDVDStreamInfo &hints, CDVDCodecOptions &options )
{
  try
  {
    CLog::Log(LOGDEBUG, "FactoryCodec - Overlay: %s - Opening", pCodec->GetName());
    if( pCodec->Open( hints, options ) )
    {
      CLog::Log(LOGDEBUG, "FactoryCodec - Overlay: %s - Opened", pCodec->GetName());
      return pCodec;
    }

    CLog::Log(LOGDEBUG, "FactoryCodec - Overlay: %s - Failed", pCodec->GetName());
    pCodec->Dispose();
    delete pCodec;
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "FactoryCodec - Audio: Failed with exception");
  }
  return nullptr;
}


CDVDVideoCodec* CDVDFactoryCodec::CreateVideoCodec(CDVDStreamInfo &hint, const CRenderInfo &info)
{
  CDVDVideoCodec* pCodec = nullptr;
  CDVDCodecOptions options;

  if (info.formats.empty())
    options.m_formats.push_back(RENDER_FMT_YUV420P);
  else
    options.m_formats = info.formats;

  options.m_opaque_pointer = info.opaque_pointer;


  if ((hint.codec == AV_CODEC_ID_MPEG2VIDEO || hint.codec == AV_CODEC_ID_MPEG1VIDEO) && (hint.stills || hint.filename == "dvd"))
  {
     // If dvd is an mpeg2 and hint.stills
     if ( (pCodec = OpenCodec(new CDVDVideoCodecLibMpeg2(), hint, options)) ) return pCodec;
  }

#if defined(HAS_LIBAMCODEC)
  // amcodec can handle dvd playback.
  if (!hint.software && CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEAMCODEC))
  {
    switch(hint.codec)
    {
      case AV_CODEC_ID_MPEG4:
      case AV_CODEC_ID_MSMPEG4V2:
      case AV_CODEC_ID_MSMPEG4V3:
      case AV_CODEC_ID_MPEG1VIDEO:
      case AV_CODEC_ID_MPEG2VIDEO:
        // Avoid h/w decoder for SD; Those files might use features
        // not supported and can easily be soft-decoded
        if (hint.width <= 800)
          break;
      default:
        if ( (pCodec = OpenCodec(new CDVDVideoCodecAmlogic(), hint, options)) ) return pCodec;
    }
  }
#endif

#if defined(HAS_IMXVPU)
  if (!hint.software)
  {
    if ( (pCodec = OpenCodec(new CDVDVideoCodecIMX(), hint, options)) ) return pCodec;
  }
#endif

#if defined(TARGET_DARWIN_OSX)
  if (!hint.software && CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEVDA) && !g_advancedSettings.m_useFfmpegVda)
  {
    if (hint.codec == AV_CODEC_ID_H264 && !hint.ptsinvalid)
    {
      if ( (pCodec = OpenCodec(new CDVDVideoCodecVDA(), hint, options)) ) return pCodec;
    }
  }
#endif

#if defined(TARGET_DARWIN_IOS)
  if (!hint.software)
  {
    switch(hint.codec)
    {
      case AV_CODEC_ID_H264:
      case AV_CODEC_ID_MPEG4:
        if (hint.codec == AV_CODEC_ID_H264 && hint.ptsinvalid)
          break;
        #if !defined(TARGET_DARWIN_TVOS)
        if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEVIDEOTOOLBOX))
          if ( (pCodec = OpenCodec(new CDVDVideoCodecVideoToolBox(), hint, options)) ) return pCodec;
        #endif
        if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEAVF))
          if ( (pCodec = OpenCodec(new CDVDVideoCodecAVFoundation(), hint, options)) ) return pCodec;
        break;
      default:
        break;
    }
  }
#endif

#if defined(TARGET_ANDROID)
  // Only give priority to Surface in 4K
  if (!hint.software && hint.height > 1080 && CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEMEDIACODECSURFACE))
  {
    switch(hint.codec)
    {
      case AV_CODEC_ID_MPEG4:
      case AV_CODEC_ID_MSMPEG4V2:
      case AV_CODEC_ID_MSMPEG4V3:
        // Avoid h/w decoder for SD; Those files might use features
        // not supported and can easily be soft-decoded
        if (hint.width <= 800)
          break;
      default:
        CLog::Log(LOGINFO, "MediaCodec (Surface) Video Decoder...");
        if ( (pCodec = OpenCodec(new CDVDVideoCodecAndroidMediaCodec(true), hint, options)) ) return pCodec;
    }
  }
  if (!hint.software && CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEMEDIACODEC))
  {
    switch(hint.codec)
    {
      case AV_CODEC_ID_MPEG4:
      case AV_CODEC_ID_MSMPEG4V2:
      case AV_CODEC_ID_MSMPEG4V3:
        // Avoid h/w decoder for SD; Those files might use features
        // not supported and can easily be soft-decoded
        if (hint.width <= 800)
          break;
      default:
        CLog::Log(LOGINFO, "MediaCodec Video Decoder...");
        if ( (pCodec = OpenCodec(new CDVDVideoCodecAndroidMediaCodec(false), hint, options)) ) return pCodec;
    }
  }
  if (!hint.software && hint.height <= 1080 && CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEMEDIACODECSURFACE))
  {
    switch(hint.codec)
    {
      case AV_CODEC_ID_MPEG4:
      case AV_CODEC_ID_MSMPEG4V2:
      case AV_CODEC_ID_MSMPEG4V3:
        // Avoid h/w decoder for SD; Those files might use features
        // not supported and can easily be soft-decoded
        if (hint.width <= 800)
          break;
      default:
        CLog::Log(LOGINFO, "MediaCodec (Surface) Video Decoder...");
        if ( (pCodec = OpenCodec(new CDVDVideoCodecAndroidMediaCodec(true), hint, options)) ) return pCodec;
    }
  }
#endif

#if defined(HAVE_LIBOPENMAX)
    if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEOMX) && !hint.software )
    {
      if (hint.codec == AV_CODEC_ID_H264 || hint.codec == AV_CODEC_ID_MPEG2VIDEO || hint.codec == AV_CODEC_ID_VC1)
      {
        if ( (pCodec = OpenCodec(new CDVDVideoCodecOpenMax(), hint, options)) ) return pCodec;
      }
    }
#endif

#if defined(HAS_MMAL)
    if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USEMMAL) && !hint.software )
    {
      if (hint.codec == AV_CODEC_ID_H264 || hint.codec == AV_CODEC_ID_H263 || hint.codec == AV_CODEC_ID_MPEG4 ||
          hint.codec == AV_CODEC_ID_MPEG1VIDEO || hint.codec == AV_CODEC_ID_MPEG2VIDEO ||
          hint.codec == AV_CODEC_ID_VP6 || hint.codec == AV_CODEC_ID_VP6F || hint.codec == AV_CODEC_ID_VP6A || hint.codec == AV_CODEC_ID_VP8 ||
          hint.codec == AV_CODEC_ID_THEORA || hint.codec == AV_CODEC_ID_MJPEG || hint.codec == AV_CODEC_ID_MJPEGB || hint.codec == AV_CODEC_ID_VC1 || hint.codec == AV_CODEC_ID_WMV3)
      {
        if ( (pCodec = OpenCodec(new CMMALVideo(), hint, options)) ) return pCodec;
      }
    }
#endif

#if defined(HAS_LIBSTAGEFRIGHT)
    if (!hint.software && CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_USESTAGEFRIGHT))
    {
      switch(hint.codec)
      {
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_MSMPEG4V2:
        case AV_CODEC_ID_MSMPEG4V3:
          // Avoid h/w decoder for SD; Those files might use features
          // not supported and can easily be soft-decoded
          if (hint.width <= 800)
            break;
        default:
          if ( (pCodec = OpenCodec(new CDVDVideoCodecStageFright(), hint, options)) ) return pCodec;
      }
    }
#endif


  // try to decide if we want to try halfres decoding
#if !defined(TARGET_POSIX)
  float pixelrate = (float)hint.width*hint.height*hint.fpsrate/hint.fpsscale;
  if (pixelrate > 1400.0f*720.0f*30.0f)
  {
    CLog::Log(LOGINFO, "CDVDFactoryCodec - High video resolution detected %dx%d, trying half resolution decoding ", hint.width, hint.height);
    options.m_keys.push_back(CDVDCodecOption("lowres","1"));
  }
#endif

  std::string value = StringUtils::Format("%d", info.max_buffer_size);
  options.m_keys.push_back(CDVDCodecOption("surfaces", value));
  pCodec = OpenCodec(new CDVDVideoCodecFFmpeg(), hint, options);
  if (pCodec)
    return pCodec;

  return nullptr;
}

CDVDAudioCodec* CDVDFactoryCodec::CreateAudioCodec(CDVDStreamInfo &hint, bool allowpassthrough)
{
  CDVDAudioCodec* pCodec = NULL;
  CDVDCodecOptions options;

  // we don't use passthrough if "sync playback to display" is enabled
  if (allowpassthrough)
  {
    pCodec = OpenCodec(new CDVDAudioCodecPassthrough(), hint, options);
    if( pCodec ) return pCodec;
  }

#if defined(TARGET_DARWIN)
  if (hint.codec == AV_CODEC_ID_AC3 || hint.codec == AV_CODEC_ID_EAC3)
  {
    if (hint.filename != "dvd")
    {
      pCodec = OpenCodec(new CDVDAudioCodecAudioConverter(), hint, options);
      if( pCodec )
        return pCodec;
    }
  }
#endif

  pCodec = OpenCodec(new CDVDAudioCodecFFmpeg(), hint, options);
  if (pCodec)
    return pCodec;

  return nullptr;
}

CDVDOverlayCodec* CDVDFactoryCodec::CreateOverlayCodec( CDVDStreamInfo &hint )
{
  CDVDOverlayCodec* pCodec = NULL;
  CDVDCodecOptions options;

  switch (hint.codec)
  {
    case AV_CODEC_ID_TEXT:
    case AV_CODEC_ID_SUBRIP:
      pCodec = OpenCodec(new CDVDOverlayCodecText(), hint, options);
      if (pCodec)
        return pCodec;
      break;

    case AV_CODEC_ID_SSA:
    case AV_CODEC_ID_ASS:
      pCodec = OpenCodec(new CDVDOverlayCodecSSA(), hint, options);
      if (pCodec)
        return pCodec;

      pCodec = OpenCodec(new CDVDOverlayCodecText(), hint, options);
      if (pCodec)
        return pCodec;
      break;

    case AV_CODEC_ID_MOV_TEXT:
      pCodec = OpenCodec(new CDVDOverlayCodecTX3G(), hint, options);
      if (pCodec)
        return pCodec;
      break;

    default:
      pCodec = OpenCodec(new CDVDOverlayCodecFFmpeg(), hint, options);
      if (pCodec)
        return pCodec;
      break;
  }

  return nullptr;
}

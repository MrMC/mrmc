/*
 *      Copyright (C) 2005-2015 Team Kodi
 *      http://kodi.tv
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
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "system.h"
#include "DVDPlayer.h"
#include "DVDPlayerRadioRDS.h"

#include "DVDInputStreams/DVDInputStream.h"
#include "DVDInputStreams/DVDFactoryInputStream.h"
#include "DVDInputStreams/DVDInputStreamNavigator.h"
#include "DVDInputStreams/DVDInputStreamBluray.h"
#include "DVDInputStreams/DVDInputStreamPVRManager.h"

#include "DVDDemuxers/DVDDemux.h"
#include "DVDDemuxers/DVDDemuxUtils.h"
#include "DVDDemuxers/DVDDemuxVobsub.h"
#include "DVDDemuxers/DVDFactoryDemuxer.h"
#include "DVDDemuxers/DVDDemuxFFmpeg.h"

#include "DVDFileInfo.h"

#include "utils/LangCodeExpander.h"
#include "input/Key.h"
#include "guilib/LocalizeStrings.h"

#include "utils/URIUtils.h"
#include "GUIInfoManager.h"
#include "cores/DataCacheCore.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/StereoscopicsManager.h"
#include "Application.h"
#include "messaging/ApplicationMessenger.h"

#include "DVDDemuxers/DVDDemuxCC.h"
#ifdef HAS_VIDEO_PLAYBACK
#include "cores/VideoRenderers/RenderManager.h"
#endif
#include "settings/AdvancedSettings.h"
#include "FileItem.h"
#include "GUIUserMessages.h"
#include "settings/Settings.h"
#include "settings/MediaSettings.h"
#include "utils/log.h"
#include "utils/StreamDetails.h"
#include "pvr/PVRManager.h"
#include "utils/StreamUtils.h"
#include "utils/Variant.h"
#include "storage/MediaManager.h"
#include "dialogs/GUIDialogBusy.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "utils/StringUtils.h"
#include "Util.h"
#include "LangInfo.h"
#include "URL.h"
#include "video/VideoReferenceClock.h"
#include "services/ServicesManager.h"

#include "DVDPlayerAudio.h"
#include "DVDCodecs/DVDCodecUtils.h"
#include "windowing/WindowingFactory.h"

using namespace PVR;
using namespace KODI::MESSAGING;

#define StandardDemuxerQueueTime (8000.0)

void CSelectionStreams::Clear(StreamType type, StreamSource source)
{
  CSingleLock lock(m_section);
  for(int i=m_Streams.size()-1;i>=0;i--)
  {
    if(type && m_Streams[i].type != type)
      continue;

    if(source && m_Streams[i].source != source)
      continue;

    m_Streams.erase(m_Streams.begin() + i);
  }
}

SelectionStream& CSelectionStreams::Get(StreamType type, int index)
{
  CSingleLock lock(m_section);
  int count = -1;
  for(size_t i=0;i<m_Streams.size();i++)
  {
    if(m_Streams[i].type != type)
      continue;
    count++;
    if(count == index)
      return m_Streams[i];
  }
  CLog::Log(LOGERROR, "%s - failed to get stream", __FUNCTION__);
  return m_invalid;
}

std::vector<SelectionStream> CSelectionStreams::Get(StreamType type)
{
  std::vector<SelectionStream> streams;
  int count = Count(type);
  for(int index = 0; index < count; ++index){
    streams.push_back(Get(type, index));
  }
  return streams;
}

#define PREDICATE_RETURN(lh, rh) \
  do { \
    if((lh) != (rh)) \
      return (lh) > (rh); \
  } while(0)

class PredicateSubtitleFilter
{
private:
  std::string audiolang;
  bool original;
  bool nosub;
  bool onlyforced;
public:
  /** \brief The class' operator() decides if the given (subtitle) SelectionStream is relevant wrt.
  *          preferred subtitle language and audio language. If the subtitle is relevant <B>false</B> false is returned.
  *
  *          A subtitle is relevant if
  *          - it was previously selected, or
  *          - it's an external sub, or
  *          - it's a forced sub and "original stream's language" was selected, or
  *          - it's a forced sub and its language matches the audio's language, or
  *          - it's a default sub, or
  *          - its language matches the preferred subtitle's language (unequal to "original stream's language")
  */
  PredicateSubtitleFilter(std::string& lang)
    : audiolang(lang),
      original(StringUtils::EqualsNoCase(CSettings::GetInstance().GetString(CSettings::SETTING_LOCALE_SUBTITLELANGUAGE), "original")),
      nosub(StringUtils::EqualsNoCase(CSettings::GetInstance().GetString(CSettings::SETTING_LOCALE_SUBTITLELANGUAGE), "none")),
      onlyforced(StringUtils::EqualsNoCase(CSettings::GetInstance().GetString(CSettings::SETTING_LOCALE_SUBTITLELANGUAGE), "forced_only"))
  {
  };

  bool operator()(const SelectionStream& ss) const
  {
    if (ss.type_index == CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleStream)
      return false;

    if (nosub)
      return true;

    if (onlyforced)
    {
      if ((ss.flags & CDemuxStream::FLAG_FORCED) &&
          (g_LangCodeExpander.CompareISO639Codes(ss.language, audiolang) || (ss.flags & CDemuxStream::FLAG_DEFAULT)))
        return false;
      else
        return true;
    }

    if(STREAM_SOURCE_MASK(ss.source) == STREAM_SOURCE_DEMUX_SUB || STREAM_SOURCE_MASK(ss.source) == STREAM_SOURCE_TEXT)
      return false;

    if ((ss.flags & CDemuxStream::FLAG_FORCED) && (original || g_LangCodeExpander.CompareISO639Codes(ss.language, audiolang)))
      return false;

    if ((ss.flags & CDemuxStream::FLAG_DEFAULT))
      return false;

    if(!original)
    {
      std::string subtitle_language = g_langInfo.GetSubtitleLanguage();
      if (g_LangCodeExpander.CompareISO639Codes(subtitle_language, ss.language))
        return false;
    }

    // last resort, m_SubtitleOn is set in guisettings.xml and if its set to "true" we should not ignore that.
    // in some cases, embeded subtitles would not trigger any of the above checks and subtitles would not be enabled.
    return !CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleOn;
  }
};

static bool PredicateAudioPriority(const SelectionStream& lh, const SelectionStream& rh)
{
  PREDICATE_RETURN(lh.type_index == CMediaSettings::GetInstance().GetCurrentVideoSettings().m_AudioStream
                 , rh.type_index == CMediaSettings::GetInstance().GetCurrentVideoSettings().m_AudioStream);

  if(!StringUtils::EqualsNoCase(CSettings::GetInstance().GetString(CSettings::SETTING_LOCALE_AUDIOLANGUAGE), "original"))
  {
    std::string audio_language = g_langInfo.GetAudioLanguage();
    PREDICATE_RETURN(g_LangCodeExpander.CompareISO639Codes(audio_language, lh.language)
                   , g_LangCodeExpander.CompareISO639Codes(audio_language, rh.language));

    bool hearingimp = CSettings::GetInstance().GetBool(CSettings::SETTING_ACCESSIBILITY_AUDIOHEARING);
    PREDICATE_RETURN(!hearingimp ? !(lh.flags & CDemuxStream::FLAG_HEARING_IMPAIRED) : lh.flags & CDemuxStream::FLAG_HEARING_IMPAIRED
                   , !hearingimp ? !(rh.flags & CDemuxStream::FLAG_HEARING_IMPAIRED) : rh.flags & CDemuxStream::FLAG_HEARING_IMPAIRED);

    bool visualimp = CSettings::GetInstance().GetBool(CSettings::SETTING_ACCESSIBILITY_AUDIOVISUAL);
    PREDICATE_RETURN(!visualimp ? !(lh.flags & CDemuxStream::FLAG_VISUAL_IMPAIRED) : lh.flags & CDemuxStream::FLAG_VISUAL_IMPAIRED
                   , !visualimp ? !(rh.flags & CDemuxStream::FLAG_VISUAL_IMPAIRED) : rh.flags & CDemuxStream::FLAG_VISUAL_IMPAIRED);
  }

  if (CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOPLAYER_PREFERDEFAULTFLAG))
  {
    PREDICATE_RETURN(lh.flags & CDemuxStream::FLAG_DEFAULT
                   , rh.flags & CDemuxStream::FLAG_DEFAULT);
  }

  PREDICATE_RETURN(lh.channels
                 , rh.channels);

  PREDICATE_RETURN(StreamUtils::GetCodecPriority(lh.codec)
                 , StreamUtils::GetCodecPriority(rh.codec));

  PREDICATE_RETURN(lh.flags & CDemuxStream::FLAG_DEFAULT
                 , rh.flags & CDemuxStream::FLAG_DEFAULT);

  return false;
}

/** \brief The class' operator() decides if the given (subtitle) SelectionStream lh is 'better than' the given (subtitle) SelectionStream rh.
*          If lh is 'better than' rh the return value is true, false otherwise.
*
*          A subtitle lh is 'better than' a subtitle rh (in evaluation order) if
*          - lh was previously selected, or
*          - lh is an external sub and rh not, or
*          - lh is a forced sub and ("original stream's language" was selected or subtitles are off) and rh not, or
*          - lh is an external sub and its language matches the preferred subtitle's language (unequal to "original stream's language") and rh not, or
*          - lh is language matches the preferred subtitle's language (unequal to "original stream's language") and rh not, or
*          - lh is a default sub and rh not
*/
class PredicateSubtitlePriority
{
private:
  std::string audiolang;
  bool original;
  bool subson;
  PredicateSubtitleFilter filter;
public:
  PredicateSubtitlePriority(std::string& lang)
    : audiolang(lang),
      original(StringUtils::EqualsNoCase(CSettings::GetInstance().GetString(CSettings::SETTING_LOCALE_SUBTITLELANGUAGE), "original")),
      subson(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleOn),
      filter(lang)
  {
  };

  bool relevant(const SelectionStream& ss) const
  {
    return !filter(ss);
  }

  bool operator()(const SelectionStream& lh, const SelectionStream& rh) const
  {
    PREDICATE_RETURN(relevant(lh)
                   , relevant(rh));

    PREDICATE_RETURN(lh.type_index == CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleStream
                   , rh.type_index == CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleStream);

    // prefer external subs
    PREDICATE_RETURN(STREAM_SOURCE_MASK(lh.source) == STREAM_SOURCE_DEMUX_SUB || STREAM_SOURCE_MASK(lh.source) == STREAM_SOURCE_TEXT
                   , STREAM_SOURCE_MASK(rh.source) == STREAM_SOURCE_DEMUX_SUB || STREAM_SOURCE_MASK(rh.source) == STREAM_SOURCE_TEXT);

    if(!subson || original)
    {
      PREDICATE_RETURN(lh.flags & CDemuxStream::FLAG_FORCED && g_LangCodeExpander.CompareISO639Codes(lh.language, audiolang)
                     , rh.flags & CDemuxStream::FLAG_FORCED && g_LangCodeExpander.CompareISO639Codes(rh.language, audiolang));

      PREDICATE_RETURN(lh.flags & CDemuxStream::FLAG_FORCED
                     , rh.flags & CDemuxStream::FLAG_FORCED);
    }

    std::string subtitle_language = g_langInfo.GetSubtitleLanguage();
    if(!original)
    {
      PREDICATE_RETURN((STREAM_SOURCE_MASK(lh.source) == STREAM_SOURCE_DEMUX_SUB || STREAM_SOURCE_MASK(lh.source) == STREAM_SOURCE_TEXT) && g_LangCodeExpander.CompareISO639Codes(subtitle_language, lh.language)
                     , (STREAM_SOURCE_MASK(rh.source) == STREAM_SOURCE_DEMUX_SUB || STREAM_SOURCE_MASK(rh.source) == STREAM_SOURCE_TEXT) && g_LangCodeExpander.CompareISO639Codes(subtitle_language, rh.language));
    }

    if(!original)
    {
      PREDICATE_RETURN(g_LangCodeExpander.CompareISO639Codes(subtitle_language, lh.language)
                     , g_LangCodeExpander.CompareISO639Codes(subtitle_language, rh.language));

      bool hearingimp = CSettings::GetInstance().GetBool(CSettings::SETTING_ACCESSIBILITY_SUBHEARING);
      PREDICATE_RETURN(!hearingimp ? !(lh.flags & CDemuxStream::FLAG_HEARING_IMPAIRED) : lh.flags & CDemuxStream::FLAG_HEARING_IMPAIRED
                     , !hearingimp ? !(rh.flags & CDemuxStream::FLAG_HEARING_IMPAIRED) : rh.flags & CDemuxStream::FLAG_HEARING_IMPAIRED);
    }

    PREDICATE_RETURN(lh.flags & CDemuxStream::FLAG_DEFAULT
                   , rh.flags & CDemuxStream::FLAG_DEFAULT);

    return false;
  }
};

static bool PredicateVideoPriority(const SelectionStream& lh, const SelectionStream& rh)
{
  PREDICATE_RETURN(lh.flags & CDemuxStream::FLAG_DEFAULT
                 , rh.flags & CDemuxStream::FLAG_DEFAULT);
  return false;
}

bool CSelectionStreams::Get(StreamType type, CDemuxStream::EFlags flag, SelectionStream& out)
{
  CSingleLock lock(m_section);
  for(size_t i=0;i<m_Streams.size();i++)
  {
    if(m_Streams[i].type != type)
      continue;
    if((m_Streams[i].flags & flag) != flag)
      continue;
    out = m_Streams[i];
    return true;
  }
  return false;
}

int CSelectionStreams::IndexOf(StreamType type, int source, int id) const
{
  CSingleLock lock(m_section);
  int count = -1;
  for(size_t i=0;i<m_Streams.size();i++)
  {
    if(type && m_Streams[i].type != type)
      continue;
    count++;
    if(source && m_Streams[i].source != source)
      continue;
    if(id < 0)
      continue;
    if(m_Streams[i].id == id)
      return count;
  }
  if(id < 0)
    return count;
  else
    return -1;
}

int CSelectionStreams::IndexOf(StreamType type, const CDVDPlayer& p) const
{
  if (p.m_pInputStream && p.m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
  {
    int id = -1;
    if(type == STREAM_AUDIO)
      id = ((CDVDInputStreamNavigator*)p.m_pInputStream)->GetActiveAudioStream();
    else if (type == STREAM_VIDEO)
      id = ((CDVDInputStreamNavigator*)p.m_pInputStream)->GetActiveAngle();
    else if(type == STREAM_SUBTITLE)
      id = ((CDVDInputStreamNavigator*)p.m_pInputStream)->GetActiveSubtitleStream();

    return IndexOf(type, STREAM_SOURCE_NAV, id);
  }

  if(type == STREAM_AUDIO)
    return IndexOf(type, p.m_CurrentAudio.source, p.m_CurrentAudio.id);
  else if(type == STREAM_VIDEO)
    return IndexOf(type, p.m_CurrentVideo.source, p.m_CurrentVideo.id);
  else if(type == STREAM_SUBTITLE)
    return IndexOf(type, p.m_CurrentSubtitle.source, p.m_CurrentSubtitle.id);
  else if(type == STREAM_TELETEXT)
    return IndexOf(type, p.m_CurrentTeletext.source, p.m_CurrentTeletext.id);
  else if(type == STREAM_RADIO_RDS)
    return IndexOf(type, p.m_CurrentRadioRDS.source, p.m_CurrentRadioRDS.id);

  return -1;
}

int CSelectionStreams::Source(StreamSource source, std::string filename)
{
  CSingleLock lock(m_section);
  int index = source - 1;
  for(size_t i=0;i<m_Streams.size();i++)
  {
    SelectionStream &s = m_Streams[i];
    if(STREAM_SOURCE_MASK(s.source) != source)
      continue;
    // if it already exists, return same
    if(s.filename == filename)
      return s.source;
    if(index < s.source)
      index = s.source;
  }
  // return next index
  return index + 1;
}

void CSelectionStreams::Update(SelectionStream& s)
{
  CSingleLock lock(m_section);
  int index = IndexOf(s.type, s.source, s.id);
  if(index >= 0)
  {
    SelectionStream& o = Get(s.type, index);
    s.type_index = o.type_index;
    o = s;
  }
  else
  {
    s.type_index = Count(s.type);
    m_Streams.push_back(s);
  }
}

void CSelectionStreams::Update(CDVDInputStream* input, CDVDDemux* demuxer, std::string filename2)
{
  if(input && input->IsStreamType(DVDSTREAM_TYPE_DVD))
  {
    CDVDInputStreamNavigator* nav = (CDVDInputStreamNavigator*)input;
    std::string filename = nav->GetFileName();
    int source = Source(STREAM_SOURCE_NAV, filename);

    int count;
    count = nav->GetAudioStreamCount();
    for(int i=0;i<count;i++)
    {
      SelectionStream s;
      s.source   = source;
      s.type     = STREAM_AUDIO;
      s.id       = i;
      s.flags    = CDemuxStream::FLAG_NONE;
      s.filename = filename;

      DVDNavStreamInfo info;
      nav->GetAudioStreamInfo(i, info);
      s.name     = info.name;
      s.language = g_LangCodeExpander.ConvertToISO6392B(info.language);
      s.channels = info.channels;
      Update(s);
    }

    count = nav->GetSubTitleStreamCount();
    for(int i=0;i<count;i++)
    {
      SelectionStream s;
      s.source   = source;
      s.type     = STREAM_SUBTITLE;
      s.id       = i;
      s.flags    = CDemuxStream::FLAG_NONE;
      s.filename = filename;
      s.channels = 0;

      DVDNavStreamInfo info;
      nav->GetSubtitleStreamInfo(i, info);
      s.name     = info.name;
      s.language = g_LangCodeExpander.ConvertToISO6392B(info.language);
      Update(s);
    }

    count = nav->GetAngleCount();
    uint32_t width = 0;
    uint32_t height = 0;
    int aspect = nav->GetVideoAspectRatio();
    nav->GetVideoResolution(&width, &height);
    for (int i = 1; i <= count; i++)
    {
      SelectionStream s;
      s.source = source;
      s.type = STREAM_VIDEO;
      s.id = i;
      s.flags = CDemuxStream::FLAG_NONE;
      s.filename = filename;
      s.channels = 0;
      s.aspect_ratio = aspect;
      s.width = (int)width;
      s.height = (int)height;
      s.name = StringUtils::Format("%s %i", g_localizeStrings.Get(38032).c_str(), i);
      Update(s);
    }
  }
  else if(demuxer)
  {
    std::string filename = demuxer->GetFileName();
    int count = demuxer->GetNrOfStreams();
    int source;
    if(input) /* hack to know this is sub decoder */
      source = Source(STREAM_SOURCE_DEMUX, filename);
    else if (!filename2.empty())
      source = Source(STREAM_SOURCE_DEMUX_SUB, filename);
    else
      source = Source(STREAM_SOURCE_VIDEOMUX, filename);

    for(int i=0;i<count;i++)
    {
      CDemuxStream* stream = demuxer->GetStream(i);
      /* skip streams with no type */
      if (stream->type == STREAM_NONE)
        continue;
      /* make sure stream is marked with right source */
      stream->source = source;

      SelectionStream s;
      s.source   = source;
      s.type     = stream->type;
      s.id       = stream->iId;
      s.language = g_LangCodeExpander.ConvertToISO6392B(stream->language);
      s.flags    = stream->flags;
      s.filename = demuxer->GetFileName();
      s.filename2 = filename2;
      stream->GetStreamName(s.name);
      std::string codec;
      demuxer->GetStreamCodecName(stream->iId, codec);
      s.codec    = codec;
      s.channels = 0; // Default to 0. Overwrite if STREAM_AUDIO below.
      if(stream->type == STREAM_VIDEO)
      {
        s.width = ((CDemuxStreamVideo*)stream)->iWidth;
        s.height = ((CDemuxStreamVideo*)stream)->iHeight;
      }
      if(stream->type == STREAM_AUDIO)
      {
        std::string type;
        ((CDemuxStreamAudio*)stream)->GetStreamType(type);
        if(type.length() > 0)
        {
          if(s.name.length() > 0)
            s.name += " - ";
          s.name += type;
        }
        s.channels = ((CDemuxStreamAudio*)stream)->iChannels;
      }
      Update(s);
    }
  }
  g_dataCacheCore.SignalAudioInfoChange();
  g_dataCacheCore.SignalVideoInfoChange();
}

int CSelectionStreams::CountSource(StreamType type, StreamSource source) const
{
  CSingleLock lock(m_section);
  int count = 0;
  for(size_t i=0;i<m_Streams.size();i++)
  {
    if(type && m_Streams[i].type != type)
      continue;
    if (source && m_Streams[i].source != source)
      continue;
    count++;
    continue;
  }
  return count;
}

void CDVDPlayer::CreatePlayers()
{
  if (m_players_created)
    return;

  m_dvdPlayerVideo = new CDVDPlayerVideo(&m_clock, &m_overlayContainer, m_messenger);
  m_dvdPlayerAudio = new CDVDPlayerAudio(&m_clock, m_messenger);
  m_dvdPlayerSubtitle = new CDVDPlayerSubtitle(&m_overlayContainer);
  m_dvdPlayerTeletext = new CDVDTeletextData();
  m_dvdPlayerRadioRDS = new CDVDRadioRDSData();
  m_players_created = true;
}

void CDVDPlayer::DestroyPlayers()
{
  if (!m_players_created)
    return;
  SAFE_DELETE(m_dvdPlayerVideo);
  SAFE_DELETE(m_dvdPlayerAudio);
  SAFE_DELETE(m_dvdPlayerSubtitle);
  SAFE_DELETE(m_dvdPlayerTeletext);
  SAFE_DELETE(m_dvdPlayerRadioRDS);
  m_players_created = false;
}

CDVDPlayer::CDVDPlayer(IPlayerCallback& callback)
    : IPlayer(callback),
      CThread("DVDPlayer"),
      m_CurrentAudio(STREAM_AUDIO, DVDPLAYER_AUDIO),
      m_CurrentVideo(STREAM_VIDEO, DVDPLAYER_VIDEO),
      m_CurrentSubtitle(STREAM_SUBTITLE, DVDPLAYER_SUBTITLE),
      m_CurrentTeletext(STREAM_TELETEXT, DVDPLAYER_TELETEXT),
      m_CurrentRadioRDS(STREAM_RADIO_RDS, DVDPLAYER_RDS),
      m_messenger("player"),
      m_ready(true)
{
  m_players_created = false;
  m_pDemuxer = NULL;
  m_pSubtitleDemuxer = NULL;
  m_pCCDemuxer = NULL;
  m_pInputStream = NULL;

  m_dvd.Clear();
  m_State.Clear();
  m_UpdateApplication = 0;

  m_bAbortRequest = false;
  m_errorCount = 0;
  m_offset_pts = 0.0;
  m_playSpeed = DVD_PLAYSPEED_NORMAL;
  m_streamPlayerSpeed = DVD_PLAYSPEED_NORMAL;
  m_caching = CACHESTATE_WAITFILL;
  m_HasVideo = false;
  m_HasAudio = false;

  memset(&m_SpeedState, 0, sizeof(m_SpeedState));

  m_SkipCommercials = true;
  CreatePlayers();

  m_displayState = AV_DISPLAY_PRESENT;
  m_displayResetDelay = 0;
  g_Windowing.Register(this);
}

CDVDPlayer::~CDVDPlayer()
{
  g_Windowing.Unregister(this);

  CloseFile();
  DestroyPlayers();
}

bool CDVDPlayer::OpenFile(const CFileItem& file, const CPlayerOptions &options)
{
  CLog::Log(LOGNOTICE, "CDVDPlayer::Opening: %s", CURL::GetRedacted(file.GetPath()).c_str());

  // if playing a file close it first
  // this has to be changed so we won't have to close it.
  if(IsRunning())
    CloseFile();

  m_bAbortRequest = false;
  SetPlaySpeed(DVD_PLAYSPEED_NORMAL);

  m_State.Clear();
  memset(&m_SpeedState, 0, sizeof(m_SpeedState));
  m_UpdateApplication = 0;
  m_offset_pts = 0;
  m_CurrentAudio.lastdts = DVD_NOPTS_VALUE;
  m_CurrentVideo.lastdts = DVD_NOPTS_VALUE;

  m_PlayerOptions = options;
  m_item = file;
  // Try to resolve the correct mime type
  m_item.SetMimeTypeForInternetFile();

  m_ready.Reset();

#if defined(HAS_VIDEO_PLAYBACK)
    g_renderManager.PreInit(&m_clock);
#endif

  Create();

  // wait for the ready event
  if (g_application.GetRenderGUI())
    CGUIDialogBusy::WaitOnEvent(m_ready, g_advancedSettings.m_videoBusyDialogDelay_ms, false);
  else
    m_ready.Wait();

  // Playback might have been stopped due to some error
  if (m_bStop || m_bAbortRequest)
    return false;

  return true;
}

bool CDVDPlayer::CloseFile(bool reopen)
{
  CLog::Log(LOGNOTICE, "CDVDPlayer::CloseFile()");

  // set the abort request so that other threads can finish up
  m_bAbortRequest = true;

  // tell demuxer to abort
  if(m_pDemuxer)
    m_pDemuxer->Abort();

  if(m_pSubtitleDemuxer)
    m_pSubtitleDemuxer->Abort();

  if(m_pInputStream)
    m_pInputStream->Abort();

  CLog::Log(LOGNOTICE, "DVDPlayer: waiting for threads to exit");

  // wait for the main thread to finish up
  // since this main thread cleans up all other resources and threads
  // we are done after the StopThread call
  StopThread();

  m_Edl.Clear();

  m_HasVideo = false;
  m_HasAudio = false;

  CLog::Log(LOGNOTICE, "DVDPlayer: finished waiting");
#if defined(HAS_VIDEO_PLAYBACK)
  g_renderManager.UnInit();
#endif
  return true;
}

bool CDVDPlayer::IsPlaying() const
{
  return !m_bStop;
}

bool CDVDPlayer::IsPlayingSplash() const
{
  if (m_item.HasProperty("VideoSplash"))
    return true;
  return m_item.GetPath() == "special://xbmc/media/Splash.mp4";
}

void CDVDPlayer::OnStartup()
{
  m_CurrentVideo.Clear();
  m_CurrentAudio.Clear();
  m_CurrentSubtitle.Clear();
  m_CurrentTeletext.Clear();
  m_CurrentRadioRDS.Clear();

  m_messenger.Init();

  CUtil::ClearTempFonts();
}

bool CDVDPlayer::OpenInputStream()
{
  if(m_pInputStream)
    SAFE_DELETE(m_pInputStream);

  CLog::Log(LOGNOTICE, "Creating InputStream");

  // correct the filename if needed
  std::string filename(m_item.GetPath());
  if (URIUtils::IsProtocol(filename, "dvd") ||
      StringUtils::EqualsNoCase(filename, "iso9660://video_ts/video_ts.ifo"))
  {
    m_item.SetPath(g_mediaManager.TranslateDevicePath(""));
  }

  m_pInputStream = CDVDFactoryInputStream::CreateInputStream(this, m_item);
  if(m_pInputStream == NULL)
  {
    CLog::Log(LOGERROR, "CDVDPlayer::OpenInputStream - unable to create input stream for [%s]",
      CURL::GetRedacted(m_item.GetPath().c_str()).c_str());
    return false;
  }

  if (!m_pInputStream->Open())
  {
    CLog::Log(LOGERROR, "CDVDPlayer::OpenInputStream - error opening [%s]",
      CURL::GetRedacted(m_item.GetPath().c_str()).c_str());
    return false;
  }

  if (m_item.IsMediaServiceBased())
  {
    CServicesManager::GetInstance().GetSubtitles(m_item);
  }

  // find any available external subtitles for non dvd files
  if (!m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD)
  &&  !m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER)
  &&  !m_pInputStream->IsStreamType(DVDSTREAM_TYPE_TV))
  {
    // find any available external subtitles
    std::vector<std::string> filenames;
    CUtil::ScanForExternalSubtitles(m_item.GetPath(), filenames);

    // load any subtitles from file item
    std::string key("subtitle:1");
    for(unsigned s = 1; m_item.HasProperty(key); key = StringUtils::Format("subtitle:%u", ++s))
      filenames.push_back(m_item.GetProperty(key).asString());

    for(unsigned int i=0;i<filenames.size();i++)
    {
      // if vobsub subtitle:
      if (URIUtils::HasExtension(filenames[i], ".idx"))
      {
        std::string strSubFile;
        if ( CUtil::FindVobSubPair( filenames, filenames[i], strSubFile ) )
          AddSubtitleFile(filenames[i], strSubFile);
      }
      else
      {
        if ( !CUtil::IsVobSub(filenames, filenames[i] ) )
        {
          bool forced = false;
          std::string key = StringUtils::Format("subtitle:%i_language", i+1);
          std::string language = m_item.GetProperty(key).asString();
          std::string forcedKey = StringUtils::Format("subtitle:%i_forced", i+1);
          if (m_item.HasProperty(forcedKey))
            forced = m_item.GetProperty(forcedKey).asBoolean();
          AddSubtitleFile(filenames[i], "", language, forced);
        }
      }
    } // end loop over all subtitle files

    CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleCached = true;
  }

  SetAVDelay(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_AudioDelay);
  SetSubTitleDelay(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleDelay);
  m_clock.Reset();
  m_dvd.Clear();
  m_errorCount = 0;
  m_ChannelEntryTimeOut.SetInfinite();

  return true;
}

bool CDVDPlayer::OpenDemuxStream()
{
  if(m_pDemuxer)
    SAFE_DELETE(m_pDemuxer);

  CLog::Log(LOGNOTICE, "Creating Demuxer");

  int attempts = 10;
  while(!m_bStop && attempts-- > 0)
  {
    m_pDemuxer = CDVDFactoryDemuxer::CreateDemuxer(m_pInputStream);
    if(!m_pDemuxer && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER))
    {
      continue;
    }
    else if(!m_pDemuxer && m_pInputStream->NextStream() != CDVDInputStream::NEXTSTREAM_NONE)
    {
      CLog::Log(LOGDEBUG, "%s - New stream available from input, retry open", __FUNCTION__);
      continue;
    }
    break;
  }

  if(!m_pDemuxer)
  {
    CLog::Log(LOGERROR, "%s - Error creating demuxer", __FUNCTION__);
    return false;
  }

  m_SelectionStreams.Clear(STREAM_NONE, STREAM_SOURCE_DEMUX);
  m_SelectionStreams.Clear(STREAM_NONE, STREAM_SOURCE_NAV);
  m_SelectionStreams.Update(m_pInputStream, m_pDemuxer);

  int64_t len = m_pInputStream->GetLength();
  int64_t tim = m_pDemuxer->GetStreamLength();
  if(len > 0 && tim > 0)
    m_pInputStream->SetReadRate((unsigned int) (len * 1000 / tim));

  m_offset_pts = 0;

  return true;
}

void CDVDPlayer::OpenDefaultSubtitleStreams(bool reset)
{
  bool valid = false;
  SelectionStreams streams;

  // enable  or disable subtitles
  bool visible = CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleOn;

  // open subtitle stream
  SelectionStream as = m_SelectionStreams.Get(STREAM_AUDIO, GetAudioStream());
  PredicateSubtitlePriority psp(as.language);
  streams = m_SelectionStreams.Get(STREAM_SUBTITLE, psp);
  valid   = false;
  CloseStream(m_CurrentSubtitle, false);
  for(SelectionStreams::iterator it = streams.begin(); it != streams.end() && !valid; ++it)
  {
    if(OpenStream(m_CurrentSubtitle, it->id, it->source))
    {
      valid = true;
      if(!psp.relevant(*it))
        visible = false;
      else if(it->flags & CDemuxStream::FLAG_FORCED)
        visible = true;
    }
  }
  if(!valid)
    CloseStream(m_CurrentSubtitle, false);

  if (!dynamic_cast<CDVDInputStreamNavigator*>(m_pInputStream) || m_PlayerOptions.state.empty())
    SetSubtitleVisibleInternal(visible); // only set subtitle visibility if state not stored by dvd navigator, because navigator will restore it (if visible)

}

void CDVDPlayer::OpenDefaultStreams(bool reset)
{
  // if input stream dictate, we will open later
  if(m_dvd.iSelectedAudioStream >= 0
  || m_dvd.iSelectedSPUStream   >= 0)
    return;

  SelectionStreams streams;
  bool valid;

  // open video stream
  streams = m_SelectionStreams.Get(STREAM_VIDEO, PredicateVideoPriority);
  valid   = false;
  for(SelectionStreams::iterator it = streams.begin(); it != streams.end() && !valid; ++it)
  {
    if(OpenStream(m_CurrentVideo, it->id, it->source, reset))
      valid = true;
  }
  if(!valid)
    CloseStream(m_CurrentVideo, true);

  // open audio stream
  if(m_PlayerOptions.video_only)
    streams.clear();
  else
    streams = m_SelectionStreams.Get(STREAM_AUDIO, PredicateAudioPriority);
  valid   = false;

  for(SelectionStreams::iterator it = streams.begin(); it != streams.end() && !valid; ++it)
  {
    if(OpenStream(m_CurrentAudio, it->id, it->source, reset))
      valid = true;
  }
  if(!valid)
    CloseStream(m_CurrentAudio, true);


  OpenDefaultSubtitleStreams(reset);

  // open teletext stream
  streams = m_SelectionStreams.Get(STREAM_TELETEXT);
  valid   = false;
  for(SelectionStreams::iterator it = streams.begin(); it != streams.end() && !valid; ++it)
  {
    if(OpenStream(m_CurrentTeletext, it->id, it->source))
      valid = true;
  }
  if(!valid)
    CloseStream(m_CurrentTeletext, false);

  // open RDS stream
  streams = m_SelectionStreams.Get(STREAM_RADIO_RDS);
  valid   = false;
  for(SelectionStreams::iterator it = streams.begin(); it != streams.end() && !valid; ++it)
  {
    if(OpenStream(m_CurrentRadioRDS, it->id, it->source))
      valid = true;
  }
  if(!valid)
    CloseStream(m_CurrentRadioRDS, false);
}

bool CDVDPlayer::ReadPacket(DemuxPacket*& packet, CDemuxStream*& stream)
{

  // check if we should read from subtitle demuxer
  if( m_pSubtitleDemuxer && m_dvdPlayerSubtitle->AcceptsData() )
  {
    packet = m_pSubtitleDemuxer->Read();

    if(packet)
    {
      UpdateCorrection(packet, m_offset_pts);
      if(packet->iStreamId < 0)
        return true;

      stream = m_pSubtitleDemuxer->GetStream(packet->iStreamId);
      if (!stream)
      {
        CLog::Log(LOGERROR, "%s - Error demux packet doesn't belong to a valid stream", __FUNCTION__);
        return false;
      }
      if(stream->source == STREAM_SOURCE_NONE)
      {
        m_SelectionStreams.Clear(STREAM_NONE, STREAM_SOURCE_DEMUX_SUB);
        m_SelectionStreams.Update(NULL, m_pSubtitleDemuxer);
      }
      return true;
    }
  }

  // read a data frame from stream.
  if(m_pDemuxer)
    packet = m_pDemuxer->Read();

  if(packet)
  {
    // stream changed, update and open defaults
    if(packet->iStreamId == DMX_SPECIALID_STREAMCHANGE)
    {
        m_SelectionStreams.Clear(STREAM_NONE, STREAM_SOURCE_DEMUX);
        m_SelectionStreams.Update(m_pInputStream, m_pDemuxer);
        OpenDefaultStreams(false);

        // reevaluate HasVideo/Audio, we may have switched from/to a radio channel
        if(m_CurrentVideo.id < 0)
          m_HasVideo = false;
        if(m_CurrentAudio.id < 0)
          m_HasAudio = false;

        return true;
    }

    UpdateCorrection(packet, m_offset_pts);

    if(packet->iStreamId < 0)
      return true;

    if(m_pDemuxer)
    {
      stream = m_pDemuxer->GetStream(packet->iStreamId);
      if (!stream)
      {
        CLog::Log(LOGERROR, "%s - Error demux packet doesn't belong to a valid stream", __FUNCTION__);
        return false;
      }
      if(stream->source == STREAM_SOURCE_NONE)
      {
        m_SelectionStreams.Clear(STREAM_NONE, STREAM_SOURCE_DEMUX);
        m_SelectionStreams.Update(m_pInputStream, m_pDemuxer);
      }
    }
    return true;
  }
  return false;
}

bool CDVDPlayer::IsValidStream(CCurrentStream& stream)
{
  if(stream.id<0)
    return true; // we consider non selected as valid

  int source = STREAM_SOURCE_MASK(stream.source);
  if(source == STREAM_SOURCE_TEXT)
    return true;
  if(source == STREAM_SOURCE_DEMUX_SUB)
  {
    CDemuxStream* st = m_pSubtitleDemuxer->GetStream(stream.id);
    if(st == NULL || st->disabled)
      return false;
    if(st->type != stream.type)
      return false;
    return true;
  }
  if(source == STREAM_SOURCE_DEMUX)
  {
    CDemuxStream* st = m_pDemuxer->GetStream(stream.id);
    if(st == NULL || st->disabled)
      return false;
    if(st->type != stream.type)
      return false;

    if (m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
    {
      if(stream.type == STREAM_AUDIO    && st->iPhysicalId != m_dvd.iSelectedAudioStream)
        return false;
      if(stream.type == STREAM_SUBTITLE && st->iPhysicalId != m_dvd.iSelectedSPUStream)
        return false;
    }

    return true;
  }
  if (source == STREAM_SOURCE_VIDEOMUX)
  {
    CDemuxStream* st = m_pCCDemuxer->GetStream(stream.id);
    if (st == NULL || st->disabled)
      return false;
    if (st->type != stream.type)
      return false;
    return true;
  }

  return false;
}

bool CDVDPlayer::IsBetterStream(CCurrentStream& current, CDemuxStream* stream)
{
  // Do not reopen non-video streams if we're in video-only mode
  if(m_PlayerOptions.video_only && current.type != STREAM_VIDEO)
    return false;

  if(stream->disabled)
    return false;

  if (m_pInputStream && ( m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD)
                       || m_pInputStream->IsStreamType(DVDSTREAM_TYPE_BLURAY) ) )
  {
    int source_type;

    source_type = STREAM_SOURCE_MASK(current.source);
    if(source_type != STREAM_SOURCE_DEMUX
    && source_type != STREAM_SOURCE_NONE)
      return false;

    source_type = STREAM_SOURCE_MASK(stream->source);
    if(source_type  != STREAM_SOURCE_DEMUX
    || stream->type != current.type
    || stream->iId  == current.id)
      return false;

    if(current.type == STREAM_AUDIO    && stream->iPhysicalId == m_dvd.iSelectedAudioStream)
      return true;
    if(current.type == STREAM_SUBTITLE && stream->iPhysicalId == m_dvd.iSelectedSPUStream)
      return true;
    if(current.type == STREAM_VIDEO    && current.id < 0)
      return true;
  }
  else
  {
    if(stream->source == current.source
    && stream->iId    == current.id)
      return false;

    if(stream->type != current.type)
      return false;

    if(current.type == STREAM_SUBTITLE)
      return false;

    if(current.id < 0)
      return true;
  }
  return false;
}

void CDVDPlayer::CheckBetterStream(CCurrentStream& current, CDemuxStream* stream)
{
  IDVDStreamPlayer* player = GetStreamPlayer(current.player);
  if (!IsValidStream(current) && (player == NULL || player->IsStalled()))
    CloseStream(current, true);

  if (IsBetterStream(current, stream))
    OpenStream(current, stream->iId, stream->source);
}

void CDVDPlayer::Process()
{
  if (!OpenInputStream())
  {
    m_bAbortRequest = true;
    return;
  }

  if (CDVDInputStream::IMenus* ptr = dynamic_cast<CDVDInputStream::IMenus*>(m_pInputStream))
  {
    CLog::Log(LOGNOTICE, "DVDPlayer: playing a file with menu's");
    if(dynamic_cast<CDVDInputStreamNavigator*>(m_pInputStream))
      m_PlayerOptions.starttime = 0;

    if(!m_PlayerOptions.state.empty())
      ptr->SetState(m_PlayerOptions.state);
    else if(CDVDInputStreamNavigator* nav = dynamic_cast<CDVDInputStreamNavigator*>(m_pInputStream))
      nav->EnableSubtitleStream(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleOn);

    CMediaSettings::GetInstance().GetCurrentVideoSettings().m_SubtitleCached = true;
  }

  if(!OpenDemuxStream())
  {
    m_bAbortRequest = true;
    return;
  }
  // give players a chance to reconsider now codecs are known
  CreatePlayers();

  // allow renderer to switch to fullscreen if requested
  m_dvdPlayerVideo->EnableFullscreen(m_PlayerOptions.fullscreen);
  m_dvdPlayerVideo->RunningVideoSplash(IsPlayingSplash());

  OpenDefaultStreams();

  // look for any EDL files
  m_Edl.Clear();
  if (!m_item.HasProperty("VideoSplash") && m_CurrentVideo.id >= 0 && m_CurrentVideo.hint.fpsrate > 0 && m_CurrentVideo.hint.fpsscale > 0)
  {
    float fFramesPerSecond = (float)m_CurrentVideo.hint.fpsrate / (float)m_CurrentVideo.hint.fpsscale;
    m_Edl.ReadEditDecisionLists(m_item.GetPath(), fFramesPerSecond, m_CurrentVideo.hint.height);
  }

  /*
   * Check to see if the demuxer should start at something other than time 0. This will be the case
   * if there was a start time specified as part of the "Start from where last stopped" (aka
   * auto-resume) feature or if there is an EDL cut or commercial break that starts at time 0.
   */
  CEdl::Cut cut;
  int starttime = 0;
  if (m_PlayerOptions.starttime > 0 || m_PlayerOptions.startpercent > 0)
  {
    if (m_PlayerOptions.startpercent > 0 && m_pDemuxer)
    {
      int playerStartTime = (int)( ( (float) m_pDemuxer->GetStreamLength() ) * ( m_PlayerOptions.startpercent/(float)100 ) );
      starttime = m_Edl.RestoreCutTime(playerStartTime);
    }
    else
    {
      starttime = m_Edl.RestoreCutTime(m_PlayerOptions.starttime * 1000); // s to ms
    }
    CLog::Log(LOGDEBUG, "%s - Start position set to last stopped position: %d", __FUNCTION__, starttime);
  }
  else if (m_Edl.InCut(starttime, &cut))
  {
    if (cut.action == CEdl::CUT)
    {
      starttime = cut.end;
      CLog::Log(LOGDEBUG, "%s - Start position set to end of first cut: %d", __FUNCTION__, starttime);
    }
    else if (cut.action == CEdl::COMM_BREAK)
    {
      if (m_SkipCommercials)
      {
        starttime = cut.end;
        CLog::Log(LOGDEBUG, "%s - Start position set to end of first commercial break: %d", __FUNCTION__, starttime);
      }

      std::string strTimeString = StringUtils::SecondsToTimeString(cut.end / 1000, TIME_FORMAT_MM_SS);
      CGUIDialogKaiToast::QueueNotification(g_localizeStrings.Get(25013), strTimeString);
    }
  }
  if (starttime > 0)
  {
    double startpts = DVD_NOPTS_VALUE;
    if (m_pDemuxer)
    {
      if (m_pDemuxer->SeekTime(starttime, true, &startpts))
        CLog::Log(LOGDEBUG, "%s - starting demuxer from: %d", __FUNCTION__, starttime);
      else
        CLog::Log(LOGDEBUG, "%s - failed to start demuxing from: %d", __FUNCTION__, starttime);
    }

    if (m_pSubtitleDemuxer)
    {
      if(m_pSubtitleDemuxer->SeekTime(starttime, true, &startpts))
        CLog::Log(LOGDEBUG, "%s - starting subtitle demuxer from: %d", __FUNCTION__, starttime);
      else
        CLog::Log(LOGDEBUG, "%s - failed to start subtitle demuxing from: %d", __FUNCTION__, starttime);
    }

    m_clock.Discontinuity(DVD_MSEC_TO_TIME(starttime));
  }

  // make sure application know our info
  UpdateApplication(0);
  UpdatePlayState(0);

  if (m_PlayerOptions.identify == false)
    m_callback.OnPlayBackStarted();

  // we are done initializing now, set the readyevent
  m_ready.Set();

  SetCaching(CACHESTATE_FLUSH, __FUNCTION__);

  while (!m_bAbortRequest)
  {
    // check display state (lost, reset, present)
    if (m_displayState == AV_DISPLAY_LOST)
    {
      Sleep(50);
      continue;
    }
    else if (m_displayState == AV_DISPLAY_RESET)
    {
      if (m_displayResetTimer.GetElapsedMilliseconds() > m_displayResetDelay)
      {
        m_displayState = AV_DISPLAY_PRESENT;
        m_dvdPlayerAudio->SendMessage(new CDVDMsgBool(CDVDMsg::GENERAL_PAUSE, false), 1);
        m_dvdPlayerVideo->SendMessage(new CDVDMsgBool(CDVDMsg::GENERAL_PAUSE, false), 1);
        m_clock.Pause(false);
      }
      else
      {
        Sleep(50);
        continue;
      }
    }

    // check if in a cut or commercial break that should be automatically skipped
    CheckAutoSceneSkip();

    // handle messages send to this thread, like seek or demuxer reset requests
    HandleMessages();

    if(m_bAbortRequest)
      break;

    // should we open a new input stream?
    if(!m_pInputStream)
    {
      if (OpenInputStream() == false)
      {
        m_bAbortRequest = true;
        break;
      }
    }

    // should we open a new demuxer?
    if(!m_pDemuxer)
    {
      if (m_pInputStream->NextStream() == CDVDInputStream::NEXTSTREAM_NONE)
        break;

      if (m_pInputStream->IsEOF())
        break;

      if (OpenDemuxStream() == false)
      {
        m_bAbortRequest = true;
        break;
      }

      // on channel switch we don't want to close stream players at this
      // time. we'll get the stream change event later
      if (!m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER) ||
          !m_SelectionStreams.m_Streams.empty())
        OpenDefaultStreams();

      UpdateApplication(0);
      UpdatePlayState(0);
    }

    // handle eventual seeks due to playspeed
    HandlePlaySpeed();

    // update player state
    UpdatePlayState(200);

    // update application with our state
    UpdateApplication(1000);

    // make sure we run subtitle process here
    m_dvdPlayerSubtitle->Process(m_clock.GetClock() + m_State.time_offset - m_dvdPlayerVideo->GetSubtitleDelay(), m_State.time_offset);

    if (CheckDelayedChannelEntry())
      continue;

    // if the queues are full, no need to read more
    if ((!m_dvdPlayerAudio->AcceptsData() && m_CurrentAudio.id >= 0) ||
        (!m_dvdPlayerVideo->AcceptsData() && m_CurrentVideo.id >= 0))
    {
      Sleep(10);
      continue;
    }

    // always yield to players if they have data levels > 50 percent
    if((m_dvdPlayerAudio->GetLevel() > 50 || m_CurrentAudio.id < 0)
    && (m_dvdPlayerVideo->GetLevel() > 50 || m_CurrentVideo.id < 0))
      Sleep(0);

    DemuxPacket* pPacket = NULL;
    CDemuxStream *pStream = NULL;
    ReadPacket(pPacket, pStream);
    if (pPacket && !pStream)
    {
      /* probably a empty packet, just free it and move on */
      CDVDDemuxUtils::FreeDemuxPacket(pPacket);
      continue;
    }

    if (!pPacket)
    {
      // when paused, demuxer could be be returning empty
      if (m_playSpeed == DVD_PLAYSPEED_PAUSE)
        continue;

      // check for a still frame state
      if (CDVDInputStream::IMenus* pStream = dynamic_cast<CDVDInputStream::IMenus*>(m_pInputStream))
      {
        // stills will be skipped
        if(m_dvd.state == DVDSTATE_STILL)
        {
          if (m_dvd.iDVDStillTime > 0)
          {
            if ((XbmcThreads::SystemClockMillis() - m_dvd.iDVDStillStartTime) >= m_dvd.iDVDStillTime)
            {
              m_dvd.iDVDStillTime = 0;
              m_dvd.iDVDStillStartTime = 0;
              m_dvd.state = DVDSTATE_NORMAL;
              pStream->SkipStill();
              continue;
            }
          }
        }
      }

      // if there is another stream available, reopen demuxer
      CDVDInputStream::ENextStream next = m_pInputStream->NextStream();
      if(next == CDVDInputStream::NEXTSTREAM_OPEN)
      {
        SAFE_DELETE(m_pDemuxer);

        SetCaching(CACHESTATE_DONE, __FUNCTION__);
        CLog::Log(LOGNOTICE, "DVDPlayerPlayer: next stream, wait for old streams to be finished");
        CloseStream(m_CurrentAudio, true);
        CloseStream(m_CurrentVideo, true);

        m_CurrentAudio.Clear();
        m_CurrentVideo.Clear();
        m_CurrentSubtitle.Clear();
        continue;
      }

      // input stream asked us to just retry
      if(next == CDVDInputStream::NEXTSTREAM_RETRY)
      {
        Sleep(100);
        continue;
      }

      if(m_CurrentAudio.inited)
        m_dvdPlayerAudio->SendMessage(new CDVDMsg(CDVDMsg::GENERAL_EOF));
      if(m_CurrentVideo.inited)
        m_dvdPlayerVideo->SendMessage(new CDVDMsg(CDVDMsg::GENERAL_EOF));
      if(m_CurrentSubtitle.inited)
        m_dvdPlayerSubtitle->SendMessage(new CDVDMsg(CDVDMsg::GENERAL_EOF));
      if(m_CurrentTeletext.inited)
        m_dvdPlayerTeletext->SendMessage(new CDVDMsg(CDVDMsg::GENERAL_EOF));
      if(m_CurrentRadioRDS.inited)
        m_dvdPlayerRadioRDS->SendMessage(new CDVDMsg(CDVDMsg::GENERAL_EOF));

      m_CurrentAudio.inited = false;
      m_CurrentVideo.inited = false;
      m_CurrentSubtitle.inited = false;
      m_CurrentTeletext.inited = false;
      m_CurrentRadioRDS.inited = false;

      // if we are caching, start playing it again
      SetCaching(CACHESTATE_DONE, __FUNCTION__);

      // while players are still playing, keep going to allow seekbacks
      if(m_dvdPlayerAudio->HasData()
      || m_dvdPlayerVideo->HasData())
      {
        Sleep(100);
        continue;
      }

      if (!m_pInputStream->IsEOF())
        CLog::Log(LOGINFO, "%s - eof reading from demuxer", __FUNCTION__);

      break;
    }

    // it's a valid data packet, reset error counter
    m_errorCount = 0;

    // see if we can find something better to play
    CheckBetterStream(m_CurrentAudio,    pStream);
    CheckBetterStream(m_CurrentVideo,    pStream);
    CheckBetterStream(m_CurrentSubtitle, pStream);
    CheckBetterStream(m_CurrentTeletext, pStream);
    CheckBetterStream(m_CurrentRadioRDS, pStream);

    // demux video stream
    if (CSettings::GetInstance().GetBool(CSettings::SETTING_SUBTITLES_PARSECAPTIONS) && CheckIsCurrent(m_CurrentVideo, pStream, pPacket))
    {
      if (m_pCCDemuxer)
      {
        bool first = true;
        while(!m_bAbortRequest)
        {
          DemuxPacket *pkt = m_pCCDemuxer->Read(first ? pPacket : NULL);
          if (!pkt)
            break;

          first = false;
          if (m_pCCDemuxer->GetNrOfStreams() != m_SelectionStreams.CountSource(STREAM_SUBTITLE, STREAM_SOURCE_VIDEOMUX))
          {
            m_SelectionStreams.Clear(STREAM_SUBTITLE, STREAM_SOURCE_VIDEOMUX);
            m_SelectionStreams.Update(NULL, m_pCCDemuxer, "");
            OpenDefaultSubtitleStreams(false);
          }
          CDemuxStream *pSubStream = m_pCCDemuxer->GetStream(pkt->iStreamId);
          if (pSubStream && m_CurrentSubtitle.id == pkt->iStreamId && m_CurrentSubtitle.source == STREAM_SOURCE_VIDEOMUX)
            ProcessSubData(pSubStream, pkt);
          else
            CDVDDemuxUtils::FreeDemuxPacket(pkt);
        }
      }
    }

    if (IsInMenuInternal())
    {
      if (CDVDInputStream::IMenus* menu = dynamic_cast<CDVDInputStream::IMenus*>(m_pInputStream))
      {
        double correction = menu->GetTimeStampCorrection();
        if (pPacket->dts != DVD_NOPTS_VALUE && pPacket->dts > correction)
          pPacket->dts -= correction;
        if (pPacket->pts != DVD_NOPTS_VALUE && pPacket->pts > correction)
          pPacket->pts -= correction;
      }
      if (m_dvd.syncClock)
      {
        m_clock.Discontinuity(pPacket->dts);
        m_dvd.syncClock = false;
      }
    }

    // process the packet
    ProcessPacket(pStream, pPacket);

    // update the player info for streams
    if (m_player_status_timer.IsTimePast())
    {
      m_player_status_timer.Set(500);
      UpdateStreamInfos();
    }
  }
}

bool CDVDPlayer::CheckDelayedChannelEntry(void)
{
  bool bReturn(false);

  if (m_ChannelEntryTimeOut.IsTimePast())
  {
    CFileItem currentFile(g_application.CurrentFileItem());
    CPVRChannelPtr currentChannel(currentFile.GetPVRChannelInfoTag());
    if (currentChannel)
    {
      SwitchChannel(currentChannel);

      bReturn = true;
    }
    m_ChannelEntryTimeOut.SetInfinite();
  }

  return bReturn;
}

bool CDVDPlayer::CheckIsCurrent(CCurrentStream& current, CDemuxStream* stream, DemuxPacket* pkg)
{
  if(current.id     == pkg->iStreamId
  && current.source == stream->source
  && current.type   == stream->type)
    return true;
  else
    return false;
}

void CDVDPlayer::ProcessPacket(CDemuxStream* pStream, DemuxPacket* pPacket)
{
  // process packet if it belongs to selected stream.
  // for dvd's don't allow automatic opening of streams*/

  if (CheckIsCurrent(m_CurrentAudio, pStream, pPacket))
    ProcessAudioData(pStream, pPacket);
  else if (CheckIsCurrent(m_CurrentVideo, pStream, pPacket))
    ProcessVideoData(pStream, pPacket);
  else if (CheckIsCurrent(m_CurrentSubtitle, pStream, pPacket))
    ProcessSubData(pStream, pPacket);
  else if (CheckIsCurrent(m_CurrentTeletext, pStream, pPacket))
    ProcessTeletextData(pStream, pPacket);
  else if (CheckIsCurrent(m_CurrentRadioRDS, pStream, pPacket))
    ProcessRadioRDSData(pStream, pPacket);
  else
  {
    pStream->SetDiscard(AVDISCARD_ALL);
    CDVDDemuxUtils::FreeDemuxPacket(pPacket); // free it since we won't do anything with it
  }
}

void CDVDPlayer::CheckStreamChanges(CCurrentStream& current, CDemuxStream* stream)
{
  if (current.stream  != (void*)stream
  ||  current.changes != stream->changes)
  {
    /* check so that dmuxer hints or extra data hasn't changed */
    /* if they have, reopen stream */

    if (current.hint != CDVDStreamInfo(*stream, true))
    {
      CloseStream(current, false);
      OpenStream(current, stream->iId, stream->source );
    }

    current.stream = (void*)stream;
    current.changes = stream->changes;
  }
}

void CDVDPlayer::ProcessAudioData(CDemuxStream* pStream, DemuxPacket* pPacket)
{
  CheckStreamChanges(m_CurrentAudio, pStream);

  bool checkcont = CheckContinuity(m_CurrentAudio, pPacket);
  UpdateTimestamps(m_CurrentAudio, pPacket);

  if (checkcont && (m_CurrentAudio.avsync == CCurrentStream::AV_SYNC_CHECK))
    m_CurrentAudio.avsync = CCurrentStream::AV_SYNC_NONE;

  bool drop = false;
  if (CheckPlayerInit(m_CurrentAudio))
    drop = true;

  /*
   * If CheckSceneSkip() returns true then demux point is inside an EDL cut and the packets are dropped.
   */
  CEdl::Cut cut;
  if (CheckSceneSkip(m_CurrentAudio))
    drop = true;
  else if (m_Edl.InCut(DVD_TIME_TO_MSEC(m_CurrentAudio.dts + m_offset_pts), &cut) && cut.action == CEdl::MUTE)
  {
    drop = true;
  }

  m_dvdPlayerAudio->SendMessage(new CDVDMsgDemuxerPacket(pPacket, drop));
  m_CurrentAudio.packets++;
}

void CDVDPlayer::ProcessVideoData(CDemuxStream* pStream, DemuxPacket* pPacket)
{
  CheckStreamChanges(m_CurrentVideo, pStream);
  bool checkcont = false;

  if( pPacket->iSize != 4) //don't check the EOF_SEQUENCE of stillframes
  {
    checkcont = CheckContinuity(m_CurrentVideo, pPacket);
    UpdateTimestamps(m_CurrentVideo, pPacket);
  }
  if (checkcont && (m_CurrentVideo.avsync == CCurrentStream::AV_SYNC_CHECK))
    m_CurrentVideo.avsync = CCurrentStream::AV_SYNC_NONE;

  bool drop = false;
  if (CheckPlayerInit(m_CurrentVideo))
    drop = true;

  if (CheckSceneSkip(m_CurrentVideo))
    drop = true;

  m_dvdPlayerVideo->SendMessage(new CDVDMsgDemuxerPacket(pPacket, drop));
  m_CurrentVideo.packets++;
}

void CDVDPlayer::ProcessSubData(CDemuxStream* pStream, DemuxPacket* pPacket)
{
  CheckStreamChanges(m_CurrentSubtitle, pStream);

  UpdateTimestamps(m_CurrentSubtitle, pPacket);

  bool drop = false;
  if (CheckPlayerInit(m_CurrentSubtitle))
    drop = true;

  if (CheckSceneSkip(m_CurrentSubtitle))
    drop = true;

  m_dvdPlayerSubtitle->SendMessage(new CDVDMsgDemuxerPacket(pPacket, drop));

  if(m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
    m_dvdPlayerSubtitle->UpdateOverlayInfo((CDVDInputStreamNavigator*)m_pInputStream, LIBDVDNAV_BUTTON_NORMAL);
}

void CDVDPlayer::ProcessTeletextData(CDemuxStream* pStream, DemuxPacket* pPacket)
{
  CheckStreamChanges(m_CurrentTeletext, pStream);

  UpdateTimestamps(m_CurrentTeletext, pPacket);

  bool drop = false;
  if (CheckPlayerInit(m_CurrentTeletext))
    drop = true;

  if (CheckSceneSkip(m_CurrentTeletext))
    drop = true;

  m_dvdPlayerTeletext->SendMessage(new CDVDMsgDemuxerPacket(pPacket, drop));
}

void CDVDPlayer::ProcessRadioRDSData(CDemuxStream* pStream, DemuxPacket* pPacket)
{
  CheckStreamChanges(m_CurrentRadioRDS, pStream);

  UpdateTimestamps(m_CurrentRadioRDS, pPacket);

  bool drop = false;
  if (CheckPlayerInit(m_CurrentRadioRDS))
    drop = true;

  if (CheckSceneSkip(m_CurrentRadioRDS))
    drop = true;

  m_dvdPlayerRadioRDS->SendMessage(new CDVDMsgDemuxerPacket(pPacket, drop));
}

void CDVDPlayer::HandlePlaySpeed()
{
  bool isInMenu = IsInMenuInternal();

  if (isInMenu && m_caching != CACHESTATE_DONE)
    SetCaching(CACHESTATE_DONE, __FUNCTION__);

  if (m_caching == CACHESTATE_WAITFILL)
  {
    if ((!m_dvdPlayerAudio->AcceptsData() && m_CurrentAudio.id >= 0)
    ||  (!m_dvdPlayerVideo->AcceptsData() && m_CurrentVideo.id >= 0))
      SetCaching(CACHESTATE_WAITSTREAM, __FUNCTION__);
  }

  if (m_caching == CACHESTATE_WAITSTREAM)
  {
    // if all enabled streams have been inited we are done
    if ((m_CurrentVideo.id >= 0 || m_CurrentAudio.id >= 0) &&
        (m_CurrentVideo.id < 0 || m_CurrentVideo.syncState != IDVDStreamPlayer::SYNC_STARTING) &&
        (m_CurrentAudio.id < 0 || m_CurrentAudio.syncState != IDVDStreamPlayer::SYNC_STARTING))
      SetCaching(CACHESTATE_WAITCODEC, __FUNCTION__);

    // handle exceptions
    if (m_CurrentAudio.id >= 0 && m_CurrentVideo.id >= 0)
    {
      if ((!m_dvdPlayerAudio->AcceptsData() || !m_dvdPlayerVideo->AcceptsData()) &&
          m_cachingTimer.IsTimePast())
      {
        SetCaching(CACHESTATE_DONE, __FUNCTION__);
      }
    }
  }

  if (m_caching == CACHESTATE_WAITCODEC)
  {
    // if all enabled streams have started playing we are done
    if ((m_CurrentVideo.id < 0 || !m_dvdPlayerVideo->IsStalled()) &&
        (m_CurrentAudio.id < 0 || !m_dvdPlayerAudio->IsStalled()))
      SetCaching(CACHESTATE_DONE, __FUNCTION__);
  }

  if (m_caching == CACHESTATE_DONE)
  {
    if (m_playSpeed == DVD_PLAYSPEED_NORMAL && !isInMenu)
    {
      // take action is audio or video stream is stalled
      if (((m_dvdPlayerAudio->IsStalled() && m_CurrentAudio.inited) ||
           (m_dvdPlayerVideo->IsStalled() && m_CurrentVideo.inited)) &&
          m_syncTimer.IsTimePast())
      {
        if (m_pInputStream->IsRealtime())
        {
          if ((m_CurrentAudio.id >= 0 && m_CurrentAudio.syncState == IDVDStreamPlayer::SYNC_INSYNC && m_dvdPlayerAudio->IsStalled()) ||
              (m_CurrentVideo.id >= 0 && m_CurrentVideo.syncState == IDVDStreamPlayer::SYNC_INSYNC && m_dvdPlayerVideo->GetLevel() == 0))
          {
            CLog::Log(LOGDEBUG, "Stream stalled, start buffering. Audio: %d - Video: %d",
                                 m_dvdPlayerAudio->GetLevel(),m_dvdPlayerVideo->GetLevel());
            FlushBuffers(DVD_NOPTS_VALUE, true, true);
          }
        }
        else
        {
          // start caching if audio and video have run dry
          if (m_dvdPlayerAudio->GetLevel() <= 50 &&
              m_dvdPlayerVideo->GetLevel() <= 50)
          {
            SetCaching(CACHESTATE_WAITFILL, __FUNCTION__);
          }
          else if (m_CurrentAudio.id >= 0 && m_CurrentAudio.inited &&
                   m_CurrentAudio.syncState == IDVDStreamPlayer::SYNC_INSYNC &&
                   m_dvdPlayerAudio->GetLevel() == 0)
          {
            CLog::Log(LOGDEBUG,"CDVDPlayer::HandlePlaySpeed - audio stream stalled, triggering re-sync");
            FlushBuffers(DVD_NOPTS_VALUE, true, true);
            CDVDMsgPlayerSeek::CMode mode;
            mode.time = (int)GetTime();
            mode.backward = false;
            mode.accurate = true;
            mode.sync = true;
            m_messenger.Put(new CDVDMsgPlayerSeek(mode));
          }
        }
      }
      // care for live streams
      else if (m_pInputStream->IsRealtime())
      {
        if (m_CurrentAudio.id >= 0)
        {
          double adjust = -1.0; // a unique value
          if (m_clock.GetSpeedAdjust() >= 0)
          {
            bool video_under = m_CurrentVideo.id >= 0 ? (m_dvdPlayerVideo->GetLevel() < 10) : false;
            if (m_dvdPlayerAudio->GetLevel() < 5 || video_under)
            adjust = -0.01;
          }

          if (m_clock.GetSpeedAdjust() < 0)
          {
            bool video_over = m_CurrentVideo.id >= 0 ? (m_dvdPlayerVideo->GetLevel() > 10) : true;
            if (m_dvdPlayerAudio->GetLevel() > 5 && video_over)
              adjust = 0.0;
          }

          if (adjust != -1.0)
          {
            m_clock.SetSpeedAdjust(adjust);
            CLog::Log(LOGDEBUG, "CDVDPlayer::HandlePlaySpeed set clock adjust: %f", adjust);
          }
        }
      }
    }
  }

  // sync streams to clock
  if ((m_CurrentVideo.syncState == IDVDStreamPlayer::SYNC_WAITSYNC) ||
      (m_CurrentAudio.syncState == IDVDStreamPlayer::SYNC_WAITSYNC))
  {
    unsigned int threshold = 20;
    if (m_pInputStream->IsRealtime())
      threshold = 40;

/*
    bool video = m_CurrentVideo.id >= 0 && (m_CurrentVideo.syncState == IDVDStreamPlayer::SYNC_WAITSYNC) &&
                 (m_dvdPlayerVideo->GetLevel() > threshold);
    bool audio = m_CurrentAudio.id >= 0 && (m_CurrentAudio.syncState == IDVDStreamPlayer::SYNC_WAITSYNC) &&
                 (m_dvdPlayerAudio->GetLevel() > threshold);
*/
    bool video = m_CurrentVideo.id < 0 || (m_CurrentVideo.syncState == IDVDStreamPlayer::SYNC_WAITSYNC) ||
                 (m_CurrentVideo.packets == 0 && m_CurrentAudio.packets > threshold);
    bool audio = m_CurrentAudio.id < 0 || (m_CurrentAudio.syncState == IDVDStreamPlayer::SYNC_WAITSYNC) ||
                 (m_CurrentAudio.packets == 0 && m_CurrentVideo.packets > threshold);

    if (m_CurrentAudio.syncState == IDVDStreamPlayer::SYNC_WAITSYNC &&
        (m_CurrentAudio.avsync == CCurrentStream::AV_SYNC_CONT ||
         m_CurrentVideo.syncState == IDVDStreamPlayer::SYNC_INSYNC))
    {
      m_CurrentAudio.syncState = IDVDStreamPlayer::SYNC_INSYNC;
      m_CurrentAudio.avsync = CCurrentStream::AV_SYNC_NONE;
      m_dvdPlayerAudio->SendMessage(new CDVDMsgDouble(CDVDMsg::GENERAL_RESYNC, m_clock.GetClock()), 1);
    }
    else if (m_CurrentVideo.syncState == IDVDStreamPlayer::SYNC_WAITSYNC &&
             m_CurrentVideo.avsync == CCurrentStream::AV_SYNC_CONT)
    {
      m_CurrentVideo.syncState = IDVDStreamPlayer::SYNC_INSYNC;
      m_CurrentVideo.avsync = CCurrentStream::AV_SYNC_NONE;
      m_dvdPlayerVideo->SendMessage(new CDVDMsgDouble(CDVDMsg::GENERAL_RESYNC, m_clock.GetClock()), 1);
    }
    else if (video && audio)
    {
      double clock = 0;
      // default to video starttime (in case of no audio stream)
      // fixes seeks in video with no audio.
      if (m_CurrentVideo.id >= 0 && m_CurrentVideo.starttime != DVD_NOPTS_VALUE)
        clock = m_CurrentVideo.starttime - m_CurrentVideo.cachetotal;

      if (m_CurrentAudio.syncState == IDVDStreamPlayer::SYNC_WAITSYNC)
        CLog::Log(LOGDEBUG, "CDVDPlayer::Sync - Audio - pts: %f, cache: %f, totalcache: %f",
                             m_CurrentAudio.starttime, m_CurrentAudio.cachetime, m_CurrentAudio.cachetotal);
      if (m_CurrentVideo.syncState == IDVDStreamPlayer::SYNC_WAITSYNC)
        CLog::Log(LOGDEBUG, "CDVDPlayer::Sync - Video - pts: %f, cache: %f, totalcache: %f",
                             m_CurrentVideo.starttime, m_CurrentVideo.cachetime, m_CurrentVideo.cachetotal);

      if (m_CurrentVideo.starttime != DVD_NOPTS_VALUE && m_CurrentVideo.packets > 0 &&
        m_playSpeed == DVD_PLAYSPEED_PAUSE)
      {
        clock = m_CurrentVideo.starttime;
      }
      else if (m_CurrentAudio.starttime != DVD_NOPTS_VALUE && m_CurrentAudio.packets > 0)
      {
        if (m_pInputStream->IsRealtime())
        {
          int livetvwait = CSettings::GetInstance().GetInt(CSettings::SETTING_PVRPLAYBACK_LIVETVWAIT);
          clock = m_CurrentAudio.starttime - m_CurrentAudio.cachetotal - DVD_MSEC_TO_TIME(livetvwait);
        }
        else
          clock = m_CurrentAudio.starttime - m_CurrentAudio.cachetime;

        if (m_CurrentVideo.starttime != DVD_NOPTS_VALUE && (m_CurrentVideo.packets > 0))
        {
          if (m_CurrentVideo.starttime - m_CurrentVideo.cachetotal < clock)
            clock = m_CurrentVideo.starttime - m_CurrentVideo.cachetotal;
          else if (m_CurrentVideo.starttime > m_CurrentAudio.starttime)
          {
            int audioLevel = m_dvdPlayerAudio->GetLevel();
            //@todo hardcoded 8 seconds in message queue
            double maxAudioTime = clock + DVD_MSEC_TO_TIME(80 * audioLevel);
            if ((m_CurrentVideo.starttime - m_CurrentVideo.cachetotal) > maxAudioTime)
              clock = maxAudioTime;
            else
              clock = m_CurrentVideo.starttime - m_CurrentVideo.cachetotal;
          }
        }
      }
      else if (m_CurrentVideo.starttime != DVD_NOPTS_VALUE && m_CurrentVideo.packets > 0)
      {
        clock = m_CurrentVideo.starttime - m_CurrentVideo.cachetotal;
      }
      m_clock.Discontinuity(clock);
      m_CurrentAudio.syncState = IDVDStreamPlayer::SYNC_INSYNC;
      m_CurrentAudio.avsync = CCurrentStream::AV_SYNC_NONE;
      m_CurrentVideo.syncState = IDVDStreamPlayer::SYNC_INSYNC;
      m_CurrentVideo.avsync = CCurrentStream::AV_SYNC_NONE;
      m_dvdPlayerAudio->SendMessage(new CDVDMsgDouble(CDVDMsg::GENERAL_RESYNC, clock), 1);
      m_dvdPlayerVideo->SendMessage(new CDVDMsgDouble(CDVDMsg::GENERAL_RESYNC, clock), 1);
      SetCaching(CACHESTATE_DONE, __FUNCTION__);
      UpdatePlayState(0);

      m_syncTimer.Set(3000);
    }
    else
    {
      // exceptions for which stream players won't start properly
      // 1. videoplayer has not detected a keyframe within lenght of demux buffers
      if (m_CurrentAudio.id >= 0 && m_CurrentVideo.id >= 0 &&
          !m_dvdPlayerAudio->AcceptsData() &&
          m_CurrentVideo.syncState == IDVDStreamPlayer::SYNC_STARTING &&
          m_dvdPlayerVideo->IsStalled())
      {
        CLog::Log(LOGWARNING, "CDVDPlayer::Sync - stream player video does not start, flushing buffers");
        FlushBuffers(DVD_NOPTS_VALUE, true, true);
      }
    }
  }

  // handle ff/rw
  if(m_playSpeed != DVD_PLAYSPEED_NORMAL && m_playSpeed != DVD_PLAYSPEED_PAUSE)
  {
    if (isInMenu)
    {
      // this can't be done in menu
      SetPlaySpeed(DVD_PLAYSPEED_NORMAL);

    }
    else
    {
      bool check = true;

      // only check if we have video
      if (m_CurrentVideo.id < 0 || m_CurrentVideo.syncState != IDVDStreamPlayer::SYNC_INSYNC)
        check = false;
      // video message queue either initiated or already seen eof
      else if (m_CurrentVideo.inited == false && m_playSpeed >= 0)
        check = false;
      // don't check if time has not advanced since last check
      else if (m_SpeedState.lasttime == GetTime())
        check = false;
      // skip if frame at screen has no valid timestamp
      else if (m_dvdPlayerVideo->GetCurrentPts() == DVD_NOPTS_VALUE)
        check = false;
      // skip if frame on screen has not changed
      else if (m_SpeedState.lastpts == m_dvdPlayerVideo->GetCurrentPts() &&
               (m_SpeedState.lastpts > m_State.dts || m_playSpeed > 0))
        check = false;

      if (check)
      {
        m_SpeedState.lastpts  = m_dvdPlayerVideo->GetCurrentPts();
        m_SpeedState.lasttime = GetTime();
        m_SpeedState.lastabstime = m_clock.GetAbsoluteClock();
        // check how much off clock video is when ff/rw:ing
        // a problem here is that seeking isn't very accurate
        // and since the clock will be resynced after seek
        // we might actually not really be playing at the wanted
        // speed. we'd need to have some way to not resync the clock
        // after a seek to remember timing. still need to handle
        // discontinuities somehow

        double error;
        error  = m_clock.GetClock() - m_SpeedState.lastpts;
        error *= m_playSpeed / abs(m_playSpeed);

        // allow a bigger error when going ff, the faster we go
        // the the bigger is the error we allow
        if (m_playSpeed > DVD_PLAYSPEED_NORMAL)
        {
          int errorwin = m_playSpeed / DVD_PLAYSPEED_NORMAL;
          if (errorwin > 8)
            errorwin = 8;
          error /= errorwin;
        }

        if(error > DVD_MSEC_TO_TIME(1000))
        {
          error  = (int)DVD_TIME_TO_MSEC(m_clock.GetClock()) - m_SpeedState.lastseekpts;

          if(std::abs(error) > 1000)
          {
            CLog::Log(LOGDEBUG, "CDVDPlayer::Process - Seeking to catch up");
            m_SpeedState.lastseekpts = (int)DVD_TIME_TO_MSEC(m_clock.GetClock());
            int direction = (m_playSpeed > 0) ? 1 : -1;
            int iTime = DVD_TIME_TO_MSEC(m_clock.GetClock() + m_State.time_offset + 1000000.0 * direction);
            CDVDMsgPlayerSeek::CMode mode;
            mode.time = iTime;
            mode.backward = (GetPlaySpeed() < 0);
            mode.accurate = false;
            mode.restore = false;
            mode.trickplay = true;
            mode.sync = false;
            m_messenger.Put(new CDVDMsgPlayerSeek(mode));
          }
        }
      }
    }
  }
}

bool CDVDPlayer::CheckPlayerInit(CCurrentStream& current)
{
  if (current.inited)
    return false;

  if (current.startpts != DVD_NOPTS_VALUE)
  {
    if(current.dts == DVD_NOPTS_VALUE)
    {
      //CLog::Log(LOGDEBUG, "%s - dropping packet type:%d dts:%f to get to start point at %f", __FUNCTION__, current.player,  current.dts, current.startpts);
      return true;
    }

    if ((current.startpts - current.dts) > DVD_SEC_TO_TIME(20))
    {
      CLog::Log(LOGDEBUG, "%s - too far to decode before finishing seek", __FUNCTION__);
      if(m_CurrentAudio.startpts != DVD_NOPTS_VALUE)
        m_CurrentAudio.startpts = current.dts;
      if(m_CurrentVideo.startpts != DVD_NOPTS_VALUE)
        m_CurrentVideo.startpts = current.dts;
      if(m_CurrentSubtitle.startpts != DVD_NOPTS_VALUE)
        m_CurrentSubtitle.startpts = current.dts;
      if(m_CurrentTeletext.startpts != DVD_NOPTS_VALUE)
        m_CurrentTeletext.startpts = current.dts;
      if(m_CurrentRadioRDS.startpts != DVD_NOPTS_VALUE)
        m_CurrentRadioRDS.startpts = current.dts;
    }

    if(current.dts < current.startpts)
    {
      //CLog::Log(LOGDEBUG, "%s - dropping packet type:%d dts:%f to get to start point at %f", __FUNCTION__, current.player,  current.dts, current.startpts);
      return true;
    }
  }

  if (current.dts != DVD_NOPTS_VALUE)
  {
    current.inited = true;
    current.startpts = current.dts;
/*
    bool setclock = false;
    if(m_playSpeed == DVD_PLAYSPEED_NORMAL)
    {
      if(     current.player == DVDPLAYER_AUDIO)
        setclock = m_clock.GetMaster() == MASTER_CLOCK_AUDIO
                || m_clock.GetMaster() == MASTER_CLOCK_AUDIO_VIDEOREF
                || !m_CurrentVideo.inited;
      else if(current.player == DVDPLAYER_VIDEO)
        setclock = m_clock.GetMaster() == MASTER_CLOCK_VIDEO
                || !m_CurrentAudio.inited;
    }
    else
    {
      if(current.player == DVDPLAYER_VIDEO)
        setclock = true;
    }

    double starttime = current.startpts;
    if(m_CurrentAudio.inited
    && m_CurrentAudio.startpts != DVD_NOPTS_VALUE
    && m_CurrentAudio.startpts < starttime)
      starttime = m_CurrentAudio.startpts;
    if(m_CurrentVideo.inited
    && m_CurrentVideo.startpts != DVD_NOPTS_VALUE
    && m_CurrentVideo.startpts < starttime)
      starttime = m_CurrentVideo.startpts;

    starttime = current.startpts - starttime;
    if(starttime > 0 && setclock)
    {
      if(starttime > DVD_SEC_TO_TIME(2))
        CLog::Log(LOGWARNING, "CDVDPlayer::CheckPlayerInit(%d) - Ignoring too large delay of %f", current.player, starttime);
      else
        SendPlayerMessage(new CDVDMsgDouble(CDVDMsg::GENERAL_DELAY, starttime), current.player);
    }

    SendPlayerMessage(new CDVDMsgGeneralResync(current.dts, setclock), current.player);
*/
  }
  return false;
}

void CDVDPlayer::UpdateCorrection(DemuxPacket* pkt, double correction)
{
  if(pkt->dts != DVD_NOPTS_VALUE)
    pkt->dts -= correction;
  if(pkt->pts != DVD_NOPTS_VALUE)
    pkt->pts -= correction;
}

void CDVDPlayer::UpdateTimestamps(CCurrentStream& current, DemuxPacket* pPacket)
{
  double dts = current.dts;
  /* update stored values */
  if(pPacket->dts != DVD_NOPTS_VALUE)
    dts = pPacket->dts;
  else if(pPacket->pts != DVD_NOPTS_VALUE)
    dts = pPacket->pts;

  /* calculate some average duration */
  if(pPacket->duration != DVD_NOPTS_VALUE)
    current.dur = pPacket->duration;
  else if(dts != DVD_NOPTS_VALUE && current.dts != DVD_NOPTS_VALUE)
    current.dur = 0.1 * (current.dur * 9 + (dts - current.dts));

  current.dts = dts;
}

static void UpdateLimits(double& minimum, double& maximum, double dts)
{
  if(dts == DVD_NOPTS_VALUE)
    return;
  if(minimum == DVD_NOPTS_VALUE || minimum > dts) minimum = dts;
  if(maximum == DVD_NOPTS_VALUE || maximum < dts) maximum = dts;
}

bool CDVDPlayer::CheckContinuity(CCurrentStream& current, DemuxPacket* pPacket)
{
  if (m_playSpeed < DVD_PLAYSPEED_PAUSE)
    return false;

  if( pPacket->dts == DVD_NOPTS_VALUE || current.dts == DVD_NOPTS_VALUE)
    return false;

  double mindts = DVD_NOPTS_VALUE, maxdts = DVD_NOPTS_VALUE;
  UpdateLimits(mindts, maxdts, m_CurrentAudio.dts);
  UpdateLimits(mindts, maxdts, m_CurrentVideo.dts);
  UpdateLimits(mindts, maxdts, m_CurrentAudio.dts_end());
  UpdateLimits(mindts, maxdts, m_CurrentVideo.dts_end());

  /* if we don't have max and min, we can't do anything more */
  if( mindts == DVD_NOPTS_VALUE || maxdts == DVD_NOPTS_VALUE )
    return false;

  double correction = 0.0;
  if( pPacket->dts > maxdts + DVD_MSEC_TO_TIME(1000))
  {
    //CLog::Log(LOGDEBUG, "CDVDPlayer::CheckContinuity - resync forward :%d, prev:%f, curr:%f, diff:%f"
    //                        , current.type, current.dts, pPacket->dts, pPacket->dts - maxdts);
    correction = pPacket->dts - maxdts;
  }

  /* if it's large scale jump, correct for it after having confirmed the jump */
  if(pPacket->dts + DVD_MSEC_TO_TIME(500) < current.dts_end())
  {
    //CLog::Log(LOGDEBUG, "CDVDPlayer::CheckContinuity - resync backward :%d, prev:%f, curr:%f, diff:%f"
    //                        , current.type, current.dts, pPacket->dts, pPacket->dts - current.dts);
    correction = pPacket->dts - current.dts_end();
  }
  else if(pPacket->dts < current.dts)
  {
    CLog::Log(LOGDEBUG, "CDVDPlayer::CheckContinuity - wrapback :%d, prev:%f, curr:%f, diff:%f"
                            , current.type, current.dts, pPacket->dts, pPacket->dts - current.dts);
  }

  double lastdts = pPacket->dts;
  if(correction != 0.0)
  {
    // we want the dts values of two streams to close, or for one to be invalid (e.g. from a missing audio stream)
    double this_dts = pPacket->dts;
    double that_dts = current.type == STREAM_AUDIO ? m_CurrentVideo.lastdts : m_CurrentAudio.lastdts;

    if (m_CurrentAudio.id == -1 || m_CurrentVideo.id == -1 ||
       current.lastdts == DVD_NOPTS_VALUE ||
       fabs(this_dts - that_dts) < DVD_MSEC_TO_TIME(1000))
    {
      m_offset_pts += correction;
      UpdateCorrection(pPacket, correction);
      lastdts = pPacket->dts;
    }
    else
    {
      // not sure yet - flags the packets as unknown until we get confirmation on another audio/video packet
      pPacket->dts = DVD_NOPTS_VALUE;
      pPacket->pts = DVD_NOPTS_VALUE;
    }
  }
  else
  {
    if (current.avsync == CCurrentStream::AV_SYNC_CHECK)
      current.avsync = CCurrentStream::AV_SYNC_CONT;
  }
  current.lastdts = lastdts;
  return true;
}

bool CDVDPlayer::CheckSceneSkip(CCurrentStream& current)
{
  if(!m_Edl.HasCut())
    return false;

  if(current.dts == DVD_NOPTS_VALUE)
    return false;

  if(current.inited == false)
    return false;

  CEdl::Cut cut;
  return m_Edl.InCut(DVD_TIME_TO_MSEC(current.dts + m_offset_pts), &cut) && cut.action == CEdl::CUT;
}

void CDVDPlayer::CheckAutoSceneSkip()
{
  if (!m_Edl.HasCut())
    return;

  // Check that there is an audio and video stream.
  if((m_CurrentAudio.id < 0 || m_CurrentAudio.syncState != IDVDStreamPlayer::SYNC_INSYNC) ||
     (m_CurrentVideo.id < 0 || m_CurrentVideo.syncState != IDVDStreamPlayer::SYNC_INSYNC))
    return;

  // If there is a startpts defined for either the audio or video stream then VideoPlayer is still
  // still decoding frames to get to the previously requested seek point.
  if (m_CurrentAudio.inited == false ||
      m_CurrentVideo.inited == false)
    return;

  const int64_t clock = GetTime();

  CEdl::Cut cut;
  if (!m_Edl.InCut(clock, &cut))
    return;

  if (cut.action == CEdl::CUT)
  {
    if ((GetPlaySpeed() > 0 && clock < cut.end - 1000) ||
        (GetPlaySpeed() < 0 && clock < cut.start + 1000))
    {
      CLog::Log(LOGDEBUG, "%s - Clock in EDL cut [%s - %s]: %s. Automatically skipping over.",
                __FUNCTION__, CEdl::MillisecondsToTimeString(cut.start).c_str(),
                CEdl::MillisecondsToTimeString(cut.end).c_str(), CEdl::MillisecondsToTimeString(clock).c_str());

      //Seeking either goes to the start or the end of the cut depending on the play direction.
      int seek = GetPlaySpeed() >= 0 ? cut.end : cut.start;

      CDVDMsgPlayerSeek::CMode mode;
      mode.time = seek;
      mode.backward = true;
      mode.accurate = true;
      mode.restore = true;
      mode.trickplay = false;
      mode.sync = true;
      m_messenger.Put(new CDVDMsgPlayerSeek(mode));
    }
  }
  else if (cut.action == CEdl::COMM_BREAK)
  {
    // marker for commbrak may be inaccurate. allow user to skip into break from the back
    if (GetPlaySpeed() >= 0 && m_Edl.GetLastCutTime() != cut.start && clock < cut.end - 1000)
    {
      std::string strTimeString = StringUtils::SecondsToTimeString((cut.end - cut.start) / 1000, TIME_FORMAT_MM_SS);
      CGUIDialogKaiToast::QueueNotification(g_localizeStrings.Get(25013), strTimeString);

      m_Edl.SetLastCutTime(cut.start);

      if (m_SkipCommercials)
      {
        CLog::Log(LOGDEBUG, "%s - Clock in commercial break [%s - %s]: %s. Automatically skipping to end of commercial break",
                  __FUNCTION__, CEdl::MillisecondsToTimeString(cut.start).c_str(),
                  CEdl::MillisecondsToTimeString(cut.end).c_str(),
                  CEdl::MillisecondsToTimeString(clock).c_str());

        CDVDMsgPlayerSeek::CMode mode;
        mode.time = cut.end;
        mode.backward = true;
        mode.accurate = true;
        mode.restore = false;
        mode.trickplay = false;
        mode.sync = true;
        m_messenger.Put(new CDVDMsgPlayerSeek(mode));
      }
    }
  }
}


void CDVDPlayer::SynchronizeDemuxer()
{
  if(IsCurrentThread())
    return;
  if(!m_messenger.IsInited())
    return;

  CDVDMsgGeneralSynchronize* message = new CDVDMsgGeneralSynchronize(500, 0);
  m_messenger.Put(message->Acquire());
  message->Wait(&m_bStop, 0);
  message->Release();
}

void CDVDPlayer::SynchronizePlayers(unsigned int sources)
{
  /* we need a big timeout as audio queue is about 8seconds for 2ch ac3 */
  const int timeout = 10*1000; // in milliseconds

  CDVDMsgGeneralSynchronize* message = new CDVDMsgGeneralSynchronize(timeout, sources);
  if (m_CurrentAudio.id >= 0)
    m_dvdPlayerAudio->SendMessage(message->Acquire());

  if (m_CurrentVideo.id >= 0)
    m_dvdPlayerVideo->SendMessage(message->Acquire());
/* TODO - we have to rewrite the sync class, to not require
          all other players waiting for subtitle, should only
          be the oposite way
  if (m_CurrentSubtitle.id >= 0)
    m_dvdPlayerSubtitle->SendMessage(message->Acquire());
*/
  message->Release();
}

IDVDStreamPlayer* CDVDPlayer::GetStreamPlayer(unsigned int target)
{
  if(target == DVDPLAYER_AUDIO)
    return m_dvdPlayerAudio;
  if(target == DVDPLAYER_VIDEO)
    return m_dvdPlayerVideo;
  if(target == DVDPLAYER_SUBTITLE)
    return m_dvdPlayerSubtitle;
  if(target == DVDPLAYER_TELETEXT)
    return m_dvdPlayerTeletext;
  if(target == DVDPLAYER_RDS)
    return m_dvdPlayerRadioRDS;
  return NULL;
}

void CDVDPlayer::SendPlayerMessage(CDVDMsg* pMsg, unsigned int target)
{
  IDVDStreamPlayer* player = GetStreamPlayer(target);
  if(player)
    player->SendMessage(pMsg, 0);
}

void CDVDPlayer::OnExit()
{
    CLog::Log(LOGNOTICE, "CDVDPlayer::OnExit()");

    // set event to inform openfile something went wrong in case openfile is still waiting for this event
    SetCaching(CACHESTATE_DONE, __FUNCTION__);

    // close each stream
    if (!m_bAbortRequest) CLog::Log(LOGNOTICE, "DVDPlayer: eof, waiting for queues to empty");
    CloseStream(m_CurrentAudio,    !m_bAbortRequest);
    CloseStream(m_CurrentVideo,    !m_bAbortRequest);

    // the generalization principle was abused for subtitle player. actually it is not a stream player like
    // video and audio. subtitle player does not run on its own thread, hence waitForBuffers makes
    // no sense here. waitForBuffers is abused to clear overlay container (false clears container)
    // subtitles are added from video player. after video player has finished, overlays have to be cleared.
    CloseStream(m_CurrentSubtitle, false);  // clear overlay container

    CloseStream(m_CurrentTeletext, !m_bAbortRequest);
    CloseStream(m_CurrentRadioRDS, !m_bAbortRequest);

    // destroy objects
    SAFE_DELETE(m_pDemuxer);
    SAFE_DELETE(m_pSubtitleDemuxer);
    SAFE_DELETE(m_pCCDemuxer);
    SAFE_DELETE(m_pInputStream);

    // clean up all selection streams
    m_SelectionStreams.Clear(STREAM_NONE, STREAM_SOURCE_NONE);

    m_messenger.End();


  m_bStop = true;
  // if we didn't stop playing, advance to the next item in xbmc's playlist
  if(m_PlayerOptions.identify == false)
  {
    if (m_bAbortRequest)
      m_callback.OnPlayBackStopped();
    else
      m_callback.OnPlayBackEnded();
  }

  // set event to inform openfile something went wrong in case openfile is still waiting for this event
  m_ready.Set();
}

void CDVDPlayer::HandleMessages()
{
  CDVDMsg* pMsg;

  while (m_messenger.Get(&pMsg, 0) == MSGQ_OK)
  {

      if (pMsg->IsType(CDVDMsg::PLAYER_SEEK) && m_messenger.GetPacketCount(CDVDMsg::PLAYER_SEEK)         == 0
                                             && m_messenger.GetPacketCount(CDVDMsg::PLAYER_SEEK_CHAPTER) == 0)
      {
        CDVDMsgPlayerSeek &msg(*((CDVDMsgPlayerSeek*)pMsg));

        if (!m_State.canseek)
        {
          pMsg->Release();
          continue;
        }

        if(!msg.GetTrickPlay())
        {
          g_infoManager.SetDisplayAfterSeek(100000);
          SetCaching(CACHESTATE_FLUSH, __FUNCTION__);
        }

        double start = DVD_NOPTS_VALUE;

        double time = msg.GetTime();
        if (msg.GetRelative())
          time = (m_clock.GetClock() + m_State.time_offset) / 1000l + time;

        time = msg.GetRestore() ? m_Edl.RestoreCutTime(time) : time;

        // if input stream doesn't support ISeekTime, convert back to pts
        // TODO:
        // After demuxer we add an offset to input pts so that displayed time and clock are
        // increasing steadily. For seeking we need to determine the boundaries and offset
        // of the desired segment. With the current approach calculated time may point
        // to nirvana
        if(dynamic_cast<CDVDInputStream::ISeekTime*>(m_pInputStream) == NULL)
          time += DVD_TIME_TO_MSEC(m_offset_pts - m_State.time_offset);

        CLog::Log(LOGDEBUG, "demuxer seek to: %f (ms)", time);
        if (m_pDemuxer && m_pDemuxer->SeekTime(time, msg.GetBackward(), &start))
        {
          CLog::Log(LOGDEBUG, "demuxer seek to: %f (ms), success", time);
          if(m_pSubtitleDemuxer)
          {
            if(!m_pSubtitleDemuxer->SeekTime(time, msg.GetBackward()))
              CLog::Log(LOGDEBUG, "failed to seek subtitle demuxer: %f, success", time);
          }
          // dts after successful seek
          if (start == DVD_NOPTS_VALUE)
            m_State.dts = DVD_MSEC_TO_TIME(time) - m_State.time_offset;
          else
          {
            start -= m_offset_pts;
            m_State.dts = start;
          }

          FlushBuffers(start, msg.GetAccurate(), msg.GetSync());
        }
        else if (m_pDemuxer)
        {
          CLog::Log(LOGDEBUG, "VideoPlayer: seek failed or hit end of stream");
          // dts after successful seek
          if (start == DVD_NOPTS_VALUE)
            start = DVD_MSEC_TO_TIME(time) - m_State.time_offset;

          m_State.dts = start;

          FlushBuffers(start, false, true);
          if (m_playSpeed != DVD_PLAYSPEED_PAUSE)
          {
            SetPlaySpeed(DVD_PLAYSPEED_NORMAL);
          }
        }

        // set flag to indicate we have finished a seeking request
        if(!msg.GetTrickPlay())
          g_infoManager.SetDisplayAfterSeek();

        // dvd's will issue a HOP_CHANNEL that we need to skip
        if(m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
          m_dvd.state = DVDSTATE_SEEK;
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_SEEK_CHAPTER) && m_messenger.GetPacketCount(CDVDMsg::PLAYER_SEEK)         == 0
                                                          && m_messenger.GetPacketCount(CDVDMsg::PLAYER_SEEK_CHAPTER) == 0)
      {
        g_infoManager.SetDisplayAfterSeek(100000);
        SetCaching(CACHESTATE_FLUSH, __FUNCTION__);

        CDVDMsgPlayerSeekChapter &msg(*((CDVDMsgPlayerSeekChapter*)pMsg));
        double start = DVD_NOPTS_VALUE;
        double offset = 0;
        int64_t beforeSeek = GetTime();

        // This should always be the case.
        if(m_pDemuxer && m_pDemuxer->SeekChapter(msg.GetChapter(), &start))
        {
          if (start != DVD_NOPTS_VALUE)
            start -= m_offset_pts;
          FlushBuffers(start, true, true);
          offset = DVD_TIME_TO_MSEC(start) - beforeSeek;
          m_callback.OnPlayBackSeek(beforeSeek, offset);
          m_callback.OnPlayBackSeekChapter(msg.GetChapter());
        }

        g_infoManager.SetDisplayAfterSeek(2500, offset);
      }
      else if (pMsg->IsType(CDVDMsg::DEMUXER_RESET))
      {
          m_CurrentAudio.stream = NULL;
          m_CurrentVideo.stream = NULL;
          m_CurrentSubtitle.stream = NULL;

          // we need to reset the demuxer, probably because the streams have changed
          if(m_pDemuxer)
            m_pDemuxer->Reset();
          if(m_pSubtitleDemuxer)
            m_pSubtitleDemuxer->Reset();
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_SET_AUDIOSTREAM))
      {
        CDVDMsgPlayerSetAudioStream* pMsg2 = (CDVDMsgPlayerSetAudioStream*)pMsg;

        SelectionStream& st = m_SelectionStreams.Get(STREAM_AUDIO, pMsg2->GetStreamId());
        if(st.source != STREAM_SOURCE_NONE)
        {
          if(st.source == STREAM_SOURCE_NAV && m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
          {
            CDVDInputStreamNavigator* pStream = (CDVDInputStreamNavigator*)m_pInputStream;
            if(pStream->SetActiveAudioStream(st.id))
            {
              m_dvd.iSelectedAudioStream = -1;
              CloseStream(m_CurrentAudio, false);
              CDVDMsgPlayerSeek::CMode mode;
              mode.time = (int)GetTime();
              mode.backward = true;
              mode.accurate = true;
              mode.trickplay = true;
              mode.sync = true;
              m_messenger.Put(new CDVDMsgPlayerSeek(mode));
            }
          }
          else
          {
            CloseStream(m_CurrentAudio, false);
            OpenStream(m_CurrentAudio, st.id, st.source);
            AdaptForcedSubtitles();
            CDVDMsgPlayerSeek::CMode mode;
            mode.time = (int)GetTime();
            mode.backward = true;
            mode.accurate = true;
            mode.trickplay = true;
            mode.sync = true;
            m_messenger.Put(new CDVDMsgPlayerSeek(mode));
          }
        }
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_SET_VIDEOSTREAM))
      {
        CDVDMsgPlayerSetVideoStream* pMsg2 = (CDVDMsgPlayerSetVideoStream*)pMsg;

        SelectionStream& st = m_SelectionStreams.Get(STREAM_VIDEO, pMsg2->GetStreamId());
        if (st.source != STREAM_SOURCE_NONE)
        {
          if (st.source == STREAM_SOURCE_NAV && m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
          {
            CDVDInputStreamNavigator* pStream = (CDVDInputStreamNavigator*)m_pInputStream;
            if (pStream->SetAngle(st.id))
            {
              m_dvd.iSelectedVideoStream = st.id;
              CDVDMsgPlayerSeek::CMode mode;
              mode.time = (int)GetTime();
              mode.backward = true;
              mode.accurate = true;
              mode.trickplay = true;
              mode.sync = true;
              m_messenger.Put(new CDVDMsgPlayerSeek(mode));
            }
          }
          else
          {
            CloseStream(m_CurrentVideo, false);
            OpenStream(m_CurrentVideo, st.id, st.source);
            CDVDMsgPlayerSeek::CMode mode;
            mode.time = (int)GetTime();
            mode.backward = true;
            mode.accurate = true;
            mode.trickplay = true;
            mode.sync = true;
            m_messenger.Put(new CDVDMsgPlayerSeek(mode));          }
        }
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_SET_SUBTITLESTREAM))
      {
        CDVDMsgPlayerSetSubtitleStream* pMsg2 = (CDVDMsgPlayerSetSubtitleStream*)pMsg;

        SelectionStream& st = m_SelectionStreams.Get(STREAM_SUBTITLE, pMsg2->GetStreamId());
        if(st.source != STREAM_SOURCE_NONE)
        {
          if(st.source == STREAM_SOURCE_NAV && m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
          {
            CDVDInputStreamNavigator* pStream = (CDVDInputStreamNavigator*)m_pInputStream;
            if(pStream->SetActiveSubtitleStream(st.id))
            {
              m_dvd.iSelectedSPUStream = -1;
              CloseStream(m_CurrentSubtitle, false);
            }
          }
          else
          {
            CloseStream(m_CurrentSubtitle, false);
            OpenStream(m_CurrentSubtitle, st.id, st.source);
          }
        }
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_SET_SUBTITLESTREAM_VISIBLE))
      {
        CDVDMsgBool* pValue = (CDVDMsgBool*)pMsg;
        SetSubtitleVisibleInternal(pValue->m_value);
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_SET_STATE))
      {
        g_infoManager.SetDisplayAfterSeek(100000);
        SetCaching(CACHESTATE_FLUSH, __FUNCTION__);

        CDVDMsgPlayerSetState* pMsgPlayerSetState = (CDVDMsgPlayerSetState*)pMsg;

        if (CDVDInputStream::IMenus* ptr = dynamic_cast<CDVDInputStream::IMenus*>(m_pInputStream))
        {
          if(ptr->SetState(pMsgPlayerSetState->GetState()))
          {
            m_dvd.state = DVDSTATE_NORMAL;
            m_dvd.iDVDStillStartTime = 0;
            m_dvd.iDVDStillTime = 0;
          }
        }

        g_infoManager.SetDisplayAfterSeek();
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_SET_RECORD))
      {
        CDVDInputStreamPVRManager* input = dynamic_cast<CDVDInputStreamPVRManager*>(m_pInputStream);
        if(input)
          input->Record(*(CDVDMsgBool*)pMsg);
      }
      else if (pMsg->IsType(CDVDMsg::GENERAL_FLUSH))
      {
        FlushBuffers(DVD_NOPTS_VALUE, true, true);
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_SETSPEED))
      {
        int speed = static_cast<CDVDMsgInt*>(pMsg)->m_value;

        // correct our current clock, as it would start going wrong otherwise
        if(m_State.timestamp > 0)
        {
          double offset;
          offset  = m_clock.GetAbsoluteClock() - m_State.timestamp;
          offset *= m_playSpeed / DVD_PLAYSPEED_NORMAL;
          offset  = DVD_TIME_TO_MSEC(offset);
          if(offset >  1000) offset =  1000;
          if(offset < -1000) offset = -1000;
          m_State.time     += offset;
          m_State.timestamp =  m_clock.GetAbsoluteClock();
        }

        if (speed != DVD_PLAYSPEED_PAUSE && m_playSpeed != DVD_PLAYSPEED_PAUSE && speed != m_playSpeed)
          m_callback.OnPlayBackSpeedChanged(speed / DVD_PLAYSPEED_NORMAL);

        if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER) && speed != m_playSpeed)
        {
          CDVDInputStreamPVRManager* pvrinputstream = static_cast<CDVDInputStreamPVRManager*>(m_pInputStream);
          pvrinputstream->Pause( speed == 0 );
        }

        if ((speed == DVD_PLAYSPEED_NORMAL) &&
                 (m_playSpeed != DVD_PLAYSPEED_NORMAL) &&
                 (m_playSpeed != DVD_PLAYSPEED_PAUSE))
        {
          int64_t iTime = (int64_t)DVD_TIME_TO_MSEC(m_clock.GetClock() + m_State.time_offset);
          if (m_State.time != DVD_NOPTS_VALUE)
            iTime = m_State.time;
          CDVDMsgPlayerSeek::CMode mode;
          mode.time = iTime;
          mode.backward = m_playSpeed < 0;
          mode.accurate = false;
          mode.trickplay = true;
          mode.sync = true;
          m_messenger.Put(new CDVDMsgPlayerSeek(mode));
        }

        // if playspeed is different then DVD_PLAYSPEED_NORMAL or DVD_PLAYSPEED_PAUSE
        // audioplayer, stops outputing audio to audiorendere, but still tries to
        // sleep an correct amount for each packet
        // videoplayer just plays faster after the clock speed has been increased
        // 1. disable audio
        // 2. skip frames and adjust their pts or the clock
        m_playSpeed = speed;
        m_caching = CACHESTATE_DONE;
        m_clock.SetSpeed(speed);
        m_dvdPlayerAudio->SetSpeed(speed);
        m_dvdPlayerVideo->SetSpeed(speed);
        m_streamPlayerSpeed = speed;
        if (m_pDemuxer)
          m_pDemuxer->SetSpeed(speed);
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_SELECT_NUMBER) && m_messenger.GetPacketCount(CDVDMsg::PLAYER_CHANNEL_SELECT_NUMBER) == 0)
      {
        FlushBuffers(DVD_NOPTS_VALUE, true, true);
        CDVDInputStreamPVRManager* input = dynamic_cast<CDVDInputStreamPVRManager*>(m_pInputStream);
        // TODO find a better solution for the "otherStreaHack"
        // a stream is not sopposed to be terminated before demuxer
        if (input && input->IsOtherStreamHack())
        {
          SAFE_DELETE(m_pDemuxer);
        }
        if(input && input->SelectChannelByNumber(static_cast<CDVDMsgInt*>(pMsg)->m_value))
        {
          SAFE_DELETE(m_pDemuxer);
          m_playSpeed = DVD_PLAYSPEED_NORMAL;
#ifdef HAS_VIDEO_PLAYBACK
          // when using fast channel switching some shortcuts are taken which
          // means we'll have to update the view mode manually
          g_renderManager.SetViewMode(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_ViewMode);
#endif
        }else
        {
          CLog::Log(LOGWARNING, "%s - failed to switch channel. playback stopped", __FUNCTION__);
          CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_STOP);
        }
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_SELECT) && m_messenger.GetPacketCount(CDVDMsg::PLAYER_CHANNEL_SELECT) == 0)
      {
        FlushBuffers(DVD_NOPTS_VALUE, true, true);
        CDVDInputStreamPVRManager* input = dynamic_cast<CDVDInputStreamPVRManager*>(m_pInputStream);
        if (input && input->IsOtherStreamHack())
        {
          SAFE_DELETE(m_pDemuxer);
        }
        if(input && input->SelectChannel(static_cast<CDVDMsgType <CPVRChannelPtr> *>(pMsg)->m_value))
        {
          SAFE_DELETE(m_pDemuxer);
          m_playSpeed = DVD_PLAYSPEED_NORMAL;
        }
        else
        {
          CLog::Log(LOGWARNING, "%s - failed to switch channel. playback stopped", __FUNCTION__);
          CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_STOP);
        }
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_NEXT) || pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_PREV) ||
               pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_PREVIEW_NEXT) || pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_PREVIEW_PREV))
      {
        CDVDInputStreamPVRManager* input = dynamic_cast<CDVDInputStreamPVRManager*>(m_pInputStream);
        if (input)
        {
          bool bSwitchSuccessful(false);
          bool bShowPreview(pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_PREVIEW_NEXT) ||
                            pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_PREVIEW_PREV) ||
                            CSettings::GetInstance().GetInt(CSettings::SETTING_PVRPLAYBACK_CHANNELENTRYTIMEOUT) > 0);

          if (!bShowPreview)
          {
            g_infoManager.SetDisplayAfterSeek(100000);
            FlushBuffers(DVD_NOPTS_VALUE, true, true);
            if (input->IsOtherStreamHack())
            {
              SAFE_DELETE(m_pDemuxer);
            }
          }

          if (pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_NEXT) || pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_PREVIEW_NEXT))
            bSwitchSuccessful = input->NextChannel(bShowPreview);
          else
            bSwitchSuccessful = input->PrevChannel(bShowPreview);

          if (bSwitchSuccessful)
          {
            if (bShowPreview)
            {
              UpdateApplication(0);

              if (pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_PREVIEW_NEXT) || pMsg->IsType(CDVDMsg::PLAYER_CHANNEL_PREVIEW_PREV))
                m_ChannelEntryTimeOut.SetInfinite();
              else
                m_ChannelEntryTimeOut.Set(CSettings::GetInstance().GetInt(CSettings::SETTING_PVRPLAYBACK_CHANNELENTRYTIMEOUT));
            }
            else
            {
              m_ChannelEntryTimeOut.SetInfinite();
              SAFE_DELETE(m_pDemuxer);
              m_playSpeed = DVD_PLAYSPEED_NORMAL;

              g_infoManager.SetDisplayAfterSeek();
#ifdef HAS_VIDEO_PLAYBACK
              // when using fast channel switching some shortcuts are taken which
              // means we'll have to update the view mode manually
              g_renderManager.SetViewMode(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_ViewMode);
#endif
            }
          }
          else
          {
            CLog::Log(LOGWARNING, "%s - failed to switch channel. playback stopped", __FUNCTION__);
            CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_STOP);
          }
        }
      }
      else if (pMsg->IsType(CDVDMsg::GENERAL_GUI_ACTION))
        OnAction(((CDVDMsgType<CAction>*)pMsg)->m_value);
      else if (pMsg->IsType(CDVDMsg::PLAYER_STARTED))
      {
        SStartMsg& msg = ((CDVDMsgType<SStartMsg>*)pMsg)->m_value;
        if (msg.player == DVDPLAYER_AUDIO)
        {
          m_CurrentAudio.syncState = IDVDStreamPlayer::SYNC_WAITSYNC;
          m_CurrentAudio.cachetime = msg.cachetime;
          m_CurrentAudio.cachetotal = msg.cachetotal;
          m_CurrentAudio.starttime = msg.timestamp;
        }
        if (msg.player == DVDPLAYER_VIDEO)
        {
          m_CurrentVideo.syncState = IDVDStreamPlayer::SYNC_WAITSYNC;
          m_CurrentVideo.cachetime = msg.cachetime;
          m_CurrentVideo.cachetotal = msg.cachetotal;
          m_CurrentVideo.starttime = msg.timestamp;
          if (m_CurrentVideo.starttime == DVD_NOPTS_VALUE)
            m_CurrentVideo.starttime = m_CurrentVideo.startpts;
        }
        std::string player_str;
        switch(msg.player)
        {
          case DVDPLAYER_AUDIO:
            player_str = "DVDPLAYER_AUDIO";
            break;
          case DVDPLAYER_VIDEO:
            player_str = "DVDPLAYER_VIDEO";
            break;
          case DVDPLAYER_SUBTITLE:
            player_str = "DVDPLAYER_SUBTITLE";
            break;
          case DVDPLAYER_TELETEXT:
            player_str = "DVDPLAYER_TELETEXT";
            break;
          case DVDPLAYER_RDS:
            player_str = "DVDPLAYER_RDS";
            break;
        }
        CLog::Log(LOGDEBUG, "CDVDPlayer::HandleMessages - player started %s", player_str.c_str());
      }
      else if (pMsg->IsType(CDVDMsg::SUBTITLE_ADDFILE))
      {
        int id = AddSubtitleFile(((CDVDMsgType<std::string>*) pMsg)->m_value);
        if (id >= 0)
        {
          SetSubtitle(id);
          SetSubtitleVisibleInternal(true);
        }
      }
      else if (pMsg->IsType(CDVDMsg::SUBTITLE_ADDSTREAMINFO))
      {
        SPlayerSubtitleStreamInfo info = ((CDVDMsgType<SPlayerSubtitleStreamInfo>*) pMsg)->m_value;
        AddSubtitleStreamInfo(info);
      }
      else if (pMsg->IsType(CDVDMsg::GENERAL_SYNCHRONIZE))
      {
        if (((CDVDMsgGeneralSynchronize*)pMsg)->Wait(100, SYNCSOURCE_OWNER))
          CLog::Log(LOGDEBUG, "CDVDPlayer - CDVDMsg::GENERAL_SYNCHRONIZE");
      }
      else if (pMsg->IsType(CDVDMsg::PLAYER_AVCHANGE))
      {
        UpdateStreamInfos();
        g_dataCacheCore.SignalAudioInfoChange();
        g_dataCacheCore.SignalVideoInfoChange();
      }

    pMsg->Release();
  }

}

void CDVDPlayer::SetCaching(ECacheState state, const std::string &msg)
{
  if(state == CACHESTATE_FLUSH)
  {
    state = CACHESTATE_WAITFILL;
  }

  if(m_caching == state)
    return;

  LogCacheState(state, msg);

  if (state == CACHESTATE_WAITFILL ||
      state == CACHESTATE_WAITSTREAM)
  {
    m_clock.SetSpeed(DVD_PLAYSPEED_PAUSE);

    m_dvdPlayerAudio->SetSpeed(DVD_PLAYSPEED_PAUSE);
    m_dvdPlayerVideo->SetSpeed(DVD_PLAYSPEED_PAUSE);
    m_streamPlayerSpeed = DVD_PLAYSPEED_PAUSE;

    m_pInputStream->ResetScanTimeout((unsigned int) CSettings::GetInstance().GetInt(CSettings::SETTING_PVRPLAYBACK_SCANTIME) * 1000);

    m_cachingTimer.Set(5000);
  }

  if (state == CACHESTATE_WAITCODEC ||
     (state == CACHESTATE_DONE && m_caching != CACHESTATE_WAITCODEC))
  {
    m_clock.SetSpeed(m_playSpeed);
    m_dvdPlayerAudio->SetSpeed(m_playSpeed);
    m_dvdPlayerVideo->SetSpeed(m_playSpeed);
    m_streamPlayerSpeed = m_playSpeed;
    m_pInputStream->ResetScanTimeout(0);
  }
  m_caching = state;

  m_clock.SetSpeedAdjust(0);
}

void CDVDPlayer::LogCacheState(ECacheState state, const std::string &msg)
{
  std::string state_str;
  switch(state)
  {
    case CACHESTATE_DONE:
      state_str = "CACHESTATE_DONE";
      break;
    case CACHESTATE_WAITFILL:
      state_str = "CACHESTATE_WAITFILL";
      break;
    case CACHESTATE_WAITSTREAM:
      state_str = "CACHESTATE_WAITSTREAM";
      break;
    case CACHESTATE_WAITCODEC:
      state_str = "CACHESTATE_WAITCODEC";
      break;
    case CACHESTATE_FLUSH:
      state_str = "CACHESTATE_FLUSH";
      break;
  }
  CLog::Log(LOGDEBUG, "CDVDPlayer::LogCacheState(%s) - %s", msg.c_str(), state_str.c_str());
}

void CDVDPlayer::LogCacheLevels(const std::string &msg)
{
  int audioLevel = m_dvdPlayerAudio->GetLevel();
  int videoLevel = m_dvdPlayerVideo->GetLevel();
  CLog::Log(LOGDEBUG, "CDVDPlayer::LogCacheLevels(%s) - audioLevel %d, videoLevel %d", msg.c_str(), audioLevel, videoLevel);
}

void CDVDPlayer::SetPlaySpeed(int speed)
{
  if (IsPlaying())
    m_messenger.Put(new CDVDMsgInt(CDVDMsg::PLAYER_SETSPEED, speed));
  else
  {
    m_playSpeed = speed;
    m_streamPlayerSpeed = speed;
  }
}

bool CDVDPlayer::CanPause()
{
  CSingleLock lock(m_StateSection);
  return m_State.canpause;
}

void CDVDPlayer::Pause()
{
  CSingleLock lock(m_StateSection);
  if (!m_State.canpause)
    return;
  lock.Leave();

  // return to normal speed if it was paused before, pause otherwise
  if (m_playSpeed == DVD_PLAYSPEED_PAUSE)
  {
    SetPlaySpeed(DVD_PLAYSPEED_NORMAL);
    m_callback.OnPlayBackResumed();
  }
  else
  {
    SetPlaySpeed(DVD_PLAYSPEED_PAUSE);
    m_callback.OnPlayBackPaused();
  }
}

bool CDVDPlayer::IsPaused() const
{
  return m_playSpeed == DVD_PLAYSPEED_PAUSE;
}

bool CDVDPlayer::HasVideo() const
{
  return m_HasVideo;
}

bool CDVDPlayer::HasAudio() const
{
  return m_HasAudio;
}

bool CDVDPlayer::HasRDS() const
{
  return m_CurrentRadioRDS.id >= 0;
}

bool CDVDPlayer::IsPassthrough() const
{
  return m_dvdPlayerAudio->IsPassthrough();
}

bool CDVDPlayer::CanSeek()
{
  CSingleLock lock(m_StateSection);
  return m_State.canseek;
}

void CDVDPlayer::Seek(bool bPlus, bool bLargeStep, bool bChapterOverride)
{
  if( m_playSpeed == DVD_PLAYSPEED_PAUSE && bPlus && !bLargeStep)
  {
    if (m_dvdPlayerVideo->StepFrame())
      return;
  }
  if (!m_State.canseek)
    return;

  if (bLargeStep && bChapterOverride && GetChapter() > 0 && GetChapterCount() > 1)
  {
    if (!bPlus)
    {
      SeekChapter(GetChapter() - 1);
      return;
    }
    else if (GetChapter() < GetChapterCount())
    {
      SeekChapter(GetChapter() + 1);
      return;
    }
  }

  int64_t seekTarget;
  if (g_advancedSettings.m_videoUseTimeSeeking && GetTotalTime() > 2000*g_advancedSettings.m_videoTimeSeekForwardBig)
  {
    if (bLargeStep)
      seekTarget = bPlus ? g_advancedSettings.m_videoTimeSeekForwardBig : g_advancedSettings.m_videoTimeSeekBackwardBig;
    else
      seekTarget = bPlus ? g_advancedSettings.m_videoTimeSeekForward : g_advancedSettings.m_videoTimeSeekBackward;
    seekTarget *= 1000;
    seekTarget += GetTime();
  }
  else
  {
    int percent;
    if (bLargeStep)
      percent = bPlus ? g_advancedSettings.m_videoPercentSeekForwardBig : g_advancedSettings.m_videoPercentSeekBackwardBig;
    else
      percent = bPlus ? g_advancedSettings.m_videoPercentSeekForward : g_advancedSettings.m_videoPercentSeekBackward;
    seekTarget = (int64_t)(GetTotalTimeInMsec()*(GetPercentage()+percent)/100);
  }

  bool restore = true;

  int64_t time = GetTime();
  if (g_application.CurrentFileItem().IsStack() &&
    (seekTarget > GetTotalTimeInMsec() || seekTarget < 0))
  {
    g_application.SeekTime((seekTarget - time) * 0.001 + g_application.GetTime());
    // warning, don't access any VideoPlayer variables here as
    // the VideoPlayer object may have been destroyed
    return;
  }

  CDVDMsgPlayerSeek::CMode mode;
  mode.time = (int)seekTarget;
  mode.backward = !bPlus;
  mode.accurate = false;
  mode.restore = restore;
  mode.trickplay = false;
  mode.sync = true;
  m_messenger.Put(new CDVDMsgPlayerSeek(mode));

  SynchronizeDemuxer();
  if (seekTarget < 0)
    seekTarget = 0;
  m_callback.OnPlayBackSeek((int)seekTarget, (int)(seekTarget - time));
}

bool CDVDPlayer::SeekScene(bool bPlus)
{
  if (!m_Edl.HasSceneMarker())
    return false;

  /*
   * There is a 5 second grace period applied when seeking for scenes backwards. If there is no
   * grace period applied it is impossible to go backwards past a scene marker.
   */
  int64_t clock = GetTime();
  if (!bPlus && clock > 5 * 1000) // 5 seconds
    clock -= 5 * 1000;

  int iScenemarker;
  if (m_Edl.GetNextSceneMarker(bPlus, clock, &iScenemarker))
  {
    /*
     * Seeking is flushed and inaccurate, just like Seek()
     */
    CDVDMsgPlayerSeek::CMode mode;
    mode.time = iScenemarker;
    mode.backward = !bPlus;
    mode.accurate = false;
    mode.restore = false;
    mode.trickplay = false;
    mode.sync = true;
    m_messenger.Put(new CDVDMsgPlayerSeek(mode));

    SynchronizeDemuxer();
    return true;
  }
  return false;
}

void CDVDPlayer::GetAudioInfo(std::string& strAudioInfo)
{
  { CSingleLock lock(m_StateSection);
    strAudioInfo = StringUtils::Format("D(%s)", m_State.demux_audio.c_str());
  }
  strAudioInfo += StringUtils::Format("\nP(%s)", m_dvdPlayerAudio->GetPlayerInfo().c_str());
}

void CDVDPlayer::GetVideoInfo(std::string& strVideoInfo)
{
  { CSingleLock lock(m_StateSection);
    strVideoInfo = StringUtils::Format("D(%s)", m_State.demux_video.c_str());
  }
  strVideoInfo += StringUtils::Format("\nP(%s)", m_dvdPlayerVideo->GetPlayerInfo().c_str());
}

void CDVDPlayer::GetGeneralInfo(std::string& strGeneralInfo)
{
  if (!m_bStop)
  {
    double dDelay = m_dvdPlayerVideo->GetDelay() / DVD_TIME_BASE - g_renderManager.GetDisplayLatency();

    double apts = m_dvdPlayerAudio->GetCurrentPts();
    double vpts = m_dvdPlayerVideo->GetCurrentPts();
    double dDiff = 0;

    if( apts != DVD_NOPTS_VALUE && vpts != DVD_NOPTS_VALUE )
      dDiff = (apts - vpts) / DVD_TIME_BASE;

    std::string strEDL = StringUtils::Format(", edl:%s", m_Edl.GetInfo().c_str());

    std::string strBuf;
    CSingleLock lock(m_StateSection);
    if(m_State.cache_bytes >= 0)
    {
      strBuf += StringUtils::Format(" forward:%s %2.0f%%"
                                    , StringUtils::SizeToString(m_State.cache_bytes).c_str()
                                    , m_State.cache_level * 100);
      if(m_playSpeed == 0 || m_caching == CACHESTATE_WAITFILL)
        strBuf += StringUtils::Format(" %d sec", DVD_TIME_TO_SEC(m_State.cache_delay));
    }

    strGeneralInfo = StringUtils::Format("C(a/v:% 6.3f%s, ad:% 6.3f, %s)"
                                         , dDiff
                                         , strEDL.c_str()
                                         , dDelay
                                         , strBuf.c_str());
  }
}

void CDVDPlayer::SeekPercentage(float iPercent)
{
  int64_t iTotalTime = GetTotalTimeInMsec();

  if (!iTotalTime)
    return;

  SeekTime((int64_t)(iTotalTime * iPercent / 100));
}

float CDVDPlayer::GetPercentage()
{
  int64_t iTotalTime = GetTotalTimeInMsec();

  if (!iTotalTime)
    return 0.0f;

  return GetTime() * 100 / (float)iTotalTime;
}

float CDVDPlayer::GetCachePercentage()
{
  CSingleLock lock(m_StateSection);
  return (float) (m_State.cache_offset * 100); // NOTE: Percentage returned is relative
}

void CDVDPlayer::SetAVDelay(float fValue)
{
  m_dvdPlayerVideo->SetDelay( (fValue * DVD_TIME_BASE) ) ;
}

float CDVDPlayer::GetAVDelay()
{
  return (float) m_dvdPlayerVideo->GetDelay() / (float)DVD_TIME_BASE;
}

void CDVDPlayer::SetSubTitleDelay(float fValue)
{
  m_dvdPlayerVideo->SetSubtitleDelay(-fValue * DVD_TIME_BASE);
}

float CDVDPlayer::GetSubTitleDelay()
{
  return (float) -m_dvdPlayerVideo->GetSubtitleDelay() / DVD_TIME_BASE;
}

// priority: 1: libdvdnav, 2: external subtitles, 3: muxed subtitles
int CDVDPlayer::GetSubtitleCount()
{
  return m_SelectionStreams.Count(STREAM_SUBTITLE);
}

int CDVDPlayer::GetSubtitle()
{
  return m_SelectionStreams.IndexOf(STREAM_SUBTITLE, *this);
}

void CDVDPlayer::UpdateStreamInfos()
{
  if (!m_pDemuxer)
    return;

  CSingleLock lock(m_SelectionStreams.m_section);
  int streamId;
  std::string retVal;

  // video
  streamId = m_SelectionStreams.IndexOf(STREAM_VIDEO, *this);

  if (streamId >= 0 && streamId < m_SelectionStreams.Count(STREAM_VIDEO))
  {
    SelectionStream& s = m_SelectionStreams.Get(STREAM_VIDEO, streamId);
    s.bitrate = m_dvdPlayerVideo->GetVideoBitrate();
    s.aspect_ratio = g_renderManager.GetAspectRatio();
    CRect viewRect;
    g_renderManager.GetVideoRect(s.SrcRect, s.DestRect, viewRect);
    s.stereo_mode = m_dvdPlayerVideo->GetStereoMode();
    if (s.stereo_mode == "mono")
      s.stereo_mode = "";

    CDemuxStream* stream = m_pDemuxer->GetStream(m_CurrentVideo.id);
    if (stream && stream->type == STREAM_VIDEO)
    {
      s.width = ((CDemuxStreamVideo*)stream)->iWidth;
      s.height = ((CDemuxStreamVideo*)stream)->iHeight;
    }
  }

  // audio
  streamId = GetAudioStream();

  if (streamId >= 0 && streamId < GetAudioStreamCount())
  {
    SelectionStream& s = m_SelectionStreams.Get(STREAM_AUDIO, streamId);
    s.bitrate = m_dvdPlayerAudio->GetAudioBitrate();
    s.channels = m_dvdPlayerAudio->GetAudioChannels();

    CDemuxStream* stream = m_pDemuxer->GetStream(m_CurrentAudio.id);
    if (stream && stream->type == STREAM_AUDIO)
    {
      m_pDemuxer->GetStreamCodecName(stream->iId, s.codec);
    }
  }
}

void CDVDPlayer::GetSubtitleStreamInfo(int index, SPlayerSubtitleStreamInfo &info)
{
  CSingleLock lock(m_SelectionStreams.m_section);
  if (index < 0 || index > (int) GetSubtitleCount() - 1)
    return;

  SelectionStream& s = m_SelectionStreams.Get(STREAM_SUBTITLE, index);
  if(s.name.length() > 0)
    info.name = s.name;

  if(s.type == STREAM_NONE)
    info.name += "(Invalid)";

  info.language = s.language;
  info.forced = s.flags == CDemuxStream::FLAG_FORCED;
}

void CDVDPlayer::SetSubtitle(int iStream)
{
  m_messenger.Put(new CDVDMsgPlayerSetSubtitleStream(iStream));
}

bool CDVDPlayer::GetSubtitleVisible()
{
  if (m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
  {
    CDVDInputStreamNavigator* pStream = (CDVDInputStreamNavigator*)m_pInputStream;
    return pStream->IsSubtitleStreamEnabled();
  }

  return m_dvdPlayerVideo->IsSubtitleEnabled();
}

void CDVDPlayer::SetSubtitleVisible(bool bVisible)
{
  m_messenger.Put(new CDVDMsgBool(CDVDMsg::PLAYER_SET_SUBTITLESTREAM_VISIBLE, bVisible));
}

void CDVDPlayer::SetSubtitleVisibleInternal(bool bVisible)
{
  m_dvdPlayerVideo->EnableSubtitle(bVisible);

  if (m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
    static_cast<CDVDInputStreamNavigator*>(m_pInputStream)->EnableSubtitleStream(bVisible);
}

int CDVDPlayer::GetAudioStreamCount()
{
  return m_SelectionStreams.Count(STREAM_AUDIO);
}

int CDVDPlayer::GetAudioStream()
{
  return m_SelectionStreams.IndexOf(STREAM_AUDIO, *this);
}

void CDVDPlayer::SetAudioStream(int iStream)
{
  m_messenger.Put(new CDVDMsgPlayerSetAudioStream(iStream));
  SynchronizeDemuxer();
}

TextCacheStruct_t* CDVDPlayer::GetTeletextCache()
{
  if (m_CurrentTeletext.id < 0)
    return 0;

  return m_dvdPlayerTeletext->GetTeletextCache();
}

void CDVDPlayer::LoadPage(int p, int sp, unsigned char* buffer)
{
  if (m_CurrentTeletext.id < 0)
      return;

  return m_dvdPlayerTeletext->LoadPage(p, sp, buffer);
}

std::string CDVDPlayer::GetRadioText(unsigned int line)
{
  if (m_CurrentRadioRDS.id < 0)
      return "";

  return m_dvdPlayerRadioRDS->GetRadioText(line);
}

void CDVDPlayer::SeekTime(int64_t iTime)
{
  int seekOffset = (int)(iTime - GetTime());
  CDVDMsgPlayerSeek::CMode mode;
  mode.time = (int)iTime;
  mode.backward = true;
  mode.accurate = true;
  mode.trickplay = false;
  mode.sync = true;
  m_messenger.Put(new CDVDMsgPlayerSeek(mode));

  SynchronizeDemuxer();
  m_callback.OnPlayBackSeek((int)iTime, seekOffset);
}

bool CDVDPlayer::SeekTimeRelative(int64_t iTime)
{
  int64_t abstime = GetTime() + iTime;
  CDVDMsgPlayerSeek::CMode mode;
  mode.time = (int)iTime;
  mode.relative = true;
  mode.backward = (iTime < 0) ? true : false;
  mode.accurate = false;
  mode.trickplay = false;
  mode.sync = true;
  m_messenger.Put(new CDVDMsgPlayerSeek(mode));

  SynchronizeDemuxer();
  m_callback.OnPlayBackSeek((int)abstime, iTime);
  return true;
}

// return the time in milliseconds
int64_t CDVDPlayer::GetTime()
{
  CSingleLock lock(m_StateSection);
  double offset = 0;
  const double limit = DVD_MSEC_TO_TIME(500);
  if (m_State.timestamp > 0)
  {
    offset  = m_clock.GetAbsoluteClock() - m_State.timestamp;
    offset *= m_playSpeed / DVD_PLAYSPEED_NORMAL;
    if (offset > limit)
      offset = limit;
    if (offset < -limit)
      offset = -limit;
  }
  return llrint(m_State.time + DVD_TIME_TO_MSEC(offset));
}

// return the time in milliseconds
int64_t CDVDPlayer::GetDisplayTime()
{
  return GetTime();
}

// return length in msec
int64_t CDVDPlayer::GetTotalTimeInMsec()
{
  CSingleLock lock(m_StateSection);
  return llrint(m_State.time_total);
}

// return length in seconds.. this should be changed to return in milleseconds throughout xbmc
int64_t CDVDPlayer::GetTotalTime()
{
  return GetTotalTimeInMsec();
}

void CDVDPlayer::ToFFRW(int iSpeed)
{
  // can't rewind in menu as seeking isn't possible
  // forward is fine
  if (iSpeed < 0 && IsInMenu()) return;
  SetPlaySpeed(iSpeed * DVD_PLAYSPEED_NORMAL);
}

bool CDVDPlayer::OpenStream(CCurrentStream& current, int iStream, int source, bool reset)
{
  CDemuxStream* stream = NULL;
  CDVDStreamInfo hint;

  CLog::Log(LOGNOTICE, "Opening stream: %i source: %i", iStream, source);

  if(STREAM_SOURCE_MASK(source) == STREAM_SOURCE_DEMUX_SUB)
  {
    int index = m_SelectionStreams.IndexOf(current.type, source, iStream);
    if(index < 0)
      return false;
    SelectionStream st = m_SelectionStreams.Get(current.type, index);

    if(!m_pSubtitleDemuxer || m_pSubtitleDemuxer->GetFileName() != st.filename)
    {
      CLog::Log(LOGNOTICE, "Opening Subtitle file: %s", st.filename.c_str());
      SAFE_DELETE(m_pSubtitleDemuxer);
      std::unique_ptr<CDVDDemuxVobsub> demux(new CDVDDemuxVobsub());
      if(!demux->Open(st.filename, source, st.filename2))
        return false;
      m_pSubtitleDemuxer = demux.release();
    }

    double pts = m_dvdPlayerVideo->GetCurrentPts();
    if(pts == DVD_NOPTS_VALUE)
      pts = m_CurrentVideo.dts;
    if(pts == DVD_NOPTS_VALUE)
      pts = 0;
    pts += m_offset_pts;
    if (!m_pSubtitleDemuxer->SeekTime((int)(1000.0 * pts / (double)DVD_TIME_BASE)))
        CLog::Log(LOGDEBUG, "%s - failed to start subtitle demuxing from: %f", __FUNCTION__, pts);
    stream = m_pSubtitleDemuxer->GetStream(iStream);
    if(!stream || stream->disabled)
      return false;
    stream->SetDiscard(AVDISCARD_NONE);

    hint.Assign(*stream, true);
  }
  else if(STREAM_SOURCE_MASK(source) == STREAM_SOURCE_TEXT)
  {
    int index = m_SelectionStreams.IndexOf(current.type, source, iStream);
    if(index < 0)
      return false;

    hint.Clear();
    hint.filename = m_SelectionStreams.Get(current.type, index).filename;
    hint.fpsscale = m_CurrentVideo.hint.fpsscale;
    hint.fpsrate  = m_CurrentVideo.hint.fpsrate;
  }
  else if(STREAM_SOURCE_MASK(source) == STREAM_SOURCE_DEMUX)
  {
    if(!m_pDemuxer)
      return false;

    stream = m_pDemuxer->GetStream(iStream);
    if(!stream || stream->disabled)
      return false;
    stream->SetDiscard(AVDISCARD_NONE);

    hint.Assign(*stream, true);

    if(m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
      hint.filename = "dvd";
  }
  else if(STREAM_SOURCE_MASK(source) == STREAM_SOURCE_VIDEOMUX)
  {
    if(!m_pCCDemuxer)
      return false;

    stream = m_pCCDemuxer->GetStream(iStream);
    if(!stream || stream->disabled)
      return false;

    hint.Assign(*stream, false);
  }

  bool res;
  switch(current.type)
  {
    case STREAM_AUDIO:
      res = OpenAudioStream(hint, reset);
      break;
    case STREAM_VIDEO:
      res = OpenVideoStream(hint, reset);
      break;
    case STREAM_SUBTITLE:
      res = OpenSubtitleStream(hint);
      break;
    case STREAM_TELETEXT:
      res = OpenTeletextStream(hint);
      break;
    case STREAM_RADIO_RDS:
      res = OpenRadioRDSStream(hint);
      break;
    default:
      res = false;
      break;
  }

  if (res)
  {
    int oldId = current.id;
    current.id = iStream;
    current.source = source;
    current.hint = hint;
    current.stream = (void*)stream;
    current.lastdts = DVD_NOPTS_VALUE;
    if (oldId >= 0 && current.avsync != CCurrentStream::AV_SYNC_FORCE)
      current.avsync = CCurrentStream::AV_SYNC_CHECK;
    if(stream)
      current.changes = stream->changes;
  }
  else
  {
    if(stream)
    {
      /* mark stream as disabled, to disallaw further attempts*/
      CLog::Log(LOGWARNING, "%s - Unsupported stream %d. Stream disabled.", __FUNCTION__, stream->iId);
      stream->disabled = true;
      stream->SetDiscard(AVDISCARD_ALL);
    }
  }

  g_dataCacheCore.SignalAudioInfoChange();
  g_dataCacheCore.SignalVideoInfoChange();

  return res;
}

bool CDVDPlayer::OpenAudioStream(CDVDStreamInfo& hint, bool reset)
{
  IDVDStreamPlayer* player = GetStreamPlayer(m_CurrentAudio.player);
  if(player == nullptr)
    return false;

  if(m_CurrentAudio.id < 0 ||
     m_CurrentAudio.hint != hint)
  {
    if (!player->OpenStream(hint))
      return false;

    static_cast<IDVDStreamPlayerAudio*>(player)->SetSpeed(m_streamPlayerSpeed);
    m_CurrentAudio.syncState = IDVDStreamPlayer::SYNC_STARTING;
    m_CurrentAudio.packets = 0;
  }
  else if (reset)
    player->SendMessage(new CDVDMsg(CDVDMsg::GENERAL_RESET), 0);

  m_HasAudio = true;

  return true;
}

bool CDVDPlayer::OpenVideoStream(CDVDStreamInfo& hint, bool reset)
{
  if (m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
  {
    /* set aspect ratio as requested by navigator for dvd's */
    float aspect = static_cast<CDVDInputStreamNavigator*>(m_pInputStream)->GetVideoAspectRatio();
    if (aspect != 0.0)
    {
      hint.aspect = aspect;
      hint.forced_aspect = true;
    }
    hint.dvd = true;
  }
  else if (m_pInputStream && m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER))
  {
    // set framerate if not set by demuxer
    if (hint.fpsrate == 0 || hint.fpsscale == 0)
    {
      int fpsidx = CSettings::GetInstance().GetInt(CSettings::SETTING_PVRPLAYBACK_FPS);
      if (fpsidx == 1)
      {
        hint.fpsscale = 1000;
        hint.fpsrate = 50000;
      }
      else if (fpsidx == 2)
      {
        hint.fpsscale = 1001;
        hint.fpsrate = 60000;
      }
    }
  }

  CDVDInputStream::IMenus* pMenus = dynamic_cast<CDVDInputStream::IMenus*>(m_pInputStream);
  if(pMenus && pMenus->IsInMenu())
    hint.stills = true;

  if (hint.stereo_mode.empty())
  {
    std::string filepath;
    if (m_item.HasVideoInfoTag() && m_item.IsMediaServiceBased())
      filepath = m_item.GetMediaServiceFile();
    else
      filepath = m_item.GetPath();
    hint.stereo_mode = CStereoscopicsManager::GetInstance().DetectStereoModeByString(filepath);
  }

  SelectionStream& s = m_SelectionStreams.Get(STREAM_VIDEO, 0);

  if(hint.flags & AV_DISPOSITION_ATTACHED_PIC)
    return false;

  IDVDStreamPlayer* player = GetStreamPlayer(m_CurrentVideo.player);
  if(player == nullptr)
    return false;

  if(m_CurrentVideo.id < 0 ||
     m_CurrentVideo.hint != hint)
  {
    if (hint.codec == AV_CODEC_ID_MPEG2VIDEO || hint.codec == AV_CODEC_ID_H264)
      SAFE_DELETE(m_pCCDemuxer);

    if (!player->OpenStream(hint))
      return false;

    s.stereo_mode = static_cast<IDVDStreamPlayerVideo*>(player)->GetStereoMode();
    if (s.stereo_mode == "mono")
      s.stereo_mode = "";

    static_cast<IDVDStreamPlayerVideo*>(player)->SetSpeed(m_streamPlayerSpeed);
    m_CurrentVideo.syncState = IDVDStreamPlayer::SYNC_STARTING;
    m_CurrentVideo.packets = 0;
  }
  else if (reset)
    player->SendMessage(new CDVDMsg(CDVDMsg::GENERAL_RESET), 0);

  m_HasVideo = true;

  // open CC demuxer if video is mpeg2
  if ((hint.codec == AV_CODEC_ID_MPEG2VIDEO || hint.codec == AV_CODEC_ID_H264) && !m_pCCDemuxer)
  {
    m_pCCDemuxer = new CDVDDemuxCC(hint.codec);
    m_SelectionStreams.Clear(STREAM_NONE, STREAM_SOURCE_VIDEOMUX);
  }

  return true;

}

bool CDVDPlayer::OpenSubtitleStream(CDVDStreamInfo& hint)
{
  IDVDStreamPlayer* player = GetStreamPlayer(m_CurrentSubtitle.player);
  if(player == nullptr)
    return false;

  if(m_CurrentSubtitle.id < 0 ||
     m_CurrentSubtitle.hint != hint)
  {
    if (!player->OpenStream(hint))
      return false;
  }

  return true;
}

void CDVDPlayer::AdaptForcedSubtitles()
{
  bool valid = false;
  SelectionStream ss = m_SelectionStreams.Get(STREAM_SUBTITLE, GetSubtitle());
  if (ss.flags & CDemuxStream::FLAG_FORCED)
  {
    SelectionStream as = m_SelectionStreams.Get(STREAM_AUDIO, GetAudioStream());
    SelectionStreams streams = m_SelectionStreams.Get(STREAM_SUBTITLE);

    for(SelectionStreams::iterator it = streams.begin(); it != streams.end() && !valid; ++it)
    {
      if (it->flags & CDemuxStream::FLAG_FORCED && g_LangCodeExpander.CompareISO639Codes(it->language, as.language))
      {
        if(OpenStream(m_CurrentSubtitle, it->id, it->source))
        {
          valid = true;
          SetSubtitleVisibleInternal(true);
        }
      }
    }
    if(!valid)
    {
      SetSubtitleVisibleInternal(false);
    }
  }
}

bool CDVDPlayer::OpenTeletextStream(CDVDStreamInfo& hint)
{
  if (!m_dvdPlayerTeletext->CheckStream(hint))
    return false;

  IDVDStreamPlayer* player = GetStreamPlayer(m_CurrentTeletext.player);
  if(player == nullptr)
    return false;

  if(m_CurrentTeletext.id < 0 ||
     m_CurrentTeletext.hint != hint)
  {
    if (!player->OpenStream(hint))
      return false;
  }

  return true;
}

bool CDVDPlayer::OpenRadioRDSStream(CDVDStreamInfo& hint)
{
  if (!m_dvdPlayerRadioRDS->CheckStream(hint))
    return false;

  IDVDStreamPlayer* player = GetStreamPlayer(m_CurrentRadioRDS.player);
  if(player == nullptr)
    return false;

  if(m_CurrentRadioRDS.id < 0 ||
     m_CurrentRadioRDS.hint != hint)
  {
    if (!player->OpenStream(hint))
      return false;
  }

  return true;
}

bool CDVDPlayer::CloseStream(CCurrentStream& current, bool bWaitForBuffers)
{
  if (current.id < 0)
    return false;

  CLog::Log(LOGNOTICE, "Closing stream player %d", current.player);

  if(bWaitForBuffers)
    SetCaching(CACHESTATE_DONE, __FUNCTION__);

  IDVDStreamPlayer* player = GetStreamPlayer(current.player);
  if(player)
  {
    if ((current.type == STREAM_AUDIO && current.syncState != IDVDStreamPlayer::SYNC_INSYNC) ||
        (current.type == STREAM_VIDEO && current.syncState != IDVDStreamPlayer::SYNC_INSYNC) ||
        m_bAbortRequest)
      bWaitForBuffers = false;
    player->CloseStream(bWaitForBuffers);
  }

  current.Clear();

  return true;
}

void CDVDPlayer::FlushBuffers(double pts, bool accurate, bool sync)
{
  CLog::Log(LOGDEBUG, "CDVDPlayer::FlushBuffers - flushing buffers");

  double startpts = DVD_NOPTS_VALUE;
  if (accurate)
    startpts = pts;

  if (sync)
  {
    m_CurrentAudio.inited = false;
    m_CurrentAudio.avsync = CCurrentStream::AV_SYNC_FORCE;
    m_CurrentVideo.inited = false;
    m_CurrentVideo.avsync = CCurrentStream::AV_SYNC_FORCE;
    m_CurrentSubtitle.inited = false;
    m_CurrentTeletext.inited = false;
    m_CurrentRadioRDS.inited  = false;
  }

  m_CurrentAudio.dts         = DVD_NOPTS_VALUE;
  m_CurrentAudio.startpts    = startpts;
  m_CurrentAudio.packets = 0;

  m_CurrentVideo.dts         = DVD_NOPTS_VALUE;
  m_CurrentVideo.startpts    = startpts;
  m_CurrentVideo.packets = 0;

  m_CurrentSubtitle.dts      = DVD_NOPTS_VALUE;
  m_CurrentSubtitle.startpts = startpts;
  m_CurrentSubtitle.packets = 0;

  m_CurrentTeletext.dts      = DVD_NOPTS_VALUE;
  m_CurrentTeletext.startpts = startpts;
  m_CurrentTeletext.packets = 0;

  m_CurrentRadioRDS.dts      = DVD_NOPTS_VALUE;
  m_CurrentRadioRDS.startpts = startpts;
  m_CurrentRadioRDS.packets = 0;

  m_dvdPlayerAudio->Flush(sync);
  m_dvdPlayerVideo->Flush(sync);
  m_dvdPlayerSubtitle->Flush();
  m_dvdPlayerTeletext->Flush();
  m_dvdPlayerRadioRDS->Flush();

  // clear subtitle and menu overlays
  m_overlayContainer.Clear();

  if(m_playSpeed == DVD_PLAYSPEED_NORMAL
  || m_playSpeed == DVD_PLAYSPEED_PAUSE)
  {
    // make sure players are properly flushed, should put them in stalled state
    CDVDMsgGeneralSynchronize* msg = new CDVDMsgGeneralSynchronize(1000, 0);
    m_dvdPlayerAudio->SendMessage(msg->Acquire(), 1);
    m_dvdPlayerVideo->SendMessage(msg->Acquire(), 1);
    msg->Wait(&m_bStop, 0);
    msg->Release();

    // purge any pending PLAYER_STARTED messages
    m_messenger.Flush(CDVDMsg::PLAYER_STARTED);

    // we should now wait for init cache
    SetCaching(CACHESTATE_FLUSH, __FUNCTION__);
    if (sync)
    {
      m_CurrentAudio.syncState = IDVDStreamPlayer::SYNC_STARTING;
      m_CurrentVideo.syncState = IDVDStreamPlayer::SYNC_STARTING;
    }
  }

  if(pts != DVD_NOPTS_VALUE && sync)
    m_clock.Discontinuity(pts);
  UpdatePlayState(0);
}

// since we call ffmpeg functions to decode, this is being called in the same thread as ::Process() is
int CDVDPlayer::OnDiscNavResult(void* pData, int iMessage)
{
  if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_BLURAY))
  {
    switch (iMessage)
    {
    case BD_EVENT_MENU_OVERLAY:
      m_overlayContainer.Add(static_cast<CDVDOverlay*>(pData));
      break;
    case BD_EVENT_PLAYLIST_STOP:
      m_messenger.Put(new CDVDMsg(CDVDMsg::GENERAL_FLUSH));
      break;
    case BD_EVENT_AUDIO_STREAM:
      m_dvd.iSelectedAudioStream = *static_cast<int*>(pData);
      break;

    case BD_EVENT_PG_TEXTST_STREAM:
      m_dvd.iSelectedSPUStream = *static_cast<int*>(pData);
      break;
    case BD_EVENT_PG_TEXTST:
    {
      bool enable = (*static_cast<int*>(pData) != 0);
      m_dvdPlayerVideo->EnableSubtitle(enable);
    }
    break;
    case BD_EVENT_STILL_TIME:
    {
      if (m_dvd.state != DVDSTATE_STILL)
      {
        // else notify the player we have received a still frame

        m_dvd.iDVDStillTime = *(int*)pData;
        m_dvd.iDVDStillStartTime = XbmcThreads::SystemClockMillis();

        /* adjust for the output delay in the video queue */
        unsigned int time = 0;
        if (m_CurrentVideo.stream && m_dvd.iDVDStillTime > 0)
        {
          time = (unsigned int)(m_dvdPlayerVideo->GetOutputDelay() / (DVD_TIME_BASE / 1000));
          if (time < 10000 && time > 0)
            m_dvd.iDVDStillTime += time;
        }
        m_dvd.state = DVDSTATE_STILL;
        CLog::Log(LOGDEBUG,
          "BD_EVENT_STILL_TIME - waiting %i sec, with delay of %d sec",
          m_dvd.iDVDStillTime, time / 1000);
      }
    }
    break;
    case BD_EVENT_MENU_ERROR:
    {
      m_dvd.state = DVDSTATE_NORMAL;
      CLog::Log(LOGDEBUG, "CDVDPlayer::OnDiscNavResult - libbluray menu not supported (DVDSTATE_NORMAL)");
      CGUIDialogKaiToast::QueueNotification(g_localizeStrings.Get(25008), g_localizeStrings.Get(25009));
    }
    break;
    case BD_EVENT_ENC_ERROR:
    {
      m_dvd.state = DVDSTATE_NORMAL;
      CLog::Log(LOGDEBUG, "CDVDPlayer::OnDiscNavResult - libbluray the disc/file is encrypted and can't be played (DVDSTATE_NORMAL)");
      CGUIDialogKaiToast::QueueNotification(g_localizeStrings.Get(16026), g_localizeStrings.Get(29805));
    }
    break;
    default:
      break;
    }

    return 0;
  }

  if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
  {
    CDVDInputStreamNavigator* pStream = (CDVDInputStreamNavigator*)m_pInputStream;

    switch (iMessage)
    {
    case DVDNAV_STILL_FRAME:
      {
        //CLog::Log(LOGDEBUG, "DVDNAV_STILL_FRAME");

        dvdnav_still_event_t *still_event = (dvdnav_still_event_t *)pData;
        // should wait the specified time here while we let the player running
        // after that call dvdnav_still_skip(m_dvdnav);

        if (m_dvd.state != DVDSTATE_STILL)
        {
          // else notify the player we have received a still frame

          if(still_event->length < 0xff)
            m_dvd.iDVDStillTime = still_event->length * 1000;
          else
            m_dvd.iDVDStillTime = 0;

          m_dvd.iDVDStillStartTime = XbmcThreads::SystemClockMillis();

          /* adjust for the output delay in the video queue */
          unsigned int time = 0;
          if( m_CurrentVideo.stream && m_dvd.iDVDStillTime > 0 )
          {
            time = (unsigned int)(m_dvdPlayerVideo->GetOutputDelay() / ( DVD_TIME_BASE / 1000 ));
            if( time < 10000 && time > 0 )
              m_dvd.iDVDStillTime += time;
          }
          m_dvd.state = DVDSTATE_STILL;
          CLog::Log(LOGDEBUG,
                    "DVDNAV_STILL_FRAME - waiting %i sec, with delay of %d sec",
                    still_event->length, time / 1000);
        }
        return NAVRESULT_HOLD;
      }
      break;
    case DVDNAV_SPU_CLUT_CHANGE:
      {
        m_dvdPlayerSubtitle->SendMessage(new CDVDMsgSubtitleClutChange((uint8_t*)pData));
      }
      break;
    case DVDNAV_SPU_STREAM_CHANGE:
      {
        dvdnav_spu_stream_change_event_t* event = (dvdnav_spu_stream_change_event_t*)pData;

        int iStream = event->physical_wide;
        bool visible = !(iStream & 0x80);

        SetSubtitleVisibleInternal(visible);

        if (iStream >= 0)
          m_dvd.iSelectedSPUStream = (iStream & ~0x80);
        else
          m_dvd.iSelectedSPUStream = -1;

        m_CurrentSubtitle.stream = NULL;
      }
      break;
    case DVDNAV_AUDIO_STREAM_CHANGE:
      {
        // This should be the correct way i think, however we don't have any streams right now
        // since the demuxer hasn't started so it doesn't change. not sure how to do this.
        dvdnav_audio_stream_change_event_t* event = (dvdnav_audio_stream_change_event_t*)pData;

        // Tell system what audiostream should be opened by default
        if (event->logical >= 0)
          m_dvd.iSelectedAudioStream = event->physical;
        else
          m_dvd.iSelectedAudioStream = -1;

        m_CurrentAudio.stream = NULL;
      }
      break;
    case DVDNAV_HIGHLIGHT:
      {
        //dvdnav_highlight_event_t* pInfo = (dvdnav_highlight_event_t*)pData;
        int iButton = pStream->GetCurrentButton();
        CLog::Log(LOGDEBUG, "DVDNAV_HIGHLIGHT: Highlight button %d\n", iButton);
        m_dvdPlayerSubtitle->UpdateOverlayInfo((CDVDInputStreamNavigator*)m_pInputStream, LIBDVDNAV_BUTTON_NORMAL);
      }
      break;
    case DVDNAV_VTS_CHANGE:
      {
        //dvdnav_vts_change_event_t* vts_change_event = (dvdnav_vts_change_event_t*)pData;
        CLog::Log(LOGDEBUG, "DVDNAV_VTS_CHANGE");

        //Make sure we clear all the old overlays here, or else old forced items are left.
        m_overlayContainer.Clear();

        //Force an aspect ratio that is set in the dvdheaders if available
        m_CurrentVideo.hint.aspect = pStream->GetVideoAspectRatio();
        if( m_dvdPlayerVideo->IsInited() )
          m_dvdPlayerVideo->SendMessage(new CDVDMsgDouble(CDVDMsg::VIDEO_SET_ASPECT, m_CurrentVideo.hint.aspect));

        m_SelectionStreams.Clear(STREAM_NONE, STREAM_SOURCE_NAV);
        m_SelectionStreams.Update(m_pInputStream, m_pDemuxer);

        return NAVRESULT_HOLD;
      }
      break;
    case DVDNAV_CELL_CHANGE:
      {
        //dvdnav_cell_change_event_t* cell_change_event = (dvdnav_cell_change_event_t*)pData;
        CLog::Log(LOGDEBUG, "DVDNAV_CELL_CHANGE");

        if (m_dvd.state != DVDSTATE_STILL)
          m_dvd.state = DVDSTATE_NORMAL;
      }
      break;
    case DVDNAV_NAV_PACKET:
      {
          //pci_t* pci = (pci_t*)pData;

          // this should be possible to use to make sure we get
          // seamless transitions over these boundaries
          // if we remember the old vobunits boundaries
          // when a packet comes out of demuxer that has
          // pts values outside that boundary, it belongs
          // to the new vobunit, wich has new timestamps
          UpdatePlayState(0);
      }
      break;
    case DVDNAV_HOP_CHANNEL:
      {
        // This event is issued whenever a non-seamless operation has been executed.
        // Applications with fifos should drop the fifos content to speed up responsiveness.
        CLog::Log(LOGDEBUG, "DVDNAV_HOP_CHANNEL");
        if(m_dvd.state == DVDSTATE_SEEK)
          m_dvd.state = DVDSTATE_NORMAL;
        else
        {
          bool sync = !IsInMenuInternal();
          FlushBuffers(DVD_NOPTS_VALUE, false, sync);
          m_dvd.syncClock = true;
          m_dvd.state = DVDSTATE_NORMAL;
          if (m_pDemuxer)
            m_pDemuxer->Flush();
        }
          //m_messenger.Put(new CDVDMsg(CDVDMsg::GENERAL_FLUSH));

        return NAVRESULT_ERROR;
      }
      break;
    case DVDNAV_STOP:
      {
        CLog::Log(LOGDEBUG, "DVDNAV_STOP");
        m_dvd.state = DVDSTATE_NORMAL;
        CGUIDialogKaiToast::QueueNotification(g_localizeStrings.Get(16026), g_localizeStrings.Get(16029));
      }
      break;
    default:
    {}
      break;
    }
  }
  return NAVRESULT_NOP;
}

bool CDVDPlayer::ShowPVRChannelInfo(void)
{
  bool bReturn(false);

  if (CSettings::GetInstance().GetInt(CSettings::SETTING_PVRMENU_DISPLAYCHANNELINFO) > 0)
  {
    g_PVRManager.ShowPlayerInfo(CSettings::GetInstance().GetInt(CSettings::SETTING_PVRMENU_DISPLAYCHANNELINFO));

    bReturn = true;
  }

  return bReturn;
}

bool CDVDPlayer::OnAction(const CAction &action)
{
#define THREAD_ACTION(action) \
  do { \
    if (!IsCurrentThread()) { \
      m_messenger.Put(new CDVDMsgType<CAction>(CDVDMsg::GENERAL_GUI_ACTION, action)); \
      return true; \
    } \
  } while(false)

  CDVDInputStream::IMenus* pMenus = dynamic_cast<CDVDInputStream::IMenus*>(m_pInputStream);
  if (pMenus)
  {
    if (m_dvd.state == DVDSTATE_STILL && m_dvd.iDVDStillTime != 0 && pMenus->GetTotalButtons() == 0)
    {
      switch(action.GetID())
      {
        case ACTION_NEXT_ITEM:
        case ACTION_MOVE_RIGHT:
        case ACTION_MOVE_UP:
        case ACTION_SELECT_ITEM:
          {
            THREAD_ACTION(action);
            /* this will force us out of the stillframe */
            CLog::Log(LOGDEBUG, "%s - User asked to exit stillframe", __FUNCTION__);
            m_dvd.iDVDStillStartTime = 0;
            m_dvd.iDVDStillTime = 1;
          }
          return true;
      }
    }


    switch (action.GetID())
    {
/* this code is disabled to allow switching playlist items (dvdimage "stacks") */
#if 0
    case ACTION_PREV_ITEM:  // SKIP-:
      {
        THREAD_ACTION(action);
        CLog::Log(LOGDEBUG, " - pushed prev");
        pMenus->OnPrevious();
        g_infoManager.SetDisplayAfterSeek();
        return true;
      }
      break;
    case ACTION_NEXT_ITEM:  // SKIP+:
      {
        THREAD_ACTION(action);
        CLog::Log(LOGDEBUG, " - pushed next");
        pMenus->OnNext();
        g_infoManager.SetDisplayAfterSeek();
        return true;
      }
      break;
#endif
    case ACTION_TOGGLE_COMMSKIP:
      m_SkipCommercials = !m_SkipCommercials;
      CGUIDialogKaiToast::QueueNotification(
        g_localizeStrings.Get(25013),
        g_localizeStrings.Get(m_SkipCommercials ? 25015 : 25014));
      break;
    case ACTION_SHOW_VIDEOMENU:   // start button
      {
        THREAD_ACTION(action);
        CLog::Log(LOGDEBUG, " - go to menu");
        pMenus->OnMenu();
        if (m_playSpeed == DVD_PLAYSPEED_PAUSE)
        {
          SetPlaySpeed(DVD_PLAYSPEED_NORMAL);
          m_callback.OnPlayBackResumed();
        }
        // send a message to everyone that we've gone to the menu
        CGUIMessage msg(GUI_MSG_VIDEO_MENU_STARTED, 0, 0);
        g_windowManager.SendThreadMessage(msg);
        return true;
      }
      break;
    }

    if (pMenus->IsInMenu())
    {
      switch (action.GetID())
      {
      case ACTION_NEXT_ITEM:
        THREAD_ACTION(action);
        CLog::Log(LOGDEBUG, " - pushed next in menu, stream will decide");
        pMenus->OnNext();
        g_infoManager.SetDisplayAfterSeek();
        return true;
      case ACTION_PREV_ITEM:
        THREAD_ACTION(action);
        CLog::Log(LOGDEBUG, " - pushed prev in menu, stream will decide");
        pMenus->OnPrevious();
        g_infoManager.SetDisplayAfterSeek();
        return true;
      case ACTION_PREVIOUS_MENU:
      case ACTION_NAV_BACK:
        {
          THREAD_ACTION(action);
          CLog::Log(LOGDEBUG, " - menu back");
          pMenus->OnBack();
        }
        break;
      case ACTION_MOVE_LEFT:
        {
          THREAD_ACTION(action);
          CLog::Log(LOGDEBUG, " - move left");
          pMenus->OnLeft();
        }
        break;
      case ACTION_MOVE_RIGHT:
        {
          THREAD_ACTION(action);
          CLog::Log(LOGDEBUG, " - move right");
          pMenus->OnRight();
        }
        break;
      case ACTION_MOVE_UP:
        {
          THREAD_ACTION(action);
          CLog::Log(LOGDEBUG, " - move up");
          pMenus->OnUp();
        }
        break;
      case ACTION_MOVE_DOWN:
        {
          THREAD_ACTION(action);
          CLog::Log(LOGDEBUG, " - move down");
          pMenus->OnDown();
        }
        break;

      case ACTION_MOUSE_MOVE:
      case ACTION_MOUSE_LEFT_CLICK:
        {
          CRect rs, rd, rv;
          m_dvdPlayerVideo->GetVideoRect(rs, rd, rv);
          CPoint pt(action.GetAmount(), action.GetAmount(1));
          if (!rd.PtInRect(pt))
            return false; // out of bounds
          THREAD_ACTION(action);
          // convert to video coords...
          pt -= CPoint(rd.x1, rd.y1);
          pt.x *= rs.Width() / rd.Width();
          pt.y *= rs.Height() / rd.Height();
          pt += CPoint(rs.x1, rs.y1);
          if (action.GetID() == ACTION_MOUSE_LEFT_CLICK)
          {
            if (pMenus->OnMouseClick(pt))
              return true;
            else
            {
              CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1, static_cast<void*>(new CAction(ACTION_TRIGGER_OSD)));
              return false;
            }
          }
          return pMenus->OnMouseMove(pt);
        }
        break;
      case ACTION_SELECT_ITEM:
        {
          THREAD_ACTION(action);
          CLog::Log(LOGDEBUG, " - button select");
          // show button pushed overlay
          if(m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
            m_dvdPlayerSubtitle->UpdateOverlayInfo((CDVDInputStreamNavigator*)m_pInputStream, LIBDVDNAV_BUTTON_CLICKED);

          pMenus->ActivateButton();
        }
        break;
      case REMOTE_0:
      case REMOTE_1:
      case REMOTE_2:
      case REMOTE_3:
      case REMOTE_4:
      case REMOTE_5:
      case REMOTE_6:
      case REMOTE_7:
      case REMOTE_8:
      case REMOTE_9:
        {
          THREAD_ACTION(action);
          // Offset from key codes back to button number
          int button = action.GetID() - REMOTE_0;
          CLog::Log(LOGDEBUG, " - button pressed %d", button);
          pMenus->SelectButton(button);
        }
       break;
      default:
        return false;
        break;
      }
      return true; // message is handled
    }
  }

  if (dynamic_cast<CDVDInputStreamPVRManager*>(m_pInputStream))
  {
    switch (action.GetID())
    {
      case ACTION_MOVE_UP:
      case ACTION_NEXT_ITEM:
      case ACTION_CHANNEL_UP:
      {
        bool bPreview(action.GetID() == ACTION_MOVE_UP && // only up/down shows a preview, all others do switch
                      CSettings::GetInstance().GetBool(CSettings::SETTING_PVRPLAYBACK_CONFIRMCHANNELSWITCH));

        if (bPreview)
          m_messenger.Put(new CDVDMsg(CDVDMsg::PLAYER_CHANNEL_PREVIEW_NEXT));
        else
        {
          m_messenger.Put(new CDVDMsg(CDVDMsg::PLAYER_CHANNEL_NEXT));

          if (CSettings::GetInstance().GetInt(CSettings::SETTING_PVRPLAYBACK_CHANNELENTRYTIMEOUT) == 0)
            g_infoManager.SetDisplayAfterSeek();
        }

        ShowPVRChannelInfo();
        return true;
      }

      case ACTION_MOVE_DOWN:
      case ACTION_PREV_ITEM:
      case ACTION_CHANNEL_DOWN:
      {
        bool bPreview(action.GetID() == ACTION_MOVE_DOWN && // only up/down shows a preview, all others do switch
                      CSettings::GetInstance().GetBool(CSettings::SETTING_PVRPLAYBACK_CONFIRMCHANNELSWITCH));

        if (bPreview)
          m_messenger.Put(new CDVDMsg(CDVDMsg::PLAYER_CHANNEL_PREVIEW_PREV));
        else
        {
          m_messenger.Put(new CDVDMsg(CDVDMsg::PLAYER_CHANNEL_PREV));

          if (CSettings::GetInstance().GetInt(CSettings::SETTING_PVRPLAYBACK_CHANNELENTRYTIMEOUT) == 0)
            g_infoManager.SetDisplayAfterSeek();
        }

        ShowPVRChannelInfo();
        return true;
      }

      case ACTION_CHANNEL_SWITCH:
      {
        // Offset from key codes back to button number
        int channel = (int) action.GetAmount();
        m_messenger.Put(new CDVDMsgInt(CDVDMsg::PLAYER_CHANNEL_SELECT_NUMBER, channel));
        g_infoManager.SetDisplayAfterSeek();
        ShowPVRChannelInfo();
        return true;
      }
      break;
    }
  }

  switch (action.GetID())
  {
    case ACTION_NEXT_ITEM:
      if (GetChapter() > 0 && GetChapter() < GetChapterCount())
      {
        m_messenger.Put(new CDVDMsgPlayerSeekChapter(GetChapter() + 1));
        g_infoManager.SetDisplayAfterSeek();
        return true;
      }
      else
        break;
    case ACTION_PREV_ITEM:
      if (GetChapter() > 0)
      {
        m_messenger.Put(new CDVDMsgPlayerSeekChapter(GetChapter() - 1));
        g_infoManager.SetDisplayAfterSeek();
        return true;
      }
      else
        break;
  }

  // return false to inform the caller we didn't handle the message
  return false;
}

bool CDVDPlayer::IsInMenuInternal() const
{
  CDVDInputStream::IMenus* pStream = dynamic_cast<CDVDInputStream::IMenus*>(m_pInputStream);
  if (pStream)
  {
    if (m_dvd.state == DVDSTATE_STILL)
      return true;
    else
      return pStream->IsInMenu();
  }
  return false;
}


bool CDVDPlayer::IsInMenu() const
{
  CSingleLock lock(m_StateSection);
  return m_State.isInMenu;
}

bool CDVDPlayer::HasMenu() const
{
  CSingleLock lock(m_StateSection);
  return m_State.hasMenu;
}

std::string CDVDPlayer::GetPlayerState()
{
  CSingleLock lock(m_StateSection);
  return m_State.player_state;
}

bool CDVDPlayer::SetPlayerState(const std::string& state)
{
  m_messenger.Put(new CDVDMsgPlayerSetState(state));
  return true;
}

int CDVDPlayer::GetChapterCount()
{
  CSingleLock lock(m_StateSection);
  return m_State.chapters.size();
}

int CDVDPlayer::GetChapter()
{
  CSingleLock lock(m_StateSection);
  return m_State.chapter;
}

void CDVDPlayer::GetChapterName(std::string& strChapterName, int chapterIdx)
{
  CSingleLock lock(m_StateSection);
  if (chapterIdx == -1 && m_State.chapter > 0 && m_State.chapter <= (int) m_State.chapters.size())
    strChapterName = m_State.chapters[m_State.chapter - 1].first;
  else if (chapterIdx > 0 && chapterIdx <= (int) m_State.chapters.size())
    strChapterName = m_State.chapters[chapterIdx - 1].first;
}

int CDVDPlayer::SeekChapter(int iChapter)
{
  if (GetChapter() > 0)
  {
    if (iChapter < 0)
      iChapter = 0;
    if (iChapter > GetChapterCount())
      return 0;

    // Seek to the chapter.
    m_messenger.Put(new CDVDMsgPlayerSeekChapter(iChapter));
    SynchronizeDemuxer();
  }

  return 0;
}

int64_t CDVDPlayer::GetChapterPos(int chapterIdx)
{
  CSingleLock lock(m_StateSection);
  if (chapterIdx > 0 && chapterIdx <= (int) m_State.chapters.size())
    return m_State.chapters[chapterIdx - 1].second;

  return -1;
}

void CDVDPlayer::AddSubtitle(const std::string& strSubPath)
{
  m_messenger.Put(new CDVDMsgType<std::string>(CDVDMsg::SUBTITLE_ADDFILE, strSubPath));
}

void CDVDPlayer::AddSubtitle(const SPlayerSubtitleStreamInfo& info)
{
  m_messenger.Put(new CDVDMsgType<SPlayerSubtitleStreamInfo>(CDVDMsg::SUBTITLE_ADDSTREAMINFO, info));
}

int CDVDPlayer::GetCacheLevel() const
{
  CSingleLock lock(m_StateSection);
  return (int)(m_State.cache_level * 100);
}

double CDVDPlayer::GetQueueTime()
{
  int a = m_dvdPlayerAudio->GetLevel();
  int v = m_dvdPlayerVideo->GetLevel();
  return std::max(a, v) * StandardDemuxerQueueTime / 100;
}

void CDVDPlayer::GetVideoStreamInfo(int streamId, SPlayerVideoStreamInfo &info)
{
  CSingleLock lock(m_SelectionStreams.m_section);
  if (streamId == CURRENT_STREAM)
    streamId = GetVideoStream();

  if (streamId < 0 || streamId > GetVideoStreamCount() - 1)
  {
    //info.valid = false;
    return;
  }

  SelectionStream& s = m_SelectionStreams.Get(STREAM_VIDEO, streamId);
  if (s.language.length() > 0)
    info.language = s.language;

  if (s.name.length() > 0)
    info.name = s.name;

  info.bitrate = s.bitrate;
  info.width = s.width;
  info.height = s.height;
  info.SrcRect = s.SrcRect;
  info.DestRect = s.DestRect;
  info.videoCodecName = s.codec;
  info.videoAspectRatio = s.aspect_ratio;
  info.stereoMode = s.stereo_mode;
}

int CDVDPlayer::GetVideoStreamCount() const
{
  return m_SelectionStreams.Count(STREAM_VIDEO);
}

int CDVDPlayer::GetVideoStream() const
{
  return m_SelectionStreams.IndexOf(STREAM_VIDEO, *this);
}

int CDVDPlayer::GetSourceBitrate()
{
  if (m_pInputStream)
    return (int)m_pInputStream->GetBitstreamStats().GetBitrate();

  return 0;
}

void CDVDPlayer::GetAudioStreamInfo(int index, SPlayerAudioStreamInfo &info)
{
  CSingleLock lock(m_SelectionStreams.m_section);
  if (index == CURRENT_STREAM)
    index = GetAudioStream();

  if (index < 0 || index > GetAudioStreamCount() - 1 )
    return;

  SelectionStream& s = m_SelectionStreams.Get(STREAM_AUDIO, index);
  if(s.language.length() > 0)
    info.language = s.language;

  if(s.name.length() > 0)
    info.name = s.name;

  if(s.type == STREAM_NONE)
    info.name += " (Invalid)";

  info.bitrate = s.bitrate;
  info.channels = s.channels;
  info.audioCodecName = s.codec;
}

int CDVDPlayer::AddSubtitleFile(const std::string& filename, const std::string& subfilename, const std::string language, bool forced)
{
  std::string ext = URIUtils::GetExtension(filename);
  std::string vobsubfile = subfilename;
  if (ext == ".idx")
  {
    if (vobsubfile.empty()) {
      // find corresponding .sub (e.g. in case of manually selected .idx sub)
      vobsubfile = CUtil::GetVobSubSubFromIdx(filename);
      if (vobsubfile.empty())
        return -1;
    }

    CDVDDemuxVobsub v;
    if (!v.Open(filename, STREAM_SOURCE_NONE, vobsubfile))
      return -1;
    m_SelectionStreams.Update(NULL, &v, vobsubfile);

    ExternalStreamInfo info;
    CUtil::GetExternalStreamDetailsFromFilename(m_item.GetPath(), vobsubfile, info);

    for (int i = 0; i < v.GetNrOfSubtitleStreams(); ++i)
    {
      int index = m_SelectionStreams.IndexOf(STREAM_SUBTITLE, m_SelectionStreams.Source(STREAM_SOURCE_DEMUX_SUB, filename), i);
      SelectionStream& stream = m_SelectionStreams.Get(STREAM_SUBTITLE, index);

      if (stream.name.empty())
        stream.name = info.name;

      if (stream.language.empty())
        stream.language = info.language;

      if (static_cast<CDemuxStream::EFlags>(info.flag) != CDemuxStream::FLAG_NONE)
        stream.flags = static_cast<CDemuxStream::EFlags>(info.flag);
    }

    return m_SelectionStreams.IndexOf(STREAM_SUBTITLE, m_SelectionStreams.Source(STREAM_SOURCE_DEMUX_SUB, filename), 0);
  }
  if(ext == ".sub")
  {
    // if this looks like vobsub file (i.e. .idx found), add it as such
    std::string vobsubidx = CUtil::GetVobSubIdxFromSub(filename);
    if (!vobsubidx.empty())
      return AddSubtitleFile(vobsubidx, filename);
  }
  SelectionStream s;
  s.source   = m_SelectionStreams.Source(STREAM_SOURCE_TEXT, filename);
  s.type     = STREAM_SUBTITLE;
  s.id       = 0;
  s.filename = filename;
  if (m_item.IsMediaServiceBased())
  {
    s.language = language;
    s.name     = g_localizeStrings.Get(21602);
    s.flags    = forced ? CDemuxStream::FLAG_FORCED : CDemuxStream::FLAG_NONE;
  }
  else
  {
    ExternalStreamInfo info;
    CUtil::GetExternalStreamDetailsFromFilename(m_item.GetPath(), filename, info);
    s.name = info.name;
    s.language = info.language;
    if (static_cast<CDemuxStream::EFlags>(info.flag) != CDemuxStream::FLAG_NONE)
      s.flags = static_cast<CDemuxStream::EFlags>(info.flag);
  }
  m_SelectionStreams.Update(s);
  return m_SelectionStreams.IndexOf(STREAM_SUBTITLE, s.source, s.id);
}

int CDVDPlayer::AddSubtitleStreamInfo(const SPlayerSubtitleStreamInfo& info)
{
  SelectionStream s;
  s.source   = m_SelectionStreams.Source(STREAM_SOURCE_TEXT, info.file);
  s.type     = STREAM_SUBTITLE;
  s.id       = 0;
  s.filename = info.file;
  s.language = info.language;
  s.name     = g_localizeStrings.Get(21602);
  s.flags    = CDemuxStream::FLAG_NONE;
  m_SelectionStreams.Update(s);
  return m_SelectionStreams.IndexOf(STREAM_SUBTITLE, s.source, s.id);
}

void CDVDPlayer::UpdatePlayState(double timeout)
{
  if(m_State.timestamp != 0
  && m_State.timestamp + DVD_MSEC_TO_TIME(timeout) > m_clock.GetAbsoluteClock())
    return;

  SPlayerState state(m_State);

  if (m_CurrentVideo.dts != DVD_NOPTS_VALUE)
    state.dts = m_CurrentVideo.dts;
  else if (m_CurrentAudio.dts != DVD_NOPTS_VALUE)
    state.dts = m_CurrentAudio.dts;
  else if (m_CurrentVideo.startpts != DVD_NOPTS_VALUE)
    state.dts = m_CurrentVideo.startpts;
  else if (m_CurrentAudio.startpts != DVD_NOPTS_VALUE)
    state.dts = m_CurrentAudio.startpts;

  if (m_pDemuxer)
  {
    if (IsInMenuInternal())
      state.chapter = 0;
    else
      state.chapter = m_pDemuxer->GetChapter();

    state.chapters.clear();
    if (m_pDemuxer->GetChapterCount() > 0)
    {
      for (int i = 0; i < m_pDemuxer->GetChapterCount(); ++i)
      {
        std::string name;
        m_pDemuxer->GetChapterName(name, i + 1);
        state.chapters.push_back(make_pair(name, m_pDemuxer->GetChapterPos(i + 1)));
      }
    }

    // time = dts - m_offset_pts
    state.time = DVD_TIME_TO_MSEC(m_clock.GetClock(false));
    state.time_offset = 0;
    state.time_total = m_pDemuxer->GetStreamLength();
  }

  state.canpause = true;
  state.canseek = true;
  state.isInMenu = false;
  state.hasMenu = false;

  if (m_pInputStream)
  {
    // override from input stream if needed
    CDVDInputStreamPVRManager* pvrStream = dynamic_cast<CDVDInputStreamPVRManager*>(m_pInputStream);
    if (pvrStream)
    {
      state.canrecord = pvrStream->CanRecord();
      state.recording = pvrStream->IsRecording();
    }

    CDVDInputStream::IDisplayTime* pDisplayTime = dynamic_cast<CDVDInputStream::IDisplayTime*>(m_pInputStream);
    if (pDisplayTime && pDisplayTime->GetTotalTime() > 0)
    {
      if (state.dts != DVD_NOPTS_VALUE)
      {
        // dts is correct by offset_pts, so we need to revert this correction here
        // the results is: time = pDisplayTime->GetTime()
        state.time_offset += DVD_MSEC_TO_TIME(pDisplayTime->GetTime()) - state.dts + m_offset_pts;
        state.time += DVD_TIME_TO_MSEC(state.time_offset);
      }
      state.time_total = pDisplayTime->GetTotalTime();
    }

    if (CDVDInputStream::IMenus* ptr = dynamic_cast<CDVDInputStream::IMenus*>(m_pInputStream))
    {
      if (!ptr->GetState(state.player_state))
        state.player_state = "";

      if (m_dvd.state == DVDSTATE_STILL)
      {
        state.time = XbmcThreads::SystemClockMillis() - m_dvd.iDVDStillStartTime;
        state.time_total = m_dvd.iDVDStillTime;
        state.isInMenu = true;
      }
      else if (IsInMenuInternal())
      {
        state.time = pDisplayTime->GetTime();
        state.time_offset = 0;
        state.isInMenu = true;
      }
      state.hasMenu = true;
    }

    if (CDVDInputStream::ISeekable* ptr = dynamic_cast<CDVDInputStream::ISeekable*>(m_pInputStream))
    {
      state.canpause = ptr->CanPause();
      state.canseek  = ptr->CanSeek();
    }
  }

  if (m_Edl.HasCut())
  {
    state.time        = (double) m_Edl.RemoveCutTime(llrint(state.time));
    state.time_total  = (double) m_Edl.RemoveCutTime(llrint(state.time_total));
  }

  if(state.time_total <= 0)
    state.canseek  = false;

  if (m_CurrentAudio.id >= 0 && m_pDemuxer)
  {
    CDemuxStream* pStream = m_pDemuxer->GetStream(m_CurrentAudio.id);
    if (pStream && pStream->type == STREAM_AUDIO)
      ((CDemuxStreamAudio*)pStream)->GetStreamInfo(state.demux_audio);
  }
  else
    state.demux_audio = "";

  if (m_CurrentVideo.id >= 0 && m_pDemuxer)
  {
    CDemuxStream* pStream = m_pDemuxer->GetStream(m_CurrentVideo.id);
    if (pStream && pStream->type == STREAM_VIDEO)
      ((CDemuxStreamVideo*)pStream)->GetStreamInfo(state.demux_video);
  }
  else
    state.demux_video = "";

  state.cache_delay  = 0.0;
  state.cache_level  = std::min(1.0, GetQueueTime() / StandardDemuxerQueueTime);
  state.cache_offset = GetQueueTime() / state.time_total;

  XFILE::SCacheStatus status;
  if(m_pInputStream && m_pInputStream->GetCacheStatus(&status))
  {
    state.cache_bytes = status.forward;
    if(state.time_total)
      state.cache_bytes += m_pInputStream->GetLength() * (int64_t) (GetQueueTime() / state.time_total);
  }
  else
    state.cache_bytes = 0;

  state.timestamp = m_clock.GetAbsoluteClock();

  CSingleLock lock(m_StateSection);
  m_State = state;
}

void CDVDPlayer::UpdateApplication(double timeout)
{
  if(m_UpdateApplication != 0
  && m_UpdateApplication + DVD_MSEC_TO_TIME(timeout) > m_clock.GetAbsoluteClock())
    return;

  CDVDInputStreamPVRManager* pStream = dynamic_cast<CDVDInputStreamPVRManager*>(m_pInputStream);
  if(pStream)
  {
    CFileItem item(g_application.CurrentFileItem());
    if(pStream->UpdateItem(item))
    {
      g_application.CurrentFileItem() = item;
      CApplicationMessenger::GetInstance().PostMsg(TMSG_UPDATE_CURRENT_ITEM, 0, -1, static_cast<void*>(new CFileItem(item)));
    }
  }
  m_UpdateApplication = m_clock.GetAbsoluteClock();
}

bool CDVDPlayer::CanRecord()
{
  CSingleLock lock(m_StateSection);
  return m_State.canrecord;
}

bool CDVDPlayer::IsRecording()
{
  CSingleLock lock(m_StateSection);
  return m_State.recording;
}

bool CDVDPlayer::Record(bool bOnOff)
{
  if (m_pInputStream && (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_TV) ||
                         m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER)) )
  {
    m_messenger.Put(new CDVDMsgBool(CDVDMsg::PLAYER_SET_RECORD, bOnOff));
    return true;
  }
  return false;
}

bool CDVDPlayer::GetStreamDetails(CStreamDetails &details)
{
  if (m_pDemuxer && !m_bAbortRequest)
  {
    std::vector<SelectionStream> subs = m_SelectionStreams.Get(STREAM_SUBTITLE);
    std::vector<CStreamDetailSubtitle> extSubDetails;
    for (unsigned int i = 0; i < subs.size(); i++)
    {
      if (subs[i].filename == m_item.GetPath())
        continue;

      CStreamDetailSubtitle p;
      p.m_strLanguage = subs[i].language;
      extSubDetails.push_back(p);
    }

    bool result = CDVDFileInfo::DemuxerToStreamDetails(m_pInputStream, m_pDemuxer, extSubDetails, details);
    if (result && details.GetStreamCount(CStreamDetail::VIDEO) > 0) // this is more correct (dvds in particular)
    {
      /*
       * We can only obtain the aspect & duration from dvdplayer when the Process() thread is running
       * and UpdatePlayState() has been called at least once. In this case dvdplayer duration/AR will
       * return 0 and we'll have to fallback to the (less accurate) info from the demuxer.
       */
      float aspect = m_dvdPlayerVideo->GetAspectRatio();
      if (aspect > 0.0f)
        ((CStreamDetailVideo*)details.GetNthStream(CStreamDetail::VIDEO,0))->m_fAspect = aspect;

      int64_t duration = GetTotalTime() / 1000;
      if (duration > 0)
        ((CStreamDetailVideo*)details.GetNthStream(CStreamDetail::VIDEO,0))->m_iDuration = (int) duration;
    }
    return result;
  }
  else
    return false;
}

std::string CDVDPlayer::GetPlayingTitle()
{
  /* Currently we support only Title Name from Teletext line 30 */
  TextCacheStruct_t* ttcache = m_dvdPlayerTeletext->GetTeletextCache();
  if (ttcache && !ttcache->line30.empty())
    return ttcache->line30;

  return "";
}

bool CDVDPlayer::SwitchChannel(const CPVRChannelPtr &channel)
{
  if (g_PVRManager.IsPlayingChannel(channel))
    return false; // desired channel already active, nothing to do.

  if (!g_PVRManager.CheckParentalLock(channel))
    return false;

  /* set GUI info */
  if (!g_PVRManager.PerformChannelSwitch(channel, true))
    return false;

  UpdateApplication(0);
  UpdatePlayState(0);

  /* select the new channel */
  m_messenger.Put(new CDVDMsgType<CPVRChannelPtr>(CDVDMsg::PLAYER_CHANNEL_SELECT, channel));

  return true;
}

// IDispResource interface
void CDVDPlayer::OnLostDevice()
{
  CLog::Log(LOGNOTICE, "CDVDPlayer: OnLostDevice received");
  m_dvdPlayerAudio->SendMessage(new CDVDMsgBool(CDVDMsg::GENERAL_PAUSE, true), 1);
  m_dvdPlayerVideo->SendMessage(new CDVDMsgBool(CDVDMsg::GENERAL_PAUSE, true), 1);
  m_clock.Pause(true);
  m_displayState = AV_DISPLAY_LOST;
}

void CDVDPlayer::OnResetDevice()
{
  CLog::Log(LOGNOTICE, "CDVDPlayer: OnResetDevice received");
  m_displayResetDelay = 100 * CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOPLAYER_PAUSEAFTERREFRESHCHANGE);
  if (CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOPLAYER_ADJUSTREFRESHRATE) == ADJUST_REFRESHRATE_OFF)
    m_displayResetDelay = 0;
  m_displayResetTimer.StartZero();
  m_displayState = AV_DISPLAY_RESET;
}

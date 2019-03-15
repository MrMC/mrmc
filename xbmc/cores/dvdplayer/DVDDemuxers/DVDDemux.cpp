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

#include "DVDDemux.h"

void CDemuxStreamTeletext::GetStreamInfo(std::string& strInfo)
{
  strInfo = "Teletext Data Stream";
}

void CDemuxStreamRadioRDS::GetStreamInfo(std::string& strInfo)
{
  strInfo = "Radio Data Stream (RDS)";
}

void CDemuxStreamAudio::GetStreamType(std::string& strInfo)
{
  char sInfo[64] = {0};

  if (codec == AV_CODEC_ID_AC3) strcpy(sInfo, "AC3 ");
  else if (codec == AV_CODEC_ID_DTS)
  {
#ifdef FF_PROFILE_DTS_HD_MA
    if (profile == FF_PROFILE_DTS_HD_MA)
      strcpy(sInfo, "DTS-HD MA ");
    else if (profile == FF_PROFILE_DTS_HD_HRA)
      strcpy(sInfo, "DTS-HD HRA ");
    else
#endif
      strcpy(sInfo, "DTS ");
  }
  else if (codec == AV_CODEC_ID_MP2) strcpy(sInfo, "MP2 ");
  else if (codec == AV_CODEC_ID_TRUEHD) strcpy(sInfo, "Dolby TrueHD ");
  else strcpy(sInfo, "");

  if (iChannels == 1) strcat(sInfo, "Mono");
  else if (iChannels == 2) strcat(sInfo, "Stereo");
  else if (iChannels == 6) strcat(sInfo, "5.1");
  else if (iChannels == 8) strcat(sInfo, "7.1");
  else if (iChannels != 0)
  {
    char temp[32];
    sprintf(temp, " %d%s", iChannels, "-chs");
    strcat(sInfo, temp);
  }
  strInfo = sInfo;
}

int CDVDDemux::GetNrOfAudioStreams()
{
  int iCounter = 0;

  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);
    if (pStream->type == STREAM_AUDIO) iCounter++;
  }

  return iCounter;
}

int CDVDDemux::GetNrOfVideoStreams()
{
  int iCounter = 0;

  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);
    if (pStream->type == STREAM_VIDEO) iCounter++;
  }

  return iCounter;
}

int CDVDDemux::GetNrOfSubtitleStreams()
{
  int iCounter = 0;

  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);
    if (pStream->type == STREAM_SUBTITLE) iCounter++;
  }

  return iCounter;
}

int CDVDDemux::GetNrOfTeletextStreams()
{
  int iCounter = 0;

  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);
    if (pStream->type == STREAM_TELETEXT) iCounter++;
  }

  return iCounter;
}

const int CDVDDemux::GetNrOfRadioRDSStreams()
{
  int iCounter = 0;

  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);
    if (pStream->type == STREAM_RADIO_RDS) iCounter++;
  }

  return iCounter;
}

CDemuxStreamAudio* CDVDDemux::GetStreamFromAudioId(int iAudioIndex)
{
  int counter = -1;
  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);

    if (pStream->type == STREAM_AUDIO) counter++;
    if (iAudioIndex == counter)
      return (CDemuxStreamAudio*)pStream;
  }
  return NULL;
}

CDemuxStreamVideo* CDVDDemux::GetStreamFromVideoId(int iVideoIndex)
{
  int counter = -1;
  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);

    if (pStream->type == STREAM_VIDEO) counter++;
    if (iVideoIndex == counter)
      return (CDemuxStreamVideo*)pStream;
  }
  return NULL;
}

CDemuxStreamSubtitle* CDVDDemux::GetStreamFromSubtitleId(int iSubtitleIndex)
{
  int counter = -1;
  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);

    if (pStream->type == STREAM_SUBTITLE) counter++;
    if (iSubtitleIndex == counter)
      return (CDemuxStreamSubtitle*)pStream;
  }
  return NULL;
}

CDemuxStreamTeletext* CDVDDemux::GetStreamFromTeletextId(int iTeletextIndex)
{
  int counter = -1;
  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);

    if (pStream->type == STREAM_TELETEXT) counter++;
    if (iTeletextIndex == counter)
      return (CDemuxStreamTeletext*)pStream;
  }
  return NULL;
}

const CDemuxStreamRadioRDS* CDVDDemux::GetStreamFromRadioRDSId(int iRadioRDSIndex)
{
  int counter = -1;
  for (int i = 0; i < GetNrOfStreams(); i++)
  {
    CDemuxStream* pStream = GetStream(i);

    if (pStream->type == STREAM_RADIO_RDS) counter++;
    if (iRadioRDSIndex == counter)
      return (CDemuxStreamRadioRDS*)pStream;
  }
  return NULL;
}

void CDemuxStream::GetStreamName( std::string& strInfo )
{
  strInfo = "";
}

AVDiscard CDemuxStream::GetDiscard()
{
  return AVDISCARD_NONE;
}

void CDemuxStream::SetDiscard(AVDiscard discard)
{
  return;
}

void CDemuxStream::CheckForInterlaced(const AVCodecParserContext *parser)
{
  CDemuxStreamVideo *vstream = dynamic_cast<CDemuxStreamVideo*>(this);
  // paranoid check to make sure we are a video stream
  if (vstream == nullptr)
    return;

  if (vstream->bMaybeInterlaced > -1) // we already came to a conclusion
    return;

  if (parser)
  {
    switch(parser->field_order)
    {
      default:
      case AV_FIELD_PROGRESSIVE:
        // default value for bMaybeInterlaced but we set it anyway
        vstream->bMaybeInterlaced = 0;
        break;
      case AV_FIELD_TT: //< Top coded_first, top displayed first
      case AV_FIELD_BB: //< Bottom coded first, bottom displayed first
      case AV_FIELD_TB: //< Top coded first, bottom displayed first
      case AV_FIELD_BT: //< Bottom coded first, top displayed first
        vstream->bMaybeInterlaced = 1;
        break;
      case AV_FIELD_UNKNOWN:
      {
        // if picture_structure is AV_PICTURE_STRUCTURE_UNKNOWN, no clue so assume progressive
        bool interlaced = parser->picture_structure == AV_PICTURE_STRUCTURE_TOP_FIELD;
        interlaced |= parser->picture_structure == AV_PICTURE_STRUCTURE_BOTTOM_FIELD;
        vstream->bMaybeInterlaced = (interlaced ? 1 : 0);
      }
        break;
    }
  }
}

void CDemuxStream::CheckForInterlaced(const AVStream *stream)
{
  CDemuxStreamVideo *vstream = dynamic_cast<CDemuxStreamVideo*>(this);
  // paranoid check to make sure we are a video stream
  if (vstream == nullptr)
    return;

  if (vstream->bMaybeInterlaced > -1) // we already came to a conclusion
    return;

  if (stream->parser)
  {
    CheckForInterlaced(stream->parser);
    return;
  }
  else
  {
    AVCodecContext *avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
      return;

    int ret = avcodec_parameters_to_context(avctx, stream->codecpar);
    if (ret < 0)
    {
      avcodec_free_context(&avctx);
      return;
    }

    switch(avctx->field_order)
    {
      default:
      case AV_FIELD_PROGRESSIVE:
        vstream->bMaybeInterlaced = 0;
        break;
      case AV_FIELD_TT: //< Top coded_first, top displayed first
      case AV_FIELD_BB: //< Bottom coded first, bottom displayed first
      case AV_FIELD_TB: //< Top coded first, bottom displayed first
      case AV_FIELD_BT: //< Bottom coded first, top displayed first
        vstream->bMaybeInterlaced = 1;
        break;
      case AV_FIELD_UNKNOWN:
        // No clue, yet
        break;
    }
  }
}

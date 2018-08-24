/*
 *      Copyright (C) 2016 Team MrMC
 *      https://github.com/MrMC
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
 *  along with MrMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <AudioToolbox/AudioToolbox.h>

#include "DVDAudioCodecAudioConverter.h"
#include "utils/log.h"

//#define DEBUG_VERBOSE 1

/*
ac3_channel_l = 1
ac3_channel_r = 2
ac3_channel_c = 4
ac3_channel_lfe = 8
ac3_channel_ls = 16
ac3_channel_rs = 32
ac3_channel_lsb = 512
ac3_channel_rsb = 1024
*/

#pragma mark - statics/structs/etc
enum DDLayout {
  DDLayout_1_0  = 4,
  DDLayout_2_0  = 3,
  DDLayout_2_1  = 11,
  DDLayout_3_0  = 7,
  DDLayout_3_1  = 15,
  DDLayout_5_0a = 55,
  DDLayout_5_0b = 1543,
  DDLayout_5_1a = 63,
  DDLayout_5_1b = 1551,
  DDLayout_7_0  = 1591,
  DDLayout_7_1  = 1599,
};

enum DDIndex {
  DDIndex_1_0  = 0,
  DDIndex_2_0  = 1,
  DDIndex_2_1  = 2,
  DDIndex_3_0  = 3,
  DDIndex_3_1  = 4,
  DDIndex_5_0a = 5,
  DDIndex_5_0b = 6,
  DDIndex_5_1a = 7,
  DDIndex_5_1b = 8,
  DDIndex_7_0  = 9,
  DDIndex_7_1  = 10,
};

static const AEChannel DolbyChannels[11][9] = {
{ AE_CH_FC , AE_CH_NULL },
{ AE_CH_FL , AE_CH_FR , AE_CH_NULL },
{ AE_CH_FL , AE_CH_FR , AE_CH_LFE, AE_CH_NULL },
{ AE_CH_FL , AE_CH_FC , AE_CH_FR , AE_CH_NULL },
{ AE_CH_FL , AE_CH_FC , AE_CH_FR , AE_CH_LFE, AE_CH_NULL },
{ AE_CH_FL , AE_CH_FC , AE_CH_FR , AE_CH_BL , AE_CH_BR , AE_CH_NULL},
{ AE_CH_FL , AE_CH_FC , AE_CH_FR , AE_CH_SL , AE_CH_SR , AE_CH_NULL},
{ AE_CH_FL , AE_CH_FC , AE_CH_FR , AE_CH_SL , AE_CH_SR , AE_CH_LFE, AE_CH_NULL},
{ AE_CH_FL , AE_CH_FC , AE_CH_FR , AE_CH_BL , AE_CH_BR , AE_CH_LFE, AE_CH_NULL},
{ AE_CH_FL , AE_CH_FC , AE_CH_FR , AE_CH_SL , AE_CH_SR , AE_CH_BL , AE_CH_BR , AE_CH_NULL},
{ AE_CH_FL , AE_CH_FC , AE_CH_FR , AE_CH_SL , AE_CH_SR , AE_CH_BL , AE_CH_BR , AE_CH_LFE, AE_CH_NULL}
};

#if FALSE
static int ParseFrameSize(uint8_t *data, int datasize)
{
  static const uint16_t AC3Bitrates [] = {32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 576, 640};

  // data chunk better be greater than 256
  if (datasize < 256)
    return 0;

  // no sync at start, bad header
  if (data[0] != 0x0b || data[1] != 0x77)
    return 0;

  unsigned frame_size = 0;
  if (data[5] >> 3 <= 10)
  {
    // Normal AC-3
    uint8_t fscod = data[4] >> 6;
    uint8_t frmsizecod = data[4] & 0x3F;
    if (fscod == 3 || frmsizecod > 37)
      return 0;

    int bitRate  = AC3Bitrates[frmsizecod >> 1];
    switch (fscod)
    {
      case 0: frame_size = bitRate * 2; break;
      case 1: frame_size = (320 * bitRate / 147 + (frmsizecod & 1 ? 1 : 0)); break;
      case 2: frame_size = bitRate * 4; break;
    }
  }
  else
  {
    // Enhanced AC-3
    frame_size = (((data[2] & 0x7) << 8) | data[3]) + 1;
  }

  return frame_size << 1;
}
#endif
typedef struct AudioBufferIO
{
	char *buffer;
	int   packets;
  int   channels;
  int   framesize;
	AudioStreamPacketDescription packet_desciption;
} AudioBufferIO;

#pragma mark - CAudioIBufferQueue
class CAudioIBufferQueue
{
public:
  CAudioIBufferQueue()
  {
    m_inuse = nullptr;
    pthread_mutex_init(&m_mutex, nullptr);
  }

 ~CAudioIBufferQueue()
  {
    pthread_mutex_destroy(&m_mutex);
    free_buffer(m_inuse);
    while (!m_active.empty())
    {
      AudioBufferIO *buff = m_active.front();
      m_active.pop();
      free_buffer(buff);
    }
  }

  void enqueue(AudioBufferIO *buff)
  {
    pthread_mutex_lock(&m_mutex);
    m_active.push(buff);
    pthread_mutex_unlock(&m_mutex);
  }

  AudioBufferIO* dequeue()
  {
    // dequeue is special, we need to keep the queue'ed buffer alive until the
    // next dequeue call. Dequeue is only used by AudioCOnverter callback and a pointer
    // to the dequeue'ed buffer is referenced until next callback.
    pthread_mutex_lock(&m_mutex);
    AudioBufferIO *buff = m_active.front();
    m_active.pop();
    free_buffer(m_inuse);
    m_inuse = buff;
    pthread_mutex_unlock(&m_mutex);
    return buff;
  }

  size_t empty()
  {
    return m_active.empty();
  }

protected:
  void free_buffer(AudioBufferIO *buff)
  {
    if (buff)
      free(buff->buffer);
    delete buff;
  }

  pthread_mutex_t m_mutex;
  AudioBufferIO  *m_inuse;
  std::queue<AudioBufferIO*> m_active;
};

#pragma mark - AudioConverter input callback
static OSStatus converterCallback(AudioConverterRef inAudioConverter,
  UInt32 *ioNumberDataPackets, AudioBufferList *ioData,
  AudioStreamPacketDescription **outDataPacketDescription,
  void *inUserData)
{
  // got nothing for you, head back to camp
  CAudioIBufferQueue *abuff = (CAudioIBufferQueue*)inUserData;
  if (abuff->empty())
  {
    *ioNumberDataPackets = 0;
    ioData->mBuffers[0].mData = nullptr;
    ioData->mBuffers[0].mDataByteSize = 0;
    // setting these and returning any error
    // will tell AudioConverter to try again later
    // on next AudioConverterFillComplexBuffer call.
    // https://developer.apple.com/library/mac/qa/qa1317/_index.html
    return -1;
  }

#ifdef DEBUG_VERBOSE
  CLog::Log(LOGDEBUG, "%s - ioNumberDataPackets1(%d)", __FUNCTION__, *ioNumberDataPackets);
#endif

  AudioBufferIO *buff = abuff->dequeue();
  // clamp to what we have
	if (*ioNumberDataPackets > (UInt32)buff->packets)
    *ioNumberDataPackets = buff->packets;

#ifdef DEBUG_VERBOSE
  CLog::Log(LOGDEBUG, "%s - ioNumberDataPackets2(%d)", __FUNCTION__, *ioNumberDataPackets);
#endif
  // assign the data pointer into the buffer list
  // note:  the callback is responsible for not freeing
  // or altering this buffer until it is called again.
  // CAudioIBufferQueue will take care of this
  ioData->mNumberBuffers = 1;
  ioData->mBuffers[0].mData = buff->buffer;
  ioData->mBuffers[0].mDataByteSize = buff->framesize;
  ioData->mBuffers[0].mNumberChannels = buff->channels;
  if (outDataPacketDescription)
  {
    buff->packet_desciption.mStartOffset = 0;
    buff->packet_desciption.mVariableFramesInPacket = *ioNumberDataPackets;
    buff->packet_desciption.mDataByteSize = buff->framesize;
    *outDataPacketDescription = &buff->packet_desciption;
  }

	return noErr;
}

#pragma mark - CDVDAudioCodecAudioConverter
CDVDAudioCodecAudioConverter::CDVDAudioCodecAudioConverter()
: m_formatName("dac")
, m_codec(nullptr)
, m_iBuffer(nullptr)
, m_oBuffer(nullptr)
, m_oBufferSize(0)
, m_gotFrame(false)
, m_currentPts(DVD_NOPTS_VALUE)
{
}

CDVDAudioCodecAudioConverter::~CDVDAudioCodecAudioConverter()
{
  Dispose();
}

bool CDVDAudioCodecAudioConverter::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  switch (hints.codec)
  {
    case AV_CODEC_ID_AC3:
      m_formatName = "dac-ac3";
      break;

    case AV_CODEC_ID_EAC3:
      if (hints.bitrate >= 640000)
        return false;
      m_formatName = "dac-ec3";
      break;

    default:
      return false;
  }

  m_hints = hints;
  // special exceptions, not a clue why, yet
  if (m_hints.channels == 0)
    m_hints.channels = 2;
  if (m_hints.samplerate == 0)
    m_hints.samplerate = 48000;

  // m_format is output format
  m_format.m_dataFormat = AE_FMT_FLOAT;
  m_format.m_sampleRate = 48000;

  char buf[1024] = {0};
  int buf_size = 1024;
  av_get_channel_layout_string(buf, buf_size, m_hints.channels, m_hints.channellayout);
  CLog::Log(LOGDEBUG, "FactoryCodec - Audio: dac - channel_layout(%llu) = '%s'", m_hints.channellayout, buf);

  DDIndex index;
  switch(m_hints.channellayout)
  {
    case DDLayout_1_0:
      index = DDIndex_1_0;
      break;
    case DDLayout_2_0:
      index = DDIndex_2_0;
      break;
    case DDLayout_2_1:
      index = DDIndex_2_1;
      break;
    case DDLayout_3_0:
      index = DDIndex_3_0;
      break;
    case DDLayout_3_1:
      index = DDIndex_3_1;
      break;
    case DDLayout_5_0a:
      index = DDIndex_5_0a;
      break;
    case DDLayout_5_0b:
      index = DDIndex_5_0b;
      break;
    case DDLayout_5_1a:
      index = DDIndex_5_1a;
      break;
    case DDLayout_5_1b:
      index = DDIndex_5_1b;
      break;
    case DDLayout_7_0:
      index = DDIndex_7_0;
      break;
    case DDLayout_7_1:
      index = DDIndex_7_1;
      break;
    default:
      // no clue about channel layout, try it old school
      switch(m_hints.channels)
      {
        case 1:
          index = DDIndex_1_0;
          break;
        case 2:
          index = DDIndex_2_0;
          break;
        case 3:
          index = DDIndex_2_1;
          break;
        case 6:
          index = DDIndex_5_1b;
          break;
        case 8:
          index = DDIndex_7_1;
          break;
        default:
          return false;
          break;
      }
      break;
  }
  for (int i = 0; i < m_hints.channels; ++i)
    m_format.m_channelLayout += DolbyChannels[index][i];

  AudioStreamBasicDescription iformat = {0};
  iformat.mSampleRate = hints.samplerate;
  iformat.mFormatID = m_hints.codec == AV_CODEC_ID_AC3 ? kAudioFormatAC3: kAudioFormatEnhancedAC3;
  // mFramesPerPacket must be 768 for all ac3/eac3 flavors
  iformat.mFramesPerPacket = 768;
  iformat.mChannelsPerFrame = m_hints.channels;

  AudioStreamBasicDescription oformat = {0};
  oformat.mSampleRate = 48000;
  oformat.mFormatID = kAudioFormatLinearPCM;
  oformat.mFormatFlags = kLinearPCMFormatFlagIsFloat | kLinearPCMFormatFlagIsPacked;
  oformat.mFramesPerPacket = 1;
  oformat.mChannelsPerFrame = m_hints.channels;
  oformat.mBitsPerChannel = CAEUtil::DataFormatToBits(AE_FMT_FLOAT);
  oformat.mBytesPerPacket = oformat.mChannelsPerFrame * oformat.mBitsPerChannel / 8;
  oformat.mBytesPerFrame  = oformat.mFramesPerPacket  * oformat.mBytesPerPacket;

  AudioConverterRef audioconverter;
  int err = AudioConverterNew(&iformat, &oformat, &audioconverter);
  if (err)
    return false;

  m_codec = (void*)audioconverter;
  m_iBuffer = new CAudioIBufferQueue();
  m_currentPts = DVD_NOPTS_VALUE;

  return true;
}

void CDVDAudioCodecAudioConverter::Dispose()
{
  if (m_codec)
  {
    AudioConverterDispose((AudioConverterRef)m_codec);
    m_codec = nullptr;
  }
  SAFE_DELETE(m_iBuffer);
  SAFE_DELETE_ARRAY(m_oBuffer);
  m_oBufferSize = 0;
}

int CDVDAudioCodecAudioConverter::Decode(uint8_t* pData, int iSize, double dts, double pts)
{
  if (!pData || iSize <= 0)
    return 0;

  // some ac3/eac3 has 14 byte padding (seems to come from ts files)
  // ffmpeg strips it internally during decode
  // but autotoolbox does not like it, so we strip it (only if padded).
  int sizeInternal = iSize;
	float fSize = (float)sizeInternal / 256;
  if ((sizeInternal - ((int)fSize * 256)) == 14)
  {
    sizeInternal = (int)fSize * 256;
    if (!m_paddingDetected)
    {
      m_paddingDetected = true;
      CLog::Log(LOGDEBUG, "%s - AC3 padding detected, iSize(%d), sizeInternal(%d)", __FUNCTION__, iSize, sizeInternal);
    }
  }

  if (!m_oBuffer)
  {
    // set up our output buffer
    // AudioCOnverter always seems to return no more than 1536
    // for any ac3/eac3 flavor.
    m_oBufferSize = 1536 * m_hints.channels * 4;
    // just a little extra because I'm paranoid
    m_oBuffer = new uint8_t[m_oBufferSize * 4];
  }

  //CLog::MemDump((char*)pData, sizeInternal);

  // set up a input buffer for reading
  // need to do it this way as some eac3 seems to
  // have variable framesize and AudioConverter
  // is very picky that ac3/eac3 frame sizes are correct.
  AudioBufferIO *converter_buff = new AudioBufferIO;
  converter_buff->packets = 1;
  converter_buff->framesize = sizeInternal;
  converter_buff->buffer = (char*)calloc(sizeInternal, 1);
  converter_buff->channels = m_hints.channels;
  converter_buff->packet_desciption = {0};
  memcpy(converter_buff->buffer, pData, sizeInternal);
  m_iBuffer->enqueue(converter_buff);
  m_currentPts = pts;

  // setup output buffer list
  AudioBufferList output_bufferlist = {0};
  output_bufferlist.mNumberBuffers = 1;
  output_bufferlist.mBuffers[0].mNumberChannels = m_hints.channels;
  output_bufferlist.mBuffers[0].mDataByteSize = m_oBufferSize;
  output_bufferlist.mBuffers[0].mData = m_oBuffer;

  AudioStreamPacketDescription *outputPktDescs = nullptr;
  UInt32 ioBytesPerPacket = m_hints.channels * CAEUtil::DataFormatToBits(m_format.m_dataFormat) / 8;
  UInt32 ioOutputDataPackets = m_oBufferSize / ioBytesPerPacket;

  // setup to run the auto converter
  int err = 0;
  int loops = 0;
  int ioOutputDataPacketsTotal = 0;
  //while (err == 0 && ioOutputDataPackets > 0)
  {
    // kAudioCodecUnsupportedFormatError = '!dat'
    output_bufferlist.mNumberBuffers = 1;
    output_bufferlist.mBuffers[0].mNumberChannels = m_hints.channels;
    output_bufferlist.mBuffers[0].mDataByteSize = m_oBufferSize;
    output_bufferlist.mBuffers[0].mData = m_oBuffer;

    // AudioConverterFillComplexBuffer is semi-sync. It will continue to call the
    // callback unless flagged in callback that there is no current data to fetch.
    // Generally, you will get a one per one conversion if you do your math right.
    err = AudioConverterFillComplexBuffer((AudioConverterRef)m_codec, converterCallback,
      m_iBuffer, &ioOutputDataPackets, &output_bufferlist, outputPktDescs);
    ioOutputDataPacketsTotal += ioOutputDataPackets;
    loops++;
  }
  if (ioOutputDataPacketsTotal > 0)
  {
#ifdef DEBUG_VERBOSE
    CLog::Log(LOGDEBUG, "%s - loops(%d) ioOutputDataPacketsTotal(%d)", __FUNCTION__, loops, ioOutputDataPacketsTotal);
#endif
    m_gotFrame = true;
  }

  return iSize;
}

void CDVDAudioCodecAudioConverter::GetData(DVDAudioFrame &frame)
{
  unsigned int framebits = CAEUtil::DataFormatToBits(m_format.m_dataFormat);

  frame.passthrough = false;
  frame.format.m_dataFormat = m_format.m_dataFormat;
  frame.format.m_channelLayout = m_format.m_channelLayout;
  frame.framesize = (framebits >> 3) * frame.format.m_channelLayout.Count();
  if(frame.framesize == 0)
    return;
  frame.nb_frames = GetData(frame.data)/frame.framesize;
  frame.planes = 1;
  frame.bits_per_sample = framebits;
  frame.format.m_sampleRate = m_format.m_sampleRate;
  frame.matrix_encoding = GetMatrixEncoding();
  frame.audio_service_type = GetAudioServiceType();
  frame.profile = GetProfile();
  // compute duration.
  if (frame.format.m_sampleRate)
    frame.duration = ((double)frame.nb_frames * DVD_TIME_BASE) / frame.format.m_sampleRate;
  else
    frame.duration = 0.0;

  frame.pts = m_currentPts;
}

int CDVDAudioCodecAudioConverter::GetData(uint8_t** dst)
{
  if (m_gotFrame)
  {
    *dst = m_oBuffer;
    m_gotFrame = false;
    return m_oBufferSize;
  }
  return 0;
}

void CDVDAudioCodecAudioConverter::Reset()
{
  if (m_codec)
    AudioConverterReset((AudioConverterRef)m_codec);
  m_currentPts = DVD_NOPTS_VALUE;
}

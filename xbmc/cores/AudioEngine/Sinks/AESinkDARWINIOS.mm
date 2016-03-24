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

#include "cores/AudioEngine/Sinks/AESinkDARWINIOS.h"
#include "cores/AudioEngine/Utils/AEUtil.h"
#include "cores/AudioEngine/Utils/AERingBuffer.h"
#include "cores/AudioEngine/Sinks/osx/CoreAudioHelpers.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "threads/Condition.h"
#include "windowing/WindowingFactory.h"

#include <AudioToolbox/AudioToolbox.h>
#import  <AVFoundation/AVFoundation.h>

#define CA_MAX_CHANNELS 8
static enum AEChannel CAChannelMap[CA_MAX_CHANNELS + 1] = {
  AE_CH_FL , AE_CH_FR , AE_CH_BL , AE_CH_BR , AE_CH_FC , AE_CH_LFE , AE_CH_SL , AE_CH_SR ,
  AE_CH_NULL
};

/***************************************************************************************/
/***************************************************************************************/
#if DO_440HZ_TONE_TEST
static void SineWaveGeneratorInitWithFrequency(SineWaveGenerator *ctx, double frequency, double samplerate)
{
  // Given:
  //   frequency in cycles per second
  //   2*PI radians per sine wave cycle
  //   sample rate in samples per second
  //
  // Then:
  //   cycles     radians     seconds     radians
  //   ------  *  -------  *  -------  =  -------
  //   second      cycle      sample      sample
  ctx->currentPhase = 0.0;
  ctx->phaseIncrement = frequency * 2*M_PI / samplerate;
}

static int16_t SineWaveGeneratorNextSampleInt16(SineWaveGenerator *ctx)
{
  int16_t sample = INT16_MAX * sinf(ctx->currentPhase);

  ctx->currentPhase += ctx->phaseIncrement;
  // Keep the value between 0 and 2*M_PI
  while (ctx->currentPhase > 2*M_PI)
    ctx->currentPhase -= 2*M_PI;

  return sample / 4;
}
static float SineWaveGeneratorNextSampleFloat(SineWaveGenerator *ctx)
{
  float sample = MAXFLOAT * sinf(ctx->currentPhase);
  
  ctx->currentPhase += ctx->phaseIncrement;
  // Keep the value between 0 and 2*M_PI
  while (ctx->currentPhase > 2*M_PI)
    ctx->currentPhase -= 2*M_PI;
  
  return sample / 4;
}
#endif

class IDarwinAudioSink
{
  public:
    IDarwinAudioSink() {};
    virtual ~IDarwinAudioSink() {};

    virtual bool         open(AudioStreamBasicDescription outputFormat, size_t buffer_size) = 0;
    virtual bool         close() = 0;
    virtual bool         activate() = 0;
    virtual bool         deactivate() = 0;
    virtual void         updatedelay(AEDelayStatus& status) = 0;
    virtual double       buffertime() = 0;
    virtual unsigned int bufferframes() = 0;
    virtual unsigned int sampletrate() = 0;
    virtual unsigned int write(uint8_t *data, unsigned int byte_count) = 0;
    virtual void         drain() = 0;
    virtual bool         hdmi() = 0;
};

/***************************************************************************************/
/***************************************************************************************/
class CAVPlayerSink : public IDarwinAudioSink
{
  public:
    CAVPlayerSink();
   ~CAVPlayerSink() override;

    bool         open(AudioStreamBasicDescription outputFormat, size_t buffer_size) override;
    bool         close() override;
    bool         activate() override;
    bool         deactivate() override;
    void         updatedelay(AEDelayStatus& status) override;
    double       buffertime() override;
    unsigned int bufferframes() override;
    unsigned int sampletrate() override;
    unsigned int write(uint8_t *data, unsigned int byte_count) override;
    void         drain() override;
    bool         hdmi() override { return m_hdmi; }

  private:
    bool                m_setup;
    bool                m_activated;
    bool                m_hdmi;
    AudioStreamBasicDescription m_outputFormat;
    AERingBuffer       *m_buffer;

    Float32             m_outputLatency;
    Float32             m_bufferDuration;

    unsigned int        m_sampleRate;
    unsigned int        m_frameSize;
    unsigned int        m_frames;

    std::atomic<bool>   m_started;

    CAESpinSection      m_render_section;
    std::atomic<int64_t>  m_render_timestamp;
    std::atomic<uint32_t> m_render_frames;
};

/***************************************************************************************/
/***************************************************************************************/
class CAAudioUnitSink : public IDarwinAudioSink
{
  public:
    CAAudioUnitSink();
   ~CAAudioUnitSink() override;

    bool         open(AudioStreamBasicDescription outputFormat, size_t buffer_size) override;
    bool         close() override;
    bool         activate() override;
    bool         deactivate() override;
    void         updatedelay(AEDelayStatus& status) override;
    double       buffertime() override;
    unsigned int bufferframes() override { return m_bufferDuration * m_sampleRate; };
    unsigned int sampletrate() override { return m_outputFormat.mSampleRate; };
    unsigned int write(uint8_t *data, unsigned int byte_count) override;
    void         drain() override;
    bool         hdmi() override { return m_hdmi; }

  private:
    void         setCoreAudioBuffersize();
    bool         setCoreAudioInputFormat();
    void         setCoreAudioPreferredSampleRate();
    bool         setupAudio();
    void         checkForHDMI();
    bool         checkSessionProperties();
 
    // callbacks
    static OSStatus renderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                  const AudioTimeStamp *inTimeStamp, UInt32 inOutputBusNumber, UInt32 inNumberFrames,
                  AudioBufferList *ioData);

    bool                m_setup;
    bool                m_activated;
    bool                m_hdmi;
    id                  m_observer;
    AudioUnit           m_audioUnit;
    AudioStreamBasicDescription m_outputFormat;
    AERingBuffer       *m_buffer;

    Float32             m_outputVolume;
    Float32             m_outputLatency;
    Float32             m_bufferDuration;

    unsigned int        m_sampleRate;
    unsigned int        m_frameSize;
    unsigned int        m_frames;

    std::atomic<bool>   m_started;

    CAESpinSection      m_render_section;
    std::atomic<int64_t>  m_render_timestamp;
    std::atomic<uint32_t> m_render_frames;
};

CAAudioUnitSink::CAAudioUnitSink()
: IDarwinAudioSink()
, m_activated(false)
, m_hdmi(false)
, m_buffer(nullptr)
, m_started(false)
, m_render_timestamp(0)
, m_render_frames(0)
{
  // creator might need to know the current setup
  checkSessionProperties();
}

CAAudioUnitSink::~CAAudioUnitSink()
{
  close();
}

bool CAAudioUnitSink::open(AudioStreamBasicDescription outputFormat, size_t buffer_size)
{
  m_setup         = false;
  m_outputFormat  = outputFormat;
  m_outputLatency = 0.0;
  m_bufferDuration= 0.0;
  m_outputVolume  = 1.0;
  m_sampleRate    = (unsigned int)outputFormat.mSampleRate;
  m_frameSize     = outputFormat.mChannelsPerFrame * outputFormat.mBitsPerChannel / 8;

  /* TODO: Reduce the size of this buffer, pre-calculate the size based on how large
           the buffers are that CA calls us with in the renderCallback - perhaps call
           the checkSessionProperties() before running this? */
  m_buffer = new AERingBuffer(buffer_size);

  return setupAudio();
}

bool CAAudioUnitSink::close()
{
  deactivate();
  SAFE_DELETE(m_buffer);
  m_started = false;

  return true;
}

bool CAAudioUnitSink::activate()
{
  if (!m_activated)
  {
    checkForHDMI();
    if (setupAudio())
    {
      AudioOutputUnitStart(m_audioUnit);
      m_activated = true;
    }
  }

  return m_activated;
}

bool CAAudioUnitSink::deactivate()
{
  if (m_activated)
  {
    // disconnect observer 1st, route goes to null on AudioOutputUnitStop
    [[NSNotificationCenter defaultCenter] removeObserver:m_observer];

    AudioUnitReset(m_audioUnit, kAudioUnitScope_Global, 0);

    // this is a delayed call, the OS will block here
    // until the autio unit actually is stopped.
    AudioOutputUnitStop(m_audioUnit);

    // detach the render callback on the unit
    AURenderCallbackStruct callbackStruct = {0};
    AudioUnitSetProperty(m_audioUnit,
      kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
      0, &callbackStruct, sizeof(callbackStruct));

    AudioUnitUninitialize(m_audioUnit);
    AudioComponentInstanceDispose(m_audioUnit), m_audioUnit = nullptr;

    m_setup = false;
    m_activated = false;
  }

  return m_activated;
}

void CAAudioUnitSink::updatedelay(AEDelayStatus &status)
{
  CAESpinLock lock(m_render_section);
  do
  {
    status.delay  = (double)m_buffer->GetReadSize() / m_frameSize;
    status.delay += (double)m_render_frames;
    status.tick   = m_render_timestamp;
  } while(lock.retry());

  status.delay /= m_sampleRate;
  status.delay += m_bufferDuration + m_outputLatency;
}

double CAAudioUnitSink::buffertime()
{
  // buffer size in seconds
  return (double)m_buffer->GetMaxSize() / (double)(m_frameSize * m_sampleRate);
}

CCriticalSection mutex;
XbmcThreads::ConditionVariable condVar;

unsigned int CAAudioUnitSink::write(uint8_t *data, unsigned int frames)
{
  if (m_buffer->GetWriteSize() < frames * m_frameSize)
  { // no space to write - wait for a bit
    CSingleLock lock(mutex);
    unsigned int timeout = 900 * frames / m_sampleRate;
    if (!m_started)
      timeout = 4500;

    // we are using a timer here for beeing sure for timeouts
    // condvar can be woken spuriously as signaled
    XbmcThreads::EndTime timer(timeout);
    condVar.wait(mutex, timeout);
    if (!m_started && timer.IsTimePast())
    {
      CLog::Log(LOGERROR, "%s engine didn't start in %d ms!", __FUNCTION__, timeout);
      return INT_MAX;
    }
  }

  unsigned int write_frames = std::min(frames, m_buffer->GetWriteSize() / m_frameSize);
  if (write_frames)
    m_buffer->Write(data, write_frames * m_frameSize);
  
  return write_frames;
}

void CAAudioUnitSink::drain()
{
  unsigned int bytes = m_buffer->GetReadSize();
  unsigned int totalBytes = bytes;
  int maxNumTimeouts = 3;
  unsigned int timeout = 900 * bytes / (m_sampleRate * m_frameSize);
  while (bytes && maxNumTimeouts > 0)
  {
    CSingleLock lock(mutex);
    XbmcThreads::EndTime timer(timeout);
    condVar.wait(mutex, timeout);

    bytes = m_buffer->GetReadSize();
    // if we timeout and don't
    // consum bytes - decrease maxNumTimeouts
    if (timer.IsTimePast() && bytes == totalBytes)
      maxNumTimeouts--;
    totalBytes = bytes;
  }
}

void CAAudioUnitSink::setCoreAudioBuffersize()
{
  // set the buffer size (in seconds), this affects the number of samples
  // that get rendered every time the audio callback is fired.
  Float32 preferredBufferSize = 512 * m_outputFormat.mChannelsPerFrame / m_outputFormat.mSampleRate;
  CLog::Log(LOGNOTICE, "%s setting buffer duration to %f", __PRETTY_FUNCTION__, preferredBufferSize);

  NSError *audioSessionError = nullptr;
  AVAudioSession *mySession = [AVAudioSession sharedInstance];
  [mySession setPreferredIOBufferDuration: (NSTimeInterval)preferredBufferSize error: &audioSessionError];
  if (audioSessionError != nullptr)
    CLog::Log(LOGWARNING, "%s preferredBufferSize couldn't be set", __PRETTY_FUNCTION__);
}

bool CAAudioUnitSink::setCoreAudioInputFormat()
{
  // Set the output stream format
  UInt32 ioDataSize = sizeof(AudioStreamBasicDescription);
  OSStatus status = AudioUnitSetProperty(m_audioUnit, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input, 0, &m_outputFormat, ioDataSize);
  if (status != noErr)
  {
    CLog::Log(LOGERROR, "%s error setting stream format on audioUnit (error: %d)", __PRETTY_FUNCTION__, (int)status);
    return false;
  }
  return true;
}

void CAAudioUnitSink::setCoreAudioPreferredSampleRate()
{
  Float64 preferredSampleRate = m_outputFormat.mSampleRate;
  CLog::Log(LOGNOTICE, "%s requesting hw samplerate %f", __PRETTY_FUNCTION__, preferredSampleRate);

  NSError *audioSessionError = nil;
  AVAudioSession *mySession = [AVAudioSession sharedInstance];
  [mySession setPreferredSampleRate: preferredSampleRate error: &audioSessionError];
  preferredSampleRate = [mySession sampleRate];
}

bool CAAudioUnitSink::setupAudio()
{
  OSStatus status = noErr;
  if (m_setup && m_audioUnit)
    return true;

  // warning, usingBlock here, this code is queued to run when note fires
  NSOperationQueue *mainQueue = [NSOperationQueue mainQueue];
  m_observer = [[NSNotificationCenter defaultCenter] addObserverForName:AVAudioSessionRouteChangeNotification
    object:[AVAudioSession sharedInstance] queue:mainQueue usingBlock:^(NSNotification *note)
  {
    checkSessionProperties();
  }];

  // Audio Unit Setup
  // Describe a default output unit.
  AudioComponentDescription description = {};
  description.componentType = kAudioUnitType_Output;
  description.componentSubType = kAudioUnitSubType_RemoteIO;
  description.componentManufacturer = kAudioUnitManufacturer_Apple;

  // Get component
  AudioComponent component;
  component = AudioComponentFindNext(NULL, &description);
  status = AudioComponentInstanceNew(component, &m_audioUnit);
  if (status != noErr)
  {
    CLog::Log(LOGERROR, "%s error creating audioUnit (error: %d)", __PRETTY_FUNCTION__, (int)status);
    return false;
  }
  
  setCoreAudioPreferredSampleRate();
 
	// Get the output samplerate for knowing what was setup in reality
  Float64 realisedSampleRate = [[AVAudioSession sharedInstance] sampleRate];
  if (m_outputFormat.mSampleRate != realisedSampleRate)
  {
    CLog::Log(LOGNOTICE, "%s couldn't set requested samplerate %d, coreaudio will resample to %d instead", __PRETTY_FUNCTION__, (int)m_outputFormat.mSampleRate, (int)realisedSampleRate);
    // if we don't ca to resample - but instead let activeae resample -
    // reflect the realised samplerate to the outputformat here
    // well maybe it is handy in the future - as of writing this
    // ca was about 6 times faster then activeae ;)
    //m_outputFormat.mSampleRate = realisedSampleRate;
    //m_sampleRate = realisedSampleRate;
  }

  setCoreAudioBuffersize();
  if (!setCoreAudioInputFormat())
    return false;

  // Attach a render callback on the unit
  AURenderCallbackStruct callbackStruct = {};
  callbackStruct.inputProc = renderCallback;
  callbackStruct.inputProcRefCon = this;
  status = AudioUnitSetProperty(m_audioUnit,
                                kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                                0, &callbackStruct, sizeof(callbackStruct));
  if (status != noErr)
  {
    CLog::Log(LOGERROR, "%s error setting render callback for audioUnit (error: %d)", __PRETTY_FUNCTION__, (int)status);
    return false;
  }

  status = AudioUnitInitialize(m_audioUnit);
	if (status != noErr)
  {
    CLog::Log(LOGERROR, "%s error initializing audioUnit (error: %d)", __PRETTY_FUNCTION__, (int)status);
    return false;
  }

  checkSessionProperties();

  m_setup = true;
  std::string formatString;
  CLog::Log(LOGNOTICE, "%s setup audio format: %s", __PRETTY_FUNCTION__, StreamDescriptionToString(m_outputFormat, formatString));

  return m_setup;
}

void CAAudioUnitSink::checkForHDMI()
{
  AVAudioSession *myAudioSession = [AVAudioSession sharedInstance];
  AVAudioSessionRouteDescription *currentRoute = [myAudioSession currentRoute];
  NSString *output = [[currentRoute.outputs objectAtIndex:0] portType];
  if (output)
  {
    std::string route = [output UTF8String];
    m_hdmi = route.find("HDMI") != std::string::npos;
  }
  else
  {
    m_hdmi = false;
  }
}

bool CAAudioUnitSink::checkSessionProperties()
{
  checkForHDMI();

  AVAudioSession *mySession = [AVAudioSession sharedInstance];
  m_outputVolume   = [mySession outputVolume];
  m_outputLatency  = [mySession outputLatency];
  m_bufferDuration = [mySession IOBufferDuration];

  CLog::Log(LOGNOTICE, "%s m_outputVolume %f", __PRETTY_FUNCTION__, m_outputVolume);
  CLog::Log(LOGNOTICE, "%s m_outputLatency %f", __PRETTY_FUNCTION__, m_outputLatency);
  CLog::Log(LOGNOTICE, "%s m_bufferDuration %f", __PRETTY_FUNCTION__, m_bufferDuration);
  CLog::Log(LOGNOTICE, "%s real sampleRate %f", __PRETTY_FUNCTION__, [mySession sampleRate]);

  return true;
}

inline void LogLevel(unsigned int got, unsigned int wanted)
{
  static unsigned int lastReported = INT_MAX;
  if (got != wanted)
  {
    if (got != lastReported)
    {
      CLog::Log(LOGWARNING, "DARWINIOS: %sflow (%u vs %u bytes)", got > wanted ? "over" : "under", got, wanted);
      lastReported = got;
    }    
  }
  else
    lastReported = INT_MAX; // indicate we were good at least once
}

OSStatus CAAudioUnitSink::renderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
  const AudioTimeStamp *inTimeStamp, UInt32 inOutputBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
  CAAudioUnitSink *sink = (CAAudioUnitSink*)inRefCon;

  sink->m_render_section.enter();
  sink->m_started = true;

  for (unsigned int i = 0; i < ioData->mNumberBuffers; i++)
  {
    unsigned int wanted = ioData->mBuffers[i].mDataByteSize;
    unsigned int bytes = std::min(sink->m_buffer->GetReadSize(), wanted);
    sink->m_buffer->Read((unsigned char*)ioData->mBuffers[i].mData, bytes);
    //LogLevel(bytes, wanted);

    if (bytes == 0)
    {
      // Apple iOS docs say kAudioUnitRenderAction_OutputIsSilence provides a hint to
      // the audio unit that there is no audio to process. and you must also explicitly
      // set the buffers contents pointed at by the ioData parameter to 0.
      memset(ioData->mBuffers[i].mData, 0x00, ioData->mBuffers[i].mDataByteSize);
      *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
    }
    else if (bytes < wanted)
    {
      // zero out what we did not copy over (underflow)
      uint8_t *empty = (uint8_t*)ioData->mBuffers[i].mData + bytes;
      memset(empty, 0x00, wanted - bytes);
    }
  }

  sink->m_render_timestamp = inTimeStamp->mHostTime;
  sink->m_render_frames    = inNumberFrames;
  sink->m_render_section.leave();
  // tell the sink we're good for more data
  condVar.notifyAll();

  return noErr;
}

/***************************************************************************************/
/***************************************************************************************/
static void EnumerateDevices(AEDeviceInfoList &list)
{
  CAEDeviceInfo device;

  device.m_deviceName = "default";
  device.m_displayName = "Default";
  device.m_displayNameExtra = "";
  // TODO screen changing on ios needs to call
  // devices changed once this is available in activae
#if defined(TARGET_DARWIN_TVOS)
  if (1)
#else
  if (g_Windowing.GetCurrentScreen() > 0)
#endif
  {
    device.m_deviceType = AE_DEVTYPE_IEC958; //allow passthrough for tvout
    device.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_AC3);
#if defined(TARGET_DARWIN_TVOS)
    device.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_EAC3);
#endif
    device.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTSHD_CORE);
    device.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTS_2048);
    device.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTS_1024);
    device.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTS_512);
    device.m_dataFormats.push_back(AE_FMT_RAW);
    // ATV cant do below
//    device.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_DTSHD);
//    device.m_streamTypes.push_back(CAEStreamInfo::STREAM_TYPE_TRUEHD);
  }
  else
    device.m_deviceType = AE_DEVTYPE_PCM;

  // add channel info
  CAEChannelInfo channel_info;
  for (UInt32 chan = 0; chan < 2; ++chan)
  {
    if (!device.m_channels.HasChannel(CAChannelMap[chan]))
      device.m_channels += CAChannelMap[chan];
    channel_info += CAChannelMap[chan];
  }

  // there are more supported ( one of those 2 gets resampled
  // by coreaudio anyway) - but for keeping it save ignore
  // the others...
  device.m_sampleRates.push_back(44100);
  device.m_sampleRates.push_back(48000);

  device.m_dataFormats.push_back(AE_FMT_S16LE);
  //device.m_dataFormats.push_back(AE_FMT_S24LE3);
  //device.m_dataFormats.push_back(AE_FMT_S32LE);
  device.m_dataFormats.push_back(AE_FMT_FLOAT);
  device.m_wantsIECPassthrough = true;

  CLog::Log(LOGDEBUG, "EnumerateDevices:Device(%s)" , device.m_deviceName.c_str());

  list.push_back(device);
}

/***************************************************************************************/
/***************************************************************************************/
AEDeviceInfoList CAESinkDARWINIOS::m_devices;

CAESinkDARWINIOS::CAESinkDARWINIOS()
:   m_audioSink(nullptr)
{
}

CAESinkDARWINIOS::~CAESinkDARWINIOS()
{
}

bool CAESinkDARWINIOS::Initialize(AEAudioFormat &format, std::string &device)
{
  bool hdmi = false;
  bool found = false;
  bool forceRaw = false;

  std::string devicelower = device;
  StringUtils::ToLower(devicelower);
  for (size_t i = 0; i < m_devices.size(); i++)
  {
    if (devicelower.find(m_devices[i].m_deviceName) != std::string::npos)
    {
      m_info = m_devices[i];
      found = true;
      break;
    }
  }
  
  if (!found)
    return false;

  m_audioSink = new CAAudioUnitSink;
  hdmi = m_audioSink->hdmi();

  AudioStreamBasicDescription audioFormat = {0};

  if (format.m_dataFormat == AE_FMT_FLOAT)
    audioFormat.mFormatFlags    |= kLinearPCMFormatFlagIsFloat;
  else// this will be selected when AE wants AC3 or DTS or anything other then float
  {
    audioFormat.mFormatFlags    |= kLinearPCMFormatFlagIsSignedInteger;
    if (hdmi && format.m_dataFormat == AE_FMT_RAW)
      forceRaw = true;
    format.m_dataFormat = AE_FMT_S16LE;
  }

  format.m_channelLayout = m_info.m_channels;
  format.m_frameSize = format.m_channelLayout.Count() * (CAEUtil::DataFormatToBits(format.m_dataFormat) >> 3);

  
  audioFormat.mFormatID = kAudioFormatLinearPCM;
  switch(format.m_sampleRate)
  {
    case 11025:
    case 22050:
    case 44100:
    case 88200:
    case 176400:
#if defined(TARGET_DARWIN_TVOS)
      if (hdmi)
        audioFormat.mSampleRate = 48000;
      else
#endif
        audioFormat.mSampleRate = 44100;
      break;
    default:
    case 8000:
    case 12000:
    case 16000:
    case 24000:
    case 32000:
    case 48000:
    case 96000:
    case 192000:
    case 384000:
      audioFormat.mSampleRate = 48000;
      break;
  }
  
  if (forceRaw)//make sure input and output samplerate match for preventing resampling
    audioFormat.mSampleRate = [[AVAudioSession sharedInstance] sampleRate];
  
  audioFormat.mFramesPerPacket = 1;
  audioFormat.mChannelsPerFrame= 2;// ios only supports 2 channels
  audioFormat.mBitsPerChannel  = CAEUtil::DataFormatToBits(format.m_dataFormat);
  audioFormat.mBytesPerFrame   = format.m_frameSize;
  audioFormat.mBytesPerPacket  = audioFormat.mBytesPerFrame * audioFormat.mFramesPerPacket;
  audioFormat.mFormatFlags    |= kLinearPCMFormatFlagIsPacked;
  
#if DO_440HZ_TONE_TEST
  SineWaveGeneratorInitWithFrequency(&m_SineWaveGenerator, 440.0, audioFormat.mSampleRate);
#endif

  size_t buffer_size = 16384;
  m_audioSink->open(audioFormat, buffer_size);

  format.m_frames = m_audioSink->bufferframes();
  // reset to the realised samplerate
  format.m_sampleRate = m_audioSink->sampletrate();
  m_format = format;

  m_audioSink->activate();

  return true;
}

void CAESinkDARWINIOS::Deinitialize()
{
  SAFE_DELETE(m_audioSink);
}

void CAESinkDARWINIOS::GetDelay(AEDelayStatus &status)
{
  if (m_audioSink)
    m_audioSink->updatedelay(status);
  else
    status.SetDelay(0.0);
}

double CAESinkDARWINIOS::GetCacheTotal()
{
  if (m_audioSink)
    return m_audioSink->buffertime();
  return 0.0;
}

unsigned int CAESinkDARWINIOS::AddPackets(uint8_t **data, unsigned int frames, unsigned int offset)
{
  uint8_t *buffer = data[0]+offset*m_format.m_frameSize;
#if DO_440HZ_TONE_TEST
  if (m_format.m_dataFormat == AE_FMT_FLOAT)
  {
    float *samples = (float*)buffer;
    for (unsigned int j = 0; j < frames ; j++)
    {
      float sample = SineWaveGeneratorNextSampleFloat(&m_SineWaveGenerator);
      *samples++ = sample;
      *samples++ = sample;
    }
    
  }
  else
  {
    int16_t *samples = (int16_t*)buffer;
    for (unsigned int j = 0; j < frames ; j++)
    {
      int16_t sample = SineWaveGeneratorNextSampleInt16(&m_SineWaveGenerator);
      *samples++ = sample;
      *samples++ = sample;
    }
  }
#endif
  if (m_audioSink)
    return m_audioSink->write(buffer, frames);
  return 0;
}

void CAESinkDARWINIOS::Drain()
{
  if (m_audioSink)
    m_audioSink->drain();
}

bool CAESinkDARWINIOS::HasVolume()
{
  return false;
}

void CAESinkDARWINIOS::EnumerateDevicesEx(AEDeviceInfoList &list, bool force)
{
  m_devices.clear();
  EnumerateDevices(m_devices);
  list = m_devices;
}

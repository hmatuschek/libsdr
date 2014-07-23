#include "portaudio.hh"

using namespace sdr;


/* ******************************************************************************************* *
 * PortAudio interface
 * ******************************************************************************************* */
void
PortAudio::init() {
  Pa_Initialize();
}

void
PortAudio::terminate() {
  Pa_Terminate();
}

int
PortAudio::numDevices() {
  return Pa_GetDeviceCount();
}

int
PortAudio::defaultInputDevice() {
  return Pa_GetDefaultInputDevice();
}

int
PortAudio::defaultOutputDevice() {
  return Pa_GetDefaultOutputDevice();
}

bool
PortAudio::hasInputStream(int idx) {
  const PaDeviceInfo *info = Pa_GetDeviceInfo(idx);
  return 0 != info->maxInputChannels;
}

bool
PortAudio::hasOutputStream(int idx) {
  const PaDeviceInfo *info = Pa_GetDeviceInfo(idx);
  return 0 != info->maxOutputChannels;
}

std::string
PortAudio::deviceName(int idx) {
  const PaDeviceInfo *info = Pa_GetDeviceInfo(idx);
  return info->name;
}



/* ******************************************************************************************* *
 * PortSink implementation
 * ******************************************************************************************* */
PortSink::PortSink()
  : SinkBase(), _stream(0), _frame_size(0)
{
  // pass...
}

PortSink::~PortSink() {
  if (0 != _stream) {
    Pa_CloseStream(_stream);
  }
}

void
PortSink::config(const Config &src_cfg) {
  // Skip if config is incomplete
  if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.bufferSize()) {
    return;
  }
  // Check type:
  PaSampleFormat fmt;
  size_t nChanels;
  switch (src_cfg.type()) {
  case Config::Type_u8:
  case Config::Type_s8:
    fmt = paInt8;
    nChanels = 1;
    _frame_size = nChanels*1;
    break;
  case Config::Type_cu8:
  case Config::Type_cs8:
    fmt = paInt8;
    nChanels = 2;
    _frame_size = nChanels*1;
    break;
  case Config::Type_u16:
  case Config::Type_s16:
    fmt = paInt16;
    nChanels = 1;
    _frame_size = nChanels*2;
    break;
  case Config::Type_cu16:
  case Config::Type_cs16:
    fmt = paInt16;
    nChanels = 2;
    _frame_size = nChanels*2;
    break;
  case Config::Type_f32:
    fmt = paFloat32;
    nChanels = 1;
    _frame_size = nChanels*4;
    break;
  case Config::Type_cf32:
    fmt = paFloat32;
    nChanels = 2;
    _frame_size = nChanels*4;
    break;
  default:
    ConfigError err;
    err << "Can not configure PortAudio sink: Unsupported format " << src_cfg.type()
        << " must be one of " << Config::Type_u8 << ", " << Config::Type_s8 << ", "
        << Config::Type_cu8 << ", " << Config::Type_cs8 << ", " << Config::Type_u16 << ", "
        << Config::Type_s16 << ", " << Config::Type_cu16 << ", " << Config::Type_cs16 << ", "
        << Config::Type_f32 << " or " << Config::Type_cf32;
    throw err;
  }
  /* Configure PortAudio stream */

  // Close stream if already open
  if (0 != _stream) { Pa_StopStream(_stream); Pa_CloseStream(_stream); }

  PaError err;
  // One output chanel (mono), format == float, samplerate and buffer size as provided by src_cfg
  if ((err = Pa_OpenDefaultStream(&_stream, 0, nChanels, fmt,
                                  (unsigned int)src_cfg.sampleRate(), src_cfg.bufferSize(), 0, 0)))
  {
    ConfigError ex;
    ex << "Can not configure PortAudio sink: "
       << ((const char *)Pa_GetErrorText(err));
    throw ex;
  }

  LogMessage msg(LOG_DEBUG);
  msg << "Configure PortAudio sink: " << std::endl
      << " sample rate " << (int)src_cfg.sampleRate()  << std::endl
      << " buffer size " << src_cfg.bufferSize() << std::endl
      << " format " << src_cfg.type() << std::endl
      << " # chanels " << nChanels;
  Logger::get().log(msg);

  // start
  Pa_StartStream(_stream);
}


void
PortSink::handleBuffer(const RawBuffer &buffer, bool allow_overwrite) {
  // Bug, check if data was send properly
  Pa_WriteStream(_stream, buffer.data(), buffer.bytesLen()/_frame_size);
}

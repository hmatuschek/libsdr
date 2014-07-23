#ifndef __SDR_PORTAUDIO_HH__
#define __SDR_PORTAUDIO_HH__

#include <portaudio.h>

#include "buffer.hh"
#include "node.hh"
#include "config.hh"
#include "logger.hh"

namespace sdr {

/** "Namespace" to collect all static, PortAudio related functions. */
class PortAudio {
public:
  /** Initializes the PortAudio system, must be called first. */
  static void init();
  /** Shutdown. */
  static void terminate();

  /** Returns the number of devices available. */
  static int numDevices();
  /** Returns the index of the default input device. */
  static int defaultInputDevice();
  /** Returns the index of the default output device. */
  static int defaultOutputDevice();
  /** Returns @c true of the given device provides an input chanel. */
  static bool hasInputStream(int idx);
  /** Returns @c true of the given device provides an output chanel. */
  static bool hasOutputStream(int idx);
  /** Returns the device name. */
  static std::string deviceName(int idx);
};


/** PortAudio playback node. */
class PortSink: public SinkBase
{
public:
  /** Constructor. */
  PortSink();
  /** Destructor. */
  virtual ~PortSink();

  /** Configures the PortAudio output. */
  virtual void config(const Config &src_cfg);
  /** Playback. */
  virtual void handleBuffer(const RawBuffer &buffer, bool allow_overwrite);

protected:
  /** The PortAudio stream. */
  PaStream *_stream;
  /** The frame-size. */
  size_t _frame_size;
};


/** PortAudio input stream as a @c Source. */
template <class Scalar>
class PortSource: public Source
{
public:
  /** Constructor. */
  PortSource(double sampleRate, size_t bufferSize, int dev=-1):
    Source(), _streamIsOpen(false), _stream(0), _sampleRate(sampleRate), _is_real(true)
  {
    // Allocate buffer
    _buffer = Buffer<Scalar>(bufferSize);
    _initializeStream(dev);
  }

  /** Destructor. */
  virtual ~PortSource() { if (0 != _stream) { Pa_CloseStream(_stream); } }

  /** Reads (blocking) the next buffer from the PortAudio stream. This function can be
   * connected to the idle event of the @c Queue. */
  void next() {
    /// @todo Signal loss of samples in debug mode.
    /// @bug Drop data if output buffer is in use.
    Pa_ReadStream(_stream, _buffer.ptr(), _buffer.size());
    this->send(_buffer);
  }

  /** Returns the currently selected input device. */
  int deviceIndex() const { return _deviceIndex; }

  /** Selects the input device, throws a @c ConfigError exception if the device can not
   * be setup as an input device. Do not call this function, while the @c Queue is running. */
  void setDeviceIndex(int idx=-1) {
    _initializeStream(idx);
  }

  /** Checks if the given sample rate is supported by the device. */
  bool hasSampleRate(double sampleRate) {
    PaStreamParameters params;
    params.device = _deviceIndex;
    params.channelCount = _is_real ? 1 : 2;
    params.sampleFormat = _fmt;
    params.hostApiSpecificStreamInfo = 0;
    return paFormatIsSupported == Pa_IsFormatSupported(&params, 0, sampleRate);
  }

  /** Resets the sample rate of the input device. Do not call this function, while the
   * @c Queue is running. */
  void setSampleRate(double sampleRate) {
    _sampleRate = sampleRate;
    _initializeStream(_deviceIndex);
  }

protected:
  /** Device setup. */
  void _initializeStream(int idx=-1) {
    // Stop and close stream if running
    if (_streamIsOpen) {
      Pa_StopStream(_stream); Pa_CloseStream(_stream); _streamIsOpen = false;
    }

    // Determine input stream format by scalar type
    switch (Config::typeId<Scalar>()) {
    case Config::Type_f32:
      _fmt = paFloat32;
      _is_real = true;
      break;
    case Config::Type_u16:
    case Config::Type_s16:
      _fmt = paInt16;
      _is_real = true;
      break;
    case Config::Type_cf32:
      _fmt = paFloat32;
      _is_real = false;
      break;
    case Config::Type_cu16:
    case Config::Type_cs16:
      _fmt = paInt16;
      _is_real = false;
      break;
    default:
      ConfigError err;
      err << "Can not configure PortAudio sink: Unsupported format " << Config::typeId<Scalar>()
          << " must be one of " << Config::Type_u16 << ", " << Config::Type_s16
          << ", " << Config::Type_f32 << ", " << Config::Type_cu16 << ", " << Config::Type_cs16
          << " or " << Config::Type_cf32 << ".";
      throw err;
    }

    // Assemble config:
    int numCh = _is_real ? 1 : 2;
    _deviceIndex = idx < 0 ? Pa_GetDefaultInputDevice() : idx;
    PaStreamParameters params;
    params.device = _deviceIndex;
    params.channelCount = numCh;
    params.sampleFormat = _fmt;
    params.suggestedLatency = Pa_GetDeviceInfo(_deviceIndex)->defaultHighInputLatency;
    params.hostApiSpecificStreamInfo = 0;

    // open stream
    PaError err = Pa_OpenStream(&_stream, &params, 0, _sampleRate, _buffer.size(), 0, 0, 0);

    // Check for errors
    if (0 > err) {
      ConfigError err;
      err << "Can not open PortAudio input stream!"; throw err;
    }
    // Mark stream as open
    _streamIsOpen = true;
    // Start stream
    Pa_StartStream(_stream);

    LogMessage msg(LOG_DEBUG);
    msg << "Configure PortAudio source: " << std::endl
        << " sample rate " << _sampleRate  << std::endl
        << " buffer size " << _buffer.size() << std::endl
        << " format " << Config::typeId<Scalar>() << std::endl
        << " num chanels " << numCh;
    Logger::get().log(msg);

    // Set config
    this->setConfig(Config(Config::typeId<Scalar>(), _sampleRate, _buffer.size(), 1));
  }


protected:
  /** If true, the PortAudio stream, @c _stream is open. */
  bool _streamIsOpen;
  /** The PortAudio input stream. */
  PaStream *_stream;
  /** The current format of the PortAudio stream. */
  PaSampleFormat _fmt;
  /** The current sample rate. */
  double _sampleRate;
  /** The current input device index. */
  int _deviceIndex;
  /** If @c true, the input stream is real (1 chanel), otherwise complex (2 chanels). */
  bool _is_real;
  /** The output buffer. */
  Buffer<Scalar> _buffer;
};

}

#endif // __SDR_PORTAUDIO_HH__

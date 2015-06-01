#ifndef __SDR_WAVFILE_HH__
#define __SDR_WAVFILE_HH__

#include "node.hh"
#include <fstream>

namespace sdr {

/** Stores the received buffers into a WAV file.
 * @ingroup sinks */
template <class Scalar>
class WavSink: public Sink<Scalar>
{
public:
  /** Constructor, @c filename specifies the file name, the WAV data is stored into.
   * @throws ConfigError If the specified file can not be opened for output. */
  WavSink(const std::string &filename)
    : Sink<Scalar>(), _file(filename.c_str(), std::ios_base::out|std::ios_base::binary),
      _frameCount(0), _sampleRate(0)
  {
    if (!_file.is_open()) {
      ConfigError err;
      err << "Can not open wav file for output: " << filename;
      throw err;
    }
    // Fill first 36+8 bytes with 0, the headers are written once close() gets called.
    for (size_t i=0; i<44; i++) { _file.write("\x00", 1); }

    // check format
    switch (Config::typeId<Scalar>()) {
    case Config::Type_u8:
    case Config::Type_s8:
      _bitsPerSample = 8;
      _numChanels = 1;
      break;
    case Config::Type_cu8:
    case Config::Type_cs8:
      _bitsPerSample = 8;
      _numChanels = 2;
      break;
    case Config::Type_u16:
    case Config::Type_s16:
      _bitsPerSample = 16;
      _numChanels = 1;
      break;
    case Config::Type_cu16:
    case Config::Type_cs16:
      _bitsPerSample = 16;
      _numChanels = 2;
      break;
    default:
      ConfigError err;
      err << "WAV format only allows (real) integer typed data.";
      throw err;
    }
  }

  /** Destructor, closes the file if not done yet. */
  virtual ~WavSink() {
    if (_file.is_open()) {
      this->close();
    }
  }

  /** Configures the sink. */
  virtual void config(const Config &src_cfg) {
    // Requires type, samplerate
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate()) { return; }
    // Check if type matches
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure WavSink: Invalid buffer type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }
    // Store sample rate
    _sampleRate = src_cfg.sampleRate();
  }

  /** Completes the WAV header and closes the file. */
  void close() {
    if (! _file.is_open()) { return; }
    uint32_t val4;
    uint16_t val2;

    _file.seekp(0);
    _file.write("RIFF", 4);
    val4 = (uint32_t)(36u+2u*_frameCount); _file.write((char *)&val4, 4);
    _file.write("WAVE", 4);

    _file.write("fmt ", 4);
    val4 = 16; _file.write((char *)&val4, 4);  // sub header size = 16
    val2 = 1;  _file.write((char *)&val2, 2);  // format PCM = 1
    _file.write((char *)&_numChanels, 2);  // num chanels = 1
    _file.write((char *)&_sampleRate, 4);
    val4 = _numChanels*_sampleRate*(_bitsPerSample/8);
    _file.write((char *)&val4, 4); // byte rate
    val2 = _numChanels*(_bitsPerSample/8);
    _file.write((char *)&val2, 2);  // block align
    _file.write((char *)&_bitsPerSample, 2);  // bits per sample

    _file.write("data", 4);
    val4 = _numChanels*_frameCount*(_bitsPerSample/8); _file.write((char *)&val4, 4);
    _file.close();
  }

  /** Writes some data into the WAV file. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    if (! _file.is_open()) { return; }
    _file.write(buffer.data(), buffer.size()*sizeof(Scalar));
    _frameCount += buffer.size();
  }


protected:
  /** The file output stream. */
  std::fstream _file;
  /** The number of bits per sample (depends on the template type). */
  uint16_t _bitsPerSample;
  /** The total number of frame counts. */
  uint32_t _frameCount;
  /** The sample rate. */
  uint32_t _sampleRate;
  /** The number of chanels. */
  uint16_t _numChanels;
};



/** A simple imput source that reads from a wav file. Some data is read from the file on every call
 * to  @c next until the end of file is reached.
 * @ingroup sources */
class WavSource: public Source
{
public:
  /** Constructor, @c buffer_size specified the output buffer size. */
  WavSource(size_t buffer_size=1024);
  /** Constructor with file name, @c buffer_size specified the output buffer size. */
  WavSource(const std::string &filename, size_t buffer_size=1024);
  /** Destructor. */
  virtual ~WavSource();

  /** Returns @c true if the file is open. */
  bool isOpen() const;
  /** Open a new file. */
  void open(const std::string &filename);
  /** Close the current file. */
  void close();

  /** Returns true, if the input is real (stereo files are handled as I/Q signals). */
  bool isReal() const;

  /** Read the next data. */
  void next();

protected:
  /** The input file stream. */
  std::fstream _file;
  /** The output buffer. */
  RawBuffer    _buffer;
  /** The current buffer size. */
  size_t _buffer_size;

  /** The number of available frames. */
  size_t _frame_count;
  /** The type of the data in the WAV file. */
  Config::Type _type;
  /** The sample rate. */
  double _sample_rate;
  /** The number of frames left to be read. */
  size_t _frames_left;
};

}

#endif // __SDR_WAVFILE_HH__

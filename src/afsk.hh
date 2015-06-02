#ifndef __SDR_AFSK_HH__
#define __SDR_AFSK_HH__

#include "node.hh"
#include "traits.hh"
#include "logger.hh"


namespace sdr {

/** Implements the basic FSK/AFSK symbol detection.
 * @ingroup demods */
class FSKDetector: public Sink<int16_t>, public Source
{
public:
  FSKDetector(float baud, float Fmark, float Fspace);

  void config(const Config &src_cfg);
  void process(const Buffer<int16_t> &buffer, bool allow_overwrite);

protected:
  /** Updates the mark/space FIR filter and returns the sampled symbol. */
  uint8_t _process(int16_t sample);

protected:
  float _baud;
  size_t  _corrLen;
  /** The current FIR filter LUT index. */
  size_t _lutIdx;
  float _Fmark;
  float _Fspace;
  /** Mark frequency FIR filter LUT. */
  Buffer< std::complex<float> > _markLUT;
  /** Space frequency FIR filter LUT. */
  Buffer< std::complex<float> > _spaceLUT;
  /** FIR filter buffer. */
  Buffer< std::complex<float> > _markHist;
  /** FIR filter buffer. */
  Buffer< std::complex<float> > _spaceHist;
  /** Output buffer. */
  Buffer<int8_t> _buffer;
};


template <class Scalar>
class ASKDetector: public Sink<Scalar>, public Source
{
public:
  ASKDetector()
    : Sink<Scalar>(), Source()
  {
    // pass...
  }

  void config(const Config &src_cfg) {
    // Check if config is complete
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate()) { return; }

    // Check if buffer type matches
    if (Config::typeId<int16_t>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure ASKDetector: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId<int16_t>();
      throw err;
    }

    // Allocate output buffer
    _buffer = Buffer<uint8_t>(src_cfg.bufferSize());

    LogMessage msg(LOG_DEBUG);
    msg << "Config ASKDetector node: " << std::endl
        << " detection threshold: " << 0 << std::endl
        << " sample/symbol rate: " << src_cfg.sampleRate() << " Hz";
    Logger::get().log(msg);

    // Forward config.
    this->setConfig(Config(Traits<uint8_t>::scalarId, src_cfg.sampleRate(), _buffer.size(), 1));
  }

  void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    for (size_t i=0; i<buffer.size(); i++) {
      _buffer[i] = (buffer[i]>0);
    }
    this->send(_buffer.head(buffer.size()), false);
  }

protected:
  Buffer<uint8_t> _buffer;
};


/** Decodes a bitstream with the desired baud rate. */
class BitStream: public Sink<uint8_t>, public Source
{
public:
  /** Possible bit decoding modes. */
  typedef enum {
    NORMAL,      ///< Normal mode (i.e. mark -> 1, space -> 0).
    TRANSITION   ///< Transition mode (i.e. transition -> 0, no transition -> 1).
  } Mode;

public:
  BitStream(float baud, Mode mode = TRANSITION);

  void config(const Config &src_cfg);
  void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

protected:
  float _baud;
  Mode  _mode;
  size_t _corrLen;

  Buffer<int8_t> _symbols;
  size_t _symIdx;

  int32_t _symSum;
  int32_t _lastSymSum;

  float _phase;
  float _omega;
  float _omegaMin;
  float _omegaMax;
  float _pllGain;

  uint8_t _lastBits;

  Buffer<uint8_t> _buffer;
};


class BitDump : public Sink<uint8_t>
{
public:
  void config(const Config &src_cfg) {}
  void process(const Buffer<uint8_t> &buffer, bool allow_overwrite) {
    for (size_t i=0; i<buffer.size(); i++) {
      std::cerr << int(buffer[i]) << " ";
    }
    std::cerr << std::endl;
  }
};

}
#endif // __SDR_AFSK_HH__

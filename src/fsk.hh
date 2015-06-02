#ifndef __SDR_FSK_HH__
#define __SDR_FSK_HH__

#include "node.hh"
#include "traits.hh"
#include "logger.hh"


namespace sdr {


/** Implements the basic FSK/AFSK symbol detection.
 * This node contains two FIR filters for the detection of the mark and space frequencies. The node
 * returns a sequence of symbols (i.e. sub-bits) which need to be processed to obtain a sequenc of
 * transmitted bits (i.e. by the @c BitStream node).
 *
 * @ingroup demods */
class FSKDetector: public Sink<int16_t>, public Source
{
public:
  /** Constructor.
   * @param baud Specifies the baud-rate of the signal.
   * @param Fmark Specifies the mark frequency in Hz.
   * @param Fspace Specifies the space frequency in Hz. */
  FSKDetector(float baud, float Fmark, float Fspace);

  void config(const Config &src_cfg);
  void process(const Buffer<int16_t> &buffer, bool allow_overwrite);

protected:
  /** Updates the mark/space FIR filter and returns the sampled symbol. */
  uint8_t _process(int16_t sample);

protected:
  /** Baudrate of the transmission. Needed to compute the filter length of the FIR mark/space
   * filters. */
  float _baud;
  /** The filter lenght. */
  size_t  _corrLen;
  /** The current FIR filter LUT index. */
  size_t _lutIdx;
  /** Mark "tone" frequency. */
  float _Fmark;
  /** Space "tone" frequency. */
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


/** Rather trivial node to detect mark/space symbols by the amplitude.
 * For low baud rates (i.e. <= 1200 baud) a FSK signal can be "demodulated" using a
 * simple FM demodulator. The result will be a series of decaying exponentials. Hence the
 * mark/space symbols can be determined by the means of the input amplitude (positive/negative).
 *
 * This node implements such a simple symbol detection by the means of the amplitude. The node
 * returns a sequence of symbols (sub-bits) that need to be processed to obtain the sequence of
 * received bits (i.e. @c BitStream).
 *
 * @ingroup demods */
template <class Scalar>
class ASKDetector: public Sink<Scalar>, public Source
{
public:
  /** Constructor. */
  ASKDetector(bool invert=false)
    : Sink<Scalar>(), Source(), _invert(invert)
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
        << " threshold:   " << 0 << std::endl
        << " invert:      " << ( _invert ? "yes" : "no" ) << std::endl
        << " symbol rate: " << src_cfg.sampleRate() << " Hz";
    Logger::get().log(msg);

    // Forward config.
    this->setConfig(Config(Traits<uint8_t>::scalarId, src_cfg.sampleRate(), _buffer.size(), 1));
  }

  void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    for (size_t i=0; i<buffer.size(); i++) {
      _buffer[i] = ((buffer[i]>0)^_invert);
    }
    this->send(_buffer.head(buffer.size()), false);
  }

protected:
  /** If true the symbol logic is inverted. */
  bool _invert;
  /** The output buffer. */
  Buffer<uint8_t> _buffer;
};


/** Decodes a bitstream with the desired baud rate.
 * This node implements a simple PLL to syncronize the bit sampling with the transitions
 * of the input symbol sequence. */
class BitStream: public Sink<uint8_t>, public Source
{
public:
  /** Possible bit decoding modes. */
  typedef enum {
    NORMAL,      ///< Normal mode (i.e. mark -> 1, space -> 0).
    TRANSITION   ///< Transition mode (i.e. transition -> 0, no transition -> 1).
  } Mode;

public:
  /** Constructor.
   * @param baud Specifies the baud-rate of the input signal.
   * @param mode Specifies the bit detection mode. */
  BitStream(float baud, Mode mode = TRANSITION);

  void config(const Config &src_cfg);
  void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

protected:
  /** The baud rate. */
  float _baud;
  /** The bit detection mode. */
  Mode  _mode;
  /** The approximative bit length in samples. */
  size_t _corrLen;
  /** Last received symbols. */
  Buffer<int8_t> _symbols;
  /** Insertion index for the next symbol. */
  size_t _symIdx;
  /** Sum over all received symbol (encoded as -1 & 1). */
  int32_t _symSum;
  /** Last sum over all received symbol (encoded as -1 & 1). */
  int32_t _lastSymSum;
  /** Current bit "phase". */
  float _phase;
  /** Phase velocity. */
  float _omega;
  /** Minimum phase velocity. */
  float _omegaMin;
  /** Maximum phase velocity. */
  float _omegaMax;
  /** PLL gain. */
  float _pllGain;
  /** The last decoded bits (needed for transition mode). */
  uint8_t _lastBits;
  /** Output buffer. */
  Buffer<uint8_t> _buffer;
};


/** Trivial node to dump a bit-stream to a std::ostream.
 * @ingroup sinks */
class BitDump : public Sink<uint8_t>
{
public:
  /** Constructor.
   * @param stream Specifies the output stream. */
  BitDump(std::ostream &stream);

  void config(const Config &src_cfg);
  void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

protected:
  /** The output stream. */
  std::ostream &_stream;
};

}
#endif // __SDR_FSK_HH__

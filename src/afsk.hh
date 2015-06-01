#ifndef __SDR_AFSK_HH__
#define __SDR_AFSK_HH__

#include "node.hh"

namespace sdr {

/** A simple (Audio) Frequency Shift Keying (AFSK) demodulator.
 * This node consists of two convolution peak-filters at the mark and space frequencies, a
 * interpolating sub-sampler to match the baud-rate exactly and a PLL to lock to the symbol
 * transitions. The node will decode the (A)FSK signal and will send a bit-stream (uint8_t).
 * @ingroup demodulator */
class AFSK: public Sink<int16_t>, public Source
{
public:
  /** Possible bit decoding modes. */
  typedef enum {
    NORMAL,      ///< Normal mode (i.e. mark -> 1, space -> 0).
    TRANSITION   ///< Transition mode (i.e. transition -> 0, no transition -> 1).
  } Mode;

public:
  /** Constructs a AFSK node with the specified @c baud rate and @c Fmark, @c Fspace frequencies.
   * The default valuse corresponds to those used for 1200 baud packet radio. */
  AFSK(double baud=1200.0, double Fmark=1200.0, double Fspace=2200.0,
       Mode mode=TRANSITION);
  /** Destructor. */
  virtual ~AFSK();

  /** Configures the node. */
  virtual void config(const Config &src_cfg);
  /** Processes the given buffer. */
  virtual void process(const Buffer<int16_t> &buffer, bool allow_overwrite);

protected:
  /** Performs the convolution filtering of the mark & space frequencies. */
  inline double _getSymbol() {
    std::complex<float> markSum(0), spaceSum(0);
    for (size_t i=0; i<_corrLen; i++) {
      markSum += _markHist[i];
      spaceSum += _spaceHist[i];
    }
    double f = markSum.real()*markSum.real() +
        markSum.imag()*markSum.imag() -
        spaceSum.real()*spaceSum.real() -
        spaceSum.imag()*spaceSum.imag();
    return f;
  }


protected:
  /** The sample rate of the input signal. */
  float _sampleRate;
  /** A multiple of the baud rate. */
  float _symbolRate;
  /** The baud rate. */
  float _baud;
  /** Mark "tone" frequency. */
  float _Fmark;
  /** Space "tone" frequency. */
  float _Fspace;
  /** Bit encoding mode. */
  Mode _mode;
  /** Correlation length, the number of "symbols" per bit. */
  uint32_t _corrLen;
  /** The current FIR filter LUT index. */
  uint32_t _lutIdx;
  /** Mark frequency FIR filter LUT. */
  Buffer< std::complex<float> > _markLUT;
  /** Space frequency FIR filter LUT. */
  Buffer< std::complex<float> > _spaceLUT;

  /** FIR filter buffer. */
  Buffer< std::complex<float> > _markHist;
  /** FIR filter buffer. */
  Buffer< std::complex<float> > _spaceHist;

  /** Symbol subsampling counter. */
  float _mu;
  /** Symbol subsampling. */
  float _muIncr;
  /** Delay line for the 8-pole interpolation filter. */
  Buffer< float > _dl;
  /** Delay line index. */
  size_t _dl_idx;

  Buffer<int16_t> _symbols;
  size_t  _symbolIdx;
  int32_t _symSum;
  int32_t _lastSymSum;

  /** Last received bits. */
  uint32_t _lastBits;
  /** Current PLL phase. */
  float _phase;
  /** PLL phase speed. */
  float _omega;
  /** Maximum PLL phase speed. */
  float _omegaMin;
  /** Minimum PLL phase speed. */
  float _omegaMax;
  /** PLL gain. */
  float _gainOmega;

  /** Output buffer. */
  Buffer<uint8_t> _buffer;
};


}
#endif // __SDR_AFSK_HH__

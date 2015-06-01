#ifndef __SDR_PSK31_HH__
#define __SDR_PSK31_HH__

#include "node.hh"
#include "traits.hh"
#include "interpolate.hh"


namespace sdr {

/** A simple BPSK31 "demodulator". This node consumes a complex input stream with a sample rate of
 * at least 2000Hz and produces a bitstream with 31.25 Hz "sample rate". Use the @c Varicode node
 * to decode this bitstream to ASCII chars. The BPSK31 signal should be centered around 0Hz. This
 * node uses a simple PLL to adjust for small detunings.
 * @ingroup demod */
template <class Scalar>
class BPSK31: public Sink< std::complex<Scalar> >, public Source
{
public:
  /** Constructs a new BPSK31 demodulator.
   *
   * This node first subsamples the input signal to a multiple of 31.25 Hz (by default to
   * 2000Hz = 64*31.25) using an interpolating sub-sampler. Therefore, the input signal must be
   * filtered sufficiently well to avoid artifacts of the interpolating sub-sampler.
   * Then, the phase-constellation of the signal is determined as either -pi or pi while the
   * frequency of the carrier is tracked using a simple PLL. Finally, the BPSK31 bit stream is
   * decoded by detecting a phase-change (0) or not (1).
   *
   * @note This node uses floating point arithmetic, hence it should not be used on streams with
   *       a high sample rate! Which is not neccessary as it only decodes a BPSK signal with approx.
   *       31 baud.
   *
   * @param dF Specfies the (relative anglular) frequency range of the PLL to adjust for small
   *        deviations of the BPSK31 signal from 0Hz. */
  BPSK31(double dF=0.1)
    : Sink< std::complex<Scalar> >(), Source(),
      _P(0), _F(0), _Fmin(-dF), _Fmax(dF)
  {
    // Assemble carrier PLL gains:
    double damping = std::sqrt(2)/2;
    double bw = M_PI/100;
    double tmp = 1. + 2*damping*bw + bw*bw;
    _alpha = 4*damping*bw/tmp;
    _beta  = 4*bw*bw/tmp;

    // Init delay line for the interpolating sub-sampler
    _dl = Buffer< std::complex<float> >(2*8); _dl_idx = 0;
    for (size_t i=0; i<16; i++) { _dl[i] = 0; }
    _mu = 0.25; _gain_mu = 0.01;

    // constant phase shift of the signal
    _theta = 0;

    // Phase detection:
    _omega = _min_omega = _max_omega = 0;
    _omega_rel = 0.001; _gain_omega = 0.001;
    _p_0T = _p_1T = _p_2T = 0;
    _c_0T = _c_1T = _c_2T = 0;

    // Super-sample (64 phase samples per symbol)
    _superSample = 64;
  }

  /** Destructor. */
  virtual ~BPSK31() {
    // unreference buffers
    _dl.unref();
    _hist.unref();
    _buffer.unref();
  }

  virtual void config(const Config &src_cfg)
  {
    // Requires type, sample rate & buffer size
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }

    // Check buffer type
    if (Config::typeId< std::complex<Scalar> >() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure BPSK31: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId< std::complex<Scalar> >();
      throw err;
    }

    // Check input sample rate
    double Fs = src_cfg.sampleRate();
    if (2000 > Fs) {
      ConfigError err;
      err << "Can not configure BPSK31: Input sample rate too low! The BPSK31 node requires at "
          << "least a sample rate of 2000Hz, got " << Fs << "Hz";
      throw err;
    }

    // Compute samples per symbol
    _omega = Fs/(_superSample*31.25);
    // Obtain limits for the sub-sample rate
    _min_omega = _omega*(1.0 - _omega_rel);
    _max_omega = _omega*(1.0 + _omega_rel);

    // Decoding history, contains up to 64 phases (if in sync)
    _hist = Buffer<float>(_superSample);
    _hist_idx = 0;
    _last_constellation = 1;

    // Output buffer
    size_t bsize = 1 + int(Fs/31.25);
    _buffer = Buffer<uint8_t>(bsize);

    // This node sends a bit-stream with 31.25 baud.
    this->setConfig(Config(Traits<uint8_t>::scalarId, 31.25, src_cfg.bufferSize(), 1));
  }


  virtual void process(const Buffer< std::complex<Scalar> > &buffer, bool allow_overwrite) {
    size_t i=0, o=0;
    while (i<buffer.size()) {
      // First, fill sampler...
      while ( (_mu > 1) && (i<buffer.size()) ) {
        _updateSampler(buffer[i]); i++;
      }
      // Then, ...
      if (i<buffer.size()) {
        // Subsample
        std::complex<float> sample = interpolate(_dl.sub(_dl_idx, 8), _mu);
        // Update error tracking
        _errorTracking(sample);
        // Update carrier PLL
        _updatePLL(sample);
        // done. real(sample) constains phase of symbol

        // Now fill history with phase constellation and try to decode a bit
        _hist[_hist_idx] = std::real(sample);
        // Check if there is a constellation transition at the current time
        if ((_hist_idx>1) && (_hasTransition())) {
          if (_hist_idx<(_superSample/2)) {
            // If less than superSample values in hist -> drop
            _hist_idx = 0;
          } else {
            // Otherwise decode
            int cconst = _currentContellation();
            _buffer[o++] = (_last_constellation == cconst);
            _last_constellation = cconst;
            _hist_idx = 0;
          }
        } else if (_hist_idx == (_superSample-1)) {
          // If the symbol is complete:
          int cconst = _currentContellation();
          _buffer[o++] = (_last_constellation == cconst);
          _last_constellation = cconst;
          _hist_idx = 0;
        } else {
          // Next sample
          _hist_idx++;
        }
      }
    }
    // If at least 1 bit was decoded -> send result
    if (o>0) { this->send(_buffer.head(o)); }
  }


protected:
  /** Returns @c true if there is a phase transition at the current sample. */
  inline bool _hasTransition() const {
    return ((_hist[_hist_idx-1]>=0) && (_hist[_hist_idx]<=0)) ||
        ((_hist[_hist_idx-1]<=0) && (_hist[_hist_idx]>=0));
  }

  /** Returns the current constellation. */
  inline int _currentContellation() const {
    float value = 0;
    for (size_t i=0; i<=_hist_idx; i++) { value += _hist[i]; }
    return (value > 0) ? 1 : -1;
  }

  /** Computes the phase error. */
  inline float _phaseError(const std::complex<float> &value) const {
    float r2 = value.real()*value.real();
    float i2 = value.imag()*value.imag();
    float nrm2 = r2+i2;
    if (0 == nrm2) { return 0; }
    return -value.real()*value.imag()/nrm2;
  }

  /** Updates the PLL (@c _F and @c _P). */
  inline void _updatePLL(const std::complex<float> &sample) {
    float phi = _phaseError(sample);
    _F += _beta*phi;
    _P += _F + _alpha*phi;
    while (_P>( 2*M_PI)) { _P -= 2*M_PI; }
    while (_P<(-2*M_PI)) { _P += 2*M_PI; }
    _F = std::min(_Fmax, std::max(_Fmin, _F));
    //std::cerr << "Update PLL: P=" << _P << "; F=" << _F << ", err= " << phi << std::endl;
  }

  /** Updates the sub-sampler. */
  inline void _updateSampler(const std::complex<Scalar> &value) {
    // decrease fractional sub-sample counter
    _mu-=1;
    // Update phase
    _P += _F;
    // Limit phase
    while (_P>( 2*M_PI)) { _P -= 2*M_PI; }
    while (_P<(-2*M_PI)) { _P += 2*M_PI; }
    // Calc down-conversion factor (consider look-up table)
    std::complex<float> fac = std::exp(std::complex<float>(0, _P+_theta));
    // Down-convert singal
    std::complex<float> sample = fac * std::complex<float>(value.real(), value.imag());
    // store sample into delay line
    _dl[_dl_idx] = sample;
    _dl[_dl_idx+8] = sample;
    _dl_idx = (_dl_idx + 1) % 8;
  }

  /** Updates the PPL state (@c _mu  and @c _omega). */
  inline void _errorTracking(const std::complex<float> &sample) {
    // Update last 2 constellation and phases
    _p_2T = _p_1T; _p_1T = _p_0T; _p_0T = sample;
    _c_2T = _c_1T; _c_1T = _c_0T; _c_0T = (sample.real() > 0) ? -1 : 1;
    // Compute difference between phase and constellation
    std::complex<float> x = (_c_0T - _c_2T) * std::conj(_p_1T);
    std::complex<float> y = (_p_0T - _p_2T) * std::conj(_c_1T);
    // Get phase error
    float err = std::real(y-x);
    if (err >  1.0) { err = 1.0; }
    if (err < -1.0) { err = -1.0; }
    // Update symbol rate (approx 31.25*64)
    _omega += _gain_omega * err;
    // Limit omega on _omega_mid +/- _omega*_omega_rel
    _omega = std::max(_min_omega, std::min(_max_omega, _omega));
    // Update mu as +_omega (samples per symbol) and a small correction term
    _mu += _omega + _gain_mu * err;
  }


protected:
  /** Holds the number of phase constellations per bit. */
  size_t _superSample;
  /** Phase of the carrier PLL. */
  float _P;
  /** Frequency of the carrier PLL. */
  float _F;
  /** Lower frequency limit of the carrier PLL. */
  float _Fmin;
  /** Upper frequency limit of the carrier PLL. */
  float _Fmax;
  /** Gain factor of the carrier PLL. */
  float _alpha;
  /** Gain factor of the carrier PLL. */
  float _beta;
  /** The delay line for the interpolating sub-sampler. */
  Buffer< std::complex<float> > _dl;
  /** The current index of the delay line. */
  size_t _dl_idx;
  /** Holds the fractional sub-sampling counter. */
  float _mu;
  /** Gain factor of the sub-sampler. */
  float _gain_mu;
  /** Constant phase shift between real axis and first constellation. (Currently unused). */
  float _theta;
  /** Current sub-sample rate. */
  float _omega;
  /** Relative error of the subsample rate. */
  float _omega_rel;
  /** Minimum of the sub-sample rate. */
  float _min_omega;
  /** Maximum of the sub-sample rate. */
  float _max_omega;
  /** Gain of the sub-sample rate correction. */
  float _gain_omega;
  /** Phase at T = 0 (samples). */
  std::complex<float> _p_0T;
  /** Phase at T=-1 (samples). */
  std::complex<float> _p_1T;
  /** Phase at T=-2 (samples). */
  std::complex<float> _p_2T;
  /** Constellation at T=0 (samples). */
  std::complex<float> _c_0T;
  /** Constellation at T=-1 (samples). */
  std::complex<float> _c_1T;
  /** Constellation at T=-2 (samples). */
  std::complex<float> _c_2T;
  /** The last @c _superSample phases. */
  Buffer<float> _hist;
  /** Current phase history index. */
  size_t _hist_idx;
  /** The last output constellation. */
  int _last_constellation;
  /** Output buffer. */
  Buffer<uint8_t> _buffer;
};



/** Simple varicode (Huffman code) decoder node. It consumes a bit-stream (uint8_t)
 * and produces a uint8_t stream of ascii chars. Non-printable chars (except for new-line) are
 * ignored. The output stream has no samplerate!
 * @ingroup datanodes */
class Varicode: public Sink<uint8_t>, public Source
{
public:
  /** Constructor. */
  Varicode();
  /** Destructor. */
  virtual ~Varicode();
  /** Configures the node. */
  virtual void config(const Config &src_cfg);
  /** Converts the input bit stream to ASCII chars. */
  virtual void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

protected:
  /** The shift register of the last received bits. */
  uint16_t _value;
  /** The output buffer. */
  Buffer<uint8_t> _buffer;
  /** The conversion table. */
  std::map<uint16_t, char> _code_table;
};


}


#endif // __SDR_PSK31_HH__

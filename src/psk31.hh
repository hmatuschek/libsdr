#ifndef __SDR_PSK31_HH__
#define __SDR_PSK31_HH__

#include "node.hh"
#include "traits.hh"
#include "interpolate.hh"


namespace sdr {

/** A simple BPSK31 "demodulator". */
template <class Scalar>
class BPSK31: public Sink< std::complex<Scalar> >, public Source
{
public:
  BPSK31(double dF=0.1)
    : Sink< std::complex<Scalar> >(), Source(),
      _P(0), _F(0), _Fmin(-dF), _Fmax(dF)
  {
    double damping = std::sqrt(2)/2;
    double bw = M_PI/100;
    double tmp = 1. + 2*damping*bw + bw*bw;
    _alpha = 4*damping*bw/tmp;
    _beta  = 4*bw*bw/tmp;

    // Init Delay line
    _dl = Buffer< std::complex<float> >(2*8); _dl_idx = 0;
    for (size_t i=0; i<16; i++) { _dl[i] = 0; }

    // Inner PLL:
    _theta = 0; _mu = 0.25; _gain_mu = 0.01;
    _omega = _min_omega = _max_omega = 0;
    _omega_rel = 0.001; _gain_omega = 0.001;
    _p_0T = _p_1T = _p_2T = 0;
    _c_0T = _c_1T = _c_2T = 0;

    // Super-sample (256 samples per symbol)
    _superSample = 64;
  }

  virtual ~BPSK31() {
    // pass...
  }

  virtual void config(const Config &src_cfg) {
    // Requires type, sample rate & buffer size
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }
    // Check buffer type
    if (Config::typeId< std::complex<Scalar> >() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure BPSK31: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId< std::complex<Scalar> >();
      throw err;
    }

    double Fs = src_cfg.sampleRate();
    // Compute samples per symbol
    _omega = Fs/(_superSample*31.25);
    _min_omega = _omega*(1.0 - _omega_rel);
    _max_omega = _omega*(1.0 + _omega_rel);
    _omega_mid = 0.5*(_min_omega+_max_omega);

    _hist = Buffer<float>(_superSample);
    _hist_idx = 0;
    _last_constellation = 1;

    _buffer = Buffer<uint8_t>(src_cfg.bufferSize());

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
        std::complex<float> sample = interpolate(_dl.sub(_dl_idx, 8), _mu);
        _errorTracking(sample);
        _updatePLL(sample);
        // done. real(sample) constains phase of symbol
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

    this->send(_buffer.head(o));
  }


protected:
  inline bool _hasTransition() const {
    return ((_hist[_hist_idx-1]>=0) && (_hist[_hist_idx]<=0)) ||
        ((_hist[_hist_idx-1]<=0) && (_hist[_hist_idx]>=0));
  }

  inline int _currentContellation() const {
    float value = 0;
    for (size_t i=0; i<=_hist_idx; i++) { value += _hist[i]; }
    return (value > 0) ? 1 : -1;
  }

  inline float _phaseError(const std::complex<float> &value) const {
    float r2 = value.real()*value.real();
    float i2 = value.imag()*value.imag();
    float nrm2 = r2+i2;
    if (0 == nrm2) { return 0; }
    return -value.real()*value.imag()/nrm2;
  }

  inline void _updatePLL(const std::complex<float> &sample) {
    float phi = _phaseError(sample);
    _F += _beta*phi;
    _P += _F + _alpha*phi;
    while (_P>( 2*M_PI)) { _P -= 2*M_PI; }
    while (_P<(-2*M_PI)) { _P += 2*M_PI; }
    _F = std::min(_Fmax, std::max(_Fmin, _F));
    //std::cerr << "Update PLL: P=" << _P << "; F=" << _F << ", err= " << phi << std::endl;
  }

  inline void _updateSampler(const std::complex<Scalar> &value) {
    _mu-=1; _P += _F;
    while (_P>( 2*M_PI)) { _P -= 2*M_PI; }
    while (_P<(-2*M_PI)) { _P += 2*M_PI; }
    std::complex<float> fac = std::exp(std::complex<float>(0, _P+_theta));
    std::complex<float> sample = fac * std::complex<float>(value.real(), value.imag());
    _dl[_dl_idx] = sample;
    _dl[_dl_idx+8] = sample;
    _dl_idx = (_dl_idx + 1) % 8;
  }

  inline void _errorTracking(const std::complex<float> &sample) {
    _p_2T = _p_1T; _p_1T = _p_0T; _p_0T = sample;
    _c_2T = _c_1T; _c_1T = _c_0T; _c_0T = (sample.real() > 0) ? -1 : 1;

    std::complex<float> x = (_c_0T - _c_2T) * std::conj(_p_1T);
    std::complex<float> y = (_p_0T - _p_2T) * std::conj(_c_1T);
    float err = std::real(y-x);
    if (err >  1.0) { err = 1.0; }
    if (err < -1.0) { err = -1.0; }
    _omega += _gain_omega * err;
    if ( (_omega-_omega_mid) > _omega_rel) { _omega = _omega_mid + _omega_rel; }
    else if ( (_omega-_omega_mid) < -_omega_rel) { _omega = _omega_mid - _omega_rel; }
    else { _omega = _omega_mid + _omega-_omega_mid; }
    _mu += _omega + _gain_mu * err;
  }


protected:
  // Carrier PLL stuff
  float _P, _F, _Fmin, _Fmax;
  float _alpha, _beta;
  // Delay line
  Buffer< std::complex<float> > _dl;
  size_t _dl_idx;
  // Second PLL
  float _theta;
  float _mu, _gain_mu;
  float _omega, _min_omega, _max_omega;
  float _omega_mid, _omega_rel, _gain_omega;
  std::complex<float> _p_0T, _p_1T, _p_2T;
  std::complex<float> _c_0T, _c_1T, _c_2T;
  // Third "PLL" -> bit decoder
  size_t _superSample;
  Buffer<float> _hist;
  size_t _hist_idx;
  int _last_constellation;
  // Output buffer
  Buffer<uint8_t> _buffer;
};



class Varicode: public Sink<uint8_t>, public Source
{
public:
  Varicode();
  virtual ~Varicode();

  virtual void config(const Config &src_cfg);

  virtual void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

protected:
  uint16_t _value;
  Buffer<uint8_t> _buffer;
  std::map<uint16_t, char> _code_table;
};




}


#endif // __SDR_PSK31_HH__

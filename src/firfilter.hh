#ifndef __SDR_FIRFILTER_HH__
#define __SDR_FIRFILTER_HH__

#include "config.hh"
#include "node.hh"
#include "logger.hh"

namespace sdr {

/** Implements the calculation of the filter coefficients for the use in the @c FIRFilter
 * template class. */
class FIRLowPassCoeffs
{
public:
  /** Calculates the filter coefficients. */
  static inline void coeffs(std::vector<double> &alpha, double Fl, double Fu, double Fs)
  {
    size_t N = alpha.size();
    double w = 2*M_PI*Fu/Fs;
    double M = double(N)/2;
    double norm = 0;
    for (size_t i=0; i<N; i++) {
      if (N == 2*i) { alpha[i] = 4*w/M_PI; }
      else { alpha[i] = std::sin(w*(i-M))/(w*(i-M)); }
      // apply window function
      alpha[i] *= (0.42 - 0.5*cos((2*M_PI*i)/N) + 0.08*cos((4*M_PI*i)/N));
      // Calc norm
      norm += std::abs(alpha[i]);
    }
    // Normalize filter coeffs:
    for (size_t i=0; i<N; i++) { alpha[i] /= norm; }
  }
};


/** Implements the calculation of the filter coefficients for the use in the @c FIRFilter
 * template class. */
class FIRHighPassCoeffs
{
public:
  /** Calculates the filter coefficients. */
  static inline void coeffs(std::vector<double> &alpha, double Fl, double Fu, double Fs)
  {
    size_t N = alpha.size();
    double w = 2*M_PI*Fl/Fs;
    double M = double(N)/2;
    double norm = 0;
    for (size_t i=0; i<N; i++) {
      if (N == 2*i) { alpha[i] = -std::sin(w*(i-M))/(w*(i-N)); }
      else { alpha[i] = 1-4*w/M_PI; }
      // apply window function
      alpha[i] *= (0.42 - 0.5*cos((2*M_PI*i)/N) + 0.08*cos((4*M_PI*i)/N));
      norm += std::abs(alpha[i]);
    }
    // Normalize filter coeffs:
    for (size_t i=0; i<N; i++) { alpha[i] /= norm; }
  }
};


/** Implements the calculation of the filter coefficients for the use in the @c FIRFilter
 * template class. */
class FIRBandPassCoeffs
{
public:
  /** Calculates the filter coefficients. */
  static inline void coeffs(std::vector<double> &alpha, double Fl, double Fu, double Fs)
  {
    size_t N = alpha.size();
    double wl = 2*M_PI*Fl/Fs;
    double wu = 2*M_PI*Fu/Fs;
    double M = double(N)/2;
    double norm = 0;
    for (size_t i=0; i<N; i++) {
      if (N == 2*i) { alpha[i] = 4*(wl-wu)/M_PI; }
      else { alpha[i] = std::sin(wl*(i-M)/(wl*(i-M))) - std::sin(wu*(i-M)/(wu*(i-M))); }
      // apply window function
      alpha[i] *= (0.42 - 0.5*cos((2*M_PI*i)/N) + 0.08*cos((4*M_PI*i)/N));
      norm += std::abs(alpha[i]);
    }
    // Normalize filter coeffs:
    for (size_t i=0; i<N; i++) { alpha[i] /= norm; }
  }
};


/** Implements the calculation of the filter coefficients for the use in the @c FIRFilter
 * template class. */
class FIRBandStopCoeffs
{
public:
  /** Calculates the filter coefficients. */
  static inline void coeffs(std::vector<double> &alpha, double Fl, double Fu, double Fs)
  {
    size_t N = alpha.size();
    double wl = 2*M_PI*Fl/Fs;
    double wu = 2*M_PI*Fl/Fs;
    double M = double(N)/2;
    double norm = 0;
    for (size_t i=0; i<N; i++) {
      if (N == 2*i) { alpha[i] = 1-4*(wl-wu)/M_PI; }
      else { alpha[i] = std::sin(wu*(i-M)/(wu*(i-M))) - std::sin(wl*(i-M)/(wl*(i-M))); }
      // apply window function
      alpha[i] *= (0.42 - 0.5*cos((2*M_PI*i)/N) + 0.08*cos((4*M_PI*i)/N));
      norm += std::abs(alpha[i]);
    }
    // Normalize filter coeffs:
    for (size_t i=0; i<N; i++) { alpha[i] /= norm; }
  }
};


/** Generic FIR filter class. Use one of the specializations below for a low-, high- or band-pass
 * filter.
 * @ingroup filters */
template <class Scalar, class FilterCoeffs>
class FIRFilter: public Sink<Scalar>, public Source
{
public:
  /** Constructor. */
  FIRFilter(size_t order, double Fl, double Fu)
    : Sink<Scalar>(), Source(), _enabled(true), _order(std::max(size_t(1), order)), _Fl(Fl), _Fu(Fu),
      _Fs(0), _alpha(_order, 0), _ring(_order), _ring_offset(0)
  {
    // pass...
  }

  /** Destructor. */
  virtual ~FIRFilter() {
    _ring.unref();
    _buffer.unref();
  }

  /** Returns true if the filter is enabled. */
  inline bool enabled() const { return _enabled; }
  /** Enable/Disable the filter. */
  inline void enable(bool enable) { _enabled = enable; }

  /** Returns the order of the filter. */
  inline size_t order() const { return _order; }
  /** Sets the order of the filter and updates the filter coefficients. */
  virtual void setOrder(size_t order) {
    // Ensure order is larger 0:
    order = std::max(size_t(1), order);
    if (order == _order) { return; }
    _order = order;
    // Resize past and coeffs
    _alpha.resize(_order);
    _ring = Buffer<Scalar>(_order);
    _ring_offset = 0;
    // Update coeffs:
    FilterCoeffs::coeffs(_alpha, _Fl, _Fu, _Fs);
  }

  /** Returns the lower edge frequency. */
  inline double lowerFreq() const { return _Fl; }
  /** Sets the lower edge frequency. */
  inline void setLowerFreq(double Fl) {
    _Fl = Fl;
    FilterCoeffs::coeffs(_alpha, _Fl, _Fu, _Fs);
  }

  /** Returns the upper edge frequency. */
  inline double uppertFreq() const { return _Fl; }
  /** Sets the upper edge frequency. */
  inline void setUpperFreq(double Fu) {
    _Fu = Fu;
    FilterCoeffs::coeffs(_alpha, _Fl, _Fu, _Fs);
  }

  /** Configures the filter. */
  virtual void config(const Config &src_cfg) {
    // Requires type, sample-rate and buffer-size
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }
    // check type
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure FIRLowPass: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }

    // Calc coeff
    _Fs = src_cfg.sampleRate();
    FilterCoeffs::coeffs(_alpha, _Fl, _Fu, _Fs);

    // unref buffer if non-empty
    if (!_buffer.isEmpty()) { _buffer.unref(); }

    // allocate buffer
    _buffer = Buffer<Scalar>(src_cfg.bufferSize());

    // Clear ring buffer
    for (size_t i=0; i<_order; i++) { _ring[i] = 0; }
    _ring_offset = 0;

    LogMessage msg(LOG_DEBUG);
    msg << "Configured FIRFilter:" << std::endl
        << " type " << src_cfg.type() << std::endl
        << " sample rate " << _Fs << std::endl
        << " buffer size " << src_cfg.bufferSize() << std::endl
        << " order " << _order;
    Logger::get().log(msg);

    // Propergate config
    this->setConfig(Config(src_cfg.type(), src_cfg.sampleRate(), src_cfg.bufferSize(), 1));
  }


  /** Performs the filtering. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    // If emtpy
    if (0 == buffer.size()) { /* drop buffer */ }
    // If disabled -> pass
    if (!_enabled) { this->send(buffer, allow_overwrite); return; }

    // Perform filtering in-place or out-of-place filtering
    if (allow_overwrite) { _process(buffer, buffer); }
    else if (_buffer.isUnused()) { _process(buffer, _buffer); }
    else {
#ifdef SDR_DEBUG
      LogMessage msg(LOG_WARNING);
      msg << "FIR Filter: Drop buffer, output buffer still in use.";
      Logger::get().log(msg);
#endif
    }
  }

protected:
  /** performs the actual computation */
  inline void _process(const Buffer<Scalar> &in, const Buffer<Scalar> &out)
  {
    for (size_t i=0; i<in.size(); i++) {
      // Store input value into ring buffer
      _ring[_ring_offset] = in[i]; _ring_offset++;
      if (_ring_offset == _order) { _ring_offset=0; }
      out[i] = 0;
      // Apply filter on ring buffer
      size_t idx = _ring_offset;
      for (size_t j=0; j<_order; j++, idx++) {
        if (idx == _order) { idx = 0; }
        out[i] += _alpha[j]*_ring[idx];
      }
    }

    // Done.
    this->send(out.head(in.size()), true);
  }


protected:
  /** If true, the filtering is enabled. */
  bool _enabled;
  /** The order of the filter. */
  size_t _order;
  /** The lower edge frequency. */
  double _Fl;
  /** The upper edge frequency. */
  double _Fu;
  /** Current sample rate. */
  double _Fs;
  /** The current filter coefficients. */
  std::vector<double> _alpha;
  /** A "ring-buffer" used to perform the filtering. */
  Buffer<Scalar> _ring;
  /** Offset of the "ring-buffer". */
  size_t _ring_offset;
  /** The output buffer, unused if filtering is performed in-place. */
  Buffer<Scalar> _buffer;
};


/** Low-pass FIR filter specialization.
 * @ingroup filters */
template <class Scalar>
class FIRLowPass: public FIRFilter<Scalar, FIRLowPassCoeffs>
{
public:
  /** Constructor. */
  FIRLowPass(size_t order, double Fc)
    : FIRFilter<Scalar, FIRLowPassCoeffs>(order, 0, Fc) { }
  /** Destructor. */
  virtual ~FIRLowPass() { }
  /** Returns the filter frequency. */
  inline double freq() const { return FIRFilter<Scalar, FIRLowPassCoeffs>::_Fu; }
  /** Sets the filter frequency. */
  inline void setFreq(double freq) { FIRFilter<Scalar, FIRLowPassCoeffs>::setUpperFreq(freq); }
};

/** High-pass FIR filter specialization.
 * @ingroup filters */
template <class Scalar>
class FIRHighPass: public FIRFilter<Scalar, FIRHighPassCoeffs>
{
public:
  /** Constructor. */
  FIRHighPass(size_t order, double Fc)
    : FIRFilter<Scalar, FIRHighPassCoeffs>(order, Fc, 0) { }
  /** Destructor. */
  virtual ~FIRHighPass() { }
  /** Returns the filter frequency. */
  inline double freq() const { return FIRFilter<Scalar, FIRLowPassCoeffs>::_Fl; }
  /** Sets the filter frequency. */
  inline void setFreq(double freq) { FIRFilter<Scalar, FIRLowPassCoeffs>::setLowerFreq(freq); }
};


/** Band-pass FIR filter specialization.
 * @ingroup filters */
template <class Scalar>
class FIRBandPass: public FIRFilter<Scalar, FIRBandPassCoeffs>
{
public:
  /** Constructor. */
  FIRBandPass(size_t order, double Fl, double Fu)
    : FIRFilter<Scalar, FIRBandPassCoeffs>(order, Fl, Fu) { }
  /** Destructor. */
  virtual ~FIRBandPass() { }
};

/** Band-stop FIR filter specialization.
 * @ingroup filters */
template <class Scalar>
class FIRBandStop: public FIRFilter<Scalar, FIRBandStopCoeffs>
{
public:
  /** Constructor. */
  FIRBandStop(size_t order, double Fl, double Fu)
    : FIRFilter<Scalar, FIRBandStopCoeffs>(order, Fl, Fu) { }
  /** Destructor. */
  virtual ~FIRBandStop() { }
};

}

#endif // FIRFILTER_HH

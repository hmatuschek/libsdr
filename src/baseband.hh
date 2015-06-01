#ifndef __SDR_BASEBAND_HH__
#define __SDR_BASEBAND_HH__

#include "node.hh"
#include "config.hh"
#include "utils.hh"
#include "traits.hh"
#include "operators.hh"
#include "freqshift.hh"

namespace sdr {


/** This class performs several operations on the complex (integral) input stream, it first filters
 * out some part of the input stream using a FIR band pass (band pass is centerred around @c Ff
 * with width @c width) then shifts the center frequency @c Fc to 0 and finally sub-samples the
 * resulting stream. This node can be used to select a portion of the input spectrum and for the
 * reduction of the stream rate, allowing for some more expensive operations to be performed on the
 * output stream.
 * @ingroup filters */
template <class Scalar>
class IQBaseBand: public Sink< std::complex<Scalar> >, public Source, public FreqShiftBase<Scalar>
{
public:
  /** The complex type of the input stream. */
  typedef std::complex<Scalar> CScalar;
  /** The (real) computation scalar type (super scalar), the computations are performed with this
   * scalar type. */
  typedef int32_t SScalar;
  /** Complex @c SScalar type. */
  typedef std::complex<SScalar> CSScalar;

public:
  /** Constructor, the filter center frequency @c Ff equals the given center frequency @c Fc. */
  IQBaseBand(double Fc, double width, size_t order, size_t sub_sample, double oFs=0.0)
    : Sink<CScalar>(), Source(), FreqShiftBase<Scalar>(Fc, 0),
      _Fc(Fc), _Ff(Fc), _Fs(0), _width(width), _order(std::max(size_t(1), order)),
      _sub_sample(sub_sample), _oFs(oFs), _ring_offset(0), _sample_count(0),
      _last(0), _kernel(_order)
  {
    // Allocate and reset ring buffer:
    _ring = Buffer<CSScalar>(_order);
    for (size_t i=0; i<_order; i++) { _ring[i] = 0; }
  }

  /** Constructor. */
  IQBaseBand(double Fc, double Ff, double width, size_t order, size_t sub_sample, double oFs=0.0)
    : Sink<CScalar>(), Source(), FreqShiftBase<Scalar>(Fc, 0),
      _Fc(Fc), _Ff(Ff), _Fs(0), _width(width), _order(std::max(size_t(1), order)),
      _sub_sample(sub_sample), _oFs(oFs), _ring_offset(0), _sample_count(0),
      _last(0), _kernel(_order)
  {
    // Allocate and reset ring buffer:
    _ring = Buffer<CSScalar>(_order);
    for (size_t i=0; i<_order; i++) { _ring[i] = 0; }
  }

  /** Destructor. */
  virtual ~IQBaseBand() {
    // Free buffers
    _buffer.unref();
    _kernel.unref();
    _ring.unref();
  }

  /** Returns the order of the band-pass filter. */
  inline size_t order() const { return _order; }
  /** (Re-) Sets the filter order. */
  void setOrder(size_t o) {
    // ensure filter order >= 1
    o = (o<1) ? 1 : o;
    // Reallocate ring buffer and kernel
    if (! _kernel.isEmpty()) { _kernel.unref(); }
    if (! _ring.isEmpty()) { _ring.unref(); }
    _kernel = Buffer<CSScalar>(o);
    _ring = Buffer<CSScalar>(o);
    // Update filter kernel
    _order = o; _update_filter_kernel();
  }

  /** Returns the center frequency. */
  inline double centerFrequency() const { return _Fc; }
  /** Resets the center frequency. */
  void setCenterFrequency(double Fc) {
    _Fc = Fc; this->setFrequencyShift(_Fc);
  }

  /** Returns the filter frequency. */
  inline double filterFrequency() const { return _Ff; }
  /** (Re-) Sets the filter frequency. */
  void setFilterFrequency(double Ff) {
    _Ff = Ff; _update_filter_kernel();
  }

  /** Returns the filter width. */
  inline double filterWidth() const { return _width; }
  /** Sets the filter width. */
  void setFilterWidth(double width) {
    _width = width; _update_filter_kernel();
  }

  /** Returns the sub sampling. */
  size_t subSample() const { return _sub_sample; }
  /** Resets the sub sampling. Please note that the Queue needs to be stopped before calling this function! */
  void setSubsample(size_t sub_sample) {
    _sub_sample = std::max(size_t(1), sub_sample); _reconfigure();
  }
  /** Resets the output sample rate. The sub-sampling will be adjusted accordingly. Please note the the resulting
   * output sample rate will be rounded up to be an integral fraction of the input sample rate. */
  void setOutputSampleRate(double Fs) {
    _oFs = Fs; _reconfigure();
  }

  /** Configures the BaseBand node. */
  virtual void config(const Config &src_cfg)
  {
    // Requires type, sample rate & buffer size
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }
    // Check buffer type
    if (Config::typeId<CScalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure IQBaseBand: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId<CScalar>();
      throw err;
    }
    // Store sample rate
    _Fs = src_cfg.sampleRate();
    // Store source buffer size
    _sourceBs = src_cfg.bufferSize();

    _reconfigure();
  }


  /** Processes the given buffer. */
  virtual void process(const Buffer< std::complex<Scalar> > &buffer, bool allow_overwrite)
  {
    if (allow_overwrite) {
      // Perform in-place if @c allow_overwrite
      _process(buffer, buffer);
    } else if (_buffer.isUnused()) {
      // Otherwise store results into output buffer.
      _process(buffer, _buffer);
    } else {
#ifdef SDR_DEBUG
      LogMessage msg(LOG_WARNING);
      msg << "IQBaseBand: Drop buffer: Output buffer still in use.";
      Logger::get().log(msg);
#endif
    }
  }


protected:
  /** Reconfigures the node. */
  void _reconfigure()
  {
    // If _oFs > 0 -> set _sub_sample to match the sample rate (approx).
    if (_oFs > 0) {
      _sub_sample = _Fs/_oFs;
      if (_sub_sample < 1) { _sub_sample=1; }
    }

    // Update filter kernel
    _update_filter_kernel();
    // Update freq shift operator:
    this->setSampleRate(_Fs);
    // Calc output buffer size
    size_t buffer_size = _sourceBs/_sub_sample;
    if (_sourceBs%_sub_sample) { buffer_size += 1; }
    // Allocate output buffer
    _buffer = Buffer<CScalar>(buffer_size);

    // Reset internal state
    _last = 0;
    _sample_count = 0;
    _ring_offset = 0;

    LogMessage msg(LOG_DEBUG);
    msg << "Configured IQBaseBand node:" << std::endl
        << " type " << Traits< std::complex<Scalar> >::scalarId << std::endl
        << " sample-rate " << _Fs << "Hz" << std::endl
        << " center freq " << _Fc << "Hz" << std::endl
        << " width " << _width << "Hz" << std::endl
        << " kernel " << _kernel << std::endl
        << " in buffer size " << _sourceBs << std::endl
        << " sub-sample by " << _sub_sample << std::endl
        << " out buffer size " << buffer_size;
    Logger::get().log(msg);

     // Propergate config
    this->setConfig(Config(Traits< std::complex<Scalar> >::scalarId,
                           _Fs/_sub_sample, buffer_size, 1));
  }

  /** Performs the base-band selection, frequency shift and sub-sampling. Stores the
   * results into @c out. The input and output buffer may overlapp. */
  inline void _process(const Buffer<CScalar> &in, const Buffer<CScalar> &out) {
    size_t i=0, j=0;
    for (; i<in.size(); i++, _sample_count++) {
      // Store sample in ring buffer
      _ring[_ring_offset] = in[i];

      // Apply filter on ring-buffer and shift freq
      _last += this->applyFrequencyShift(_filter_ring());

      // _ring_offset modulo _order
      _ring_offset++;
      if (_order == _ring_offset) { _ring_offset = 0; }

      // If _sample_count samples have been averaged:
      if (_sub_sample == _sample_count) {
        // Store average in output buffer
        CSScalar value = _last/CSScalar(_sub_sample);
        out[j] = value;
        // reset average, sample count and increment output buffer index j
        _last = 0; _sample_count=0; j++;
      } else if (_sub_sample == 1) {
        out[j] = _last; _last = 0; _sample_count = 0; j++;
      }
    }
    this->send(out.head(j), true);
  }

  /** Applies the filter on the data stored in the ring buffer. */
  inline CSScalar _filter_ring() const
  {
    CSScalar res = 0;
    size_t idx = _ring_offset+1;
    if (_order == idx) { idx = 0; }
    for (size_t i=0; i<_order; i++, idx++) {
      if (_order == idx) { idx = 0; }
      res += _kernel[i]*_ring[idx];
    }
    return (res>>14);
  }

  /** Updates the band-pass filter kernel. */
  void _update_filter_kernel() {
    // First, create a filter kernel of a low-pass filter with _width/2 cut-off
    Buffer< std::complex<double> > alpha(_order);
    double w = (M_PI*_width)/(_Fs);
    double M = double(_order)/2.;
    double norm = 0;

    //std::complex<double> phi(0.0, (2*M_PI*_Ff)/_Fs); phi = std::exp(phi);
    for (size_t i=0; i<_order; i++) {
      if (_order == 2*i) { alpha[i] = 4*(w/M_PI); }
      else { alpha[i] = std::sin(w*(i-M))/(w*(i-M)); }
      // Shift freq by +_Ff:
      //alpha[i] = phi*alpha[i]; phi = phi*phi;
      alpha[i] *= std::exp(std::complex<double>(0.0, (-2*M_PI*_Ff*i)/_Fs));
      // apply window function
      alpha[i] *= (0.42 - 0.5*cos((2*M_PI*i)/_order) + 0.08*cos((4*M_PI*i)/_order));
      // Calc norm
      norm += std::abs(alpha[i]);
    }
    // Normalize filter coeffs and store in _kernel:
    for (size_t i=0; i<_order; i++) {
      _kernel[i] = (double(1<<14) * alpha[i]) / norm;
    }
  }

protected:
  /** The frequency shift of the base band. The filter result will be shifted by @c -_Fc. */
  int32_t _Fc;
  /** The center frequency of the base band. */
  int32_t _Ff;
  /** The input sample rate. */
  int32_t _Fs;
  /** The filter width. */
  int32_t _width;
  /** The order of the filter. Must be greater that 0. */
  size_t _order;
  /** The number of sample averages for the sub sampling. @c _sub_sample==1 means no subsampling. */
  size_t _sub_sample;
  /** Holds the desired output sample rate, _sub_sample will be adjusted accordingly. */
  double _oFs;
  /** The current index of the ring buffer. */
  size_t _ring_offset;
  /** Holds the current number of samples averaged. */
  size_t _sample_count;
  /** Holds the current sum of the last @c _sample_count samples. */
  CSScalar _last;
  /** Buffer size of the source. */
  size_t _sourceBs;

  /** The filter kernel of order _order. */
  Buffer<CSScalar> _kernel;
  /** A ring buffer of past values. The band-pass filtering is performed on this buffer. */
  Buffer<CSScalar> _ring;
  /** The output buffer. */
  Buffer<CScalar> _buffer;
};




/** This class performs several operations on the real input stream,
 * It first filters out some part of the input stream using a FIR band pass filter
 * then shifts the center frequency to 0 and finally sub-samples the resulting stream such that
 * the selected base-band is well represented.
 * @ingroup filters */
template <class Scalar>
class BaseBand: public Sink<Scalar>, public Source, public FreqShiftBase<Scalar>
{
public:
  /** The complex input scalar. */
  typedef typename FreqShiftBase<Scalar>::CScalar  CScalar;
  /** The real super scalar. */
  typedef typename FreqShiftBase<Scalar>::SScalar  SScalar;
  /** The complex super scalar. */
  typedef typename FreqShiftBase<Scalar>::CSScalar CSScalar;

public:
  /** Constructs a new BaseBand instance.
   * @param Fc Specifies the center frequency of the base band. The resulting
   *        signal will be shifted down by this frequency.
   * @param width Specifies the with of the band pass filter.
   * @param order Specifies the order of the FIR band pass filter.
   * @param sub_sample Specifies the sub-sampling of the resulting singnal. */
  BaseBand(double Fc, double width, size_t order, size_t sub_sample)
    : Sink<Scalar>(), Source(), FreqShiftBase<Scalar>(Fc, 0),
      _Ff(Fc), _width(width), _order(std::max(size_t(1), order)),
      _sub_sample(sub_sample), _ring_offset(0), _sample_count(0),
      _last(0), _kernel(_order)
  {
    // Allocate and reset ring buffer:
    _ring = Buffer<SScalar>(_order);
    for (size_t i=0; i<_order; i++) { _ring[i] = 0; }
  }

  /** Constructs a new BaseBand instance.
   * @param Fc Specifies the frequency of the base band. The resulting
   *        signal will be shifted down by this frequency.
   * @param Ff Specifies the center frequency of the band pass filter.
   * @param width Specifies the with of the band pass filter.
   * @param order Specifies the order of the FIR band pass filter.
   * @param sub_sample Specifies the sub-sampling of the resulting singnal. */
  BaseBand(double Fc, double Ff, double width, size_t order, size_t sub_sample)
    : Sink<Scalar>(), Source(), FreqShiftBase<Scalar>(Fc, 0),
      _Ff(Ff), _width(width), _order(std::max(size_t(1), order)),
      _sub_sample(sub_sample), _ring_offset(0), _sample_count(0),
      _last(0), _kernel(_order)
  {
    // Allocate and reset ring buffer:
    _ring = Buffer<SScalar>(_order);
    for (size_t i=0; i<_order; i++) { _ring[i] = 0; }
  }

  /** Destructor. */
  virtual ~BaseBand() {
    // Free buffers
    _buffer.unref();
    _kernel.unref();
    _ring.unref();
  }

  /** Configures the base band node. Implements the @c Sink interface. */
  virtual void config(const Config &src_cfg) {
    // Requires type, sample rate & buffer size
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }
    // Check buffer type
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure BaseBand: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }
    // Store sample rate
    this->setSampleRate(src_cfg.sampleRate());

    // Calc output buffer size
    size_t buffer_size = src_cfg.bufferSize()/_sub_sample;
    if (src_cfg.bufferSize()%_sub_sample) { buffer_size += 1; }
    // Allocate output buffer
    _buffer = Buffer<CScalar>(buffer_size);

    _last = 0;
    _sample_count = 0;
    _ring_offset = 0;

    LogMessage msg(LOG_DEBUG);
    msg << "Configured BaseBand node:" << std::endl
        << " type " << Traits< std::complex<Scalar> >::scalarId << std::endl
        << " sample-rate " << FreqShiftBase<Scalar>::sampleRate() << "Hz" << std::endl
        << " center freq " << FreqShiftBase<Scalar>::frequencyShift() << "Hz" << std::endl
        << " width " << _width << "Hz" << std::endl
        << " kernel " << _kernel << std::endl
        << " in buffer size " << src_cfg.bufferSize() << std::endl
        << " sub-sample by " << _sub_sample << std::endl
        << " out buffer size " << buffer_size;
    Logger::get().log(msg);

     // Propergate config
    this->setConfig(Config(Traits< std::complex<Scalar> >::scalarId,
                           FreqShiftBase<Scalar>::sampleRate()/_sub_sample, buffer_size, 1));
  }

  virtual void setSampleRate(double Fs) {
    FreqShiftBase<Scalar>::setSampleRate(Fs);
    _update_filter_kernel();
    /// @bug Also signal config change of the sourcce by setConfig()!
  }

  /** Processes the input buffer. Implements the @c Sink interface. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite)
  {
    if (_buffer.isUnused()) {
      _process(buffer, _buffer);
    } else {
#ifdef SDR_DEBUG
      LogMessage msg(LOG_WARNING);
      msg << "BaseBand: Drop buffer: Output buffer still in use.";
      Logger::get().log(msg);
#endif
    }
  }


protected:
  /** Performs the actual procssing. First, the bandpass filter is applied, then the filtered signal
   * is shifted and finally the signal gets averaged over @c _sub_sample samples, implementing the
   * averaging sub-sampling. */
  inline void _process(const Buffer<Scalar> &in, const Buffer<CScalar> &out) {
    size_t i=0, j=0;
    for (; i<in.size(); i++) {
      // Store sample in ring buffer
      _ring[_ring_offset] = in[i];

      _last += this->applyFrequencyShift(_filter_ring());
      _sample_count++;

      // _ring_offset modulo _order
      _ring_offset++;
      if (_order == _ring_offset) { _ring_offset = 0; }

      // If _sample_count samples have been averaged:
      if (_sub_sample == _sample_count) {
        // Store average in output buffer
        out[j] = _last/CSScalar(_sub_sample);;
        // reset average, sample count and increment output buffer index j
        _last = 0; _sample_count=0; j++;
      }
    }
    this->send(out.head(j), true);
  }

  /** Applies the filter on the data stored in the ring buffer. */
  inline CSScalar _filter_ring()
  {
    CSScalar res = 0;
    size_t idx = _ring_offset+1;
    if (_order == idx) { idx = 0; }
    for (size_t i=0; i<_order; i++, idx++) {
      if (_order == idx) { idx = 0; }
      res += _kernel[i]*_ring[idx];
    }
    return (res>>Traits<Scalar>::shift);
  }

  /** Calculates or updates the filter kernel. */
  void _update_filter_kernel() {
    // First, create a filter kernel of a low-pass filter with
    // _width/2 cut-off
    Buffer< std::complex<double> > alpha(_order);
    double w = (2*M_PI*_width)/(2*FreqShiftBase<Scalar>::sampleRate());
    double M = double(_order)/2;
    double norm = 0;
    for (size_t i=0; i<_order; i++) {
      if (_order == (2*i)) { alpha[i] = 1; }
      else { alpha[i] = std::sin(w*(i-M))/(w*(i-M)); }
    }
    // Shift filter by _Fc, apply window function and calc norm of filter
    std::complex<double> phi(0.0, (2*M_PI*_Ff)/FreqShiftBase<Scalar>::sampleRate()); phi = std::exp(phi);
    for (size_t i=0; i<_order; i++) {
      // Shift filter kernel
      alpha[i] = alpha[i]*std::exp(std::complex<double>(0, (2*M_PI*_Ff*i)/FreqShiftBase<Scalar>::sampleRate()));
      // apply window function
      alpha[i] *= (0.42 - 0.5*cos((2*M_PI*(i+1))/(_order+2)) +
                   0.08*cos((4*M_PI*(i+1))/(_order+2)));
      // Calc norm
      norm += std::abs(alpha[i]);
    }
    // Normalize filter coeffs and store in _kernel:
    for (size_t i=0; i<_order; i++) {
      _kernel[i] = ((double(1<<Traits<Scalar>::shift) * alpha[i]) / norm);
    }
    // free alpha
    alpha.unref();
  }

protected:
  /** The center frequency of the band pass filter. */
  double _Ff;
  /** The width of the band pass filter. */
  double _width;
  /** The order of the band pass filter. */
  size_t _order;
  /** The number of averages taken for subsampling. */
  size_t _sub_sample;
  /** The current index of the ring buffer. */
  size_t _ring_offset;

  /** The current number of averages. */
  size_t _sample_count;
  /** The current sum of the last @c _sample_count samples. */
  CSScalar _last;

  /** If true, @c Fc!=0. */
  bool _shift_freq;
  /** \f$\exp(i\phi)\f$ look-up table */
  Buffer< CSScalar > _lut;
  /** The LUT index increment per (1<<4) samples. */
  size_t _lut_inc;
  /** The current LUT index times (1<<4). */
  size_t _lut_count;

  /** The filter kernel of order _order. */
  Buffer<CSScalar> _kernel;
  /** A ring buffer of past values. */
  Buffer<SScalar> _ring;
  /** The output buffer. */
  Buffer<CScalar> _buffer;

protected:
  /** Size of the look-up table. */
  static const size_t _lut_size=128;
};

}

#endif // __SDR_BASEBAND_HH__

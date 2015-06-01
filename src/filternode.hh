#ifndef __SDR_FILTERNODE_HH__
#define __SDR_FILTERNODE_HH__

#include <limits>
#include <list>

#include "config.hh"
#include "node.hh"
#include "buffer.hh"
#include "buffernode.hh"
#include "fftplan.hh"


namespace sdr {

/** Evaluates the shifted filter function. */
template <class Scalar>
inline std::complex<Scalar> sinc_flt_kernel(int i, int N, double Fc, double bw, double Fs) {
  std::complex<Scalar> v;
  // Eval sinc
  if ( (N/2) == i) { v = M_PI*(bw/Fs); }
  else { v = std::sin(M_PI*(bw/Fs)*(i-N/2))/(i-N/2); }
  // shift frequency
  v *= std::exp(std::complex<Scalar>(0.0, (2*M_PI*Fc*i)/Fs));
  // Apply window
  v *= (0.42 - 0.5*cos((2*M_PI*i)/N) + 0.08*cos((4*M_PI*i)/N));
  return v;
}



/** Performs the FFT forward transform. */
template <class Scalar>
class FilterSink: public Sink< std::complex<Scalar> >, public Source
{
public:
  /** Constructor, best performance with block-size being a power of 2. */
  FilterSink(size_t block_size)
    : Sink< std::complex<Scalar> >(), Source(), _block_size(block_size),
      _in_buffer(2*block_size), _out_buffer(2*block_size),
      _plan(_in_buffer, _out_buffer, FFT::FORWARD)
  {
    // Fill second half of input buffer with 0s
    for(size_t i=0; i<2*_block_size; i++) {
      _in_buffer[i] = 0;
    }
  }

  /** Destructor. */
  virtual ~FilterSink() {
    _in_buffer.unref();
    _out_buffer.unref();
  }

  /** Configures the node. */
  virtual void config(const Config &src_cfg) {
    // Skip if config is incomplete
    if ((Config::Type_UNDEFINED==src_cfg.type()) || (0 == src_cfg.sampleRate()) || (0 == src_cfg.bufferSize())) {
      return;
    }
    // Now check type (must be real double)
    if (Config::typeId< std::complex<Scalar> >() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure filter-sink: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId< std::complex<Scalar> >();
      throw err;
    }
    // Check buffer size
    if (_block_size != src_cfg.bufferSize()) {
      ConfigError err;
      err << "Can not configure filter-sink: Invalid buffer size " << src_cfg.bufferSize()
          << ", expected " << _block_size;
      throw err;
    }
    // Propagate configuration
    setConfig(Config(Config::typeId< std::complex<Scalar> >(), src_cfg.sampleRate(),
                     src_cfg.bufferSize(), src_cfg.numBuffers()));
  }

  /** Performs the FFT forward transform. */
  virtual void process(const Buffer< std::complex<Scalar> > &buffer, bool allow_overwrite) {
    // Copy buffer into 1st half of input buffer
    for (size_t i=0; i<_block_size; i++) { _in_buffer[i] = buffer[i]; }
    // perform fft
    _plan();
    // send fft result
    this->send(_out_buffer);
  }

protected:
  /** The block size. */
  size_t _block_size;
  /** The input buffer. */
  Buffer< std::complex<Scalar> > _in_buffer;
  /** The output buffer (transformed). */
  Buffer< std::complex<Scalar> > _out_buffer;
  /** The plan for the FFT forward transform. */
  FFTPlan<Scalar> _plan;
};



/** Performs the overlap-add FFT filtering and back-transform. */
template <class Scalar>
class FilterSource: public Sink< std::complex<Scalar> >, public Source
{
public:
  /** Constructor. */
  FilterSource(size_t block_size, double fmin, double fmax)
    : Sink< std::complex<Scalar> >(), Source(),
    _block_size(block_size), _sample_rate(0),
    _in_buffer(2u*_block_size), _trafo_buffer(2u*_block_size), _last_trafo(_block_size),
    _kern(2u*_block_size), _buffers(1, _block_size), _fmin(fmin), _fmax(fmax),
    _plan(_in_buffer, _trafo_buffer, FFT::BACKWARD)
  {
    // Set input & output buffer to 0
    for (size_t i=0; i<_block_size; i++) {
      _last_trafo[i] = 0;
    }
  }

  /** Destructor. */
  virtual ~FilterSource() {
    // pas...
  }

  /** Set the frequency range. */
  void setFreq(double fmin, double fmax) {
    _fmin = fmin; _fmax = fmax; _updateFilter();
  }

  /** Configures the filter node. */
  virtual void config(const Config &src_cfg) {
    // Check config
    if ((0 == src_cfg.sampleRate()) || (0 == src_cfg.bufferSize())) { return; }
    if (_block_size != src_cfg.bufferSize()) {
      ConfigError err;
      err << "Can not configure FilterSource, block-size (=" << _block_size
          << ") != buffer-size (=" << src_cfg.bufferSize() << ")!";
      throw err;
    }
    // calc filter kernel
    _sample_rate = src_cfg.sampleRate();
    _updateFilter();
    // Resize buffer-set
    _buffers.resize(src_cfg.numBuffers());

    LogMessage msg(LOG_DEBUG);
    double fmin = std::max(_fmin, -src_cfg.sampleRate()/2);
    double fmax = std::min(_fmax, src_cfg.sampleRate()/2);
    msg << "Configured FFT Filter: " << std::endl
        << " range: [" << fmin << ", " << fmax << "]" << std::endl
        << " fft size: " << 2*_block_size <<  std::endl
        << " Fc/BW: " <<  fmin+(fmax-fmin)/2 << " / " << (fmax-fmin) << std::endl
        << " sample rate: " << src_cfg.sampleRate();
    Logger::get().log(msg);

    // Propergate config
    Source::setConfig(Config(Config::typeId< std::complex<Scalar> >(), src_cfg.sampleRate(),
                             _block_size, src_cfg.numBuffers()));
  }

  /** Performs the FFT filtering and back-transform. */
  virtual void process(const Buffer<std::complex<Scalar> > &buffer, bool allow_overwrite) {
    // Multiply with F(_kern)
    for (size_t i=0; i<(2*_block_size); i++) {
      _in_buffer[i] = buffer[i]*_kern[i];
    }
    // perform FFT
    _plan();
    // Get a output buffer
    Buffer< std::complex<Scalar> > out = _buffers.getBuffer();
    // Add first half of trafo buffer to second half of last trafo
    // and store second half of the current trafo
    for (size_t i=0; i<_block_size; i++) {
      out[i] = _last_trafo[i] + (_trafo_buffer[i]/((Scalar)(2*_block_size)));
      _last_trafo[i] = (_trafo_buffer[i+_block_size]/((Scalar)(2*_block_size)));
    }
    // Send output
    this->send(out);
  }


protected:
  /** Updates the sink-filter. */
  void _updateFilter() {
    // Calc filter kernel
    double Fs = _sample_rate;
    double fmin = std::max(_fmin, -Fs/2);
    double fmax = std::min(_fmax, Fs/2);
    double bw = fmax-fmin;
    double Fc = fmin + bw/2;
    for (size_t i=0; i<_block_size; i++) {
      // Eval kernel
      _kern[i] = sinc_flt_kernel<Scalar>(i, _block_size, Fc, bw, Fs);
      // set second half to 0
      _kern[i+_block_size] = 0;
    }
    // Calculate FFT in-place
    FFT::exec(_kern, FFT::FORWARD);
    // Normalize filter kernel
    _kern /= _kern.norm2();
  }

protected:
  /** Holds the block size of the filter. */
  size_t _block_size;
  /** The current sample-rate. */
  double _sample_rate;
  /** An input buffer. */
  Buffer< std::complex<Scalar> > _in_buffer;
  /** A compute buffer. */
  Buffer< std::complex<Scalar> > _trafo_buffer;
  /** Holds a copy of the second-half of the last output signal. */
  Buffer< std::complex<Scalar> > _last_trafo;
  /** Holds the current filter kernel. */
  Buffer< std::complex<Scalar> > _kern;
  /** The output buffers. */
  BufferSet< std::complex<Scalar> >_buffers;
  /** The lower frequency range. */
  double _fmin;
  /** The upper frequency range. */
  double _fmax;
  /** The FFT plan for the FFT back-transform. */
  FFTPlan<Scalar> _plan;
};


/** A FFT filter bank node wich consists of several filters.
 * @ingroup filters */
template <class Scalar>
class FilterNode
{
public:
  /** Constructor. */
  FilterNode(size_t block_size=1024)
    : _block_size(block_size), _buffer(0), _fft_fwd(0), _filters()
  {
    // Create input buffer node
    _buffer = new BufferNode< std::complex<Scalar> >(block_size);
    // Create FFT fwd transform
    _fft_fwd = new FilterSink<Scalar>(block_size);
    // Connect buffer source to FFT sink directly
    _buffer->connect(_fft_fwd, true);
  }

  /** Destructor. */
  virtual ~FilterNode() {
    delete _buffer; delete _fft_fwd;
    typename std::list< FilterSource<Scalar> *>::iterator item=_filters.begin();
    for (; item!=_filters.end(); item++) {
      delete *item;
    }
  }

  /** The filter sink. */
  Sink< std::complex<Scalar> > *sink() const {
    return _buffer;
  }

  /** Adds a filter to the filter bank. */
  FilterSource<Scalar> *addFilter(double fmin, double fmax) {
    // Check if fmin < fmax
    if (fmax < fmin) { std::swap(fmin, fmax); }
    // Create and store filter instance
    _filters.push_back(new FilterSource<Scalar>(_block_size, fmin, fmax));
    // Connect fft_fwd trafo to filter directly
    _fft_fwd->connect(_filters.back(), true);
    return _filters.back();
  }

protected:
  /** The block size of the filters. */
  size_t _block_size;
  /** The current sample rate. */
  double _sample_rate;
  /** The input buffer (to ensure buffers of _block_size size. */
  BufferNode< std::complex<Scalar> > *_buffer;
  /** The filter sink (forward FFT). */
  FilterSink<Scalar> *_fft_fwd;
  /** The filter bank. */
  std::list<FilterSource<Scalar> *> _filters;
};

}

#endif // __SDR_FILTERNODE_HH__

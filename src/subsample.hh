#ifndef __SDR_SUBSAMPLE_HH__
#define __SDR_SUBSAMPLE_HH__

#include "node.hh"
#include "buffer.hh"
#include "traits.hh"
#include "interpolate.hh"
#include "logger.hh"


namespace sdr {

/** Simple averaging sub-sampler. */
template <class Scalar>
class SubSample: public Sink<Scalar>, public Source
{
public:
  /** The super-scalar of the input type. */
  typedef typename Traits<Scalar>::SScalar SScalar;

public:
  /** Constructs a sub-sampler. */
  SubSample(size_t n)
    : Sink<Scalar>(), Source(),
      _n(n), _oFs(0), _last(0), _left(0), _buffer()
  {
    // pass...
  }

  /** Constructs a sub-sampler by target sample rate. */
  SubSample(double Fs)
    : Sink<Scalar>(), Source(),
      _n(1), _oFs(Fs), _last(0), _left(0), _buffer()
  {
    // pass...
  }

  /** Configures the sub-sampler. */
  virtual void config(const Config &src_cfg) {
    // Requires type and buffer size
    if (!src_cfg.hasType() || !src_cfg.hasBufferSize()) { return; }
    // check buffer type
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure SubSample node: Invalid buffer type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }

    // If target sample rate is specified, determine _n by _oFs
    if (_oFs > 0) {
      _n = std::max(1.0, src_cfg.sampleRate()/_oFs);
    }
    // Determine buffer size
    size_t out_size = src_cfg.bufferSize()/_n;
    if (src_cfg.bufferSize() % _n) { out_size += 1; }

    LogMessage msg(LOG_DEBUG);
    msg << "Configure SubSample node:" << std::endl
        << " by: " << _n << std::endl
        << " type: " << src_cfg.type() << std::endl
        << " sample-rate: " << src_cfg.sampleRate()
        << " -> " << src_cfg.sampleRate()/_n << std::endl
        << " buffer-size: " << src_cfg.bufferSize()
        << " -> " << out_size;
    Logger::get().log(msg);

    // Resize buffer
    _buffer = Buffer<Scalar>(out_size);
    // Propergate config
    this->setConfig(Config(src_cfg.type(), src_cfg.sampleRate()/_n, out_size, 1));
  }

  /** Performs the sub-sampling on the given buffer. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    if (allow_overwrite) {
      _process(buffer, buffer);
    } else if (_buffer.isUnused()) {
      _process(buffer, _buffer);
    } else {
#ifdef SDR_DEBUG
      LogMessage msg(LOG_WARNING);
      msg << "SubSample: Drop buffer, output buffer still in use.";
      Logger::get().log(msg);
#endif
    }
  }

protected:
  /** Performs the sub-sampling from @c in into @c out. */
  void _process(const Buffer<Scalar> &in, const Buffer<Scalar> &out) {
    size_t j=0;
    for (size_t i=0; i<in.size(); i++) {
      _last += in[i]; _left++;
      if (_n <= _left) {
        out[j] = _last/SScalar(_n); j++; _last=0; _left=0;
      }
    }
    this->send(out.head(j), true);
  }


protected:
  /** The sub-sampling, n=1 means no sub-sampling at all. */
  size_t _n;
  /** Target sample rate. */
  double _oFs;
  /** The last value. */
  SScalar _last;
  /** How many samples are left. */
  size_t _left;
  /** The output buffer, unused if the sub-sampling is performed in-place. */
  Buffer<Scalar> _buffer;
};


/** Implements a fractional sub-sampler. */
template <class Scalar>
class FracSubSampleBase
{
public:
  /** The input & output type super-scalar. */
  typedef typename Traits<Scalar>::SScalar SScalar;

public:
  /** Constructor.
   * @param frac Specifies the output sample rate relative to the input sample rate. I.e. 2 means
   *        half the input sample rate. */
  FracSubSampleBase(double frac)
    : _avg(0), _sample_count(0), _period(0) {
    if (frac < 1) {
      ConfigError err;
      err << "FracSubSampleBase: Can not sub-sample with fraction smaller one: " << frac;
      throw err;
    }
    _period = (frac*(1<<16));
  }

  /** Destructor. */
  virtual ~FracSubSampleBase() {
    // pass...
  }

  /** Resets the sample rate fraction. */
  inline void setFrac(double frac) {
    if (frac < 1) {
      ConfigError err;
      err << "FracSubSampleBase: Can not sub-sample with fraction smaller one: " << frac;
      throw err;
    }
    _period = (frac*(1<<16)); _sample_count=0; _avg = 0;
  }

  /** Returns the effective sub-sample fraction. */
  inline double frac() const {
    return double(_period)/(1<<16);
  }

  /** Reset sample counter. */
  inline void reset() {
    _avg=0; _sample_count=0;
  }

  /** Performs the sub-sampling. @c in and @c out may refer to the same buffer allowing for an
   * in-place operation. Returns a view on the output buffer containing the sub-samples. */
  inline Buffer<Scalar> subsample(const Buffer<Scalar> &in, const Buffer<Scalar> &out) {
    size_t oidx = 0;
    for (size_t i=0; i<in.size(); i++) {
      _avg += in[i]; _sample_count += (1<<16);
      if (_sample_count >= _period) {
        out[oidx] = _avg/SScalar(_sample_count/(1<<16));
        _sample_count=0; _avg = 0; oidx++;
      }
    }
    return out.head(oidx);
  }

protected:
  /** The average. */
  SScalar _avg;
  /** The number of samples collected times (1<<16). */
  size_t _sample_count;
  /** The sub-sample period. */
  size_t _period;
};


/** An interpolating sub-sampler. This node uses an 8-tap FIR filter to interpolate between
 * two values (given 8). Please do not use this node to subsample by a factor grater than 8,
 * as this may result into artifacts unless the signal was filtered accordingly before
 * subsampling it. */
template <class iScalar, class oScalar = iScalar>
class InpolSubSampler: public Sink<iScalar>, public Source
{
public:
  /** Constructor.
   * @param frac Specifies the sub-sampling fraction. I.e. frac=2 will result into half the input
   *        sample-rate. */
  InpolSubSampler(float frac)
    : Sink<iScalar>(), Source(), _frac(frac), _mu(0)
  {
    if (_frac <= 0) {
      ConfigError err;
      err << "Can not configure InpolSubSample node: Sample rate fraction must be > 0!"
          << " Fraction given: " << _frac;
      throw err;
    }
  }

  /** Destructor. */
  virtual ~InpolSubSampler() {
    // pass...
  }

  /** Configures the sub-sampling node. */
  virtual void config(const Config &src_cfg) {
    // Requires type and buffer size
    if (!src_cfg.hasType() || !src_cfg.hasBufferSize()) { return; }

    // check buffer type
    if (Config::typeId<iScalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure InpolSubSample node: Invalid buffer type " << src_cfg.type()
          << ", expected " << Config::typeId<iScalar>();
      throw err;
    }

    // Allocate buffer
    size_t bufSize = std::ceil(src_cfg.bufferSize() * src_cfg.sampleRate()/_frac);
    _buffer = Buffer<oScalar>(bufSize);

    // Allocate & init delay line
    _dl = Buffer<oScalar>(16); _dl_idx = 0;
    for (size_t i=0; i<16; i++) { _dl[i] = 0; }
    _mu = 0;

    // Propergate config
    this->setConfig(Config(Traits<oScalar>::scalarId, src_cfg.sampleRate()/_frac, bufSize, 1));
  }

  /** Performs the sub-sampling. */
  virtual void process(const Buffer<iScalar> &buffer, bool allow_overwrite) {
    size_t i=0, o=0;
    while (i<buffer.size()) {
      // Fill delay line
      // First, fill sampler...
      while ( (_mu > 1) && (i<buffer.size()) ) {
        _dl[_dl_idx] = _dl[_dl_idx+8] = buffer[i]; i++;
        _dl_idx = (_dl_idx + 1) % 8; _mu -= 1;
      }
      // Interpolate
      _buffer[o] = interpolate(_dl.sub(_dl_idx,8), _mu); _mu += _frac;
    }
    this->send(_buffer.head(o));
  }


protected:
  /** The sub-sampling fraction. */
  float _frac;
  /** The current (fractional) sample count. */
  float _mu;
  /** A delay-line (buffer) for the interpolation. */
  Buffer<oScalar> _dl;
  /** Index of the delay-line. */
  size_t _dl_idx;
  /** The output buffer. */
  Buffer<oScalar> _buffer;
};

}
#endif // __SDR_SUBSAMPLE_HH__

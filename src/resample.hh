#ifndef __SDR_RESAMPLE_HH__
#define __SDR_RESAMPLE_HH__

#include "config.hh"
#include "traits.hh"
#include "node.hh"


namespace sdr {

/** A linear interpolating resampling node. */
template<class Scalar>
class Resample: public Sink<Scalar>, public Source
{
public:
  /** The computation (super) scalar. */
  typedef typename Traits<Scalar>::SScalar SScalar;

public:
  /** Constructor with target sample-rate. */
  Resample(double sampleRate)
    : Sink<Scalar>(), Source(), _sample_rate(sampleRate), _last_value(0), _last_count(0),
      _incr((1<<_incr_mult))
  {
    // pass...
  }

  /** Configures the re-sampling node. */
  virtual void config(const Config &src_cfg) {
    // Requires type, sample rate and buffer size
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }
    // Check type
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not confiure Resample node: Invalid input type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }
    // Compute sub/super sampling periods
    _incr = size_t((1<<_incr_mult)*src_cfg.sampleRate()/_sample_rate);
    // Allocate buffer
    if (!_buffer.isEmpty()) { _buffer.unref(); }
    _buffer = Buffer<Scalar>(src_cfg.bufferSize());
    // Propergate config
    setConfig(Config(src_cfg.type(), _sample_rate, _buffer.size(), 1));
  }

  /** Performs the processing (actually dispatches the call to @c _process). */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    if (buffer.isEmpty()) { return; }
    else if (allow_overwrite && ((_incr>>_incr_mult)>0)) {
      // perform in-place if source allows it and we do a sub-sampling
      _process(buffer, buffer);
    } else if (_buffer.isUnused()) {
      _process(buffer, _buffer);
    } else {
#ifdef SDR_DEBUG
      LogMessage msg(LOG_WARNING);
      msg << "Resample: Drop buffer: Output buffer still in use.";
      Logger::get().log(msg);
#endif
    }
  }


protected:
  /** Performs the re-sampling. */
  inline void _process(const Buffer<Scalar> &in, const Buffer<Scalar> &out) {
    size_t j=0;
    // calculate indices to interpolate:
    int i1 = (_last_count>>_incr_mult), i2 = i1+1;
    int r  = _last_count - (i1<<_incr_mult);

    while (i2 < in.size() || ((0 == r) && (i1 < in.size())) ) {
      SScalar v1 = (i1 < 0) ?  _last_value : in[i1];
      // Linear interpolation
      out[j] = v1 + ( (0 == r) ? 0 : ((r*(in[i2]-v1))>>_incr_mult) ); j++;
      // Update tick count
      _last_count += _incr;
      // calculate indices to interpolate:
      i1 = (_last_count>>_incr_mult); i2 = i1+1;
      r  = _last_count - (i1<<_incr_mult);
    }

    _last_value = in[in.size()-1];
    // store offset as negative distance from last sample tick (_last_count-_incr)
    // and tick-index of the first sample of the next buffer:
    _last_count =
    send(out.head(j), true);
  }

protected:
  /** The output sample-rate. */
  double _sample_rate;
  /** The last value. */
  SScalar _last_value;
  /** The number of samples left from the last buffer. */
  int _last_count;
  /** Ration between input sample rate and output sample rate times 256. */
  int _incr;
  /** Buffer. */
  Buffer<Scalar> _buffer;

protected:
  /** Increment multiplier. This is used to avoid round-off errors in the integer arithmetic to
   * calculate the interpolation position. */
  static const size_t _incr_mult = 8;
};


}

#endif // __SDR_RESAMPLE_HH__

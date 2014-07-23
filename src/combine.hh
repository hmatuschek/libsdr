#ifndef __SDR_COMBINE_HH__
#define __SDR_COMBINE_HH__

#include "config.hh"
#include "traits.hh"
#include "node.hh"
#include "buffer.hh"
#include <limits>

namespace sdr {

template <class Scalar> class Combine;

/** A single sink of a Combine node. */
template <class Scalar>
class CombineSink: public Sink<Scalar>
{
public:
  /** Constructor. */
  CombineSink(Combine<Scalar> *combine, size_t index, RingBuffer<Scalar> &buffer)
    : Sink<Scalar>(), _index(index), _parent(combine), _buffer(buffer)
  {
    // Pass...
  }
  /** Destructor. */
  virtual ~CombineSink() {
    // pass...
  }

  /** Configures the sink. */
  virtual void config(const Config &src_cfg) {
    // Requires sample rate and type
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate()) { return; }
    // Check type
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure CombinSink: Invalid source type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }
    // Notify parent
    _parent->notifyConfig(_index, src_cfg);
  }

  /** Handles the given buffer. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    // Copy data into ring buffer and notify parent
    _buffer.put(buffer);
    _parent->notifyData(_index);
  }

protected:
  /** The index of the sink within the combine node. */
  size_t _index;
  /** A reference to the combine node. */
  Combine<Scalar> *_parent;
  /** The input ring-buffer. */
  RingBuffer<Scalar> &_buffer;
};


/** A combine node. This node allows to combine several streams into one. */
template <class Scalar>
class Combine
{
public:
  /** Constructor, @n N specifies the number of sinks. */
  Combine(size_t N) {
    _buffers.reserve(N); _sinks.reserve(N);
    for (size_t i=0; i<N; i++) {
      _buffers.push_back(RingBuffer<Scalar>());
      _sinks.push_back(new CombineSink<Scalar>(this, i, _buffers.back()));
    }
  }

  /** Destructor. */
  virtual ~Combine() {
    // Unref all buffers and free sinks
    for (size_t i=0; i<_sinks.size(); i++) {
      _buffers[i].unref(); delete _sinks[i];
    }
    _buffers.clear(); _sinks.clear();
  }

  /** Needs to be overridden. */
  virtual void config(const Config &cfg) = 0;
  /** Needs to be overridden. */
  virtual void process(std::vector< RingBuffer<Scalar> > &buffers, size_t N) = 0;


protected:
  /** Unifies the configuration of all sinks. */
  void notifyConfig(size_t idx, const Config &cfg)
  {
    // Requires type, sampleRate and buffer size
    if (!cfg.hasType() || !cfg.hasSampleRate() || !cfg.hasBufferSize()) { return; }
    // check or unify type
    if (!_config.hasType()) { _config.setType(cfg.type()); }
    else if (_config.type() != cfg.type()) {
      ConfigError err;
      err << "Can not configure Combine node: Invalid type of sink #" << idx
          << " " << cfg.type() << ", expected " << _config.type();
      throw err;
    }
    // Check sample rate
    if (!_config.hasSampleRate()) { _config.setSampleRate(cfg.sampleRate()); }
    else if (_config.sampleRate() != cfg.sampleRate()) {
      ConfigError err;
      err << "Can ont configure Combine node: Invalid sample rate of sink #" << idx
          << " " << cfg.sampleRate() << ", expected " << _config.sampleRate();
      throw err;
    }
    // Determine max buffer size
    if (!_config.hasBufferSize()) { _config.setBufferSize(cfg.bufferSize()); }
    else {
      // Take maximum:
      _config.setBufferSize(std::max(_config.bufferSize(), cfg.bufferSize()));
    }
    // Reallocate buffers
    for (size_t i=0; i<_sinks.size(); i++) {
      _buffers[i] = RingBuffer<Scalar>(_config.bufferSize());
    }
    // Propergate config
    this->config(_config);
  }

  /** Determines the minimum amount of data that is available on all ring buffers. */
  void notifyData(size_t idx) {
    // Determine minimum size of available data
    size_t N = std::numeric_limits<size_t>::max();
    for (size_t i=0; i<_sinks.size(); i++) {
      N = std::min(N, _buffers[i].stored());
    }
    if (N > 0) { this->process(_buffers, N); }
  }

protected:
  /** The ring buffers of all combine sinks. */
  std::vector< RingBuffer<Scalar> > _buffers;
  /** The combine sinks. */
  std::vector< CombineSink<Scalar> *> _sinks;
  /** The output configuration. */
  Config _config;

  friend class CombineSink<Scalar>;
};


/** Interleaves several input streams. */
template <class Scalar>
class Interleave : public Combine<Scalar>, public Source
{
public:
  /** Constructor. */
  Interleave(size_t N)
    : Combine<Scalar>(N), Source(), _N(N), _buffer()
  {
    // pass...
  }

  /** Retunrs the i-th sink. */
  Sink<Scalar> *sink(size_t i) {
    if (i >= _N) {
      RuntimeError err;
      err << "Interleave: Sink index " << i << " out of range [0," << _N << ")";
      throw err;
    }
    return Combine<Scalar>::_sinks[i];
  }

  /** Configures the interleave node. */
  virtual void config(const Config &cfg) {
    //Requres type & buffer size
    if (!cfg.hasType() || !cfg.hasBufferSize()) { return; }
    // Check type
    if (Config::typeId<Scalar>() != cfg.type()) {
      ConfigError err;
      err << "Can not configure Interleave node: Invalid source type " << cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }
    // Allocate buffer:
    _buffer = Buffer<Scalar>(_N*cfg.bufferSize());
    // Propergate config
    this->setConfig(Config(Config::typeId<Scalar>(), cfg.sampleRate(), _buffer.size(), 1));
  }

  /** Processes the data from all sinks. */
  virtual void process(std::vector<RingBuffer<Scalar> > &buffers, size_t N) {
    if (0 == N) { return; }
    if (! _buffer.isUnused()) {
#ifdef SDR_DEBUG
      LogMessage msg(LOG_WARNING);
      msg << "Interleave: Output buffer in use: Drop " << _N << "x" << N
          << " input values";
      Logger::get().log(msg);
#endif
      for (size_t i=0; i<buffers.size(); i++) { buffers[i].drop(N); }
      return;
    }
    size_t num = std::min(_buffer.size()/_N,N);
    // Interleave data
    size_t idx = 0;
    for (size_t i=0; i<num; i++) {
      for (size_t j=0; j<_N; j++, idx++) {
        _buffer[idx] = buffers[j][i];
      }
    }
    // Drop num elements from all ring buffers
    for (size_t i=0; i<_N; i++) {
      buffers[i].drop(num);
    }
    // Send buffer
    this->send(_buffer.head(num*_N));
  }

protected:
  /** The number of sinks. */
  size_t _N;
  /** The putput buffer. */
  Buffer<Scalar> _buffer;
};

}
#endif // __SDR_COMBINE_HH__

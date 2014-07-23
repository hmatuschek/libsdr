#ifndef __SDR_NODE_HH__
#define __SDR_NODE_HH__

#include <set>
#include <list>
#include <iostream>
#include <complex>
#include <stdint.h>
#include <pthread.h>

#include "buffer.hh"
#include "queue.hh"
#include "exception.hh"

namespace sdr {

/** A collection of configuration information that is send by a source to all connected sinks
 * to propergate and check the configuration of the processing network. */
class Config
{
public:
  /** The type IDs. */
  typedef enum {
    Type_UNDEFINED = 0,
    Type_u8,   ///< Real unsigned 8b ints.
    Type_s8,   ///< Real signed 8b ints.
    Type_u16,  ///< Real unsigned 16b ints.
    Type_s16,  ///< Real signed 16b ints.
    Type_f32,  ///< Real 32b floats aka. "float".
    Type_f64,  ///< Real 64b floats aka. "double".
    Type_cu8,  ///< Complex (aka I/Q) type of unsigned 8b ints.
    Type_cs8,  ///< Complex (aka I/Q) type of signed 8b ints.
    Type_cu16, ///< Complex (aka I/Q) type of unsigned 16b ints.
    Type_cs16, ///< Complex (aka I/Q) type of signed 16b ints.
    Type_cf32, ///< Complex (aka I/Q) type of 32bit floats aka. std::complex<float>.
    Type_cf64  ///< Complex (aka I/Q) type of 64bit floats aka. std::complex<double>.
  } Type;

public:
  /** Empty constructor, will result into an invalid configuration. */
  Config();
  /** Constructor. */
  Config(Type type, double sampleRate, size_t bufferSize, size_t numBuffers);
  /** Copy constructor. */
  Config(const Config &other);

  /** Assignment operator. */
  const Config &operator= (const Config &other);
  /** Coparison operator. */
  bool operator== (const Config &other) const;

  /** If true, the configuration has a type. */
  inline bool hasType() const { return Type_UNDEFINED != _type; }
  /** Returns the type. */
  inline Type type() const { return _type; }
  /** Sets the type. */
  inline void setType(Type type) { _type = type; }
  /** If true, the configuration has a sample rate. */
  inline bool hasSampleRate() const { return 0 != _sampleRate; }
  /** Returns the sample rate. */
  inline double sampleRate() const { return _sampleRate; }
  /** Sets the sample rate. */
  inline void setSampleRate(double rate) { _sampleRate = rate; }
  /** If true, the configuration has a buffer size. */
  inline bool hasBufferSize() const { return 0 != _bufferSize; }
  /** Returns the max. buffer size. */
  inline size_t bufferSize() const { return _bufferSize; }
  /** Sets the max. buffer size. */
  inline void setBufferSize(size_t size) { _bufferSize = size; }
  /** If true, the configuration has a number of buffers. */
  inline bool hasNumBuffers() const { return 0 != _numBuffers; }
  /** Returns the max. number of buffers. */
  inline size_t numBuffers() const { return _numBuffers; }
  /** Sets the max. number of buffers. */
  inline void setNumBuffers(size_t N) { _numBuffers = N; }

  /** Returns the type-id of the template type. */
  template <typename T> static inline Type typeId();

protected:
  /** Holds the type of the source. */
  Type   _type;
  /** Holds the sample rate of the source. */
  double _sampleRate;
  /** Holds the max. buffer size of the source. */
  size_t _bufferSize;
  /** Holds the max. number of buffers of the source. */
  size_t _numBuffers;
};

/** Type-id for @c uint8. */
template <>
inline Config::Type Config::typeId<uint8_t>() { return Type_u8; }
/** Type-id for @c int8. */
template <>
inline Config::Type Config::typeId<int8_t>() { return Type_s8; }
/** Type-id for @c uint16. */
template <>
inline Config::Type Config::typeId<uint16_t>() { return Type_u16; }
/** Type-id for @c int16. */
template <>
inline Config::Type Config::typeId<int16_t>() { return Type_s16; }
/** Type-id for @c float. */
template <>
inline Config::Type Config::typeId<float>() { return Type_f32; }
/** Type-id for @c double. */
template <>
inline Config::Type Config::typeId<double>() { return Type_f64; }
/** Type-id for @c std::complex<uint8>. */
template <>
inline Config::Type Config::typeId< std::complex<uint8_t> >() { return Type_cu8; }
/** Type-id for @c std::complex<int8>. */
template <>
inline Config::Type Config::typeId< std::complex<int8_t> >() { return Type_cs8; }
/** Type-id for @c std::complex<uint16>. */
template <>
inline Config::Type Config::typeId< std::complex<uint16_t> >() { return Type_cu16; }
/** Type-id for @c std::complex<int16>. */
template <>
inline Config::Type Config::typeId< std::complex<int16_t> >() { return Type_cs16; }
/** Type-id for @c std::complex<float>. */
template <>
inline Config::Type Config::typeId< std::complex<float> >() { return Type_cf32; }
/** Type-id for @c std::complex<double>. */
template <>
inline Config::Type Config::typeId< std::complex<double> >() { return Type_cf64; }


/** Convert type constant into type name. */
inline const char *typeName(Config::Type type) {
  switch (type) {
  case Config::Type_UNDEFINED: return "UNDEFINED";
  case Config::Type_u8: return "uint8";
  case Config::Type_s8: return "int8";
  case Config::Type_u16: return "uint16";
  case Config::Type_s16: return "int16";
  case Config::Type_f32: return "float";
  case Config::Type_f64: return "double";
  case Config::Type_cu8: return "complex uint8";
  case Config::Type_cs8: return "complex int8";
  case Config::Type_cu16: return "complex uint16";
  case Config::Type_cs16: return "complex int16";
  case Config::Type_cf32: return "complex float";
  case Config::Type_cf64: return "complex double";
  }
  return "unknown";
}

/** Printing type constants. */
inline std::ostream &operator<<(std::ostream &stream, Config::Type type) {
  stream << typeName(type) << " (" << (int)type << ")";
  return stream;
}


/** Basic interface of all Sinks. Usually, sinks are derived from
 * the @c Sink template. */
class SinkBase
{
public:
  /** Constructor. */
  SinkBase();
  /** Destructor. */
  virtual ~SinkBase();

  /** Needs to be implemented by any sub-type to process the received data. */
  virtual void handleBuffer(const RawBuffer &buffer, bool allow_overwrite) = 0;
  /** Needs to be implemented by any sub-type to check and perform the configuration of the node. */
  virtual void config(const Config &src_cfg) = 0;
};



/** Typed sink. */
template <class Scalar>
class Sink: public SinkBase
{
public:
  /** Constructor. */
  Sink() : SinkBase() { }
  /** Drestructor. */
  virtual ~Sink() { }

  /** Needs to be implemented by any sub-type to process the received data. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) = 0;

  /** Re-implemented from @c SinkBase. Casts the buffer into the requested type
   * and forwards the call to @c process. */
  virtual void handleBuffer(const RawBuffer &buffer, bool allow_overwrite) {
    this->process(Buffer<Scalar>(buffer), allow_overwrite);
  }
};



/** Generic source class. */
class Source
{
public:
  /** Constructor. */
  Source();
  /** Destructor. */
  virtual ~Source();

  /** Sends the given buffer to all connected sinks. */
  virtual void send(const RawBuffer &buffer, bool allow_overwrite=false);

  /** Connect this source to a sink. */
  void connect(SinkBase *sink, bool direct=false);
  /** Disconnect a sink again. */
  void disconnect(SinkBase *sink);

  /** Stores the configuration and propergates it if
   * the configuration has been changed. */
  virtual void setConfig(const Config &config);

  /** Returns the configured sample rate or @c 0 otherwise. */
  virtual double sampleRate() const;
  /** Returns the configured source type or @c Config::Type_UNDEFINED otherwise. */
  virtual Config::Type type() const;

  /** Adds a callback to the end-of-stream signal of the source. */
  template <class T>
  void addEOS(T* instance, void (T::*function)()) {
    _eos.push_back(new Delegate<T>(instance, function));
  }

protected:
  /** Signals the EOS. */
  void signalEOS();

  /** Propagates the given configuration to all connected sinks. */
  void propagateConfig(const Config &config);

protected:
  /** Holds the source configuration, this can be updated by calling @c setConfig. */
  Config _config;
  /** The connected sinks. */
  std::map<SinkBase *, bool> _sinks;
  /** The connected EOS singal handlers. */
  std::list<DelegateInterface *> _eos;
};



/** Iterface of a blocking source. Blocking sources are usually input sources that wait for data
 * from a device or file.
 * Please note, a proper input source that reads data from a file should emmit the
 * eos signal using the @c signalEOS method. This can be used to stop the @c Queue loop
 * once the input can not provide data anymore. */
class BlockingSource: public Source {
public:
  /** Constructor.
   * @param parallel Specifies whether the source is waiting in a separate thread for new data.
   * @param connect_idle Specifies wheter the input source @c next() method should be connected
   *        to the idle signal of the @c Queue.
   * @param stop_queue_on_eos Signals the that the Queue should be stopped once the
   *        EOS is reached. Use this flag only if this source is the only source in the
   *        data stream. */
  BlockingSource(bool parallel=false, bool connect_idle=true, bool stop_queue_on_eos=false);
  /** Destructor. */
  virtual ~BlockingSource();

  /** This method gets called either by the @c Queue on idle events or by a thread to read more
   * data from the input stream. The @c next function should be blocking in order to avoid busy
   * waiting on incomming data. */
  virtual void next() = 0;

  /** Returns true if the source is active. */
  inline bool isActive() const { return _is_active; }

  /** This function starts the input stream. */
  virtual void start();
  /** This function stops the input stream. */
  virtual void stop();

protected:
  /** The parallel main loop. */
  void _parallel_main();
  /** The non-virtual idle callback. */
  void _nonvirt_idle_cb();

protected:
  /** If true, the source is active. */
  bool _is_active;
  /** If true, the surce is processed in parallel. */
  bool _is_parallel;
  /** The thread of the source. */
  pthread_t _thread;

private:
  /** Wrapper for the pthread library. */
  static void *_pthread_main_wrapper(void *);
};


/** A NOP node. */
class Proxy: public SinkBase, public Source
{
public:
  /** Constructor. */
  Proxy();
  /** Destructor. */
  virtual ~Proxy();

  /** Configures the node. */
  virtual void config(const Config &src_cfg);
  /** Forwards the buffer. */
  virtual void handleBuffer(const RawBuffer &buffer, bool allow_overwrite);
};


}

#endif // __SDR_NODE_HH__

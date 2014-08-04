#ifndef __SDR_UTILS_HH__
#define __SDR_UTILS_HH__

#include "config.hh"
#include "node.hh"
#include "traits.hh"
#include "operators.hh"
#include "logger.hh"
#include <ctime>


namespace sdr {

/** Extracts the real or imaginary part of a complex valued data stream. */
template <class Scalar>
class RealImagPart: public Sink< std::complex<Scalar> >, public Source
{
public:
  /** Constructor. If @c select_real is @c true, the real part is selected, if @c select_real is
   * @c false, the imaginary part is selected. */
  RealImagPart(bool select_real, double scale=1.0)
    : Sink< std::complex<Scalar> >(), Source(), _buffer(), _select_real(select_real), _scale(scale)
  {
    // pass...
  }

  /** Configures the node. */
  virtual void config(const Config &src_cfg) {
    // Needs full config
    if ((Config::Type_UNDEFINED==src_cfg.type()) || (0 == src_cfg.bufferSize())) { return; }
    // Assert complex type
    if (src_cfg.type() != Config::typeId< std::complex<Scalar> >()) {
      ConfigError err;
      err << "Can not configure sink of RealPart: Invalid buffer type " << src_cfg.type()
          << " expected " << Config::typeId< std::complex<Scalar> >();
      throw err;
    }
    // Resize buffer
    _buffer = Buffer<Scalar>(src_cfg.bufferSize());
    // propergate config
    this->setConfig(Config(Config::typeId< Scalar >(), src_cfg.sampleRate(),
                           src_cfg.bufferSize(), 1));

    if (_select_real) {
      LogMessage msg(LOG_DEBUG);
      msg << "Configured RealPart node:" << std::endl
          << " type: " << Config::typeId<Scalar>() << std::endl
          << " sample-rate: " << src_cfg.sampleRate() << std::endl
          << " buffer-size: " << src_cfg.bufferSize();
      Logger::get().log(msg);
    }
  }

  /** Processes the incomming data. */
  virtual void process(const Buffer<std::complex<Scalar> > &buffer, bool allow_overwrite) {
    // Convert
    if (_select_real) {
      for (size_t i=0; i<buffer.size(); i++) {
        _buffer[i] = _scale*buffer[i].real();
      }
    } else {
      for (size_t i=0; i<buffer.size(); i++) {
        _buffer[i] = _scale*buffer[i].imag();
      }
    }
    this->send(_buffer);
  }

protected:
  /** The output buffer. */
  Buffer<Scalar> _buffer;
  /** Real/Imag selection. */
  bool _select_real;
  /** The scale. */
  double _scale;
};


/** Selects the real part of a complex signal. */
template <class Scalar>
class RealPart: public RealImagPart<Scalar>
{
public:
  /** Constructor. */
  RealPart(double scale=1.0)
    : RealImagPart<Scalar>(true, scale)
  {
    // pass...
  }
};


/** Selects the imaginary part of a complex signal. */
template <class Scalar>
class ImagPart: public RealImagPart<Scalar>
{
public:
  /** Constructor. */
  ImagPart(double scale=1.0)
    : RealImagPart<Scalar>(false, scale)
  {
    // pass...
  }
};



/** Tiny helper node to transform a real part into a complex, including
 * a possible type-cast*/
template <class iScalar, class oScalar=iScalar>
class ToComplex: public Sink<iScalar>, public Source
{
public:
  /** Constructor. */
  ToComplex(double scale=1.0)
    : Sink<iScalar>(), Source(), _scale(scale)
  {
    // pass...
  }

  /** Configures the node. */
  virtual void config(const Config &src_cfg) {
    // Requires at least type & buffer size
    if (!src_cfg.hasType() || !src_cfg.hasBufferSize()) { return; }
    // Check input type
    if (Config::typeId<iScalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure ToComplex node: Invalid buffer type " << src_cfg.type()
          << ", expected " << Config::typeId<iScalar>();
      throw err;
    }
    // Allocate buffer:
    _buffer = Buffer< std::complex<oScalar> >(src_cfg.bufferSize());
    // Propergate config
    this->setConfig(
          Config(Config::typeId< std::complex<oScalar> >(),
                 src_cfg.sampleRate(), src_cfg.bufferSize(), src_cfg.numBuffers()));
  }

  /** Casts the input real buffer into the complex output buffer. */
  virtual void process(const Buffer<iScalar> &buffer, bool allow_overwrite) {
    if (1.0 == _scale) {
      for (size_t i=0; i<buffer.size(); i++) {
        _buffer[i] = std::complex<oScalar>(oScalar(buffer[i]));
      }
    } else  {
      for (size_t i=0; i<buffer.size(); i++) {
          _buffer[i] = std::complex<oScalar>(_scale*oScalar(buffer[i]));
      }
    }
    // propergate buffer
    this->send(_buffer.head(buffer.size()));
  }

protected:
  /** The scale. */
  double _scale;
  /** The output buffer. */
  Buffer< std::complex<oScalar> > _buffer;
};



/** Explicit type cast node. */
template <class iScalar, class oScalar>
class Cast : public Sink<iScalar>, public Source
{
public:
  /** Specifies the input super scalar. */
  typedef typename Traits<iScalar>::SScalar iSScalar;
  /** Specified the output super scalar. */
  typedef typename Traits<oScalar>::SScalar oSScalar;

public:
  /** Constructs a type-cast with optional scaleing. */
  Cast(oScalar scale=1, iScalar shift=0)
    : Sink<iScalar>(), Source(), _can_overwrite(false), _do_scale(false),
      _scale(scale), _shift(shift), _buffer()
  {
    // pass...
  }

  /** Configures the type-cast node. */
  virtual void config(const Config &src_cfg) {
    // only check type
    if (!src_cfg.hasType()) { return; }
    if (src_cfg.type() != Config::typeId<iScalar>()) {
      ConfigError err;
      err << "Can not configure Cast: Invalid input type " << src_cfg.type()
          << ", expected " << Config::typeId<iScalar>();
      throw err;
    }

    // unreference buffer if non-empty
    if (! _buffer.isEmpty()) { _buffer.unref(); }

    // allocate buffer
    _buffer = Buffer<oScalar>(src_cfg.bufferSize());

    LogMessage msg(LOG_DEBUG);
    msg << "Configure Cast node:" << std::endl
        << " conversion: "<< Config::typeId<iScalar>()
        << " -> " << Config::typeId<oScalar>() << std::endl
        << " in-place " << (_can_overwrite ? "true" : "false") << std::endl
        << " scale: " << _scale;
    Logger::get().log(msg);

    _can_overwrite = sizeof(iScalar) >= sizeof(oScalar);
    _do_scale = (_scale != oScalar(0));

    // forward config
    this->setConfig(Config(Config::typeId<oScalar>(), src_cfg.sampleRate(),
                           src_cfg.bufferSize(), 1));
  }

  /** Performs the type-cast node. */
  virtual void process(const Buffer<iScalar> &buffer, bool allow_overwrite) {
    if (allow_overwrite && _can_overwrite) {
      _process(buffer, Buffer<oScalar>(buffer));
    } else if (_buffer.isUnused()) {
      _process(buffer, _buffer);
    } else {
#ifdef SDR_DEBUG
      std::cerr << "Cast: Drop buffer: Output buffer is still in use by "
                << _buffer.refCount() << std::endl;
#endif
    }
  }

  /** Returns the scaling. */
  inline double scale() const { return _scale; }

  /** Sets the scaling. */
  void setScale(double scale) {
    _scale = scale;
    _do_scale = (0 != _scale);
  }

protected:
  /** Internal used method to perform the type-case out-of-place. */
  inline void _process(const Buffer<iScalar> &in, const Buffer<oScalar> &out) {
    if (_do_scale) {
      for (size_t i=0; i<in.size(); i++) {
        out[i] = _scale*( cast<iScalar,oScalar>(in[i]) + cast<iScalar, oScalar>(_shift) );
      }
    } else {
      for (size_t i=0; i<in.size(); i++) {
        out[i] = in[i]+_shift;
      }
    }
    this->send(out.head(in.size()));
  }

protected:
  /** If true, the type-cast (an scaleing) can be performed in-place. */
  bool _can_overwrite;
  /** If true, the output gets scaled. */
  bool _do_scale;
  /** The scaling. */
  oScalar _scale;
  /** Another scaling, using integer shift operation (faster). */
  iScalar _shift;
  /** The output buffer, unused if the type-cast is performed in-place . */
  Buffer<oScalar> _buffer;
};



/** Performs a reinterprete cast from an unsinged value to a singed one. */
class UnsignedToSigned: public SinkBase, public Source
{
public:
  /** Constructor with optional scaleing. */
  UnsignedToSigned(float scale=1.0);
  /** Destructor. */
  virtual ~UnsignedToSigned();

  /** Configures the cast node. */
  virtual void config(const Config &src_cfg);
  /** Performs the cast. */
  virtual void handleBuffer(const RawBuffer &buffer, bool allow_overwrite);

protected:
  /** Performs the cast for @c uint8 -> @c int8. */
  void _process_int8(const RawBuffer &in, const RawBuffer &out);
  /** Performs the cast for @c uint16 -> @c int16. */
  void _process_int16(const RawBuffer &in, const RawBuffer &out);

protected:
  /** Type-cast callback. */
  void (UnsignedToSigned::*_process)(const RawBuffer &in, const RawBuffer &out);
  /** The output buffer, unused if the cast can be performed in-place. */
  RawBuffer _buffer;
  /** Holds the scaleing. */
  float _scale;
};



/** Performs a reinterprete cast from an unsinged value to a singed one. */
class SignedToUnsigned: public SinkBase, public Source
{
public:
  /** Constructor with optional scaleing. */
  SignedToUnsigned();
  /** Destructor. */
  virtual ~SignedToUnsigned();

  /** Configures the cast node. */
  virtual void config(const Config &src_cfg);
  /** Performs the cast. */
  virtual void handleBuffer(const RawBuffer &buffer, bool allow_overwrite);

protected:
  /** Performs the int8 -> uint8 cast. */
  void _process_int8(const RawBuffer &in, const RawBuffer &out);
  /** Performs the int16 -> uint16 cast. */
  void _process_int16(const RawBuffer &in, const RawBuffer &out);

protected:
  /** Type-cast callback. */
  void (SignedToUnsigned::*_process)(const RawBuffer &in, const RawBuffer &out);
  /** The output buffer, unused if the cast is performed in-place. */
  RawBuffer _buffer;
};


/** Performs a frequency shift on a complex input signal, by multiplying it with \f$e^{i\omega t}\f$.
 * Please note, this node performs not optimal in cases, where the input scalar is an integer, as
 * the computation is performed using double precision floating point numbers.
 * @deprecated Implement a more efficient variant using FreqShiftBase. */
template <class Scalar>
class FreqShift: public Sink< std::complex<Scalar> >, public Source
{
public:
  /** Constructs a frequency shift node with optional scaleing of the result. */
  FreqShift(double shift, Scalar scale=1.0)
    : Sink< std::complex<Scalar> >(), Source(),
      _shift(shift), _scale(scale), _factor(1), _sample_rate(0), _delta(1)
  {
    // pass...
  }

  /** Destructor. */
  virtual ~FreqShift() {
    // pass...
  }

  /** Returns the frequency shift. */
  inline double shift() const { return _shift; }

  /** Sets the frequency shift. */
  void setShift(double shift) {
    // Update delta
    _shift = shift;
    _delta  = exp(std::complex<double>(0,2*M_PI*_shift/_sample_rate));
  }

  /** Configures the frequency shift node. */
  virtual void config(const Config &src_cfg) {
    // Requires type, samplerate & buffersize
    if ((Config::Type_UNDEFINED==src_cfg.type()) || (0==src_cfg.sampleRate()) || (0==src_cfg.bufferSize())) { return; }
    // Assert type
    if (src_cfg.type() != Config::typeId< std::complex<Scalar> >()) {
      ConfigError err;
      err << "Can not configure FreqShift node: Invalid source type " << src_cfg.type()
          << ", expected " << Config::typeId< std::complex<Scalar> >();
      throw err;
    }
    // Allocate buffer
    _buffer = Buffer< std::complex<Scalar> >(src_cfg.bufferSize());
    // Store sample rate
    _sample_rate = src_cfg.sampleRate();
    // Precalc delta
    _delta  = exp(std::complex<double>(0,2*M_PI*_shift/_sample_rate));
    // reset factor
    _factor = 1;

    LogMessage msg(LOG_DEBUG);
    msg << "Configure FreqShift node:" << std::endl
        << " shift: " << _shift << std::endl
        << " scale: " << _scale << std::endl
        << " sample-rate: " << src_cfg.sampleRate() << std::endl
        << " buffer-suize: " << src_cfg.bufferSize();
    Logger::get().log(msg);

    // Propergate config
    this->setConfig(Config(Config::typeId< std::complex<Scalar> >(), src_cfg.sampleRate(),
                           src_cfg.bufferSize(), 1));
  }

  /** Performs the frequency shift. */
  virtual void process(const Buffer<std::complex<Scalar> > &buffer, bool allow_overwrite) {
    // Shift freq:
    for (size_t i=0; i<buffer.size(); i++) {
      _buffer[i] = (double(_scale)*_factor)*buffer[i]; _factor *= _delta;
    }
    // Send buffer
    this->send(_buffer);
  }

protected:
  /** The output buffer. */
  Buffer< std::complex<Scalar> > _buffer;
  /** The frequency shift \f$f\f$ in Hz (\f$\omega=2\pi f\f$). */
  double _shift;
  /** The optional scale. */
  Scalar _scale;
  /** The current exponental factor, gets updated for every sample. */
  std::complex<double> _factor;
  /** The current sample rate. */
  double _sample_rate;
  /** \f$\exp(i\omega t)\f$. */
  std::complex<double> _delta;
};


/** Reads raw samples from an imput stream, (ie a file). */
template<class Scalar>
class StreamSource: public Source
{
public:
  /** Constructs a raw input source. */
  StreamSource(std::istream &stream, double sample_rate=1, size_t buffersize=1024)
    : Source(), _stream(stream), _buffer(buffersize)
  {
    // Assemble config for stream:
    this->_config = Config(Config::typeId<Scalar>(), sample_rate, buffersize, 1);
  }

  /** Reads the next chunk. This function might be connected to the idle event of the Queue, then
   * a new chunk gets read once all processing has been performed. */
  void next() {
    if(_stream.eof()) { Queue::get().stop(); return; }
    int len = _stream.read(_buffer.ptr(), _buffer.storageSize());
    if (len > 0) {
      this->send(_buffer.head(len/sizeof(Scalar)));
    }
  }

protected:
  /** The input stream. */
  std::istream &_stream;
  /** The output buffer. */
  Buffer<Scalar> _buffer;
};


/** Serializes the incomming buffers as raw data. */
template <class Scalar>
class StreamSink: public Sink<Scalar>
{
public:
  /** Constructor. */
  StreamSink(std::ostream &stream)
    : Sink<Scalar>(), _stream(stream)
  {
    // pass...
  }

  /** Configures the raw sink. */
  virtual void config(const Config &src_cfg) {
    if (Config::Type_UNDEFINED == src_cfg.type()) { return; }
    if (src_cfg.type() != Config::typeId<Scalar>()) {
      ConfigError err;
      err << "Can not configure StreamSink: Invalid buffer type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }
    // done...
  }

  /** Dumps the buffer into the stream as raw data. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    /// @bug Check if buffer was send completely:
    _stream.write(buffer.data(), buffer.size()*sizeof(Scalar));
  }

protected:
  /** The output stream. */
  std::ostream &_stream;
};




/** Simple scaling node. */
template <class Scalar>
class Scale : public Sink<Scalar>, public Source
{
public:
  /** Constructs the scaling node. */
  Scale(float scale=1, Scalar shift=0)
    : Sink<Scalar>(), Source(), _scale(scale), _shift(shift)
  {
    // pass...
  }

  /** Destructor. */
  virtual ~Scale() {
    // pass...
  }

  /** Configures the scaleing node. */
  virtual void config(const Config &src_cfg) {
    // Check for type & buffer size
    if (!src_cfg.hasType() || !src_cfg.bufferSize()) { return; }
    // Check type
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure Scale node: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }
    // Allocate buffer
    _buffer = Buffer<Scalar>(src_cfg.bufferSize());
    // Done, propergate config
    this->setConfig(src_cfg);
  }

  /** Performs the scaleing. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    if ((1 == _scale) && (Scalar(0) == _shift)) {
      this->send(buffer, allow_overwrite);
    } else if (allow_overwrite) {
      // Scale inplace
      for (size_t i=0; i<buffer.size(); i++) { buffer[i] *= _scale; }
      this->send(buffer, allow_overwrite);
    } else if (_buffer.isUnused()) {
      // Scale out-of-place
      for (size_t i=0; i<buffer.size(); i++) { _buffer[i] = _scale*_buffer[i]; }
      this->send(_buffer.head(buffer.size()), true);
    }
    // else if buffer is still in use -> drop buffer...
  }

protected:
  /** The output buffer, unused if the scaling is performed in-place. */
  Buffer<Scalar> _buffer;
  /** The scaling. */
  float _scale;
  /** Alternative formulation for the scaling, using integer shift operators. */
  Scalar _shift;
};



/** An automatic gain control node. */
template <class Scalar>
class AGC: public Sink<Scalar>, public Source
{
public:
  /** Constructor. */
  AGC(double tau=0.1, double target=0)
    : Sink<Scalar>(), Source(), _enabled(true), _tau(tau), _lambda(0), _sd(0),
      _target(target), _gain(1), _sample_rate(0)
  {
    if (0 == target) {
      // Determine target by scalar type
      switch (Config::typeId<Scalar>()) {
      case Config::Type_u8:
      case Config::Type_s8:
      case Config::Type_cu8:
      case Config::Type_cs8:
        _target = 127; break;
      case Config::Type_u16:
      case Config::Type_s16:
      case Config::Type_cu16:
      case Config::Type_cs16:
        _target = 32000; break;
      case Config::Type_f32:
      case Config::Type_f64:
      case Config::Type_cf32:
      case Config::Type_cf64:
        _target = 1.; break;
      case Config::Type_UNDEFINED: {
        ConfigError err; err << "Can not configure AGC node: Unsupported type."; throw err;
      }
      }
    }
    _sd = _target;
  }

  /** Returns true, if the AGC is enabled. */
  inline bool enabled() const {
    return _enabled;
  }

  /** Enable/disable the AGC node. */
  inline void enable(bool enabled) {
    _enabled = enabled;
  }

  /** Returns the current gain factor. */
  inline float gain() const {
    return _gain;
  }

  /** Resets the current gain factor. */
  inline void setGain(float gain) {
    _gain = gain;
  }

  /** Returns the time-constant of the AGC. */
  inline double tau() const {
    return _tau;
  }

  /** Sets the time-constant of the AGC. */
  inline void setTau(double tau) {
    // calc decay factor
    _tau = tau;
    _lambda = exp(-1./(_tau*_sample_rate));
  }

  /** Configures the AGC node. */
  virtual void config(const Config &src_cfg) {
    // Need sample rate & type
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure AGC node: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }

    // calc decay factor
    _sample_rate = src_cfg.sampleRate();
    _lambda = exp(-1./(_tau*src_cfg.sampleRate()));
    // reset variance
    _sd = _target;

    // Allocate buffer
    _buffer = Buffer<Scalar>(src_cfg.bufferSize());

    LogMessage msg(LOG_DEBUG);
    msg << "Configured AGC:" << std::endl
        << " type: " << src_cfg.type() << std::endl
        << " sample-rate: " << src_cfg.sampleRate() << std::endl
        << " tau: " << _tau << std::endl
        << " lambda [1/s]: " << pow(_lambda, src_cfg.sampleRate()) << std::endl
        << " lambda [1/sam]: " << _lambda << std::endl
        << " target value: " << _target;
    Logger::get().log(msg);

    // Propergate config
    this->setConfig(Config(Config::typeId<Scalar>(), src_cfg.sampleRate(), 
                    src_cfg.bufferSize(), 1));
  }


  /** Performs the amplification and adjusts the gain. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    // Simply forward buffer if disabled
    if ((! _enabled) && (0 == _gain) ) {
      this->send(buffer, allow_overwrite); return;
    }
    // Update signal ampl
    for (size_t i=0; i<buffer.size(); i++) {
      _sd = _lambda*_sd + (1-_lambda)*std::abs(buffer[i]);
      if (_enabled) { _gain = _target/(4*_sd); }
      _buffer[i] = _gain*buffer[i];
    }
    this->send(_buffer);
  }


protected:
  /** If true, the automatic gain adjustment is enabled. */
  bool _enabled;
  /** The time-constant of the AGC. */
  float _tau;
  /** One over the time-constant. */
  float _lambda;
  /** The averaged std. dev. of the input signal. */
  float _sd;
  /** The target level of the output signal. */
  float _target;
  /** The current gain factor. */
  float _gain;
  /** The current sample-rate. */
  double _sample_rate;
  /** The output buffer. */
  Buffer<Scalar> _buffer;
};


/** Keeps a copy of the last buffer received. */
template <class Scalar>
class DebugStore: public Sink<Scalar>
{
public:
  /** Constrctor. */
  DebugStore() : Sink<Scalar>(), _buffer(), _view() { }
  virtual ~DebugStore() {
    _buffer.unref();
  }

  /** Configures the node. */
  virtual void config(const Config &src_cfg) {
    // Requires type and buffer size
    if (!src_cfg.hasType() || !src_cfg.hasBufferSize()) { return; }
    // Check type
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure DebugStore node: Invalid input type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }
    // Allocate buffer
    _buffer = Buffer<Scalar>(src_cfg.bufferSize());
  }

  /** Stores the given buffer. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    size_t N = std::min(buffer.size(), _buffer.size());
    memcpy(_buffer.ptr(), buffer.data(), N*sizeof(Scalar));
    // Store view
    _view = _buffer.head(N);
  }

  /** Retunrs a reference to the last received buffer. */
  inline const Buffer<Scalar> &buffer() const { return _view; }
  /** Clears the buffer. */
  inline void clear() { _view = Buffer<Scalar>(); }

public:
  /** A pre-allocated buffer, that will hold the last data received. */
  Buffer<Scalar> _buffer;
  /** A view to the last data received. */
  Buffer<Scalar> _view;
};


/** Dumps buffers in a human readable form. */
template <class Scalar>
class DebugDump: public Sink<Scalar>
{
public:
  /** Constructor. */
  DebugDump(std::ostream &stream=std::cerr)
    : Sink<Scalar>(), _stream(stream)
  {
    // pass...
  }

  /** Destructor. */
  virtual ~DebugDump() {
    // pass...
  }

  /** Configures the dump-node. */
  virtual void config(const Config &src_cfg) {
    // Requires type
    if (!src_cfg.hasType()) { return; }
    // check type
    if (Config::typeId<Scalar>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure DebugDump sink: Invalid input type " << src_cfg.type()
          << ", expected " << Config::typeId<Scalar>();
      throw err;
    }
    // Done...
  }

  /** Dumps the received buffer. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite) {
    _stream << buffer << std::endl;
  }

protected:
  /** A reference to the output stream. */
  std::ostream &_stream;
};


/** A Gaussian White Noise source. */
template <class Scalar>
class GWNSource: public Source
{
public:
  /** Constructor. */
  GWNSource(double sample_rate, size_t buffer_size=1024)
    : Source(), _sample_rate(sample_rate), _buffer_size(buffer_size), _buffer(_buffer_size),
      _mean(0)
  {
    // Seed RNG
    std::srand(std::time(0));
    // Determine mean
    switch (Config::typeId<Scalar>()) {
    case Config::Type_u8:
    case Config::Type_cu8:
    case Config::Type_u16:
    case Config::Type_cu16:
      _mean = 1;
      break;
    default:
      _mean = 0;
      break;
    }
    // Propergate config
    setConfig(Config(Config::typeId<Scalar>(), _sample_rate, _buffer_size, 1));
  }

  /** Destructor. */
  virtual ~GWNSource() {
    _buffer.unref();
  }

  /** Samples and emits the next chunk of data. This function can be connected to the idle event
   * of the @c Queue instance, such that a new chunk gets emitted, once all previous chunks are
   * processed. */
  void next() {
    double a,b;
    for (size_t i=0; i<_buffer_size; i+=2) {
      _getNormal(a,b);
      _buffer[i] = Scalar(Traits<Scalar>::scale*(a+_mean));
      _buffer[i+1] = Scalar(Traits<Scalar>::scale*(b+_mean));
    }
    if (_buffer_size%2) {
      _getNormal(a,b); _buffer[_buffer_size-1] = Scalar(Traits<Scalar>::scale*(a+_mean));
    }
    this->send(_buffer, true);
  }


protected:
  /** Sample two std. normal distributed RVs. */
  void _getNormal(double &a, double &b) {
    // Obtain pair of std. normal floating point values
    double x = 2*(double(std::rand())/RAND_MAX) - 1;
    double y = 2*(double(std::rand())/RAND_MAX) - 1;
    double s = std::sqrt(x*x + y*y);
    while (s >= 1) {
      x = 2*(double(std::rand())/RAND_MAX) - 1;
      y = 2*(double(std::rand())/RAND_MAX) - 1;
      s = std::sqrt(x*x + y*y);
    }
    a = x*std::sqrt(-2*log(s)/s);
    b = y*std::sqrt(-2*log(s)/s);
  }

protected:
  /** The sample rate. */
  double _sample_rate;
  /** The size of the buffer. */
  size_t _buffer_size;
  /** The output buffer. */
  Buffer<Scalar> _buffer;
  /** The mean value of the GWN. */
  double _mean;
};

}

#endif // __SDR_UTILS_HH__

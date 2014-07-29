#ifndef __SDR_AUTOCAST_HH__
#define __SDR_AUTOCAST_HH__

#include "node.hh"
#include "traits.hh"
#include "logger.hh"

namespace sdr {

/** This class performs some automatic casts to a certain buffer type if possible specified by
 * the template argument. Currently only integer casts are supported. */
template <class Scalar>
class AutoCast: public SinkBase, public Source
{
public:
  /** Constructor. */
  AutoCast()
    : SinkBase(), Source(), _buffer(), _cast(0)
  {
    // pass...
  }

  /** Configures the auto cast node. */
  virtual void config(const Config &src_cfg) {
    // Requires buffer size, sample rate and type:
    if ((Config::Type_UNDEFINED==src_cfg.type()) || (0==src_cfg.sampleRate()) || (0==src_cfg.bufferSize())) { return; }

    // Check type cast combination
    if (Config::Type_s8 == Traits<Scalar>::scalarId) {
      switch (src_cfg.type()) {
      case Config::Type_u8:
      case Config::Type_s8: _cast = _identity; break;
      case Config::Type_u16:
      case Config::Type_s16: _cast = _int16_int8; break;
      default: break;
      }
    } else if (Config::Type_cs8 == Traits<Scalar>::scalarId) {
      switch (src_cfg.type()) {
      case Config::Type_u8: _cast = _uint8_cint8; break;
      case Config::Type_s8: _cast = _int8_cint8; break;
      case Config::Type_cu8: _cast = _cuint8_cint8; break;
      case Config::Type_cs8: _cast = _identity; break;
      case Config::Type_u16:
      case Config::Type_s16: _cast = _int16_cint8; break;
      default: break;
      }
    } else if (Config::Type_s16 == Traits<Scalar>::scalarId) {
      switch (src_cfg.type()) {
      case Config::Type_u8:
      case Config::Type_s8: _cast = _int8_int16; break;
      case Config::Type_u16:
      case Config::Type_s16: _cast = _identity; break;
      default: break;
      }
    } else if (Config::Type_cs16 == Traits<Scalar>::scalarId) {
      switch (src_cfg.type()) {
      case Config::Type_u8: _cast = _uint8_cint16; break;
      case Config::Type_s8: _cast = _int8_cint16; break;
      case Config::Type_cu8: _cast = _cuint8_cint16; break;
      case Config::Type_cs8: _cast = _cint8_cint16; break;
      case Config::Type_u16: _cast = _uint16_cint16; break;
      case Config::Type_s16: _cast = _int16_cint16; break;
      case Config::Type_cu16:
      case Config::Type_cs16: _cast = _identity; break;
      default: break;
      }
    }

    // Check if there exists a cast to the required type
    if (0 == _cast) {
      ConfigError err;
      err << "AutoCast: Can not cast from type " << src_cfg.type() << " to " << Traits<Scalar>::scalarId;
      throw err;
    }

    // Allocate buffer
    _buffer = Buffer<Scalar>(src_cfg.bufferSize());

    LogMessage msg(LOG_DEBUG);
    msg << "Configure AutoCast node:" << std::endl
        << " input type: " << src_cfg.type() << std::endl
        << " output type: " << Traits<Scalar>::scalarId;
    Logger::get().log(msg);

    // Propergate config
    this->setConfig(Config(Config::typeId<Scalar>(), src_cfg.sampleRate(), src_cfg.bufferSize(), 1));
  }

  virtual void handleBuffer(const RawBuffer &buffer, bool allow_overwrite) {
    // If no conversion is selected
    if (0 == _cast) { return; }
    // If the identity conversion is selected -> forward buffer
    if (_identity == _cast) { this->send(buffer, allow_overwrite); }
    // Otherwise cast
    size_t bytes = _cast(buffer, _buffer);
    this->send(RawBuffer(_buffer, 0, bytes), true);
  }


protected:
  /** Output buffer. */
  Buffer<Scalar> _buffer;
  /** Cast function. */
  size_t (*_cast)(const RawBuffer &in, const RawBuffer &out);

protected:
  /** Performs no cast at all. */
  static size_t _identity(const RawBuffer &in, const RawBuffer &out) {
    memcpy(out.data(), in.data(), in.bytesLen());
    return in.bytesLen();
  }

  /** int16 -> int8 */
  static size_t _int16_int8(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen()/2;
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<int8_t *>(out.data())[i] = reinterpret_cast<int16_t *>(in.data())[i]>>8;
    }
    return N;
  }

  /** uint8 -> complex int8. */
  static size_t _uint8_cint8(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen();
    uint8_t *values = reinterpret_cast<uint8_t *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int8_t> *>(out.data())[i] =
          (int16_t(values[i])-127);
    }
    return 2*N;
  }

  /** int8 -> complex int8. */
  static size_t _int8_cint8(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen();
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int8_t> *>(out.data())[i] = reinterpret_cast<int8_t *>(in.data())[i];
    }
    return 2*N;
  }

  static size_t _cuint8_cint8(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen()/2;
    std::complex<uint8_t> *values = reinterpret_cast<std::complex<uint8_t> *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int8_t> *>(out.data())[i] =
          std::complex<int8_t>(int16_t(values[i].real())-127, int16_t(values[i].imag())-127);
    }
    return 2*N;
  }

  /** int16 -> complex int 8. */
  static size_t _int16_cint8(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen()/2;
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int8_t> *>(out.data())[i] = reinterpret_cast<int16_t *>(in.data())[i]>>8;
    }
    return 2*N;
  }

  /** complex int16 -> complex int 8. */
  static size_t _cint16_cint8(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen()/4;
    std::complex<int16_t> *values = reinterpret_cast<std::complex<int16_t> *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int8_t> *>(out.data())[i] = std::complex<int8_t>(values[i].real()>>8, values[i].imag()>>8);
    }
    return 2*N;
  }

  /** int8 -> int16. */
  static size_t _int8_int16(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen();
    int8_t *values = reinterpret_cast<int8_t *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<int16_t *>(out.data())[i] = int16_t(values[i])<<8;
    }
    return 2*N;
  }

  /** unsinged int8 -> complex int16. */
  static size_t _uint8_cint16(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen();
    uint8_t *values = reinterpret_cast<uint8_t *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int16_t> *>(out.data())[i]
          = std::complex<int16_t>((int16_t(values[i])-127)<<8);
    }
    return 4*N;
  }

  /** int8 -> complex int16. */
  static size_t _int8_cint16(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen();
    int8_t *values = reinterpret_cast<int8_t *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int16_t> *>(out.data())[i]
          = std::complex<int16_t>(int16_t(values[i])*(1<<8));
    }
    return 4*N;
  }

  /** complex unsigned int8 -> complex int16. */
  static size_t _cuint8_cint16(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen()/2;
    std::complex<uint8_t> *values = reinterpret_cast<std::complex<uint8_t> *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int16_t> *>(out.data())[i] =
          std::complex<int16_t>((int16_t(values[i].real())-127)*(1<<8),
                                (int16_t(values[i].imag())-127)*(1<<8));
    }
    return 4*N;
  }

  /** complex int8 -> complex int16. */
  static size_t _cint8_cint16(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen()/2;
    std::complex<int8_t> *values = reinterpret_cast<std::complex<int8_t> *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int16_t> *>(out.data())[i] =
          std::complex<int16_t>(int16_t(values[i].real())*(1<<8),
                                int16_t(values[i].imag())*(1<<8));
    }
    return 4*N;
  }

  /** uint16 -> complex int16. */
  static size_t _uint16_cint16(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen()/2;
    uint16_t *values = reinterpret_cast<uint16_t *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int16_t> *>(out.data())[i]
          = std::complex<int16_t>(int32_t(values[i])-(1<<15));
    }
    return 4*N;
  }

  /** int16 -> complex int16. */
  static size_t _int16_cint16(const RawBuffer &in, const RawBuffer &out) {
    size_t N = in.bytesLen()/2;
    int16_t *values = reinterpret_cast<int16_t *>(in.data());
    for (size_t i=0; i<N; i++) {
      reinterpret_cast<std::complex<int16_t> *>(out.data())[i] = std::complex<int16_t>(values[i]);
    }
    return 4*N;
  }
};


}
#endif // __SDR_AUTOCAST_HH__

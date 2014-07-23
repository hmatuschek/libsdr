#ifndef __SDR_TRAITS_HH__
#define __SDR_TRAITS_HH__

#include <inttypes.h>
#include <complex>
#include "node.hh"

namespace sdr {

/** Forward declaration of type tratis template. */
template <class T> class Traits;

/** Template specialization of type tratis for uint8_t scalar. */
template <>
class Traits<uint8_t> {
public:
  /** The Scalar type. */
  typedef uint8_t Scalar;
  /** The compute scalar type. */
  typedef uint16_t SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** Shift from Scalar to SScalar. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_u8;
};

/** Template specialization of type tratis for complex uint8_t scalar. */
template <>
class Traits< std::complex<uint8_t> > {
public:
  /** The Scalar type. */
  typedef std::complex<uint8_t> Scalar;
  /** The compute scalar type. */
  typedef std::complex<uint16_t> SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_cu8;
};

/** Template specialization of type tratis for int8_t scalar. */
template <>
class Traits<int8_t> {
public:
  /** The Scalar type. */
  typedef int8_t Scalar;
  /** The compute scalar type. */
  typedef int16_t SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_s8;
};

/** Template specialization of type tratis for complex int8_t scalar. */
template <>
class Traits< std::complex<int8_t> > {
public:
  /** The Scalar type. */
  typedef std::complex<int8_t> Scalar;
  /** The compute scalar type. */
  typedef std::complex<int16_t> SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_cs8;
};

/** Template specialization of type traits for uint16_t scalar. */
template <>
class Traits<uint16_t> {
public:
  /** The Scalar type. */
  typedef uint16_t Scalar;
  /** The compute scalar type. */
  typedef uint32_t SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_u16;
};

/** Template specialization of type traits for complex uint16_t scalar. */
template <>
class Traits< std::complex<uint16_t> > {
public:
  /** The Scalar type. */
  typedef std::complex<uint16_t> Scalar;
  /** The compute scalar type. */
  typedef std::complex<uint32_t> SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_cu16;
};

/** Template specialization of type traits for int16_t scalar. */
template <>
class Traits<int16_t> {
public:
  /** The Scalar type. */
  typedef int16_t Scalar;
  /** The compute scalar type. */
  typedef int32_t SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_s16;
};

/** Template specialization of type traits for complex int16_t scalar. */
template <>
class Traits< std::complex<int16_t> > {
public:
  /** The Scalar type. */
  typedef std::complex<int16_t> Scalar;
  /** The compute scalar type. */
  typedef std::complex<int32_t> SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_cs16;
};

/** Template specialization of type traits for float scalar. */
template <>
class Traits<float> {
public:
  /** The Scalar type. */
  typedef float Scalar;
  /** The compute scalar type. */
  typedef float SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_f32;
};

/** Template specialization of type traits for complex float scalar. */
template <>
class Traits< std::complex<float> > {
public:
  /** The Scalar type. */
  typedef std::complex<float> Scalar;
  /** The compute scalar type. */
  typedef std::complex<float> SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_cf32;
};

/** Template specialization of type traits for float scalar. */
template <>
class Traits<double> {
public:
  /** The Scalar type. */
  typedef double Scalar;
  /** The compute scalar type. */
  typedef double SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_f64;
};

/** Template specialization of type traits for complex float scalar. */
template <>
class Traits< std::complex<double> > {
public:
  /** The Scalar type. */
  typedef std::complex<double> Scalar;
  /** The compute (super) scalar type. */
  typedef std::complex<double> SScalar;
  /** The scaleing factor from floating point to integer. */
  const static float scale;
  /** The scaleing factor from floating point to integer expressed as 2^exp. */
  const static size_t shift;
  /** The type id of the scalar type. */
  const static Config::Type scalarId = Config::Type_cf64;
};


}

#endif // TRAITS_HH

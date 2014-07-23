#ifndef __SDR_OPERATORS_HH__
#define __SDR_OPERATORS_HH__

#include <stdint.h>
#include <complex>


// Tiny helper functions to handle std::real & std::conj on real integers
namespace std {
inline int16_t conj(int16_t value) { return value; }
inline int16_t real(int16_t value) { return value; }
inline int16_t imag(int16_t value) { return 0; }
}


inline std::complex<double> operator*(const double &a, const std::complex<float> &b) {
  return std::complex<double>(a*b.real(), a*b.imag());
}

inline std::complex<double> operator*(const double &a, const std::complex<uint16_t> &b) {
  return std::complex<double>(a*b.real(), a*b.imag());
}

inline std::complex<double> operator*(const double &a, const std::complex<int16_t> &b) {
  return std::complex<double>(a*b.real(), a*b.imag());
}

inline std::complex<double> operator*(const double &a, const std::complex<uint8_t> &b) {
  return std::complex<double>(a*b.real(), a*b.imag());
}

inline std::complex<double> operator*(const double &a, const std::complex<int8_t> &b) {
  return std::complex<double>(a*b.real(), a*b.imag());
}

inline std::complex<float> operator* (const std::complex<float> &a, const std::complex<int16_t> &b) {
  return a*std::complex<float>(std::real(b), std::imag(b));
}

inline std::complex<double> operator* (const std::complex<double> &a, const std::complex<int16_t> &b) {
  return a*std::complex<double>(std::real(b), std::imag(b));
}

inline std::complex<int32_t> operator<< (const std::complex<int32_t> &a, int b) {
  return std::complex<int32_t>(std::real(a)<<b, std::imag(a)<<b);
}

inline std::complex<int32_t> operator>> (const std::complex<int32_t> &a, int b) {
  return std::complex<int32_t>(std::real(a)>>b, std::imag(a)>>b);
}


namespace sdr {

template <class iScalar, class oScalar>
inline oScalar cast(const iScalar &a) { return oScalar(a); }

template<>
inline std::complex<double> cast<int8_t, std::complex<double> >(const int8_t &a) {
  return std::complex<double>(std::real(a), std::imag(a));
}

template<>
inline std::complex<double> cast<std::complex<int8_t>, std::complex<double> >(const std::complex<int8_t> &a) {
  return std::complex<double>(std::real(a), std::imag(a));
}

template<>
inline std::complex<double> cast<int16_t, std::complex<double> >(const int16_t &a) {
  return std::complex<double>(std::real(a), std::imag(a));
}

template<>
inline std::complex<double> cast<std::complex<int16_t>, std::complex<double> >(const std::complex<int16_t> &a) {
  return std::complex<double>(std::real(a), std::imag(a));
}


/** Mulitplication by a power of two. */
inline uint8_t mul2(uint8_t a, int n) {
  if (n < 0) { return a >> -n; }
  else { return a << n; }
}

/** Division by a power of two. */
inline uint8_t div2(uint8_t a, int n) {
  if (n < 0) { return a << -n; }
  else { return a >> n; }
}

/** Mulitplication by a power of two. */
inline std::complex<uint8_t> mul2(const std::complex<uint8_t> &a, int n) {
  if (n < 0) {
    return std::complex<uint8_t>(std::real(a)>>(-n), std::imag(a)>>(-n));
  } else {
    return std::complex<uint8_t>(std::real(a)<<(n), std::imag(a)<<(n));
  }
}

/** Division by a power of two. */
inline std::complex<uint8_t> div2(const std::complex<uint8_t> &a, int n) {
  if (n < 0) {
    return std::complex<uint8_t>(std::real(a)<<(-n), std::imag(a)<<(-n));
  } else {
    return std::complex<uint8_t>(std::real(a)>>(n), std::imag(a)>>(n));
  }
}

/** Mulitplication by a power of two. */
inline int8_t mul2(int8_t a, int n) {
  if (n < 0) { return a >> -n; }
  else { return a << n; }
}

/** Mulitplication by a power of two. */
inline std::complex<int8_t> mul2(const std::complex<int8_t> &a, int n) {
  if (n < 0) {
    return std::complex<int8_t>(std::real(a)>>(-n), std::imag(a)>>(-n));
  } else {
    return std::complex<int8_t>(std::real(a)>>(-n), std::imag(a)>>(-n));
  }
}

/** Mulitplication by a power of two. */
inline uint16_t mul2(uint16_t a, int n) {
  if (n < 0) { return a >> -n; }
  else { return a << n; }
}

/** Mulitplication by a power of two. */
inline std::complex<uint16_t> mul2(const std::complex<uint16_t> &a, int n) {
  if (n < 0) {
    return std::complex<uint16_t>(std::real(a)>>(-n), std::imag(a)>>(-n));
  } else {
    return std::complex<uint16_t>(std::real(a)>>(-n), std::imag(a)>>(-n));
  }
}

/** Mulitplication by a power of two. */
inline int16_t mul2(int16_t a, int n) {
  if (n < 0) { return a >> -n; }
  else { return a << n; }
}

/** Mulitplication by a power of two. */
inline std::complex<int16_t> mul2(const std::complex<int16_t> &a, int n) {
  if (n < 0) {
    return std::complex<int16_t>(std::real(a)>>(-n), std::imag(a)>>(-n));
  } else {
    return std::complex<int16_t>(std::real(a)>>(-n), std::imag(a)>>(-n));
  }
}

/** Mulitplication by a power of two. */
inline float mul2(float a, int n) {
  if (n < 0) { return a/(1<<(-n)); }
  else { return a*(1<<n); }
}

/** Mulitplication by a power of two. */
inline std::complex<float> mul2(const std::complex<float> &a, int n) {
  if (n < 0) { return a/float(1<<(-n)); }
  else { return a*float(1<<n); }
}

/** Mulitplication by a power of two. */
inline std::complex<double> mul2(const std::complex<double> &a, int n) {
  if (n < 0) { return a/double(1<<(-n)); }
  else { return a*double(1<<n); }
}

}

#endif // OPERATORS_HH

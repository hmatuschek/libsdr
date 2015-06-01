#ifndef __SDR_MATH_HH__
#define __SDR_MATH_HH__

#include <inttypes.h>

namespace sdr {

/** Template prototype for @c fast_atan2. */
template <class iScalar, class oScalar> oScalar fast_atan2(iScalar a, iScalar b);

/** Implementation of atan2 approximation using integers. */
template <> inline int16_t fast_atan2<int8_t, int16_t>(int8_t a, int8_t b) {
  const int32_t pi4 = (1<<12);
  const int32_t pi34 = 3*(1<<12);
  int32_t aabs, angle;
  if ((0 == a) && (0 == b)) { return 0; }
  aabs = (a >= 0) ? a : -a;
  if (b >= 0) { angle = pi4 - pi4*(b-aabs) / (b+aabs); }
  else { angle = pi34 - pi4*(b+aabs) / (aabs-b); }
  return (a >= 0) ? angle : -angle;
}

/** Implementation of atan2 approximation using integers. */
template <> inline int16_t fast_atan2<uint8_t, int16_t>(uint8_t ua, uint8_t ub) {
  int8_t a = (int16_t(ua)-(1<<7));
  int8_t b = (int16_t(ub)-(1<<7));
  return fast_atan2<int8_t, int16_t>(a,b);
}

/** Implementation of atan2 approximation using integers. */
template <> inline int16_t fast_atan2<int16_t, int16_t>(int16_t a, int16_t b) {
  //return (1<<15)*(std::atan2(float(a), float(b))/M_PI);
  const int32_t pi4 = (1<<12);
  const int32_t pi34 = 3*(1<<12);
  int32_t aabs, angle;
  if ((0 == a) && (0 == b)) { return 0; }
  aabs = (a >= 0) ? a : -a;
  if (b >= 0) { angle = pi4 - pi4*(b-aabs) / (b+aabs); }
  else { angle = pi34 - pi4*(b+aabs) / (aabs-b); }
  return (a >= 0) ? angle : -angle;
}


}
#endif // MATH_HH

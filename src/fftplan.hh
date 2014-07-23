#ifndef __SDR_FFTPLAN_HH__
#define __SDR_FFTPLAN_HH__

#include "buffer.hh"
#include "node.hh"
#include "config.hh"

namespace sdr {

// Forward declaration of FFTPlan
template <class Scalar> class FFTPlan { };

/** FFT module class, provides static methods to perfrom a FFT directly. */
class FFT {
public:
  /** Direction type. */
  typedef enum {
    FORWARD, BACKWARD
  } Direction;

  /** Performs a FFT transform. */
  template <class Scalar>
  static void exec(const Buffer< std::complex<Scalar> > &in,
                   const Buffer< std::complex<Scalar> > &out, FFT::Direction dir)
  {
    FFTPlan<Scalar> plan(in, out, dir); plan();
  }

  /** Performs an in-place FFT transform. */
  template <class Scalar>
  static void exec(const Buffer< std::complex<Scalar> > &inplace, FFT::Direction dir)
  {
    FFTPlan<Scalar> plan(inplace, dir); plan();
  }

};

}


#ifdef SDR_WITH_FFTW
#include "fftplan_fftw3.hh"
#endif


#endif // __SDR_FFTPLAN_HH__

#ifndef __SDR_FFTPLAN_NATIVE_HH__
#define __SDR_FFTPLAN_NATIVE_HH__

#include "traits.hh"
#include "fftplan.hh"

namespace sdr {

/** Trivial FFT implementation for buffer sizes of N=2**K. */
template <class Scalar>
class FFTPlan
{
public:
  /** The super-scalar of the input type. */
  typedef typename Traits<Scalar >::SScalar SScalar;

public:
  /** Constructs a FFT plan for the input and output buffers. */
  FFTPlan(const Buffer< std::complex<Scalar> > &in, const Buffer< std::complex<Scalar> > &out,
          FFT::Direction dir)
    : _N(in.size()), _in(in), _out(out)
  {
    // Check dimensions
    if (_in.size() != _out.size()) {
      ConfigError err;
      err << "Can not construct FFT plan: input & output buffers are of different size!";
      throw err;
    }
    if (_in.isEmpty() || _out.isEmpty()) {
      ConfigError err;
      err << "Can not construct FFT plan: input or output buffer is empty!";
      throw err;
    }
    // Check if N is power of two
    if (_N != std::pow(2.0, std::log2(_N))) {
      err << "Can not construct FFT plan: input and output buffer length must be a power of 2!";
      throw err;
    }

    // Assemble lookuptable
    int dir_fac = (dir==FFT::FORWARD) ? 1 : -1;
    for (size_t i=0; i<_N; i++) {
      _lut[i] = (std::exp(std::complex<double>(0,(-2*M_PI*i*dir_fac)/N)) * (1<<Traits<Scalar>::shift));
    }
  }

  /** Destructor. */
  virtual ~FFTPlan() {
    _lut.unref();
  }

  /** Performs the FFT. */
  void operator() () {
    _calc(reinterpret_cast< std::complex<Scalar> >(_in.data()),
          reinterpret_cast< std::complex<Scalar> >(_out.data()), _N, 1);
  }

protected:
  /** Actual FFT implmenetation. */
  void _calc(std::complex<Scalar> *a, std::complex<Scalar> *b, size_t N, size_t stride) {
    // Recursion exit
    if (1 == N) { b[0] = a[0]; return; }
    // Calc FFT recursive
    _calc(a, b, N/2, 2*stride);
    _calc(a+stride, b+N/2, N/2, 2*stride);
    // Radix-2...
    for (size_t i=0; i<N/2; i++) {
      std::complex<SScalar> tmp = b[i];
      // I hope that the compiler will turn the (x/(1<<L)) into a (x >> L) if x is an integer...
      b[i] = tmp + (_lut[i]*std::complex<SScalar>(b[i+N/2])) / (1<<Traits<Scalar>::shift);
      b[i+N/2] = tmp - (_lut[i]*std::complex<SScalar>(b[i+N/2])) / (1<<Traits<Scalar>::shift);
    }
  }

protected:
  /** FFT size, needs to be a power of 2. */
  size_t _N;
  /** The input buffer. */
  Buffer< std::complex<Scalar> > _in;
  /** The output buffer. */
  Buffer< std::complex<Scalar> > _out;
  /** The exp(-i 2 pi k / N) look-up table. */
  Buffer< std::complex<SScalar> > _lut;
};

}

#endif // __SDR_FFTPLAN_NATIVE_HH__

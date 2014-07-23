#ifndef __SDR_FFTPLAN_FFTW3_HH__
#define __SDR_FFTPLAN_FFTW3_HH__

#include "fftplan.hh"
#include <fftw3.h>


namespace sdr {

/** Template specialization for a FFT transform on std::complex<double> values. */
template<>
class FFTPlan<double>
{
public:
  /** Constructor. */
  FFTPlan(const Buffer< std::complex<double> > &in, const Buffer< std::complex<double> > &out,
          FFT::Direction dir)
    : _in(in), _out(out)
  {
    if (in.size() != out.size()) {
      ConfigError err;
      err << "Can not construct FFT plan: input & output buffers are of different size!";
      throw err;
    }
    if (in.isEmpty() || out.isEmpty()) {
      ConfigError err;
      err << "Can not construct FFT plan: input or output buffer is empty!";
      throw err;
    }

    int fftw_dir = FFTW_FORWARD;
    if (FFT::BACKWARD == dir) { fftw_dir = FFTW_BACKWARD; }

    _plan = fftw_plan_dft_1d(
          in.size(), (fftw_complex *)in.data(), (fftw_complex *)out.data(),
          fftw_dir, FFTW_ESTIMATE);
  }

  /** Constructor. */
  FFTPlan(const Buffer< std::complex<double> > &inplace, FFT::Direction dir)
    : _in(inplace), _out(inplace)
  {
    if (inplace.isEmpty()) {
      ConfigError err;
      err << "Can not construct FFT plan: Buffer is empty!";
      throw err;
    }

    int fftw_dir = FFTW_FORWARD;
    if (FFT::BACKWARD == dir) { fftw_dir = FFTW_BACKWARD; }

    _plan = fftw_plan_dft_1d(
          inplace.size(), (fftw_complex *)inplace.data(), (fftw_complex *)inplace.data(),
          fftw_dir, FFTW_ESTIMATE);
  }

  /** Destructor. */
  virtual ~FFTPlan() {
    fftw_destroy_plan(_plan);
  }

  /** Performs the transformation. */
  void operator() () {
    fftw_execute(_plan);
  }

protected:
  /** Input buffer. */
  Buffer< std::complex<double> > _in;
  /** Output buffer. */
  Buffer< std::complex<double> > _out;
  /** The FFT plan. */
  fftw_plan _plan;
};


/** Template specialization for a FFT transform on std::complex<float> values. */
template<>
class FFTPlan<float>
{
public:
  /** Constructor. */
  FFTPlan(const Buffer< std::complex<float> > &in, const Buffer< std::complex<float> > &out,
          FFT::Direction dir)
    : _in(in), _out(out)
  {
    if (in.size() != out.size()) {
      ConfigError err;
      err << "Can not construct FFT plan: input & output buffers are of different size!";
      throw err;
    }

    if (in.isEmpty() || out.isEmpty()) {
      ConfigError err;
      err << "Can not construct FFT plan: input or output buffer is empty!";
      throw err;
    }

    int fftw_dir = FFTW_FORWARD;
    if (FFT::BACKWARD == dir) { fftw_dir = FFTW_BACKWARD; }

    _plan = fftwf_plan_dft_1d(
          in.size(), (fftwf_complex *)in.data(), (fftwf_complex *)out.data(),
          fftw_dir, FFTW_ESTIMATE);
  }

  /** Constructor. */
  FFTPlan(const Buffer< std::complex<float> > &inplace, FFT::Direction dir)
    : _in(inplace), _out(inplace)
  {
    if (inplace.isEmpty()) {
      ConfigError err;
      err << "Can not construct FFT plan: Buffer is empty!";
      throw err;
    }

    int fftw_dir = FFTW_FORWARD;
    if (FFT::BACKWARD == dir) { fftw_dir = FFTW_BACKWARD; }

    _plan = fftwf_plan_dft_1d(
          inplace.size(), (fftwf_complex *)inplace.data(), (fftwf_complex *)inplace.data(),
          fftw_dir, FFTW_ESTIMATE);
  }

  /** Destructor. */
  virtual ~FFTPlan() {
    fftwf_destroy_plan(_plan);
  }

  /** Performs the FFT transform. */
  void operator() () {
    fftwf_execute(_plan);
  }

protected:
  /** Input buffer. */
  Buffer< std::complex<float> > _in;
  /** Output buffer. */
  Buffer< std::complex<float> > _out;
  /** The fft plan. */
  fftwf_plan _plan;
};




}

#endif // __SDR_FFTPLAN_FFTW3_HH__

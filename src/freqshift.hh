#ifndef __SDR_FREQSHIFT_HH__
#define __SDR_FREQSHIFT_HH__

#include "config.hh"
#include "traits.hh"
#include "node.hh"
#include "operators.hh"

namespace sdr {

/** A performant implementation of a frequency shift operation on integer signals. */
template <class Scalar>
class FreqShiftBase
{
public:
  /** The complex input signal. */
  typedef std::complex<Scalar> CScalar;
  /** The compute (super) scalar of the input type. */
  typedef typename Traits<Scalar>::SScalar SScalar;
  /** The complex compute (super) scalar of the input type. */
  typedef std::complex<SScalar> CSScalar;

public:
  /** Constructor. */
  FreqShiftBase(double F, double Fs)
    : _freq_shift(F), _Fs(Fs), _lut_inc(0), _lut_count(0), _lut(_lut_size)
  {
    // Allocate and assemble LUT
    // Allocate LUT for (neg) frequency shift
    _lut = Buffer<CSScalar>(_lut_size);
    for (size_t i=0; i<_lut_size; i++) {
      _lut[i] = double(1 << Traits<Scalar>::shift) *
          std::exp(std::complex<double>(0,-(2*M_PI*i)/_lut_size));
    }
  }

  /** Destructor. */
  virtual ~FreqShiftBase() {
    _lut.unref();
  }

  /** Returns the sample rate. */
  inline double sampleRate() const { return _Fs; }
  /** Sets the sample rate and updates the LUT. */
  virtual void setSampleRate(double Fs) {
    _Fs = Fs; _update_lut_incr();
  }

  /** Returns the frequency shift. */
  inline double frequencyShift() const { return _freq_shift; }
  /** Sets the frequency shift and updates the LUT. */
  virtual void setFrequencyShift(double F) {
    _freq_shift = F; _update_lut_incr();
  }

  /** Performs the frequency shift on a single sample. */
  inline CSScalar applyFrequencyShift(CSScalar value)
  {
    // If frequency shift is actually 0 -> return
    if (0 == _lut_inc) { return value; }
    // Get index, idx = (_lut_count/256)
    size_t idx = (_lut_count>>8);
    if (0 > _freq_shift) { idx = _lut_size - idx - 1; }
    value = ((_lut[idx] * value) >> Traits<Scalar>::shift);
    // Incement _lut_count
    _lut_count += _lut_inc;
    // _lut_count modulo (_lut_size*256)
    while (_lut_count >= (_lut_size<<8)) { _lut_count -= (_lut_size<<8); }
    return value;
  }

protected:
  /** Updates the multiplier LUT. */
  void _update_lut_incr() {
    // Every sample increments the LUT index by lut_inc/256.
    // The multiple is needed as ratio between the frequency shift _Fc and the sample rate _Fs
    // may not result in an integer increment. By simply flooring _lut_size*_Fc/_Fs, the actual
    // down conversion may be much smaller than actual reuqired. Hence, the counter in therefore
    // incremented by the integer (256*_lut_size*_Fc/_Fs) and the index is then obtained by
    // dividing _lut_count by 256 (right shift 8 bits).
    _lut_inc = (_lut_size*(1<<8)*std::abs(_freq_shift))/_Fs;
    _lut_count = 0;
  }

protected:
  /** The current frequency shift. */
  double _freq_shift;
  /** The current sample rate. */
  double _Fs;
  /** The LUT increment. */
  size_t _lut_inc;
  /** The LUT index counter. */
  size_t _lut_count;
  /** The LUT. */
  Buffer<CSScalar> _lut;

protected:
  /** The size of the LUT. */
  static const size_t _lut_size = 128;
};


}

#endif // FREQSHIFT_HH

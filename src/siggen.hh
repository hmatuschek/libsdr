#ifndef __SDR_SIGGEN_HH__
#define __SDR_SIGGEN_HH__

#include "node.hh"


namespace sdr {

/** Arbitrary function generator. */
template <class Scalar>
class SigGen: public Source
{
public:
  /** Constructs the function generator. */
  SigGen(double samplerate, size_t buffersize, double tmax=-1)
    : Source(), _sampleRate(samplerate), _dt(1./_sampleRate), _t(0), _tMax(tmax), _scale(1),
      _bufferSize(buffersize), _buffer(buffersize)
  {
    switch (Config::typeId<Scalar>()) {
    case Config::Type_u8:
    case Config::Type_s8:
    case Config::Type_cu8:
    case Config::Type_cs8:
      _scale = 127; break;
    case Config::Type_u16:
    case Config::Type_s16:
    case Config::Type_cu16:
    case Config::Type_cs16:
      _scale = 32000; break;
    default:
      _scale = 1; break;
    }

    this->setConfig(Config(Config::typeId<Scalar>(), samplerate, buffersize, 1));
  }

  /** Destructor. */
  virtual ~SigGen() {}

  /** Computes the next buffer. This function can be connected to the idle signal of the @c Queue. */
  void next() {
    // Stop processing once the max time elapsed
    if ((_tMax>0) && (_t >= _tMax)) { Queue::get().stop(); return; }
    // Assemble signal
    for (size_t i=0; i<_bufferSize; i++) {
      _buffer[i] = 0;
      if (_signals.size() > 0) {
        std::list< std::vector<double> >::iterator item = _signals.begin();
        for (; item != _signals.end(); item++) {
          _buffer[i] += (_scale*((*item)[1] * sin(2*M_PI*(*item)[0]*_t + (*item)[2]))/_signals.size());
        }
      }
      _t += _dt;
    }
    // Send buffer
    this->send(_buffer);
  }

  /** Add a sine function to the function generator. */
  void addSine(double freq, double ampl=1, double phase=0) {
    std::vector<double> tmp(3); tmp[0] = freq; tmp[1] = ampl; tmp[2] = phase;
    _signals.push_back(tmp);
  }

protected:
  /** The sample rate of the function generator. */
  double _sampleRate;
  /** The sample period. */
  double _dt;
  /** The current time. */
  double _t;
  /** The maximum time. */
  double _tMax;
  /** The scaling of the signal. */
  double _scale;
  /** A list of functions. */
  std::list< std::vector<double> > _signals;
  /** The size of the output buffer. */
  size_t _bufferSize;
  /** The output buffer. */
  Buffer<Scalar> _buffer;
};

}


#endif // __SDR_SIGGEN_HH__

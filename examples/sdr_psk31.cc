#include "baseband.hh"
#include "autocast.hh"
#include "demod.hh"
#include "portaudio.hh"
#include "wavfile.hh"
#include "gui/gui.hh"

#include <iostream>

#include <QApplication>
#include <QMainWindow>
#include <QThread>

using namespace sdr;

static void __sigint_handler(int signo) {
  // On SIGINT -> stop queue properly
  Queue::get().stop();
}

/** Implements a fractional sub-sampler. */
template <class Scalar>
class FracSubSampleBase
{
public:
  /** The input & output type super-scalar. */
  typedef typename Traits<Scalar>::SScalar SScalar;

public:
  /** Constructor.
   * @param frac Specifies the output sample rate relative to the input sample rate. I.e. 2 means
   *        half the input sample rate. */
  FracSubSampleBase(double frac)
    : _avg(0), _sample_count(0), _period(0) {
    if (frac < 1) {
      ConfigError err;
      err << "FracSubSampleBase: Can not sub-sample with fraction smaller one: " << frac;
      throw err;
    }
    _period = (frac*(1<<16));
  }

  /** Destructor. */
  virtual ~FracSubSampleBase() {
    // pass...
  }

  /** Resets the sample rate fraction. */
  inline void setFrac(double frac) {
    if (frac < 1) {
      ConfigError err;
      err << "FracSubSampleBase: Can not sub-sample with fraction smaller one: " << frac;
      throw err;
    }
    _period = (frac*(1<<16)); _sample_count=0; _avg = 0;
  }

  /** Returns the effective sub-sample fraction. */
  inline double frac() const {
    return double(_period)/(1<<16);
  }

  /** Reset sample counter. */
  inline void reset() {
    _avg=0; _sample_count=0;
  }

  /** Performs the sub-sampling. @c in and @c out may refer to the same buffer allowing for an
   * in-place operation. Returns a view on the output buffer containing the sub-samples. */
  inline Buffer<Scalar> subsample(const Buffer<Scalar> &in, const Buffer<Scalar> &out) {
    size_t oidx = 0;
    for (size_t i=0; i<in.size(); i++) {
      _avg += in[i]; _sample_count += (1<<16);
      if (_sample_count >= _period) {
        out[oidx] = _avg/SScalar(_sample_count/(1<<16));
        _sample_count=0; _avg = 0; oidx++;
      }
    }
    return out.head(oidx);
  }

protected:
  /** The average. */
  SScalar _avg;
  /** The number of samples collected times (1<<16). */
  size_t _sample_count;
  /** The sub-sample period. */
  size_t _period;
};



template <class Scalar>
class PLL
{
public:
  PLL(double F0, double dF, double bw)
    : _F(F0), _dF(dF), _P(0), _dP(0)
  {
    double damp = std::sqrt(2.0)/2;
    double tmp = (1.+2*damp*bw + bw*bw);
    _alpha = 4*damp*bw/tmp;
    _beta  = 4*bw*bw/tmp;
    _Fmin = _F-_dF; _Fmax = _F+_dF;
  }

  virtual ~PLL() {
    // pass...
  }

  inline void updatePLL(const Buffer<std::complex<Scalar> > &in, const Buffer<uint8_t> &out)
  {
    for (size_t i=0; i<in.size(); i++) {
      out[i] = updatePLL(in[i]);
    }
  }

  inline uint8_t updatePLL(const std::complex<Scalar> &in)
  {
    float phi = _mod2PI(std::atan2(in.imag(), in.real()) - _P);
    _F = _F + _beta*phi;
    _dP = _mod2PI(_dP + _alpha*phi);
    _P = _P + _F + _alpha*phi;
    // Limit phase and frequency
    _phaseNorm(_P); _phaseNorm(_dP);
    _F = std::min(_Fmax, std::max(_Fmin, _F));
    return (_dP/(2*M_PI))*(1<<7);
  }

  inline float phase() const { return _P; }
  inline float phaseShift() const { return _dP; }
  inline float frequency() const { return _F; }

  void setFrequency(float F) {
    _F = F; _Fmin = _F-_dF; _Fmax = _F+_dF;
  }

  void setFreqWidth(float dF) {
    _dF = dF;
    _Fmin = _F-_dF; _Fmax = _F+_dF;
  }

protected:
  static inline void _phaseNorm(float &arg) {
    while (arg > (2*M_PI)) { arg -= 2*M_PI; }
    while (arg < (-2*M_PI)) { arg += 2*M_PI; }
  }

  static inline float _mod2PI(const float &arg) {
    if (arg > M_PI) { return arg - 2*M_PI;}
    if (arg < -M_PI) { return arg + 2*M_PI; }
    return arg;
  }

protected:
  float _F, _dF, _P, _dP;
  float _Fmin, _Fmax;
  float _alpha, _beta;
};



template <class Scalar>
class BPSK31: public Sink< std::complex<Scalar> >, public Source, public PLL<Scalar>
{
  typedef typename Traits<Scalar>::SScalar SScalar;

public:
  BPSK31(double F0, double dF=200.0, double bw=3e-1)
    : Sink< std::complex<Scalar> >(), Source(), PLL<Scalar>(2*M_PI*F0/8000., 2*M_PI*dF/8000, bw),
      _subsampler(1), _subsamplebuffer(), _F0(F0), _dF(dF)
  {
    // pass...
  }

  /** Destructor. */
  virtual ~BPSK31() {
    // pass...
  }

  /** Configures the BPSK demodulator. */
  virtual void config(const Config &src_cfg) {
    // Requires type, buffer size & sample rate
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }

    // Check if buffer type matches template
    if (Config::typeId< std::complex<Scalar> >() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure BPSK31: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId< std::complex<Scalar> >();
      throw err;
    }

    // Check input sample-rate
    if (src_cfg.sampleRate() < 8000) {
      ConfigError err;
      err << "Can not configure BPSK31 node: Input sample rate must be at least 8000Hz! "
          << "Sample rate = " << src_cfg.sampleRate();
      throw err;
    }

    // Update frequencies of PLL
    this->setFrequency(2*M_PI*_F0/8000);
    this->setFreqWidth(2*M_PI*_dF/8000);

    // Configure sub-sampler to 8kHz sample rate:
    _subsampler.setFrac(src_cfg.sampleRate()/8000);
    _subsamplebuffer = Buffer< std::complex<Scalar> >(src_cfg.bufferSize());

    // Unreference buffer if non-empty
    if (! _buffer.isEmpty()) { _buffer.unref(); }
    // Allocate buffer
    _buffer = Buffer<float>(256);
    _hist   = Buffer<float>(256);
    // Clear history
    for (size_t i=0; i<_hist.size(); i++) { _hist[i] = 0; }

    // bit counter...
    _dec_count = 0;

    LogMessage msg(LOG_DEBUG);
    msg << "Configured BPSK31 node:" << std::endl
        << " sample-rate: " << src_cfg.sampleRate() << std::endl
        << " sub-sample by: " << _subsampler.frac()
        << " to: " << src_cfg.sampleRate()/_subsampler.frac() << "Hz" << std::endl
        << " in-type / out-type: " << src_cfg.type()
        << " / " << Config::typeId<uint8_t>() << std::endl;
    Logger::get().log(msg);

    // Propergate config
    this->setConfig(Config(Config::typeId<float>(), 8000.0, 256, 1));
  }


  /** Performs the FM demodulation. */
  virtual void process(const Buffer<std::complex<Scalar> > &buffer, bool allow_overwrite)
  {
    // First, sub-sample to 8000Hz
    Buffer< std::complex<Scalar> > samples =
        _subsampler.subsample(buffer, _subsamplebuffer);

    float phase = 0;
    for (size_t i=1; i<samples.size(); i++) {
      // Update PLL
      this->updatePLL(samples[i]); phase = this->phaseShift();
      // Obtain phase-difference with respect to last bit
      // and store current phase
      _buffer[_dec_count] = phase - _hist[_dec_count];
      _hist[_dec_count] = phase; _dec_count++;
      if (256 == _dec_count) {
        std::cerr << "PLL Freq: " << 8000.*this->frequency()/(2*M_PI) << std::endl;
        for (size_t j=0; j<256; j++) {
          std::cout << _buffer[j] << "\t";
        }
        std::cout << std::endl;
        // propergate resulting 256 bits
        this->send(_buffer.head(256)); _dec_count = 0;
      }
    }
  }


protected:
  /** Precise sub-sampler to 8000Hz sample rate. */
  FracSubSampleBase< std::complex<Scalar> > _subsampler;
  Buffer< std::complex<Scalar> > _subsamplebuffer;

  double _F0, _dF;
  size_t _dec_count;

  /** The output buffer, unused if demodulation is performed in-place. */
  Buffer<float> _buffer;
  Buffer<float> _hist;
};



int main(int argc, char *argv[]) {
  if (2 != argc) {
    std::cerr << "Usage: sdr_psk31 FILENAME" << std::endl;
    return -1;
  }

  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register handler:
  signal(SIGINT, __sigint_handler);

  QApplication app(argc, argv);

  QMainWindow       *win       = new QMainWindow();
  gui::Spectrum     *spec      = new gui::Spectrum(2, 1024, 5);
  gui::SpectrumView *spec_view = new gui::SpectrumView(spec);
  spec_view->setMindB(-60);
  win->setCentralWidget(spec_view);
  win->setMinimumSize(640, 240);
  win->show();

  PortAudio::init();
  Queue &queue = Queue::get();

  WavSource src(argv[1]);
  AutoCast< std::complex<int16_t> > cast;
  IQBaseBand<int16_t> baseband(0, 2144., 200.0, 63, 1);
  baseband.setCenterFrequency(1244.0);
  BPSK31<int16_t> demod(1000.0, 100.0, 4e-1);
  PortSink sink;
  DebugDump<float> dump;

  src.connect(&cast, true);
  cast.connect(&baseband, true);
  baseband.connect(spec);
  baseband.connect(&sink);
  baseband.connect(&demod);
  demod.connect(&dump, true);

  queue.addIdle(&src, &WavSource::next);
  //queue.addIdle(&src, &IQSigGen<double>::next);

  queue.start();
  app.exec();
  queue.stop();
  queue.wait();

  PortAudio::terminate();

  return 0;
}

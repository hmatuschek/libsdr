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

template <class Scalar>
class BPSK31: public Sink< std::complex<Scalar> >, public Source
{
  typedef typename Traits<Scalar>::SScalar SScalar;

public:
  BPSK31()
    : Sink< std::complex<Scalar> >(), Source(), _lut(256)
  {
    _shift = 8*(sizeof(int16_t)-sizeof(Scalar));
    // Initialize LUT
    for (size_t i=0; i<256; i++) {
      _lut[i] = std::exp(std::complex<double>(0., (-2*M_PI*i)/256))*(1<<8);
    }
  }

  /** Destructor. */
  virtual ~BPSK31() {
    // pass...
  }

  /** Configures the FM demodulator. */
  virtual void config(const Config &src_cfg) {
    // Requires type & buffer size
    if (!src_cfg.hasType() || !src_cfg.hasBufferSize()) { return; }
    // Check if buffer type matches template
    if (Config::typeId< std::complex<Scalar> >() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure BPSK31: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId< std::complex<Scalar> >();
      throw err;
    }
    // Unreference buffer if non-empty
    if (! _buffer.isEmpty()) { _buffer.unref(); }
    // Allocate buffer
    _buffer =  Buffer<int16_t>(16);

    _lut_idx = 0;
    _lut_incr = ((256.*256.*2144.)/src_cfg.sampleRate());
    _pll_incr = 0;
    double tau = 1./20;
    _alpha = 2*M_PI/(2*M_PI+src_cfg.sampleRate()*tau);
    _lp_phase = 0;
    // Results into a 4*31.25 Hz sample
    _subsample = (256.*src_cfg.sampleRate()/(4*32.25));
    _sample_count = 0;
    _dec_count = 0;

    LogMessage msg(LOG_DEBUG);
    msg << "Configured BPSK31 node:" << std::endl
        << " sample-rate: " << src_cfg.sampleRate() << std::endl
        << " in-type / out-type: " << src_cfg.type()
        << " / " << Config::typeId<int16_t>() << std::endl
        << " in-place: " << (_can_overwrite ? "true" : "false") << std::endl
        << " output scale: 2^" << _shift << std::endl
        << " PLL tau/alpha: " << tau << "/" << _alpha;
    Logger::get().log(msg);

    // Propergate config
    this->setConfig(Config(Config::typeId<int16_t>(), 4*31.25, 16, 1));
  }

  /** Performs the FM demodulation. */
  virtual void process(const Buffer<std::complex<Scalar> > &buffer, bool allow_overwrite)
  {
    if (0 == buffer.size()) { return; }
    _process(buffer, _buffer);
  }

protected:
  /** The actual demodulation. */
  void _process(const Buffer< std::complex<Scalar> > &in, const Buffer<int16_t> &out)
  {
    // Calc abs phase values
    for (size_t i=1; i<in.size(); i++) {
      std::complex<double> val =
          std::complex<double>(in[i].real(), in[i].imag()) * _lut[_lut_idx>>8];
      _lut_idx += (_lut_incr+_pll_incr);
      _sample_count += 256;
      // Get phase difference
      double phase = std::atan2(double(val.imag()), double(val.real()))/M_PI;
      // low-pass
      _lp_phase = (1.-_alpha)*_lp_phase + _alpha*phase;
      // Correct PLL frequency by LP-ed phase
      _pll_incr += _lp_phase*10;
      std::cout << "Fcorr=" << 22050. * double(_pll_incr)/(1<<16)
                << "; Phi=" << 180*phase << " (" << 180*_lp_phase << ")" << std::endl;
      if (_sample_count >= _subsample) {
        out[_dec_count] = (_lp_phase*(1<<14));
        _sample_count -= _subsample; _dec_count++;
        // If 8 bits has been collected:
        if (8 == _dec_count) {
          // propergate resulting 8 bits
          this->send(out.head(8));
          _dec_count = 0;
        }
      }
    }
  }

protected:
  int _shift;
  /** If true, in-place demodulation is poissible. */
  bool _can_overwrite;
  Buffer< std::complex<SScalar> > _lut;
  /** The current LUT index. It is defined in multiples of 256 for heiher precision. */
  uint16_t _lut_idx;
  /** The LUT increment is defined in multiples of 256 for higher precision. */
  int32_t _lut_incr;
  /** The PLL increment correction is defined in multiples of 256 for higher precision. */
  double _pll_incr;
  double _alpha;
  double _lp_phase;
  size_t _subsample;
  size_t _sample_count;
  size_t _dec_count;
  /** The output buffer, unused if demodulation is performed in-place. */
  Buffer<int16_t> _buffer;
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

  //WavSource src(argv[1]);
  SigGen<int16_t> src(22050., 1024);
  src.addSine(2144);
  AutoCast< std::complex<int16_t> > cast;
  IQBaseBand<int16_t> baseband(0.0, 2144., 400.0, 15, 1, 8000.0);
  BPSK31<int16_t> demod;
  PortSink sink;

  src.connect(&cast, true);
  cast.connect(&baseband, true);
  baseband.connect(spec);
  baseband.connect(&sink);
  baseband.connect(&demod);

  //queue.addIdle(&src, &WavSource::next);
  queue.addIdle(&src, &SigGen<int16_t>::next);

  queue.start();
  app.exec();
  queue.stop();
  queue.wait();

  PortAudio::terminate();

  return 0;
}

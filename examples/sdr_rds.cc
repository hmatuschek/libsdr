#include "sdr.hh"
#include "rtlsource.hh"
#include "baseband.hh"
#include "autocast.hh"
#include "gui/gui.hh"
#include "logger.hh"
#include <signal.h>

#include <QApplication>
#include <QMainWindow>
#include <QThread>

using namespace sdr;

static void __sigint_handler(int signo) {
  // On SIGINT -> stop queue properly
  Queue::get().stop();
}


int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::cerr << "USAGE: sdr_rds FREQUENCY" << std::endl;
    return -1;
  }
  double freq = atof(argv[1]);

  PortAudio::init();
  Queue &queue = Queue::get();

  // Register handler:
  signal(SIGINT, __sigint_handler);

  QApplication app(argc, argv);

  QMainWindow       *win       = new QMainWindow();
  gui::Spectrum     *spec      = new gui::Spectrum(2, 1024, 5);
  gui::SpectrumView *spec_view = new gui::SpectrumView(spec);
  spec_view->setMindB(-200);
  win->setCentralWidget(spec_view);
  win->setMinimumSize(640, 240);

  win->show();

  sdr::Logger::get().addHandler(new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Assemble processing chain
  //RTLSource src(freq, 1e6);
  WavSource src(argv[1]);
  AutoCast< std::complex<int16_t> > cast;
  IQBaseBand<int16_t> baseband(0, 200e3, 16, 5);
  FMDemod<int16_t, int16_t> demod;
  BaseBand<int16_t> mono(0, 15e3, 16, 6);
  BaseBand<int16_t> pilot(19e3, 5e2, 16, 84);
  BaseBand<int16_t> rds(57e3, 3e3, 16, 84);
  PortSink sink;

  src.connect(&cast, true);
  cast.connect(&baseband, true);
  //src.connect(&baseband, true);
  baseband.connect(&demod, true);
  demod.connect(&mono);
  demod.connect(&pilot);
  demod.connect(&rds);

  mono.connect(&sink);
  mono.connect(spec);

  //queue.addStart(&src, &RTLSource::start);
  //queue.addStop(&src, &RTLSource::stop);
  queue.addIdle(&src, &WavSource::next);

  queue.start();
  app.exec();
  queue.stop();
  queue.wait();

  PortAudio::terminate();

  return 0;
}

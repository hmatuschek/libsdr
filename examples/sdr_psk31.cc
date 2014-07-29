#include "baseband.hh"
#include "autocast.hh"
#include "demod.hh"
#include "portaudio.hh"
#include "wavfile.hh"
#include "gui/gui.hh"
#include "psk31.hh"

#include <iostream>
#include <cmath>
#include <csignal>

#include <QApplication>
#include <QMainWindow>
#include <QThread>

using namespace sdr;

static void __sigint_handler(int signo) {
  // On SIGINT -> stop queue properly
  Queue::get().stop();
}





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
  baseband.setCenterFrequency(2144.0);
  BPSK31<int16_t> demod;
  PortSink sink;
  Varicode decode;
  DebugDump<uint8_t> dump;

  src.connect(&cast, true);
  cast.connect(&baseband, true);
  baseband.connect(spec);
  baseband.connect(&sink);
  baseband.connect(&demod);
  demod.connect(&decode, true);
  decode.connect(&dump, true);

  queue.addIdle(&src, &WavSource::next);
  //queue.addIdle(&src, &IQSigGen<double>::next);

  queue.start();
  app.exec();
  queue.stop();
  queue.wait();

  PortAudio::terminate();

  return 0;
}

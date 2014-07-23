#include "sdr.hh"
#include "rtlsource.hh"
#include "baseband.hh"
#include "utils.hh"
#include "gui/gui.hh"
#include <signal.h>
#include "portaudio.hh"

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
  Queue &queue = Queue::get();

  // Register handler:
  signal(SIGINT, __sigint_handler);

  PortAudio::init();

  QApplication app(argc, argv);

  QMainWindow       *win       = new QMainWindow();
  gui::Spectrum     *spec      = new gui::Spectrum(2, 1024, 5);
  gui::WaterFallView *spec_view = new gui::WaterFallView(spec);
  win->setCentralWidget(spec_view);
  win->setMinimumSize(640, 240);

  win->show();

  // Assemble processing chain
  PortSource< int16_t > src(44100.0, 2048);
  AGC<int16_t> agc;
  //IQBaseBand<int16_t> baseband(0, 500e3, 8, 5);
  //AMDemod<int16_t> demod;
  PortSink sink;

  src.connect(&agc, true);
  agc.connect(&sink, true);
  //baseband.connect(&demod);
  //demod.connect(&sink, true);
  src.connect(spec);

  queue.addIdle(&src, &PortSource< int16_t >::next);

  queue.start();
  app.exec();

  queue.stop();
  queue.wait();

  PortAudio::terminate();

  return 0;
}

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

#define F_SAMPLE 11000.0
#define B_SIZE   16384

int main(int argc, char *argv[])
{
  Queue &queue = Queue::get();

  // Register handler:
  signal(SIGINT, __sigint_handler);

  PortAudio::init();

  QApplication app(argc, argv);

  QMainWindow        *win       = new QMainWindow();
  gui::Spectrum      *spec      = new gui::Spectrum(F_SAMPLE/B_SIZE, 1024, 8);
  gui::WaterFallView *spec_view = new gui::WaterFallView(spec);
  win->setCentralWidget(spec_view);
  win->setMinimumSize(640, 240);

  win->show();

  // Assemble processing chain
  PortSource< int16_t > src(F_SAMPLE, 2048);
  AGC< int16_t > agc;

  src.connect(&agc);
  agc.connect(spec);

  queue.addIdle(&src, &PortSource< int16_t >::next);

  queue.start();
  app.exec();

  queue.stop();
  queue.wait();

  PortAudio::terminate();

  return 0;
}

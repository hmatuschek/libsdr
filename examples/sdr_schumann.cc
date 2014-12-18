#include <libsdr/sdr.hh>
#include <signal.h>
#include <libsdr/gui/gui.hh>

#include <QApplication>
#include <QMainWindow>
#include "schumann.hh"


using namespace sdr;



int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  sdr::PortAudio::init();

  sdr::PortSource<int16_t> src(32000, 1024);
  Schumann schumann;
  src.connect(&schumann);

  QMainWindow *win = new QMainWindow();
  sdr::gui::SpectrumView *view = new sdr::gui::SpectrumView(&schumann);
  win->setCentralWidget(view);
  win->show();

  sdr::Queue::get().addIdle(&src, &sdr::PortSource<int16_t>::next);

  sdr::Queue::get().start();
  app.exec();

  sdr::Queue::get().stop();
  sdr::Queue::get().wait();

  sdr::PortAudio::terminate();

  return 0;
}

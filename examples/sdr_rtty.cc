#include "baseband.hh"
#include "autocast.hh"
#include "demod.hh"
#include "wavfile.hh"
#include "portaudio.hh"

using namespace sdr;

static void __sigint_handler(int signo) {
  // On SIGINT -> stop queue properly
  Queue::get().stop();
}


int main(int argc, char *argv[]) {
  if (2 != argc) {
    std::cerr << "Usage: sdr_rtty FILENAME" << std::endl;
    return -1;
  }

  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register handler:
  signal(SIGINT, __sigint_handler);

  PortAudio::init();
  Queue &queue = Queue::get();

  WavSource src(argv[1]);
  AutoCast< std::complex<int16_t> > cast;
  IQBaseBand<int16_t> baseband(0, 2144., 200.0, 63, 1);
  baseband.setCenterFrequency(2144.0);

  PortSink sink;
  src.connect(&cast, true);
  cast.connect(&baseband, true);
  baseband.connect(&sink);

  queue.addIdle(&src, &WavSource::next);

  queue.start();
  queue.wait();

  PortAudio::terminate();

  return 0;
}

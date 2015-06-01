#include "autocast.hh"
#include "portaudio.hh"
#include "wavfile.hh"
#include "afsk.hh"
#include "baudot.hh"
#include "utils.hh"

#include <iostream>
#include <cmath>
#include <csignal>

using namespace sdr;

static void __sigint_handler(int signo) {
  // On SIGINT -> stop queue properly
  Queue::get().stop();
}


int main(int argc, char *argv[])
{
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
  PortSink sink;
  AutoCast<int16_t> cast;
  AFSK fsk(90.90, 930., 1100., AFSK::NORMAL);
  Baudot decoder;
  TextDump dump;

  // Playback
  src.connect(&sink);
  // Cast to int16
  src.connect(&cast);
  // FSK demod
  cast.connect(&fsk);
  // Baudot decoder
  fsk.connect(&decoder);
  // dump to std::cerr
  decoder.connect(&dump);

  // on idle -> read next buffer from input file
  queue.addIdle(&src, &WavSource::next);
  // on end-of-file -> stop queue
  src.addEOS(&queue, &Queue::stop);

  // Start queue
  queue.start();
  // wait for queue to exit
  queue.wait();

  // terminate port audio system properly
  PortAudio::terminate();

  // quit.
  return 0;
}

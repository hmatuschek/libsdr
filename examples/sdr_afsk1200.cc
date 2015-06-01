#include "wavfile.hh"
#include "autocast.hh"
#include "interpolate.hh"
#include "afsk.hh"
#include "ax25.hh"
#include "utils.hh"


using namespace sdr;



int main(int argc, char *argv[]) {
  if (2 > argc) { std::cout << "USAGE: sdr_afsk1200 FILENAME" << std::endl; return -1; }

  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  Queue &queue = Queue::get();

  WavSource src(argv[1], 1024);
  if (! src.isOpen()) { std::cout << "Can not open file " << argv[1] << std::endl; return -1; }

  AutoCast< int16_t > cast;
  AFSK     demod(1200, 1200, 2200, AFSK::TRANSITION);
  AX25     decode;
  TextDump dump;

  src.connect(&cast);
  cast.connect(&demod);
  demod.connect(&decode);
  decode.connect(&dump);

  Queue::get().addIdle(&src, &WavSource::next);
  src.addEOS(&queue, &Queue::stop);

  Queue::get().start();
  Queue::get().wait();

  return 0;
}

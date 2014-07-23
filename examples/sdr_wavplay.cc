#include "sdr.hh"
#include <iostream>

using namespace sdr;


int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::cerr << "USAGE: sdr_wavplay FILENAME" << std::endl;
    return -1;
  }

  Queue &queue = Queue::get();

  PortAudio::init();

  WavSource src(argv[1]);
  if (! src.isOpen() ) {
    std::cerr << "Can not open file " << argv[1] << std::endl;
    return -1;
  }
  queue.addIdle(&src, &WavSource::next);

  RealPart<int16_t> to_real;
  PortSink sink;

  if (src.isReal()) {
    src.connect(&sink, true);
  } else {
    src.connect(&to_real, true);
    to_real.connect(&sink, true);
  }

  // run...
  queue.start();
  queue.wait();

  PortAudio::terminate();
  return 0;
}

#include "demod.hh"
#include "rtlsource.hh"
#include "baseband.hh"
#include "autocast.hh"
#include "portaudio.hh"
#include "wavfile.hh"

#include <iostream>
#include <csignal>

using namespace sdr;

static void __sigint_handler(int signo) {
  std::cerr << "Stop Queue..." << std::endl;
  // On SIGINT -> stop queue properly
  Queue::get().stop();
  Queue::get().wait();
}


int main(int argc, char *argv[]) {
  if (2 > argc) { std::cout << "USAGE: sdr_fm FREQUENCY [OUTPUT.wav]" << std::endl; return -1; }

  // get frequency
  double freq = atof(argv[1]);
  // Get output file (if given)
  std::string outFile;
  if (3 <= argc) { outFile = argv[2]; }

  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register handler:
  signal(SIGINT, __sigint_handler);

  PortAudio::init();

  RTLSource src(freq-100e3, 1e6);
  AutoCast< std::complex<int16_t> > cast;
  IQBaseBand<int16_t> baseband(100e3, 12.5e3, 21, 1, 8000.0);
  baseband.setCenterFrequency(100e3);
  baseband.setFilterFrequency(100e3);
  FMDemod<int16_t>    demod;
  FMDeemph<int16_t>   deemph;
  PortSink            audio;
  WavSink<int16_t>    *wav_sink = 0;

  // Assemble processing chain:
  src.connect(&cast, true);
  cast.connect(&baseband);
  baseband.connect(&demod, true);
  demod.connect(&deemph, true);
  deemph.connect(&audio);

  if (outFile.size()) {
    wav_sink = new WavSink<int16_t>(outFile);
    deemph.connect(wav_sink);
  }

  Queue::get().addStart(&src, &RTLSource::start);
  Queue::get().addStop(&src, &RTLSource::stop);

  Queue::get().start();
  Queue::get().wait();

  if (wav_sink) {
    delete wav_sink;
  }

  PortAudio::terminate();

  return 0;
}

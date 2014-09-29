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
  if (2 > argc) {
    std::cout << "USAGE: sdr_wspr FREQUENCY [OUTPUT.wav]" << std::endl; return -1;
  }

  // get frequency
  double freq = atof(argv[1]);
  // Get output file (if given)
  std::string outFile;
  if (4 <= argc) { outFile = argv[3]; }


  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register handler:
  signal(SIGINT, __sigint_handler);

  PortAudio::init();


  // Create nodes
  RTLSource src(freq, 240e3);   // <- set sample rate to 240kHz -> 20*12kHz
  AutoCast< std::complex<int16_t> > cast;
  IQBaseBand<int16_t> baseband(0, 1500, 3000, 16, 1, 12000);
  USBDemod<int16_t>   usb_demod;
  PortSink            audio;
  WavSink<int16_t>    *wav_sink = outFile.size() ? new WavSink<int16_t>(outFile) : 0;

  src.connect(&cast, true);
  cast.connect(&baseband, true);
  baseband.connect(&usb_demod, true);
  usb_demod.connect(&audio, false);
  if (wav_sink) { usb_demod.connect(wav_sink); }

  Queue::get().addStart(&src, &RTLSource::start);
  Queue::get().addStop(&src, &RTLSource::stop);

  std::cerr << "Start recording at " << src.frequency()
            << "Hz. Press CTRL-C to stop recoding." << std::endl;

  Queue::get().start();
  Queue::get().wait();

  if (wav_sink) { delete wav_sink; }

  PortAudio::terminate();

  std::cerr << "Recording stopped." << std::endl;

  return 0;
}

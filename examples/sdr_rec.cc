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
  if (3 > argc) {
    std::cout << "USAGE: sdr_rec FREQUENCY MODE [OUTPUT.wav]" << std::endl; return -1;
  }

  // get frequency
  double freq = atof(argv[1]);
  std::string mode = argv[2];
  // Get output file (if given)
  std::string outFile;
  if (4 <= argc) { outFile = argv[3]; }

  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register handler:
  signal(SIGINT, __sigint_handler);

  PortAudio::init();

  // obtain base-band config
  double f_center = 0, f_filter = 0, flt_width = 0;
  int sub_sample = 1; double out_f_sample = 12e3;
  if (mode == "WFM") {
    f_center = 0; f_filter = 0; flt_width = 50e3;
    out_f_sample = 48e3;
  } else if (mode == "NFM") {
    f_center = 0; f_filter = 0; flt_width = 12.5e3;
    out_f_sample = 12e3;
  } else if (mode == "AM") {
    f_center = 0; f_filter = 0; flt_width = 15e3;
    out_f_sample = 12e3;
  } else if (mode == "USB") {
    f_center = 0; f_filter = 1500; flt_width = 3e3;
    out_f_sample = 12e3;
  } else if (mode == "LSB") {
    f_center = 0; f_filter = -1500; flt_width = 3e3;
    out_f_sample = 12e3;
  } else {
    std::cerr << "Unknown mode '" << mode
              << "': Possible values are WFM, NFM, AM, USB, LSB." << std::endl;
    return -1;
  }

  // Create nodes
  RTLSource src(freq, 1e6);
  AutoCast< std::complex<int16_t> > cast;
  IQBaseBand<int16_t> baseband(f_center, f_filter, flt_width, 16, sub_sample, out_f_sample);
  FMDemod<int16_t>    *fm_demod = 0;
  FMDeemph<int16_t>   *fm_deemph = 0;
  AMDemod<int16_t>    *am_demod = 0;
  USBDemod<int16_t>   *usb_demod = 0;
  PortSink            audio;
  WavSink<int16_t>    *wav_sink = 0;

  if (outFile.size()) {
    wav_sink = new WavSink<int16_t>(outFile);
  }

  // Assemble processing chain:
  src.connect(&cast, true);
  cast.connect(&baseband);

  if (mode == "WFM") {
    fm_demod = new FMDemod<int16_t>();
    fm_deemph= new FMDeemph<int16_t>();
    baseband.connect(fm_demod, true);
    fm_demod->connect(fm_deemph, true);
    fm_deemph->connect(&audio);
    if (wav_sink) { fm_deemph->connect(wav_sink); }
  } else if (mode == "NFM") {
    fm_demod = new FMDemod<int16_t>();
    fm_deemph= new FMDeemph<int16_t>();
    fm_demod->connect(fm_deemph, true);
    baseband.connect(fm_demod, true);
    fm_demod->connect(fm_deemph, true);
    fm_deemph->connect(&audio);
    if (wav_sink) { fm_deemph->connect(wav_sink); }
  } else if (mode == "AM") {
    am_demod = new AMDemod<int16_t>();
    baseband.connect(am_demod);
    am_demod->connect(&audio);
    if (wav_sink) { am_demod->connect(wav_sink); }
  } else if ((mode == "USB") || (mode == "LSB")){
    usb_demod = new USBDemod<int16_t>();
    baseband.connect(usb_demod);
    usb_demod->connect(&audio);
    if (wav_sink) { usb_demod->connect(wav_sink); }
  }

  Queue::get().addStart(&src, &RTLSource::start);
  Queue::get().addStop(&src, &RTLSource::stop);

  std::cerr << "Start recording at " << src.frequency()
            << "Hz in mode " << mode << ". Press CTRL-C to stop recoding." << std::endl;

  Queue::get().start();
  Queue::get().wait();

  if (fm_demod) { delete fm_demod; }
  if (fm_deemph) { delete fm_deemph; }
  if (usb_demod) { delete usb_demod; }
  if (wav_sink) { delete wav_sink; }

  PortAudio::terminate();

  std::cerr << "Recording stopped." << std::endl;

  return 0;
}

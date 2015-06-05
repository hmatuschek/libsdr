#include "options.hh"
#include "queue.hh"
#include "logger.hh"
#include "wavfile.hh"
#include "autocast.hh"
#include "portaudio.hh"
#include "rtlsource.hh"
#include "baseband.hh"
#include "demod.hh"

#include <iostream>
#include <csignal>

using namespace sdr;

// On SIGINT -> stop queue properly
static void __sigint_handler(int signo) {
  Queue::get().stop();
}

// Command line options
static Options::Definition options[] = {
  {"rtl2832", 'R', Options::FLOAT,
   "Specifies a RTL2832 USB dongle as the input source"},
  {"rtl-device", 0, Options::INTEGER,
   "Specifies the RTL2832 device index. (default 0)"},
  {"disable-rtl-agc", 0 , Options::FLAG,
   "Disables the IF AGC of the RTL2832 device, default on."},
  {"rtl-agc-gain", 0, Options::INTEGER,
   "In conjecture with --disable-rtl-agc, specifies the fixed IF gain of the RTL2832 device."},
  {"rtl-ppm", 0, Options::FLOAT,
   "Specifies the frequency correction for the RTL2832 device in parts-per-million (ppm)."},
  {"audio", 'a', Options::FLAG,
   "Specifies the system audio as the input source."},
  {"audio-iq", 'a', Options::FLAG,
   "Specifies the system audio as the input source (I/Q channels)."},
  {"source-rate", 0, Options::FLOAT,
   "Specifies the sample rate of the input device."},
  {"file", 'f', Options::ANY,
   "Specifies a WAV file as input source."},
  {"demod", 'd', Options::ANY,
   "Specifies the demodulator (wfm, nfm, am, usb, lsb)."},
  {"demod-offset", 0, Options::FLOAT,
   "Specifies the reception offset in Hz. (default 0)"},
  {"loglevel", 0, Options::INTEGER,
   "Specifies the log-level. 0 = DEBUG, 1 = INFO, 2 = WARNING, 3 = ERROR."},
  {"help", '?', Options::FLAG,
   "Prints this help."},
  {0,0,Options::FLAG,0}
};


void print_help() {
  std::cerr << "USAGE: sdr_cmd SOURCE [OPTIONS] OUTPUT" << std::endl << std::endl;
  Options::print_help(std::cerr, options);
}


int main(int argc, char *argv[]) {
  // Parse command line options.
  Options opts;
  if (! Options::parse(options, argc, argv, opts)) {
    print_help(); return -1;
  }

  // If help flag is present -> print and done.
  if (opts.has("help")) { print_help(); return 0; }

  // Install log-handler
  LogLevel loglevel = LOG_INFO;
  if (opts.has("loglevel")) {
    if (0 >= opts.get("loglevel").toInteger()) { loglevel = LOG_DEBUG; }
    else if (1 >= opts.get("loglevel").toInteger()) { loglevel = LOG_INFO; }
    else if (2 >= opts.get("loglevel").toInteger()) { loglevel = LOG_WARNING; }
    else { loglevel = LOG_ERROR; }
  }
  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, loglevel));

  // If no source has been selected -> error
  if (! (opts.has("rtl2832")|opts.has("audio")|opts.has("file"))) {
    std::cerr << "No source has been selected!" << std::endl;
    print_help(); return -1;
  }

  // If no demodulator has been selected.
  if (!opts.has("demod")) {
    std::cerr << "No demodulator has been selected!" << std::endl;
    print_help(); return -1;
  }

  double bbFc = 0, bbFf = 0, bbFw = 0, bbFs = 0;
  // Configure baseband node depending on the selected demodulator
  if ("wfm" == opts.get("demod").toString()) {
    bbFw = 100e3; bbFs = 100e3;
  } else if ("nfm" == opts.get("demod").toString()) {
    bbFw = 12.5e3; bbFs = 22.05e3;
  } else if ("am" == opts.get("demod").toString()) {
    bbFw = 10.0e3; bbFs = 22.05e3;
  } else if ("usb" == opts.get("demod").toString()) {
    bbFf = 1.5e3; bbFw = 3e3; bbFs = 22.05e3;
  } else if ("lsb" == opts.get("demod").toString()) {
    bbFf = -1.5e3; bbFw = 3e3; bbFs = 22.05e3;
  } else {
    std::cerr << "Unknown demodulator '" << opts.get("demod").toString() << "'." << std::endl;
    print_help(); return -1;
  }

  // Register signal handler to stop queue on SIGINT
  signal(SIGINT, __sigint_handler);

  // Init audio system
  PortAudio::init();

  // Get the global queue
  Queue &queue = Queue::get();

  // reference to selected source node
  Source *source = 0;
  // Nodes for file input
  WavSource *wav_src=0;
  AutoCast< std::complex<int16_t> > *wav_cast=0;
  // nodes for real audio input
  PortSource<int16_t> *raudio_src=0;
  AutoCast< std::complex<int16_t> > *raudio_cast=0;
  // nodes for IQ audio input
  PortSource< std::complex<int16_t> > *iqaudio_src=0;
  // nodes for RTL2832 input
  RTLSource *rtl_source=0;
  AutoCast< std::complex<int16_t> > *rtl_cast=0;

  /* Demodulator nodes. */
  Source *demod = 0;
  FMDemod<int16_t> *fm_demod = 0;
  FMDeemph<int16_t> *fm_deemph = 0;
  AMDemod<int16_t> *am_demod = 0;
  USBDemod<int16_t> *ssb_demod = 0;

  // Dispatch by selected source
  if (opts.has("rtl2832")) {
    // Instantiate & config RTL2832 node
    size_t devIdx = 0;
    double sampleRate = 1e6;
    if (opts.has("rtl-device")) { devIdx = opts.get("rtl-device").toInteger(); }
    if (opts.has("source-rate")) { sampleRate = opts.get("source-rate").toFloat(); }
    rtl_source = new RTLSource(opts.get("rtl2832").toFloat(), sampleRate, devIdx);
    if (opts.has("rtl-disable-agc")) { rtl_source->enableAGC(false); }
    if (opts.has("rtl-gain")) { rtl_source->setGain(opts.get("rtl-gain").toFloat()); }
    if (opts.has("rtl-ppm")) { rtl_source->setFreqCorrection(opts.get("rtl-ppm").toFloat()); }
    rtl_cast = new AutoCast< std::complex<int16_t> >();
    rtl_source->connect(rtl_cast);
    source = rtl_cast;
    // Connect start & stop event to RTL2832 device
    queue.addStart(rtl_source, &RTLSource::start);
    queue.addStop(rtl_source, &RTLSource::stop);
  } else if (opts.has("audio")) {
    double sampleRate = 44100;
    if (opts.has("source-rate")) { sampleRate = opts.get("source-rate").toFloat(); }
    raudio_src = new PortSource<int16_t>(sampleRate, 1024);
    raudio_cast = new AutoCast< std::complex<int16_t> >();
    source = raudio_cast;
    // Connect idle event to source
    queue.addIdle(raudio_src, &PortSource<int16_t>::next);
  } else if (opts.has("audio-iq")) {
    double sampleRate = 44100;
    if (opts.has("source-rate")) { sampleRate = opts.get("source-rate").toFloat(); }
    iqaudio_src = new PortSource< std::complex<int16_t> >(sampleRate, 1024);
    source = iqaudio_src;
    // Connect idle event to source
    queue.addIdle(iqaudio_src, &PortSource< std::complex<int16_t> >::next);
  }

  // Shift baseband by offset
  if (opts.has("demod-offset")) {
    bbFc += opts.get("demod-offset").toFloat();
    bbFf += opts.get("demod-offset").toFloat();
  }

  IQBaseBand< int16_t > baseband(bbFc, bbFf, bbFw, 31, 0, bbFs);
  source->connect(&baseband, true);

  // Dispatch by selected demodulator
  if (("wfm" == opts.get("demod").toString()) || ("nfm" == opts.get("demod").toString())) {
    fm_demod = new FMDemod<int16_t>();
    fm_deemph = new FMDeemph<int16_t>();
    source->connect(fm_demod, true);
    fm_demod->connect(fm_deemph, true);
    demod = fm_deemph;
  } else if ("am" == opts.get("demod").toString()) {
    am_demod = new AMDemod<int16_t>();
    source->connect(am_demod, true);
    demod = am_demod;
  } else if (("usb" == opts.get("demod").toString()) || ("lsb" == opts.get("demod").toString())) {
    ssb_demod = new USBDemod<int16_t>();
    source->connect(ssb_demod, true);
    demod = ssb_demod;
  }

  // Enable playback
  PortSink audio_sink;
  demod->connect(&audio_sink, true);

  // Start queue & processing
  queue.start();
  // Wait for queue to to stop
  queue.wait();

  // Free allocated input nodes
  if (rtl_source) { delete rtl_source; }
  if (rtl_cast) { delete rtl_cast; }
  if (raudio_src) { delete raudio_src; }
  if (raudio_cast) { delete raudio_cast; }
  if (iqaudio_src) { delete iqaudio_src; }
  if (wav_src) { delete wav_src; }
  if (wav_cast) { delete wav_cast; }
  if (fm_demod) { delete fm_demod; }
  if (fm_deemph) { delete fm_deemph; }
  if (am_demod) { delete am_demod; }
  if (ssb_demod) { delete ssb_demod; }

  PortAudio::terminate();

  // Done.
  return 0;
}

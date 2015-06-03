/*
 * sdr_pocsag -- A POCSAG receiver using libsdr.
 *
 * (c) 2015 Hannes Matuschek <hmatuschek at gmail dot com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "autocast.hh"
#include "portaudio.hh"
#include "wavfile.hh"
#include "fsk.hh"
#include "utils.hh"
#include "pocsag.hh"
#include "options.hh"
#include "rtlsource.hh"
#include "demod.hh"
#include "baseband.hh"

#include <iostream>
#include <cmath>
#include <csignal>

using namespace sdr;

// On SIGINT -> stop queue properly
static void __sigint_handler(int signo) {
  Queue::get().stop();
}

// Command line options
static Options::Definition options[] = {
  {"frequency", 'F', Options::FLOAT,
   "Selects a RTL2832 as the source and specifies the frequency in Hz."},
  {"correction", 0, Options::FLOAT,
   "Specifies the frequency correction for the RTL2832 device in parts-per-million (ppm)."},
  {"audio", 'a', Options::FLAG, "Selects the system audio as the source."},
  {"file", 'f', Options::ANY, "Selects a WAV file as the source."},
  {"monitor", 'M', Options::FLAG, "Enable sound monitor."},
  {"invert", 0, Options::FLAG, "Inverts mark/space logic."},
  {"help", 0, Options::FLAG, "Prints this help message."},
  {0,0,Options::FLAG,0}
};

void print_help() {
  std::cerr << "USAGE: sdr_pocsag SOURCE [OPTIONS]" << std::endl << std::endl;
  Options::print_help(std::cerr, options);
}


int main(int argc, char *argv[])
{
  // Install log handler
  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register signal handler:
  signal(SIGINT, __sigint_handler);

  // Parse command line options.
  Options opts;
  if (! Options::parse(options, argc, argv, opts)) {
    print_help(); return -1;
  }

  if (opts.has("help")) {
    print_help(); return 0;
  }

  // If no source has been selected
  if (! (opts.has("frequency")|opts.has("audio")|opts.has("file"))) {
    print_help(); return -1;
  }

  // Init audio system
  PortAudio::init();

  // Get the global queue
  Queue &queue = Queue::get();

  // pointer to the selected source
  Source *src = 0;

  // Nodes for WAV file input
  WavSource *wav_src=0;
  AutoCast<int16_t> *wav_cast=0;

  // Nodes for PortAudio input
  PortSource<int16_t> *audio_src=0;

  // Nodes for RTL2832 input
  RTLSource *rtl_source=0;
  AutoCast< std::complex<int16_t> > *rtl_cast=0;
  IQBaseBand<int16_t> *rtl_baseband=0;
  FMDemod<int16_t> *rtl_demod=0;
  FMDeemph<int16_t> *rtl_deemph=0;

  if (opts.has("frequency")) {
    // Assemble processing chain for the RTL2832 intput
    rtl_source   = new RTLSource(opts.get("frequency").toFloat());
    if (opts.has("correction")) {
      rtl_source->setFreqCorrection(opts.get("correction").toFloat());
    }
    rtl_cast     = new AutoCast< std::complex<int16_t> >();
    rtl_baseband = new IQBaseBand<int16_t>(0, 12.5e3, 21, 0, 22050.0);
    rtl_demod    = new FMDemod<int16_t>();
    rtl_deemph   = new FMDeemph<int16_t>();
    rtl_source->connect(rtl_cast);
    rtl_cast->connect(rtl_baseband, true);
    rtl_baseband->connect(rtl_demod);
    rtl_demod->connect(rtl_deemph);
    // FM deemph. is source for decoder
    src = rtl_deemph;
    // On queue start, start RTL source
    Queue::get().addStart(rtl_source, &RTLSource::start);
    // On queue stop, stop RTL source
    Queue::get().addStop(rtl_source, &RTLSource::stop);
  } else if (opts.has("audio")) {
    // Configure audio source
    audio_src = new PortSource<int16_t>(22010., 1024);
    src = audio_src;
    // On queue idle, read next chunk from audio source
    Queue::get().addIdle(audio_src, &PortSource<int16_t>::next);
  } else if (opts.has("file")) {
    // Assemble processing chain for WAV file input
    wav_src = new WavSource(opts.get("file").toString());
    wav_cast = new AutoCast<int16_t>();
    wav_src->connect(wav_cast);
    src = wav_cast;
    // On queue idle, read next chunk from file
    Queue::get().addIdle(wav_src, &WavSource::next);
    // On end of file, stop queue
    wav_src->addEOS(&(Queue::get()), &Queue::stop);
  }

  /* Common demodulation nodes. */
  // amplitude detector
  ASKDetector<int16_t> detector(opts.has("invert"));
  // Bit decoder
  BitStream bits(1200, BitStream::NORMAL);
  // POCSAG decoder
  POCSAGDump pocsag(std::cout);
  // Audio sink for monitor
  PortSink sink;


  // connect source ASK detector
  src->connect(&detector);
  // detector to bit decoder
  detector.connect(&bits);
  // and bit decoder to POCSAG decoder and print
  bits.connect(&pocsag);

  // If monitor is enabled -> connect to sink
  if (opts.has("monitor")) {
    src->connect(&sink);
  }

  // Start queue
  queue.start();
  // wait for queue to exit
  queue.wait();

  // Free allocated nodes
  if (rtl_source) { delete rtl_source; }
  if (rtl_cast) { delete rtl_cast; }
  if (rtl_baseband) { delete rtl_baseband; }
  if (rtl_demod) { delete rtl_demod; }
  if (rtl_deemph) { delete rtl_deemph; }
  if (audio_src) { delete audio_src; }
  if (wav_src) { delete wav_src; }
  if (wav_cast) { delete wav_cast; }

  // terminate port audio system properly
  PortAudio::terminate();

  // quit.
  return 0;
}

/** @mainpage A C++ library for software defined radio (SDR).
 *
 * libsdr is a simple C++ library allowing to assemble software defined radio (SDR) applications
 * easily. The library is a collection of (mostly) template classes implementing a wide varity of
 * procssing nodes. By connecting these processing nodes, a stream-processing chain is constructed
 * which turns raw input data into something meaningful.
 *
 * Please note, I have written this library for my own amusement and to learn something about SDR.
 * If you search for a more complete and efficient SDR library, consider GNU radio.
 *
 * A processing node is either a @c sdr::Source, @c sdr::Sink, both or may provide one or more
 * sources or sinks. A source is always connected to a sink. Another important object is the
 * @c sdr::Queue. It is a singleton class that orchestrates the processing of the data. It may
 * request further data from the sources once all present data has been processed. It also
 * routes the date from the sources to the sinks.
 *
 * \section intro A practical introduction
 * The following examples shows a trivial application that recods some audio from the systems
 * default audio source and play it back.
 * \code
 * #include <libsdr/sdr.hh>
 *
 * int main(int argc, char *argv[]) {
 *   // Initialize PortAudio system
 *   sdr::PortAudio::init();
 *
 *   // Create an audio source using PortAudio
 *   sdr::PortSource<int16_t> source(44.1e3);
 *   // Create an audio sink using PortAudio
 *   sdr::PortSink sink;
 *
 *   // Connect them
 *   source.connect(&sink);
 *
 *   // Read new data from audio sink if queue is idle
 *   sdr::Queue::get().addIdle(&source, &sdr::PortSource<int16_t>::next);
 *
 *   // Get and start queue
 *   sdr::Queue::get().start();
 *   // Wait for queue to stop
 *   sdr::Queue::get().wait();
 *
 *   // Terminate PortAudio system
 *   sdr::PortAudio::terminate();
 *
 *   // done.
 *   return 0;
 * }
 * \endcode
 *
 * First, the PortAudio system gets initialized.
 *
 * Then, the audio source is constructed. The
 * argument specifies the sample rate in Hz. Here a sample rate of 44100 Hz is used. The template
 * argument of @c sdr::PortSource specifies the input type. Here a signed 16bit integer is used.
 * The audio source will have only one channel (mono).
 *
 * The second node is the audio (playback) sink, which takes no arguments. It gets configured once
 * the source is connected to the sink with @c source.connect(&sink).
 *
 * In a next step, the sources @c next method gets connected to the "idle" signal of the queue.
 * This is necessary as the audio source does not send data by its own. Whenever the @c next method
 * gets called, the source will send a certain amount of captured data to the connected sinks. Some
 * nodes will send data to the connected sinks without the need to explicit triggering. The
 * @c sdr::PortSource node, however, needs that explicit triggering. The "idle" event gets emitted
 * once the queue gets empty, means whenever all data has been processes (here, played back).
 *
 * As mentioned aboce, the queue is a singleton class. Means that for every process, there is
 * exactly one instance of the queue. This singleton instance is accessed by calling the static
 * method @c sdr::Queue::get. By calling @c sdr::Queue::start, the queue is started in a separate
 * thread. This threads is responsible for all the processing of the data which allows to perform
 * other tasks in the main thread, i.e. GUI stuff. A call to @c sdr::Queue::wait, will wait for
 * the processing thread to exit, which will never happen in that particular example.
 *
 * The queue can be stopped by calling the @c sdr::Queue::stop method. This can be implemented for
 * this example by the means of process signals.
 *
 * \code
 * #include <signal.h>
 * ...
 *
 * static void __sigint_handler(int signo) {
 *   // On SIGINT -> stop queue properly
 *   sdr::Queue::get().stop();
 * }
 *
 * int main(int argc, char *argv[]) {
 *   // Register signal handler
 *   signal(SIGINT, __sigint_handler);
 *   ...
 * }
 * \endcode
 *
 * Whenever a SIGINT is send to the process, i.e. by pressing CTRL+C, the @c sdr::Queue::stop method
 * gets called. This will cause the processing thread of the queue to exit and the call to
 * @c sdr::Queue::wait to return.
 *
 * \subsection example2 Queue less operation
 * Sometimes, the queue is simply not needed. This is particularily the case if the data processing
 * can happen in the main thread, i.e. if there is not GUI. The example above can be implemented
 * without the Queue, as the main thread is just waiting for the processing thread to exit.
 *
 * \code
 * #include <libsdr/sdr.hh>
 *
 * int main(int argc, char *argv[]) {
 *   // Initialize PortAudio system
 *   sdr::PortAudio::init();
 *
 *   // Create an audio source using PortAudio
 *   sdr::PortSource<int16_t> source(44.1e3);
 *   // Create an audio sink using PortAudio
 *   sdr::PortSink sink;
 *
 *   // Connect them directly
 *   source.connect(&sink, true);
 *
 *   // Loop to infinity7
 *   while(true) { source.next(); }
 *
 *   // Terminate PortAudio system
 *   sdr::PortAudio::terminate();
 *
 *   // done.
 *   return 0;
 * }
 * \endcode
 *
 * The major difference between the first example and this one, is the way how the nodes are
 * connected. The @c sdr::Source::connect method takes an optional argument specifying wheter the
 * source is connected directly to the sink or not. If @c false (the default) is specified, the
 * data of the source will be send to the Queue first. In a direct connection (passing @c true), the
 * source will send the data directly to the sink, bypassing the queue.
 *
 * Instead of starting the processing thread of the queue, here the main thread is doing all the
 * work by calling the @c next mehtod of the audio source.
 *
 * \subsection logging Log messages
 * During configuration and operation, processing nodes will send log messages of different levels
 * (DEBUG, INFO, WARNING, ERROR), which allow to debug the operation of the complete processing
 * chain. These log messages are passed around using the build-in @c sdr::Logger class. To make
 * them visible, a log handler must be installed.
 *
 * \code
 * int main(int argc, char *argv[]) {
 *   ...
 *   // Install the log handler...
 *   sdr::Logger::get().addHandler(
 *       new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));
 *   ...
 * }
 * \endcode
 *
 * Like the @c sdr::Queue, the logger is also a singleton object, which can be obtained by
 * @c sdr::Logger::get. By calling @c sdr::Logger::addHandler, a new message handler is installed.
 * In this example, a @c sdr::StreamLogHandler instance is installed, which serializes the
 * log messages into @c std::cerr.
 *
 * \subsection intro_summary In summary
 * In summary, the complete example above using the queue including a singal handler to properly
 * terminate the application by SIGINT and a log handler will look like
 *
 * \code
 * #include <libsdr/sdr.hh>
 * #include <signal.h>
 *
 * static void __sigint_handler(int signo) {
 *   // On SIGINT -> stop queue properly
 *   sdr::Queue::get().stop();
 * }
 *
 * int main(int argc, char *argv[]) {
 *   // Register signal handler
 *   signal(SIGINT, __sigint_handler);
 *
 *   // Initialize PortAudio system
 *   sdr::PortAudio::init();
 *
 *   // Create an audio source using PortAudio
 *   sdr::PortSource<int16_t> source(44.1e3);
 *   // Create an audio sink using PortAudio
 *   sdr::PortSink sink;
 *
 *   // Connect them
 *   source.connect(&sink);
 *
 *   // Read new data from audio sink if queue is idle
 *   sdr::Queue::get().addIdle(&source, &sdr::PortSource<int16_t>::next);
 *
 *   // Get and start queue
 *   sdr::Queue::get().start();
 *   // Wait for queue to stop
 *   sdr::Queue::get().wait();
 *
 *   // Terminate PortAudio system
 *   sdr::PortAudio::terminate();
 *
 *   // done.
 *   return 0;
 * }
 * \endcode
 * This may appear quiet bloated for such a simple application. I designed the library to be rather
 * explicit. No feature is implicit and hidden from the user. This turns simple examples like the
 * one above quite bloated but it is imediately clear how the example works whithout any knowledge
 * of "hidden features" and the complexity does not suddenly increases for non-trivial examples.
 *
 * Finally, we may have a look at a more relaistic example implementing a FM broadcast receiver
 * using a RTL2832 based USB dongle as the input source.
 * \code
 * #include <libsdr/sdr.hh>
 * #include <signal.h>
 *
 * static void __sigint_handler(int signo) {
 *   // On SIGINT -> stop queue properly
 *   sdr::Queue::get().stop();
 * }
 *
 * int main(int argc, char *argv[]) {
 *   // Register signal handler
 *   signal(SIGINT, __sigint_handler);
 *
 *   // Initialize PortAudio system
 *   sdr::PortAudio::init();
 *
 *   // Frequency of the FM station (in Hz)
 *   double freq = 100e6;
 *
 *   // Create a RTL2832 input node
 *   sdr::RTLSource src(freq);
 *   // Filter 100kHz around the center frequency (0) with an 16th order FIR filter and
 *   // subsample the result to a sample rate of approx. 100kHz.
 *   sdr::IQBaseBand<int8_t> baseband(0, 100e3, 16, 0, 100e3);
 *   // FM demodulator, takes a complex int8_t stream and returns a real int16_t stream
 *   sdr::FMDemod<int8_t, int16_t> demod;
 *   // Deemphesize the result (actually part of the demodulation)
 *   sdr::FMDeemph<int16_t> deemph;
 *   // Playback the final signal
 *   sdr::PortSink sink;
 *
 *   // Connect signals
 *   src.connect(&baseband, true);
 *   baseband.connect(&demod);
 *   demod.connect(&deemph);
 *   deemph.connect(&sink);
 *
 *   // Connect start and stop signals of Queue to RTL2832 source
 *   sdr::Queue::get().addStart(&src, &sdr::RTLSource::start);
 *   sdr::Queue::get().addStop(&src, &sdr::RTLSource::stop);
 *
 *   // Start queue
 *   sdr::Queue::get().start();
 *   // Wait for queue
 *   sdr::Queue::get().wait();
 *
 *   // Terminate PortAudio system
 *   sdr::PortAudio::terminate();
 *
 *   // Done...
 *   return 0;
 * }
 * \endcode
 *
 */

#ifndef __SDR_HH__
#define __SDR_HH__

#include "config.hh"
#include "operators.hh"
#include "math.hh"
#include "traits.hh"
#include "exception.hh"
#include "buffer.hh"
#include "node.hh"
#include "queue.hh"
#include "combine.hh"
#include "logger.hh"
#include "options.hh"

#include "utils.hh"
#include "siggen.hh"
#include "buffernode.hh"
#include "wavfile.hh"
#include "firfilter.hh"
#include "autocast.hh"
#include "freqshift.hh"
#include "interpolate.hh"
#include "subsample.hh"
#include "baseband.hh"

#include "demod.hh"
#include "psk31.hh"

#include "fftplan.hh"

#ifdef SDR_WITH_FFTW
#include "filternode.hh"
#endif

#ifdef SDR_WITH_PORTAUDIO
#include "portaudio.hh"
#endif

#ifdef SDR_WITH_RTLSDR
#include "rtlsource.hh"
#endif

#endif // __SDR_HH__

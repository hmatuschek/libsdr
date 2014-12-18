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
 * The following examples shows a trivial application that recods some audio from the systems
 * default audio source and play it back.
 * \code
 *  #include <libsdr/sdr.hh>
 *
 *  int main(int argc, char *argv[]) {
 *    // Initialize PortAudio system
 *    sdr::PortAudio::init();
 *
 *    // Create an audio source using PortAudio
 *    sdr::PortSource<int16_t> source(44.1e3);
 *    // Create an audio sink using PortAudio
 *    sdr::PortSink sink;
 *
 *    // Connect them
 *    source.connect(&sink);
 *
 *    // Read new data from audio sink if queue is idle
 *    sdr::Queue::get().addIdle(&source, &sdr::PortSource<int16_t>::next);
 *
 *    // Get and start queue
 *    sdr::Queue::get().start();
 *    // Wait for queue to stop
 *    sdr::Queue::get().wait();
 *
 *    // Terminate PortAudio system
 *    sdr::PortAudio::terminate();
 *
 *    // done.
 *    return 0;
 *  }
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
#include "demod.hh"
#include "siggen.hh"
#include "buffernode.hh"
#include "wavfile.hh"
#include "firfilter.hh"
#include "autocast.hh"

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

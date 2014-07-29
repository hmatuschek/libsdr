/** @mainpage A C++ library for software defined radio (SDR).
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

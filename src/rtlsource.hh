#ifndef __SDR_RTLSOURCE_HH__
#define __SDR_RTLSOURCE_HH__

#include <rtl-sdr.h>
#include <pthread.h>
#include "node.hh"


namespace sdr {

/** Implements a @c uint_8 I/Q source for RTL2832 based TV dongles. */
class RTLSource: public Source
{
public:
  /** Constructor.
   *
   * By default the gain is set to be automatically determined, this can be changed with the
   * @c enableAGC and @c setGain methods.
   *
   * @param frequency Specifies the tuner frequency.
   * @param sample_rate Specifies the sample rate.  */
  RTLSource(double frequency, double sample_rate=1000000);

  /** Destructor. */
  virtual ~RTLSource();

  /** Returns the freuency of the tuner. */
  inline double frequency() const { return _frequency; }
  /** (Re-) Sets the tuner frequency. */
  void setFrequency(double frequency);

  /** Returns the frequency correction in parts per million (ppm). */
  inline double freqCorrection() const { return rtlsdr_get_freq_correction(_device); }
  /** (Re-) Sets the frequency correction in ppm. */
  void setFreqCorrection(double ppm);

  /** Returns the sample rate. */
  inline double sampleRate() const { return _sample_rate; }
  /** (Re-) sets the sample rate. This method also triggers the reconfiguration of all
   * connected sinks. */
  void setSampleRate(double sample_rate);

  /** Returns true if AGC is enabled. */
  inline bool agcEnabled() const { return _agc_enabled; }
  /** Enable/Disable AGC. */
  void enableAGC(bool enable);

  /** Returns the tuner gain. */
  inline double gain() const { return rtlsdr_get_tuner_gain(_device); }
  /** (Re-) Sets the tuner gain. Has no effect in AGC mode. */
  void setGain(double gain);

  /** Starts the reception. */
  void start();
  /** Stops the reception. */
  void stop();


protected:
  /** Prallel routine to receive some data from the device. */
  static void *__rtl_srd_parallel_main(void *ctx);
  /** Callback to process received data. */
  static void __rtl_sdr_callback(unsigned char *buffer, uint32_t len, void *ctx);

protected:
  /** The current tuner frequency. */
  double _frequency;
  /** The current sample rate. */
  double _sample_rate;
  /** If true, the AGC is enabled. */
  bool _agc_enabled;
  /** The buffer size. */
  size_t _buffer_size;
  /** The RTL2832 device object. */
  rtlsdr_dev_t *_device;
  /** The thread object. */
  pthread_t _thread;
};


}

#endif // __SDR_RTLSOURCE_HH__

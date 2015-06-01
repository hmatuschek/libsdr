#ifndef __SDR_RTLSOURCE_HH__
#define __SDR_RTLSOURCE_HH__

#include <rtl-sdr.h>
#include <pthread.h>
#include "node.hh"


namespace sdr {

/** Implements a @c uint_8 I/Q source for RTL2832 based TV dongles.
 * This source runs in its own thread, hence the user does not need to trigger the reception of
 * the next data chunk explicitly. The reception is started by calling the @c start method and
 * stopped by calling the @c stop method.
 * @ingroup sources */
class RTLSource: public Source
{
public:
  /** Constructor.
   *
   * By default the gain is set to be automatically determined, this can be changed with the
   * @c enableAGC and @c setGain methods.
   *
   * @param frequency Specifies the tuner frequency.
   * @param sample_rate Specifies the sample rate in Hz.
   * @param device_idx Specifies the device to be used. The @c numDevices
   *        and @c deviceName static method can be used to select the desired device index. */
  RTLSource(double frequency, double sample_rate=1e6, size_t device_idx=0);

  /** Destructor. */
  virtual ~RTLSource();

  /** Returns the tuner frequency. */
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
  /** Retunrs a vector of supported gain factors. */
  inline const std::vector<double> & gainFactors() const { return _gains; }

  /** Starts the reception. */
  void start();
  /** Stops the reception. */
  void stop();

  /** Returns the number of compatible devices found. */
  static size_t numDevices();
  /** Returns the name of the specified device. */
  static std::string deviceName(size_t idx);

protected:
  /** Parallel routine to receive some data from the device. */
  static void *__rtl_sdr_parallel_main(void *ctx);
  /** Callback to process received data. */
  static void __rtl_sdr_callback(unsigned char *buffer, uint32_t len, void *ctx);

protected:
  /** The current tuner frequency. */
  double _frequency;
  /** The current sample rate. */
  double _sample_rate;
  /** If true, the AGC is enabled. */
  bool _agc_enabled;
  /** A vector of gain factors supported by the device. */
  std::vector<double> _gains;
  /** The buffer size. */
  size_t _buffer_size;
  /** The RTL2832 device object. */
  rtlsdr_dev_t *_device;
  /** The thread object. */
  pthread_t _thread;
};


}

#endif // __SDR_RTLSOURCE_HH__

#include "rtlsource.hh"
#include "logger.hh"

using namespace sdr;


RTLSource::RTLSource(double frequency, double sample_rate, size_t device_idx)
  : Source(), _frequency(frequency), _sample_rate(sample_rate), _agc_enabled(true), _gains(),
    _buffer_size(131072), _device(0)
{
  {
    LogMessage msg(LOG_DEBUG);
    msg << "Found " << rtlsdr_get_device_count()
        << " RTL2832 devices, using No. " << device_idx << ".";
    Logger::get().log(msg);
  }

  // Open device
  if (0 < rtlsdr_get_device_count()) {
    if (rtlsdr_open(&_device, device_idx)) {
      ConfigError err; err << "Can not open RTL2832 USB device " << device_idx;
      throw err;
    }
  } else {
    ConfigError err; err << "Can not open RTL2832 USB device: No device with index "
                         << device_idx << " found.";
    throw err;
  }

  {
    LogMessage msg(LOG_DEBUG);
    msg << "Using device: " << rtlsdr_get_device_name(device_idx);
    Logger::get().log(msg);
  }

  // Try to set center frequency
  if (0 < _frequency) { setFrequency(_frequency); }
  // Set sample rate
  if (_sample_rate > 0) { setSampleRate(sample_rate); }

  // Query gain factors:
  int num_gains = rtlsdr_get_tuner_gains(_device, 0);
  if (num_gains > 0) {
    int* gains = new int[num_gains];
    rtlsdr_get_tuner_gains(_device, gains);
    _gains.reserve(num_gains);
    for (int i=0; i<num_gains; i++) { _gains.push_back(gains[i]); }
    delete gains;
  }

  // Enable AGC/manual gain
  enableAGC(_agc_enabled);

  // Clear or allocate buffers
  rtlsdr_reset_buffer(_device);

  // Propergate config:
  this->setConfig(Config(Config::typeId< std::complex<uint8_t> >(), _sample_rate, _buffer_size, 15));
}


RTLSource::~RTLSource() {
  rtlsdr_close(_device);
}


void
RTLSource::setFrequency(double frequency) {
  rtlsdr_set_center_freq(_device, uint32_t(frequency));
  // Update center frequency
  _frequency = double (rtlsdr_get_center_freq(_device));
}

void
RTLSource::setFreqCorrection(double ppm) {
  rtlsdr_set_freq_correction(_device, ppm);
}

void
RTLSource::setSampleRate(double sample_rate) {
  uint32_t sr = sample_rate;
  if (sr < 225001) { sr = 225001; }
  else if ((sr>300000) && (sr<900001)) { sr = 900001; }
  else if (sr>2400000) { sr = 2400000; }

  rtlsdr_set_sample_rate(_device, sr);
  rtlsdr_reset_buffer(_device);
  _sample_rate = rtlsdr_get_sample_rate(_device);

  this->setConfig(Config(Config::Type_cu8, _sample_rate, _buffer_size, 15));
}

void
RTLSource::enableAGC(bool enable) {
  _agc_enabled = enable;
  rtlsdr_set_tuner_gain_mode(_device, !_agc_enabled);
  rtlsdr_set_agc_mode(_device, _agc_enabled);
}

void
RTLSource::setGain(double gain) {
  if (!_agc_enabled) {
    rtlsdr_set_tuner_gain(_device, gain);
  }
}


void
RTLSource::start() {
  pthread_create(&_thread, 0, RTLSource::__rtl_sdr_parallel_main, this);
}

void
RTLSource::stop() {
  void *p;
  // stop async reading of RTL device
  rtlsdr_cancel_async(_device);
  // Wait for blocked thread to exit:
  //  This is important to ensure that no new messages are added to the
  //  queue after this function returned.
  pthread_join(_thread, &p);
}

size_t
RTLSource::numDevices() {
  return rtlsdr_get_device_count();
}

std::string
RTLSource::deviceName(size_t idx) {
  return rtlsdr_get_device_name(idx);
}


void *
RTLSource::__rtl_sdr_parallel_main(void *ctx) {
  RTLSource *self = reinterpret_cast<RTLSource *>(ctx);
  rtlsdr_read_async(self->_device, &RTLSource::__rtl_sdr_callback, self,
                    15, self->_buffer_size*2);
  return 0;
}

void
RTLSource::__rtl_sdr_callback(unsigned char *buffer, uint32_t len, void *ctx) {
  RTLSource *self = reinterpret_cast<RTLSource *>(ctx);
  self->send(Buffer< std::complex<uint8_t> >((std::complex<uint8_t> *)buffer, len/2));
}

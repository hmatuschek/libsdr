#include "spectrum.hh"
#include "config.hh"

using namespace sdr;
using namespace sdr::gui;


/* ********************************************************************************************* *
 * SpectrumProvider
 * ********************************************************************************************* */
SpectrumProvider::SpectrumProvider(QObject *parent)
  : QObject(parent)
{
  // pass...
}

SpectrumProvider::~SpectrumProvider() {
  // pass...
}


/* ********************************************************************************************* *
 * Spectrum
 * ********************************************************************************************* */
Spectrum::Spectrum(double rrate, size_t fftsize, size_t max_avg, QObject *parent) :
  SpectrumProvider(parent), _rrate(rrate), _fft_size(fftsize), _fft_buffer(fftsize),
  _compute(fftsize), _spectrum(fftsize), _sample_count(0), _N_samples(0),
  _trafo_count(0), _Ntrafo(max_avg),
  _samples_left(0), _input_type(Config::Type_UNDEFINED), _sample_rate(0)
{
  // Construct FFT plan
  _plan = fftw_plan_dft_1d(
        fftsize, (fftw_complex *)_fft_buffer.ptr(), (fftw_complex *)_fft_buffer.ptr(),
        FFTW_FORWARD, FFTW_ESTIMATE);

  // Set spectrum to 0:
  for (size_t i=0; i<_fft_size; i++) { _spectrum[i] = 0; _compute[i] = 0; }
}

Spectrum::~Spectrum() {
  fftw_destroy_plan(_plan);
}


void
Spectrum::config(const Config &src_cfg) {
  // Requires at least sample rate and type
  if (!src_cfg.hasType() || !src_cfg.hasSampleRate()) { return; }

  // Store sample rate
  _sample_rate = src_cfg.sampleRate();
  _samples_left = 0;
  _trafo_count = 0; _sample_count=0;
  _input_type = src_cfg.type();
  // Compute number of averages to compute to meet rrate approximately
  _N_samples = _sample_rate/(_Ntrafo*_rrate);

  LogMessage msg(LOG_DEBUG);
  msg << "Configured SpectrumView: " << std::endl
      << " Data type: " << _input_type << std::endl
      << " sample-rate: " << _sample_rate << std::endl
      << " FFT size: " << _fft_size << std::endl
      << " # sample drops: " << _N_samples-1 << std::endl
      << " # averages: " << _Ntrafo << std::endl
      << " refresh rate: " << _sample_rate/(_N_samples*_Ntrafo) << "Hz";
  Logger::get().log(msg);

  // Signal spectrum reconfiguration
  emit spectrumConfigured();
}


bool
Spectrum::isInputReal() const {
  switch (_input_type) {
  case Config::Type_u8:
  case Config::Type_s8:
  case Config::Type_u16:
  case Config::Type_s16:
  case Config::Type_f32:
  case Config::Type_f64:
    return true;
  default:
    break;
  }
  return false;
}

double
Spectrum::sampleRate() const {
  return _sample_rate;
}

size_t
Spectrum::fftSize() const {
  return _fft_size;
}

const Buffer<double> &
Spectrum::spectrum() const {
  return _spectrum;
}

void
Spectrum::handleBuffer(const RawBuffer &buffer, bool allow_overwrite)
{
  double scale=1, offset=0;
  switch (_input_type) {
  case Config::Type_u8:   scale = 1<<8;  offset=-128;   break;
  case Config::Type_cu8:  scale = 1<<8;  offset=-128;   break;
  case Config::Type_s8:   scale = 1<<8;  offset=0;      break;
  case Config::Type_cs8:  scale = 1<<8;  offset=0;      break;
  case Config::Type_u16:  scale = 1<<16; offset=-32768; break;
  case Config::Type_cu16: scale = 1<<16; offset=-32768; break;
  case Config::Type_s16:  scale = 1<<16; offset=0;      break;
  case Config::Type_cs16: scale = 1<<16; offset=0;      break;
  default: break;
  }

  // Dispatch by input type:
  if (Config::Type_u8 == _input_type) {
    Buffer<uint8_t> input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = (double(input[i])+offset)/scale; _samples_left++;
      // Once a FFT buffer is completed -> transform
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_s8 == _input_type) {
    Buffer<int8_t> input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = (double(input[i])+offset)/scale; _samples_left++;
      // Once a FFT buffer is completed -> transform
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_u16 == _input_type) {
    Buffer<uint16_t> input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = (double(input[i])+offset)/scale; _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_s16 == _input_type) {
    Buffer<int16_t> input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = (double(input[i])+offset)/scale; _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_f32 == _input_type) {
    Buffer<float> input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = input[i]; _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_f64 == _input_type) {
    Buffer<double> input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = input[i]; _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0;}
    }
  } else if (Config::Type_cu8 == _input_type) {
    Buffer< std::complex<uint8_t> > input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = std::complex<double>(
            double(input[i].real())+offset,
            double(input[i].imag())+offset)/scale;
      _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_cs8 == _input_type) {
    Buffer< std::complex<int8_t> > input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = std::complex<double>(
            double(input[i].real())+offset,
            double(input[i].imag())+offset)/scale;
      _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_cu16 == _input_type) {
    Buffer< std::complex<uint16_t> > input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = std::complex<double>(
            double(input[i].real())+offset,double(input[i].imag())+offset)/scale;
      _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_cs16 == _input_type) {
    Buffer< std::complex<int16_t> > input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = std::complex<double>(
            double(input[i].real())+offset,double(input[i].imag())+offset)/scale;
      _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_cf32 == _input_type) {
    Buffer< std::complex<float> > input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = input[i]; _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else if (Config::Type_cf64 == _input_type) {
    Buffer< std::complex<double> > input(buffer);
    for (size_t i=0; i<input.size(); i++, _sample_count++) {
      // Skip until _N_samples is reached
      if (_sample_count < _N_samples) { continue; }
      // Copy value into buffer
      _fft_buffer[_samples_left] = input[i]; _samples_left++;
      if (_samples_left == _fft_size) { _updateFFT(); _samples_left=0; _sample_count=0; }
    }
  } else {
    RuntimeError err;
    err << "SpectrumView: Can not process buffer of type " << _input_type
        << ", unsupported type.";
    throw err;
  }
}


void
Spectrum::_updateFFT() {
  // calc fft
  fftw_execute(_plan);
  // Update _compute with PSD
  for(size_t i=0; i<_fft_size; i++) {
    _compute[i] += std::real(std::conj(_fft_buffer[i])*_fft_buffer[i])/_Ntrafo;
  }
  _trafo_count++;
  // If number of averages reached:
  if (_trafo_count == _Ntrafo) {
    // copy _compute to _spectrum and reset _compute to 0
    for (size_t i=0; i<_fft_size; i++) {
      _spectrum[i] = _compute[i];
      _compute[i] = 0;
    }
    // done...
    _trafo_count=0;
    // signal spectrum update
    emit spectrumUpdated();
  }
}

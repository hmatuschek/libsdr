#ifndef SCHUMANN_HH
#define SCHUMANN_HH

#include <libsdr/sdr.hh>
#include <libsdr/gui/gui.hh>

class Schumann: public sdr::gui::SpectrumProvider, public sdr::Sink<int16_t>
{
  Q_OBJECT

public:
  explicit Schumann(QObject *parent=0)
    : sdr::gui::SpectrumProvider(parent), sdr::Sink<int16_t>(),
      _buffer_idx(0), _avg_count(0), _buffer(2048), _spectrum(2048),
      _spec_count(0), _avg_spectrum(2048), _subsample(0), _Fs(0), _fft(_buffer, sdr::FFT::FORWARD)
  {
    // init spectrum and buffer
    for (size_t i=0; i<2048; i++) {
      _buffer[i] = 0;
      _spectrum[i] = 0;
    }
  }

  virtual ~Schumann() {
    // pass...
  }

  void config(const sdr::Config &src_cfg) {
    // Requires type, sample rate & buffer size
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }

    // Check buffer type
    if (sdr::Config::typeId< int16_t >() != src_cfg.type()) {
      sdr::ConfigError err;
      err << "Can not configure Schumann: Invalid type " << src_cfg.type()
          << ", expected " << sdr::Config::typeId<int16_t>();
      throw err;
    }

    // Check input sample rate
    double Fs = src_cfg.sampleRate();
    if (40 > Fs) {
      sdr::ConfigError err;
      err << "Can not configure Schumann: Input sample rate too low! The Schumann node requires at "
          << "least a sample rate of 40Hz, got " << Fs << "Hz";
      throw err;
    }

    // Compute subsample
    _subsample = Fs/40; _Fs = Fs/_subsample;
  }

  void process(const sdr::Buffer<int16_t> &buffer, bool allow_overwrite) {
    for (size_t i=0; i<buffer.size(); i++) {
      // Add value to buffer
      _buffer[_buffer_idx] += float(buffer[i])/((1<<15)*_subsample); _avg_count++;
      // If subsample count is reached:
      if (_avg_count == _subsample) { _buffer_idx++; _avg_count = 0; }
      if (_buffer_idx < 2048) { _buffer[_buffer_idx] = 0; }
      if (_buffer_idx == 2048) {
        // Compute FFT and add to spectrum:
        _fft();
        _spec_count++;
        for (size_t i=0; i<2048; i++) {
          _spectrum[i] += _buffer[i].real()*_buffer[i].real() + _buffer[i].imag()*_buffer[i].imag();
          _avg_spectrum[i] = 10*(_spectrum[i]/_spec_count);
          std::cout << _avg_spectrum[i] << "\t";
        }
        std::cout << std::endl;
        _buffer[0] = 0; _buffer_idx=0;
        emit spectrumUpdated();
      }
    }
  }

  bool isInputReal() const {
    return true;
  }

  double sampleRate() const {
    return _Fs;
  }

  size_t fftSize() const {
    return 2048;
  }

  const sdr::Buffer<double> &spectrum() const {
    return _avg_spectrum;
  }

protected:
  size_t _buffer_idx;
  size_t _avg_count;
  sdr::Buffer< std::complex<float> > _buffer;
  sdr::Buffer< double > _spectrum;
  size_t _spec_count;
  sdr::Buffer< double > _avg_spectrum;
  size_t _subsample;
  double _Fs;
  sdr::FFTPlan<float> _fft;
};



#endif // SCHUMANN_HH

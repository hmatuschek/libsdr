#ifndef __SDR_GUI_SPECTRUM_HH__
#define __SDR_GUI_SPECTRUM_HH__

#include <QObject>
#include <fftw3.h>
#include "sdr.hh"


namespace sdr {
namespace gui {


class SpectrumProvider: public QObject
{
  Q_OBJECT

public:
  SpectrumProvider(QObject *parent=0);

  virtual ~SpectrumProvider();

  /** Returns true if the input is real. */
  virtual bool isInputReal() const = 0;

  /** Retunrs the sample rate. */
  virtual double sampleRate() const = 0;

  /** Returns the FFT size. */
  virtual size_t fftSize() const = 0;

  /** Returns the current spectrum. */
  virtual const Buffer<double> &spectrum() const = 0;

signals:
  /** Gets emitted once the spectrum was updated. */
  void spectrumUpdated();
  /** Gets emitted once the spectrum was reconfigured. */
  void spectrumConfigured();
};


/** Calculates the power spectrum of a singnal. The spectrum gets updated with a specified
 * rate (if possible). */
class Spectrum : public SpectrumProvider, public SinkBase
{
  Q_OBJECT

public:
  /** Constructs a new spectrum recorder (sink).
   * @param rrate Specifies the desired refresh-rate of the spectrum.
   * @param fftsize Specifies the size (bins) of the spectrum.
   * @param max_avg Specifies the max. number of averages to take.
   * @param parent Specifies the parent of the @c Spectrum instance. */
  explicit Spectrum(double rrate=2, size_t fftsize=1024, size_t max_avg=1, QObject *parent = 0);

  /** Destructor. */
  virtual ~Spectrum();

  /** Configures the Sink. */
  virtual void config(const Config &src_cfg);

  /** Receives the data stream and updates the _fft_buffer. */
  virtual void handleBuffer(const RawBuffer &buffer, bool allow_overwrite);

  /** Returns true if the input is real. */
  bool isInputReal() const;

  /** Retunrs the sample rate. */
  double sampleRate() const;

  /** Returns the FFT size. */
  size_t fftSize() const;

  /** Returns the current spectrum. */
  const Buffer<double> &spectrum() const;

protected:
  /** Updates the FFT in the _compute buffer. */
  void _updateFFT();

protected:
  /** The desired refresh rate for the spectrum plot. */
  double _rrate;
  /** Size of the FFT (resolution). */
  size_t _fft_size;
  /** Input and compute buffer for the FFT. */
  Buffer< std::complex<double> > _fft_buffer;
  /** Summation buffer of the spectrum. */
  Buffer< double > _compute;
  /** The current spectrum. */
  Buffer< double > _spectrum;
  /** Specifies the number of received samples since last FFT. */
  size_t _sample_count;
  /** Specifies the number of samples to drop before performing a FFT. */
  size_t _N_samples;
  /** Number of transforms in the _compute buffer. */
  size_t _trafo_count;
  /** Max. number of transforms. */
  size_t _Ntrafo;
  /** Number of samples, left in the buffer by the previous input. */
  size_t _samples_left;
  /** The input type. */
  Config::Type _input_type;
  /** The sample-rate of the input. */
  double _sample_rate;
  /** The FFTW plan. */
  fftw_plan _plan;
};


}
}

#endif // SPECTRUM_HH

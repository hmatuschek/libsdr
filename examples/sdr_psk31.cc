#include "baseband.hh"
#include "autocast.hh"
#include "demod.hh"
#include "portaudio.hh"
#include "wavfile.hh"
#include "gui/gui.hh"

#include <iostream>

#include <QApplication>
#include <QMainWindow>
#include <QThread>

using namespace sdr;

static void __sigint_handler(int signo) {
  // On SIGINT -> stop queue properly
  Queue::get().stop();
}

/** Implements a fractional sub-sampler. */
template <class Scalar>
class FracSubSampleBase
{
public:
  /** The input & output type super-scalar. */
  typedef typename Traits<Scalar>::SScalar SScalar;

public:
  /** Constructor.
   * @param frac Specifies the output sample rate relative to the input sample rate. I.e. 2 means
   *        half the input sample rate. */
  FracSubSampleBase(double frac)
    : _avg(0), _sample_count(0), _period(0) {
    if (frac < 1) {
      ConfigError err;
      err << "FracSubSampleBase: Can not sub-sample with fraction smaller one: " << frac;
      throw err;
    }
    _period = (frac*(1<<16));
  }

  /** Destructor. */
  virtual ~FracSubSampleBase() {
    // pass...
  }

  /** Resets the sample rate fraction. */
  inline void setFrac(double frac) {
    if (frac < 1) {
      ConfigError err;
      err << "FracSubSampleBase: Can not sub-sample with fraction smaller one: " << frac;
      throw err;
    }
    _period = (frac*(1<<16)); _sample_count=0; _avg = 0;
  }

  /** Returns the effective sub-sample fraction. */
  inline double frac() const {
    return double(_period)/(1<<16);
  }

  /** Reset sample counter. */
  inline void reset() {
    _avg=0; _sample_count=0;
  }

  /** Performs the sub-sampling. @c in and @c out may refer to the same buffer allowing for an
   * in-place operation. Returns a view on the output buffer containing the sub-samples. */
  inline Buffer<Scalar> subsample(const Buffer<Scalar> &in, const Buffer<Scalar> &out) {
    size_t oidx = 0;
    for (size_t i=0; i<in.size(); i++) {
      _avg += in[i]; _sample_count += (1<<16);
      if (_sample_count >= _period) {
        out[oidx] = _avg/SScalar(_sample_count/(1<<16));
        _sample_count=0; _avg = 0; oidx++;
      }
    }
    return out.head(oidx);
  }

protected:
  /** The average. */
  SScalar _avg;
  /** The number of samples collected times (1<<16). */
  size_t _sample_count;
  /** The sub-sample period. */
  size_t _period;
};


static const float interpolate_taps[129][8] = {
  //    -4            -3            -2            -1             0             1             2             3                mu
  {  0.00000e+00,  0.00000e+00,  0.00000e+00,  0.00000e+00,  1.00000e+00,  0.00000e+00,  0.00000e+00,  0.00000e+00 }, //   0/128
  { -1.54700e-04,  8.53777e-04, -2.76968e-03,  7.89295e-03,  9.98534e-01, -5.41054e-03,  1.24642e-03, -1.98993e-04 }, //   1/128
  { -3.09412e-04,  1.70888e-03, -5.55134e-03,  1.58840e-02,  9.96891e-01, -1.07209e-02,  2.47942e-03, -3.96391e-04 }, //   2/128
  { -4.64053e-04,  2.56486e-03, -8.34364e-03,  2.39714e-02,  9.95074e-01, -1.59305e-02,  3.69852e-03, -5.92100e-04 }, //   3/128
  { -6.18544e-04,  3.42130e-03, -1.11453e-02,  3.21531e-02,  9.93082e-01, -2.10389e-02,  4.90322e-03, -7.86031e-04 }, //   4/128
  { -7.72802e-04,  4.27773e-03, -1.39548e-02,  4.04274e-02,  9.90917e-01, -2.60456e-02,  6.09305e-03, -9.78093e-04 }, //   5/128
  { -9.26747e-04,  5.13372e-03, -1.67710e-02,  4.87921e-02,  9.88580e-01, -3.09503e-02,  7.26755e-03, -1.16820e-03 }, //   6/128
  { -1.08030e-03,  5.98883e-03, -1.95925e-02,  5.72454e-02,  9.86071e-01, -3.57525e-02,  8.42626e-03, -1.35627e-03 }, //   7/128
  { -1.23337e-03,  6.84261e-03, -2.24178e-02,  6.57852e-02,  9.83392e-01, -4.04519e-02,  9.56876e-03, -1.54221e-03 }, //   8/128
  { -1.38589e-03,  7.69462e-03, -2.52457e-02,  7.44095e-02,  9.80543e-01, -4.50483e-02,  1.06946e-02, -1.72594e-03 }, //   9/128
  { -1.53777e-03,  8.54441e-03, -2.80746e-02,  8.31162e-02,  9.77526e-01, -4.95412e-02,  1.18034e-02, -1.90738e-03 }, //  10/128
  { -1.68894e-03,  9.39154e-03, -3.09033e-02,  9.19033e-02,  9.74342e-01, -5.39305e-02,  1.28947e-02, -2.08645e-03 }, //  11/128
  { -1.83931e-03,  1.02356e-02, -3.37303e-02,  1.00769e-01,  9.70992e-01, -5.82159e-02,  1.39681e-02, -2.26307e-03 }, //  12/128
  { -1.98880e-03,  1.10760e-02, -3.65541e-02,  1.09710e-01,  9.67477e-01, -6.23972e-02,  1.50233e-02, -2.43718e-03 }, //  13/128
  { -2.13733e-03,  1.19125e-02, -3.93735e-02,  1.18725e-01,  9.63798e-01, -6.64743e-02,  1.60599e-02, -2.60868e-03 }, //  14/128
  { -2.28483e-03,  1.27445e-02, -4.21869e-02,  1.27812e-01,  9.59958e-01, -7.04471e-02,  1.70776e-02, -2.77751e-03 }, //  15/128
  { -2.43121e-03,  1.35716e-02, -4.49929e-02,  1.36968e-01,  9.55956e-01, -7.43154e-02,  1.80759e-02, -2.94361e-03 }, //  16/128
  { -2.57640e-03,  1.43934e-02, -4.77900e-02,  1.46192e-01,  9.51795e-01, -7.80792e-02,  1.90545e-02, -3.10689e-03 }, //  17/128
  { -2.72032e-03,  1.52095e-02, -5.05770e-02,  1.55480e-01,  9.47477e-01, -8.17385e-02,  2.00132e-02, -3.26730e-03 }, //  18/128
  { -2.86289e-03,  1.60193e-02, -5.33522e-02,  1.64831e-01,  9.43001e-01, -8.52933e-02,  2.09516e-02, -3.42477e-03 }, //  19/128
  { -3.00403e-03,  1.68225e-02, -5.61142e-02,  1.74242e-01,  9.38371e-01, -8.87435e-02,  2.18695e-02, -3.57923e-03 }, //  20/128
  { -3.14367e-03,  1.76185e-02, -5.88617e-02,  1.83711e-01,  9.33586e-01, -9.20893e-02,  2.27664e-02, -3.73062e-03 }, //  21/128
  { -3.28174e-03,  1.84071e-02, -6.15931e-02,  1.93236e-01,  9.28650e-01, -9.53307e-02,  2.36423e-02, -3.87888e-03 }, //  22/128
  { -3.41815e-03,  1.91877e-02, -6.43069e-02,  2.02814e-01,  9.23564e-01, -9.84679e-02,  2.44967e-02, -4.02397e-03 }, //  23/128
  { -3.55283e-03,  1.99599e-02, -6.70018e-02,  2.12443e-01,  9.18329e-01, -1.01501e-01,  2.53295e-02, -4.16581e-03 }, //  24/128
  { -3.68570e-03,  2.07233e-02, -6.96762e-02,  2.22120e-01,  9.12947e-01, -1.04430e-01,  2.61404e-02, -4.30435e-03 }, //  25/128
  { -3.81671e-03,  2.14774e-02, -7.23286e-02,  2.31843e-01,  9.07420e-01, -1.07256e-01,  2.69293e-02, -4.43955e-03 }, //  26/128
  { -3.94576e-03,  2.22218e-02, -7.49577e-02,  2.41609e-01,  9.01749e-01, -1.09978e-01,  2.76957e-02, -4.57135e-03 }, //  27/128
  { -4.07279e-03,  2.29562e-02, -7.75620e-02,  2.51417e-01,  8.95936e-01, -1.12597e-01,  2.84397e-02, -4.69970e-03 }, //  28/128
  { -4.19774e-03,  2.36801e-02, -8.01399e-02,  2.61263e-01,  8.89984e-01, -1.15113e-01,  2.91609e-02, -4.82456e-03 }, //  29/128
  { -4.32052e-03,  2.43930e-02, -8.26900e-02,  2.71144e-01,  8.83893e-01, -1.17526e-01,  2.98593e-02, -4.94589e-03 }, //  30/128
  { -4.44107e-03,  2.50946e-02, -8.52109e-02,  2.81060e-01,  8.77666e-01, -1.19837e-01,  3.05345e-02, -5.06363e-03 }, //  31/128
  { -4.55932e-03,  2.57844e-02, -8.77011e-02,  2.91006e-01,  8.71305e-01, -1.22047e-01,  3.11866e-02, -5.17776e-03 }, //  32/128
  { -4.67520e-03,  2.64621e-02, -9.01591e-02,  3.00980e-01,  8.64812e-01, -1.24154e-01,  3.18153e-02, -5.28823e-03 }, //  33/128
  { -4.78866e-03,  2.71272e-02, -9.25834e-02,  3.10980e-01,  8.58189e-01, -1.26161e-01,  3.24205e-02, -5.39500e-03 }, //  34/128
  { -4.89961e-03,  2.77794e-02, -9.49727e-02,  3.21004e-01,  8.51437e-01, -1.28068e-01,  3.30021e-02, -5.49804e-03 }, //  35/128
  { -5.00800e-03,  2.84182e-02, -9.73254e-02,  3.31048e-01,  8.44559e-01, -1.29874e-01,  3.35600e-02, -5.59731e-03 }, //  36/128
  { -5.11376e-03,  2.90433e-02, -9.96402e-02,  3.41109e-01,  8.37557e-01, -1.31581e-01,  3.40940e-02, -5.69280e-03 }, //  37/128
  { -5.21683e-03,  2.96543e-02, -1.01915e-01,  3.51186e-01,  8.30432e-01, -1.33189e-01,  3.46042e-02, -5.78446e-03 }, //  38/128
  { -5.31716e-03,  3.02507e-02, -1.04150e-01,  3.61276e-01,  8.23188e-01, -1.34699e-01,  3.50903e-02, -5.87227e-03 }, //  39/128
  { -5.41467e-03,  3.08323e-02, -1.06342e-01,  3.71376e-01,  8.15826e-01, -1.36111e-01,  3.55525e-02, -5.95620e-03 }, //  40/128
  { -5.50931e-03,  3.13987e-02, -1.08490e-01,  3.81484e-01,  8.08348e-01, -1.37426e-01,  3.59905e-02, -6.03624e-03 }, //  41/128
  { -5.60103e-03,  3.19495e-02, -1.10593e-01,  3.91596e-01,  8.00757e-01, -1.38644e-01,  3.64044e-02, -6.11236e-03 }, //  42/128
  { -5.68976e-03,  3.24843e-02, -1.12650e-01,  4.01710e-01,  7.93055e-01, -1.39767e-01,  3.67941e-02, -6.18454e-03 }, //  43/128
  { -5.77544e-03,  3.30027e-02, -1.14659e-01,  4.11823e-01,  7.85244e-01, -1.40794e-01,  3.71596e-02, -6.25277e-03 }, //  44/128
  { -5.85804e-03,  3.35046e-02, -1.16618e-01,  4.21934e-01,  7.77327e-01, -1.41727e-01,  3.75010e-02, -6.31703e-03 }, //  45/128
  { -5.93749e-03,  3.39894e-02, -1.18526e-01,  4.32038e-01,  7.69305e-01, -1.42566e-01,  3.78182e-02, -6.37730e-03 }, //  46/128
  { -6.01374e-03,  3.44568e-02, -1.20382e-01,  4.42134e-01,  7.61181e-01, -1.43313e-01,  3.81111e-02, -6.43358e-03 }, //  47/128
  { -6.08674e-03,  3.49066e-02, -1.22185e-01,  4.52218e-01,  7.52958e-01, -1.43968e-01,  3.83800e-02, -6.48585e-03 }, //  48/128
  { -6.15644e-03,  3.53384e-02, -1.23933e-01,  4.62289e-01,  7.44637e-01, -1.44531e-01,  3.86247e-02, -6.53412e-03 }, //  49/128
  { -6.22280e-03,  3.57519e-02, -1.25624e-01,  4.72342e-01,  7.36222e-01, -1.45004e-01,  3.88454e-02, -6.57836e-03 }, //  50/128
  { -6.28577e-03,  3.61468e-02, -1.27258e-01,  4.82377e-01,  7.27714e-01, -1.45387e-01,  3.90420e-02, -6.61859e-03 }, //  51/128
  { -6.34530e-03,  3.65227e-02, -1.28832e-01,  4.92389e-01,  7.19116e-01, -1.45682e-01,  3.92147e-02, -6.65479e-03 }, //  52/128
  { -6.40135e-03,  3.68795e-02, -1.30347e-01,  5.02377e-01,  7.10431e-01, -1.45889e-01,  3.93636e-02, -6.68698e-03 }, //  53/128
  { -6.45388e-03,  3.72167e-02, -1.31800e-01,  5.12337e-01,  7.01661e-01, -1.46009e-01,  3.94886e-02, -6.71514e-03 }, //  54/128
  { -6.50285e-03,  3.75341e-02, -1.33190e-01,  5.22267e-01,  6.92808e-01, -1.46043e-01,  3.95900e-02, -6.73929e-03 }, //  55/128
  { -6.54823e-03,  3.78315e-02, -1.34515e-01,  5.32164e-01,  6.83875e-01, -1.45993e-01,  3.96678e-02, -6.75943e-03 }, //  56/128
  { -6.58996e-03,  3.81085e-02, -1.35775e-01,  5.42025e-01,  6.74865e-01, -1.45859e-01,  3.97222e-02, -6.77557e-03 }, //  57/128
  { -6.62802e-03,  3.83650e-02, -1.36969e-01,  5.51849e-01,  6.65779e-01, -1.45641e-01,  3.97532e-02, -6.78771e-03 }, //  58/128
  { -6.66238e-03,  3.86006e-02, -1.38094e-01,  5.61631e-01,  6.56621e-01, -1.45343e-01,  3.97610e-02, -6.79588e-03 }, //  59/128
  { -6.69300e-03,  3.88151e-02, -1.39150e-01,  5.71370e-01,  6.47394e-01, -1.44963e-01,  3.97458e-02, -6.80007e-03 }, //  60/128
  { -6.71985e-03,  3.90083e-02, -1.40136e-01,  5.81063e-01,  6.38099e-01, -1.44503e-01,  3.97077e-02, -6.80032e-03 }, //  61/128
  { -6.74291e-03,  3.91800e-02, -1.41050e-01,  5.90706e-01,  6.28739e-01, -1.43965e-01,  3.96469e-02, -6.79662e-03 }, //  62/128
  { -6.76214e-03,  3.93299e-02, -1.41891e-01,  6.00298e-01,  6.19318e-01, -1.43350e-01,  3.95635e-02, -6.78902e-03 }, //  63/128
  { -6.77751e-03,  3.94578e-02, -1.42658e-01,  6.09836e-01,  6.09836e-01, -1.42658e-01,  3.94578e-02, -6.77751e-03 }, //  64/128
  { -6.78902e-03,  3.95635e-02, -1.43350e-01,  6.19318e-01,  6.00298e-01, -1.41891e-01,  3.93299e-02, -6.76214e-03 }, //  65/128
  { -6.79662e-03,  3.96469e-02, -1.43965e-01,  6.28739e-01,  5.90706e-01, -1.41050e-01,  3.91800e-02, -6.74291e-03 }, //  66/128
  { -6.80032e-03,  3.97077e-02, -1.44503e-01,  6.38099e-01,  5.81063e-01, -1.40136e-01,  3.90083e-02, -6.71985e-03 }, //  67/128
  { -6.80007e-03,  3.97458e-02, -1.44963e-01,  6.47394e-01,  5.71370e-01, -1.39150e-01,  3.88151e-02, -6.69300e-03 }, //  68/128
  { -6.79588e-03,  3.97610e-02, -1.45343e-01,  6.56621e-01,  5.61631e-01, -1.38094e-01,  3.86006e-02, -6.66238e-03 }, //  69/128
  { -6.78771e-03,  3.97532e-02, -1.45641e-01,  6.65779e-01,  5.51849e-01, -1.36969e-01,  3.83650e-02, -6.62802e-03 }, //  70/128
  { -6.77557e-03,  3.97222e-02, -1.45859e-01,  6.74865e-01,  5.42025e-01, -1.35775e-01,  3.81085e-02, -6.58996e-03 }, //  71/128
  { -6.75943e-03,  3.96678e-02, -1.45993e-01,  6.83875e-01,  5.32164e-01, -1.34515e-01,  3.78315e-02, -6.54823e-03 }, //  72/128
  { -6.73929e-03,  3.95900e-02, -1.46043e-01,  6.92808e-01,  5.22267e-01, -1.33190e-01,  3.75341e-02, -6.50285e-03 }, //  73/128
  { -6.71514e-03,  3.94886e-02, -1.46009e-01,  7.01661e-01,  5.12337e-01, -1.31800e-01,  3.72167e-02, -6.45388e-03 }, //  74/128
  { -6.68698e-03,  3.93636e-02, -1.45889e-01,  7.10431e-01,  5.02377e-01, -1.30347e-01,  3.68795e-02, -6.40135e-03 }, //  75/128
  { -6.65479e-03,  3.92147e-02, -1.45682e-01,  7.19116e-01,  4.92389e-01, -1.28832e-01,  3.65227e-02, -6.34530e-03 }, //  76/128
  { -6.61859e-03,  3.90420e-02, -1.45387e-01,  7.27714e-01,  4.82377e-01, -1.27258e-01,  3.61468e-02, -6.28577e-03 }, //  77/128
  { -6.57836e-03,  3.88454e-02, -1.45004e-01,  7.36222e-01,  4.72342e-01, -1.25624e-01,  3.57519e-02, -6.22280e-03 }, //  78/128
  { -6.53412e-03,  3.86247e-02, -1.44531e-01,  7.44637e-01,  4.62289e-01, -1.23933e-01,  3.53384e-02, -6.15644e-03 }, //  79/128
  { -6.48585e-03,  3.83800e-02, -1.43968e-01,  7.52958e-01,  4.52218e-01, -1.22185e-01,  3.49066e-02, -6.08674e-03 }, //  80/128
  { -6.43358e-03,  3.81111e-02, -1.43313e-01,  7.61181e-01,  4.42134e-01, -1.20382e-01,  3.44568e-02, -6.01374e-03 }, //  81/128
  { -6.37730e-03,  3.78182e-02, -1.42566e-01,  7.69305e-01,  4.32038e-01, -1.18526e-01,  3.39894e-02, -5.93749e-03 }, //  82/128
  { -6.31703e-03,  3.75010e-02, -1.41727e-01,  7.77327e-01,  4.21934e-01, -1.16618e-01,  3.35046e-02, -5.85804e-03 }, //  83/128
  { -6.25277e-03,  3.71596e-02, -1.40794e-01,  7.85244e-01,  4.11823e-01, -1.14659e-01,  3.30027e-02, -5.77544e-03 }, //  84/128
  { -6.18454e-03,  3.67941e-02, -1.39767e-01,  7.93055e-01,  4.01710e-01, -1.12650e-01,  3.24843e-02, -5.68976e-03 }, //  85/128
  { -6.11236e-03,  3.64044e-02, -1.38644e-01,  8.00757e-01,  3.91596e-01, -1.10593e-01,  3.19495e-02, -5.60103e-03 }, //  86/128
  { -6.03624e-03,  3.59905e-02, -1.37426e-01,  8.08348e-01,  3.81484e-01, -1.08490e-01,  3.13987e-02, -5.50931e-03 }, //  87/128
  { -5.95620e-03,  3.55525e-02, -1.36111e-01,  8.15826e-01,  3.71376e-01, -1.06342e-01,  3.08323e-02, -5.41467e-03 }, //  88/128
  { -5.87227e-03,  3.50903e-02, -1.34699e-01,  8.23188e-01,  3.61276e-01, -1.04150e-01,  3.02507e-02, -5.31716e-03 }, //  89/128
  { -5.78446e-03,  3.46042e-02, -1.33189e-01,  8.30432e-01,  3.51186e-01, -1.01915e-01,  2.96543e-02, -5.21683e-03 }, //  90/128
  { -5.69280e-03,  3.40940e-02, -1.31581e-01,  8.37557e-01,  3.41109e-01, -9.96402e-02,  2.90433e-02, -5.11376e-03 }, //  91/128
  { -5.59731e-03,  3.35600e-02, -1.29874e-01,  8.44559e-01,  3.31048e-01, -9.73254e-02,  2.84182e-02, -5.00800e-03 }, //  92/128
  { -5.49804e-03,  3.30021e-02, -1.28068e-01,  8.51437e-01,  3.21004e-01, -9.49727e-02,  2.77794e-02, -4.89961e-03 }, //  93/128
  { -5.39500e-03,  3.24205e-02, -1.26161e-01,  8.58189e-01,  3.10980e-01, -9.25834e-02,  2.71272e-02, -4.78866e-03 }, //  94/128
  { -5.28823e-03,  3.18153e-02, -1.24154e-01,  8.64812e-01,  3.00980e-01, -9.01591e-02,  2.64621e-02, -4.67520e-03 }, //  95/128
  { -5.17776e-03,  3.11866e-02, -1.22047e-01,  8.71305e-01,  2.91006e-01, -8.77011e-02,  2.57844e-02, -4.55932e-03 }, //  96/128
  { -5.06363e-03,  3.05345e-02, -1.19837e-01,  8.77666e-01,  2.81060e-01, -8.52109e-02,  2.50946e-02, -4.44107e-03 }, //  97/128
  { -4.94589e-03,  2.98593e-02, -1.17526e-01,  8.83893e-01,  2.71144e-01, -8.26900e-02,  2.43930e-02, -4.32052e-03 }, //  98/128
  { -4.82456e-03,  2.91609e-02, -1.15113e-01,  8.89984e-01,  2.61263e-01, -8.01399e-02,  2.36801e-02, -4.19774e-03 }, //  99/128
  { -4.69970e-03,  2.84397e-02, -1.12597e-01,  8.95936e-01,  2.51417e-01, -7.75620e-02,  2.29562e-02, -4.07279e-03 }, // 100/128
  { -4.57135e-03,  2.76957e-02, -1.09978e-01,  9.01749e-01,  2.41609e-01, -7.49577e-02,  2.22218e-02, -3.94576e-03 }, // 101/128
  { -4.43955e-03,  2.69293e-02, -1.07256e-01,  9.07420e-01,  2.31843e-01, -7.23286e-02,  2.14774e-02, -3.81671e-03 }, // 102/128
  { -4.30435e-03,  2.61404e-02, -1.04430e-01,  9.12947e-01,  2.22120e-01, -6.96762e-02,  2.07233e-02, -3.68570e-03 }, // 103/128
  { -4.16581e-03,  2.53295e-02, -1.01501e-01,  9.18329e-01,  2.12443e-01, -6.70018e-02,  1.99599e-02, -3.55283e-03 }, // 104/128
  { -4.02397e-03,  2.44967e-02, -9.84679e-02,  9.23564e-01,  2.02814e-01, -6.43069e-02,  1.91877e-02, -3.41815e-03 }, // 105/128
  { -3.87888e-03,  2.36423e-02, -9.53307e-02,  9.28650e-01,  1.93236e-01, -6.15931e-02,  1.84071e-02, -3.28174e-03 }, // 106/128
  { -3.73062e-03,  2.27664e-02, -9.20893e-02,  9.33586e-01,  1.83711e-01, -5.88617e-02,  1.76185e-02, -3.14367e-03 }, // 107/128
  { -3.57923e-03,  2.18695e-02, -8.87435e-02,  9.38371e-01,  1.74242e-01, -5.61142e-02,  1.68225e-02, -3.00403e-03 }, // 108/128
  { -3.42477e-03,  2.09516e-02, -8.52933e-02,  9.43001e-01,  1.64831e-01, -5.33522e-02,  1.60193e-02, -2.86289e-03 }, // 109/128
  { -3.26730e-03,  2.00132e-02, -8.17385e-02,  9.47477e-01,  1.55480e-01, -5.05770e-02,  1.52095e-02, -2.72032e-03 }, // 110/128
  { -3.10689e-03,  1.90545e-02, -7.80792e-02,  9.51795e-01,  1.46192e-01, -4.77900e-02,  1.43934e-02, -2.57640e-03 }, // 111/128
  { -2.94361e-03,  1.80759e-02, -7.43154e-02,  9.55956e-01,  1.36968e-01, -4.49929e-02,  1.35716e-02, -2.43121e-03 }, // 112/128
  { -2.77751e-03,  1.70776e-02, -7.04471e-02,  9.59958e-01,  1.27812e-01, -4.21869e-02,  1.27445e-02, -2.28483e-03 }, // 113/128
  { -2.60868e-03,  1.60599e-02, -6.64743e-02,  9.63798e-01,  1.18725e-01, -3.93735e-02,  1.19125e-02, -2.13733e-03 }, // 114/128
  { -2.43718e-03,  1.50233e-02, -6.23972e-02,  9.67477e-01,  1.09710e-01, -3.65541e-02,  1.10760e-02, -1.98880e-03 }, // 115/128
  { -2.26307e-03,  1.39681e-02, -5.82159e-02,  9.70992e-01,  1.00769e-01, -3.37303e-02,  1.02356e-02, -1.83931e-03 }, // 116/128
  { -2.08645e-03,  1.28947e-02, -5.39305e-02,  9.74342e-01,  9.19033e-02, -3.09033e-02,  9.39154e-03, -1.68894e-03 }, // 117/128
  { -1.90738e-03,  1.18034e-02, -4.95412e-02,  9.77526e-01,  8.31162e-02, -2.80746e-02,  8.54441e-03, -1.53777e-03 }, // 118/128
  { -1.72594e-03,  1.06946e-02, -4.50483e-02,  9.80543e-01,  7.44095e-02, -2.52457e-02,  7.69462e-03, -1.38589e-03 }, // 119/128
  { -1.54221e-03,  9.56876e-03, -4.04519e-02,  9.83392e-01,  6.57852e-02, -2.24178e-02,  6.84261e-03, -1.23337e-03 }, // 120/128
  { -1.35627e-03,  8.42626e-03, -3.57525e-02,  9.86071e-01,  5.72454e-02, -1.95925e-02,  5.98883e-03, -1.08030e-03 }, // 121/128
  { -1.16820e-03,  7.26755e-03, -3.09503e-02,  9.88580e-01,  4.87921e-02, -1.67710e-02,  5.13372e-03, -9.26747e-04 }, // 122/128
  { -9.78093e-04,  6.09305e-03, -2.60456e-02,  9.90917e-01,  4.04274e-02, -1.39548e-02,  4.27773e-03, -7.72802e-04 }, // 123/128
  { -7.86031e-04,  4.90322e-03, -2.10389e-02,  9.93082e-01,  3.21531e-02, -1.11453e-02,  3.42130e-03, -6.18544e-04 }, // 124/128
  { -5.92100e-04,  3.69852e-03, -1.59305e-02,  9.95074e-01,  2.39714e-02, -8.34364e-03,  2.56486e-03, -4.64053e-04 }, // 125/128
  { -3.96391e-04,  2.47942e-03, -1.07209e-02,  9.96891e-01,  1.58840e-02, -5.55134e-03,  1.70888e-03, -3.09412e-04 }, // 126/128
  { -1.98993e-04,  1.24642e-03, -5.41054e-03,  9.98534e-01,  7.89295e-03, -2.76968e-03,  8.53777e-04, -1.54700e-04 }, // 127/128
  {  0.00000e+00,  0.00000e+00,  0.00000e+00,  1.00000e+00,  0.00000e+00,  0.00000e+00,  0.00000e+00,  0.00000e+00 }, // 128/128
};


template <class Scalar>
static inline Scalar interpolate(const Buffer<Scalar> &in, float mu) {
  Scalar out = 0;
  const float *filter = interpolate_taps[int(std::round(mu*128))];
  for (size_t i=0; i<8; i++) { out += in[i]*filter[i]; }
  return Scalar(out);
}


template <class Scalar>
class BPSK31: public Sink< std::complex<Scalar> >, public Source
{
public:
  BPSK31(double F0, double dF)
    : Sink< std::complex<Scalar> >(), Source(), _F0(F0), _dF(dF),
      _P(0), _F(0), _Fmin(-0.1), _Fmax(0.1)
  {
    double damping = std::sqrt(2)/2;
    double bw = M_PI/100;
    double tmp = 1. + 2*damping*bw + bw*bw;
    _alpha = 4*damping*bw/tmp;
    _beta  = 4*bw*bw/tmp;

    // Init Delay line
    _dl = Buffer< std::complex<float> >(2*8); _dl_idx = 0;
    for (size_t i=0; i<16; i++) { _dl[i] = 0; }

    // Inner PLL:
    _theta = 0; _mu = 0.25; _gain_mu = 0.01;
    _omega = _min_omega = _max_omega = 0;
    _omega_rel = 0.001; _gain_omega = 0.001;
    _p_0T = _p_1T = _p_2T = 0;
    _c_0T = _c_1T = _c_2T = 0;

    // Super-sample (256 samples per symbol)
    _superSample = 64;
  }

  virtual ~BPSK31() {
    // pass...
  }

  virtual void config(const Config &src_cfg) {
    // Requires type, sample rate & buffer size
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate() || !src_cfg.hasBufferSize()) { return; }
    // Check buffer type
    if (Config::typeId< std::complex<Scalar> >() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure BPSK31: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId< std::complex<Scalar> >();
      throw err;
    }

    // Store sample rate
    double Fs = src_cfg.sampleRate();

    // Compute samples per symbol
    _omega = Fs/(_superSample*31.25);
    _min_omega = _omega*(1.0 - _omega_rel);
    _max_omega = _omega*(1.0 + _omega_rel);
    _omega_mid = 0.5*(_min_omega+_max_omega);

    _hist = Buffer<float>(_superSample);
    _hist_idx = 0;
    _last_constellation = 1;

    _buffer = Buffer<uint8_t>(src_cfg.bufferSize());

    this->setConfig(Config(Traits<uint8_t>::scalarId, 31.25, src_cfg.bufferSize(), 1));
  }


  virtual void process(const Buffer< std::complex<Scalar> > &buffer, bool allow_overwrite) {
    size_t i=0, o=0;
    while (i<buffer.size()) {
      // First, fill sampler...
      while ( (_mu > 1) && (i<buffer.size()) ) {
        _updateSampler(buffer[i]); i++;
      }
      // Then, ...
      if (i<buffer.size()) {
        std::complex<float> sample = interpolate(_dl.sub(_dl_idx, 8), _mu);
        _errorTracking(sample);
        _updatePLL(sample);
        // done. real(sample) constains phase of symbol
        _hist[_hist_idx] = std::real(sample);
        // Check if there is a constellation transition at the current time
        if ((_hist_idx>1) && (_hasTransition())) {
          if (_hist_idx<(_superSample/2)) {
            // If less than superSample values in hist -> drop
            _hist_idx = 0;
          } else {
            // Otherwise decode
            int cconst = _currentContellation();
            _buffer[o++] = (_last_constellation == cconst);
            _last_constellation = cconst;
            _hist_idx = 0;
          }
        } else if (_hist_idx == (_superSample-1)) {
          // If the symbol is complete:
          int cconst = _currentContellation();
          _buffer[o++] = (_last_constellation == cconst);
          _last_constellation = cconst;
          _hist_idx = 0;
        } else {
          // Next sample
          _hist_idx++;
        }
      }
    }

    this->send(_buffer.head(o));
  }


protected:
  inline bool _hasTransition() const {
    return ((_hist[_hist_idx-1]>=0) && (_hist[_hist_idx]<=0)) ||
        ((_hist[_hist_idx-1]<=0) && (_hist[_hist_idx]>=0));
  }

  inline int _currentContellation() const {
    float value = 0;
    for (size_t i=0; i<=_hist_idx; i++) { value += _hist[i]; }
    return (value > 0) ? 1 : -1;
  }

  inline float _phaseError(const std::complex<float> &value) const {
    float r2 = value.real()*value.real();
    float i2 = value.imag()*value.imag();
    float nrm2 = r2+i2;
    if (0 == nrm2) { return 0; }
    return -value.real()*value.imag()/nrm2;
  }

  inline void _updatePLL(const std::complex<float> &sample) {
    float phi = _phaseError(sample);
    _F += _beta*phi;
    _P += _F + _alpha*phi;
    while (_P>( 2*M_PI)) { _P -= 2*M_PI; }
    while (_P<(-2*M_PI)) { _P += 2*M_PI; }
    _F = std::min(_Fmax, std::max(_Fmin, _F));
    //std::cerr << "Update PLL: P=" << _P << "; F=" << _F << ", err= " << phi << std::endl;
  }

  inline void _updateSampler(const std::complex<Scalar> &value) {
    _mu-=1; _P += _F;
    while (_P>( 2*M_PI)) { _P -= 2*M_PI; }
    while (_P<(-2*M_PI)) { _P += 2*M_PI; }
    std::complex<float> fac = std::exp(std::complex<float>(0, _P+_theta));
    std::complex<float> sample = fac * std::complex<float>(value.real(), value.imag());
    _dl[_dl_idx] = sample;
    _dl[_dl_idx+8] = sample;
    _dl_idx = (_dl_idx + 1) % 8;
  }

  inline void _errorTracking(const std::complex<float> &sample) {
    _p_2T = _p_1T; _p_1T = _p_0T; _p_0T = sample;
    _c_2T = _c_1T; _c_1T = _c_0T; _c_0T = (sample.real() > 0) ? -1 : 1;

    std::complex<float> x = (_c_0T - _c_2T) * std::conj(_p_1T);
    std::complex<float> y = (_p_0T - _p_2T) * std::conj(_c_1T);
    float err = std::real(y-x);
    if (err >  1.0) { err = 1.0; }
    if (err < -1.0) { err = -1.0; }
    _omega += _gain_omega * err;
    if ( (_omega-_omega_mid) > _omega_rel) { _omega = _omega_mid + _omega_rel; }
    else if ( (_omega-_omega_mid) < -_omega_rel) { _omega = _omega_mid - _omega_rel; }
    else { _omega = _omega_mid + _omega-_omega_mid; }
    _mu += _omega + _gain_mu * err;
  }


protected:
  double _F0, _dF;
  // Carrier PLL stuff
  float _P, _F, _Fmin, _Fmax;
  float _alpha, _beta;
  // Delay line
  Buffer< std::complex<float> > _dl;
  size_t _dl_idx;
  // Second PLL
  float _theta;
  float _mu, _gain_mu;
  float _omega, _min_omega, _max_omega;
  float _omega_mid, _omega_rel, _gain_omega;
  std::complex<float> _p_0T, _p_1T, _p_2T;
  std::complex<float> _c_0T, _c_1T, _c_2T;
  // Third PLL
  size_t _superSample;
  Buffer<float> _hist;
  size_t _hist_idx;
  int _last_constellation;
  // Output buffer
  Buffer<uint8_t> _buffer;
};


class Varicode: public Sink<uint8_t>, public Source
{
public:
  Varicode()
    : Sink<uint8_t>(), Source()
  {
    _code_table[1023] = '!';  _code_table[87]   = '.';  _code_table[895]  = '\'';
    _code_table[367]  = '*';  _code_table[495]  = '\\'; _code_table[687]  = '?';
    _code_table[475]  = '$';  _code_table[701]  = '@';  _code_table[365]  = '_';
    _code_table[735]  = '`';  _code_table[351]  = '"';  _code_table[493]  = '<';
    _code_table[727]  = '~';  _code_table[699]  = '&';  _code_table[703]  = '^';
    _code_table[507]  = ']';  _code_table[117]  = '-';  _code_table[445]  = ';';
    _code_table[1013] = '#';  _code_table[695]  = '{';  _code_table[245]  = ':';
    _code_table[693]  = '}';  _code_table[503]  = ')';  _code_table[1749] = '%';
    _code_table[471]  = '>';  _code_table[991]  = '+';  _code_table[251]  = '[';
    _code_table[85]   = '=';  _code_table[943]  = '/';  _code_table[29]   = '\n';
    _code_table[443]  = '|';  _code_table[1]    = ' ';  _code_table[125]  = 'A';
    _code_table[235]  = 'B';  _code_table[173]  = 'C';  _code_table[181]  = 'D';
    _code_table[119]  = 'E';  _code_table[219]  = 'F';  _code_table[253]  = 'G';
    _code_table[341]  = 'H';  _code_table[127]  = 'I';  _code_table[509]  = 'J';
    _code_table[381]  = 'K';  _code_table[215]  = 'L';  _code_table[187]  = 'M';
    _code_table[221]  = 'N';  _code_table[171]  = 'O';  _code_table[213]  = 'P';
    _code_table[477]  = 'Q';  _code_table[175]  = 'R';  _code_table[111]  = 'S';
    _code_table[109]  = 'T';  _code_table[343]  = 'U';  _code_table[437]  = 'V';
    _code_table[349]  = 'W';  _code_table[373]  = 'X';  _code_table[379]  = 'Y';
    _code_table[685]  = 'Z';  _code_table[11]   = 'a';  _code_table[95]   = 'b';
    _code_table[47]   = 'c';  _code_table[45]   = 'd';  _code_table[3]    = 'e';
    _code_table[61]   = 'f';  _code_table[91]   = 'g';  _code_table[43]   = 'h';
    _code_table[13]   = 'i';  _code_table[491]  = 'j';  _code_table[191]  = 'k';
    _code_table[27]   = 'l';  _code_table[59]   = 'm';  _code_table[15]   = 'n';
    _code_table[7]    = 'o';  _code_table[63]   = 'p';  _code_table[447]  = 'q';
    _code_table[21]   = 'r';  _code_table[23]   = 's';  _code_table[5]    = 't';
    _code_table[55]   = 'u';  _code_table[123]  = 'v';  _code_table[107]  = 'w';
    _code_table[223]  = 'x';  _code_table[93]   = 'y';  _code_table[469]  = 'z';
    _code_table[183]  = '0';  _code_table[445]  = '1';  _code_table[237]  = '2';
    //                          ^- Collides with ';'!
    _code_table[511]  = '3';  _code_table[375]  = '4';  _code_table[859]  = '5';
    _code_table[363]  = '6';  _code_table[941]  = '7';  _code_table[427]  = '8';
    _code_table[951]  = '9';
  }

  virtual ~Varicode() {
    // pass...
  }

  virtual void config(const Config &src_cfg) {
    // Requires type, sample rate & buffer size
    if (!src_cfg.hasType() || !src_cfg.hasBufferSize()) { return; }
    // Check buffer type
    if (Config::typeId<uint8_t>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure Varicode: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId<uint8_t>();
      throw err;
    }

    _value = 0;
    _buffer = Buffer<uint8_t>(18);
    this->setConfig(Config(Traits<uint8_t>::scalarId, 0, 18, 1));
  }

  virtual void process(const Buffer<uint8_t> &buffer, bool allow_overwrite) {
    size_t oidx = 0;
    for (size_t i=0; i<buffer.size(); i++) {
      _value = (_value << 1) | (buffer[i]&0x01);
      if (0 == (_value&0x03)) {
        _value >>= 2;
        if (_value) {
          std::map<uint16_t, char>::iterator item = _code_table.find(_value);
          if (item != _code_table.end()) {
            _buffer[oidx++] = item->second;
          } else {
            LogMessage msg(LOG_INFO);
            msg << "Can not decode varicode " << _value << ": Unkown symbol.";
            Logger::get().log(msg);
          }
        }
        _value = 0;
      }
    }
    if (oidx) {
      this->send(_buffer.head(oidx));
    }
  }

protected:
  uint16_t _value;
  Buffer<uint8_t> _buffer;
  std::map<uint16_t, char> _code_table;
};


int main(int argc, char *argv[]) {
  if (2 != argc) {
    std::cerr << "Usage: sdr_psk31 FILENAME" << std::endl;
    return -1;
  }

  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register handler:
  signal(SIGINT, __sigint_handler);

  QApplication app(argc, argv);

  QMainWindow       *win       = new QMainWindow();
  gui::Spectrum     *spec      = new gui::Spectrum(2, 1024, 5);
  gui::SpectrumView *spec_view = new gui::SpectrumView(spec);
  spec_view->setMindB(-60);
  win->setCentralWidget(spec_view);
  win->setMinimumSize(640, 240);
  win->show();

  PortAudio::init();
  Queue &queue = Queue::get();

  WavSource src(argv[1]);
  AutoCast< std::complex<int16_t> > cast;
  IQBaseBand<int16_t> baseband(0, 2144., 200.0, 63, 1);
  baseband.setCenterFrequency(2144.0);
  BPSK31<int16_t> demod(0.0, 200.0);
  PortSink sink;
  Varicode decode;
  DebugDump<uint8_t> dump;

  src.connect(&cast, true);
  cast.connect(&baseband, true);
  baseband.connect(spec);
  baseband.connect(&sink);
  baseband.connect(&demod);
  demod.connect(&decode, true);
  decode.connect(&dump, true);

  queue.addIdle(&src, &WavSource::next);
  //queue.addIdle(&src, &IQSigGen<double>::next);

  queue.start();
  app.exec();
  queue.stop();
  queue.wait();

  PortAudio::terminate();

  return 0;
}

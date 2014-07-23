#ifndef __SDR_GUI_WATERFALLVIEW_HH__
#define __SDR_GUI_WATERFALLVIEW_HH__

#include <QWidget>
#include <QImage>
#include "spectrum.hh"


namespace sdr {
namespace gui {

/** Interface for all color maps. */
class ColorMap
{
protected:
  /** Hidden constructor. */
  ColorMap();

public:
  /** Destructor. */
  virtual ~ColorMap();
  /** Needs to be implemented by all sub-classes. Maps the value (dB) to a color value. */
  virtual QColor operator()(const double &value) = 0;
};

/** A simple gray-scale color map. */
class GrayScaleColorMap: public ColorMap
{
public:
  /** Constructor.
   * @param mindB Specifices the minimum value in dB of the color-scale. Mapping values [0, mindB]
   * to a gray-scale. */
  GrayScaleColorMap(double mindB=-120);
  /** Destructor. */
  virtual ~GrayScaleColorMap();
  /** Implements the color mapping. */
  virtual QColor operator()(const double &value);

protected:
  /** The minimum value. */
  double _mindB;
};


/** A simple waterfall display of the spectrogram. */
class WaterFallView : public QWidget
{
  Q_OBJECT

public:
  /** Constructor.
   * @param spectrum Specifies the spectrum sink.
   * @param height Specifies the number of PSDs to display.
   * @param parent The parent widget. */
  explicit WaterFallView(Spectrum *spectrum, size_t height=100, QWidget *parent = 0);

protected:
  /** Draws the scaled waterfall spectrogram. */
  virtual void paintEvent(QPaintEvent *evt);

protected slots:
  /** Gets called once a new PSD is available. */
  void _onSpectrumUpdated();

protected:
  /** The spectrum sink. */
  Spectrum *_spectrum;
  /** The size of the spectrum. */
  size_t _N;
  /** "Height of the spectrum. */
  size_t _M;
  /** The waterfall spectrogram. */
  QPixmap _waterfall;
  /** The color map to be used. */
  ColorMap *_colorMap;
};

}
}

#endif // WATERFALLVIEW_HH

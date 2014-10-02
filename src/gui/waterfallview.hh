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
  ColorMap(double min, double max);

public:
  /** Destructor. */
  virtual ~ColorMap();

  /** Maps a value to a color. */
  inline QColor operator()(const double &value) {
    if (value > _max) { return this->map(1); }
    if (value < _min) { return this->map(0); }
    return this->map((value-_min)/(_max-_min));
  }

  /** Maps a value on the interval [0,1] to a color.
   * Needs to be implemented by all sub-classes. */
  virtual QColor map(const double &value) = 0;

protected:
  /** Minimum value. */
  double _min;
  /** Maximum value. */
  double _max;
};

/** A simple gray-scale color map. */
class GrayScaleColorMap: public ColorMap
{
public:
  /** Constructor.
   * @param mindB Specifices the minimum value in dB of the color-scale. Mapping values [min, max]
   * to a gray-scale. */
  GrayScaleColorMap(double min, double max);
  /** Destructor. */
  virtual ~GrayScaleColorMap();
  /** Implements the color mapping. */
  virtual QColor map(const double &value);
};

/** A linear interpolating color map. */
class LinearColorMap: public ColorMap {
public:
  LinearColorMap(const QVector<QColor> &colors, double min, double max);
  /** Destructor. */
  virtual ~LinearColorMap();
  virtual QColor map(const double &value);

protected:
  QVector<QColor> _colors;
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
  explicit WaterFallView(SpectrumProvider *spectrum, size_t height=100, QWidget *parent = 0);

signals:
  void click(double f);

protected:
  /** Handles mouse clicks. */
  virtual void mouseReleaseEvent(QMouseEvent *evt);

  /** Draws the scaled waterfall spectrogram. */
  virtual void paintEvent(QPaintEvent *evt);

protected slots:
  /** Gets called once a new PSD is available. */
  void _onSpectrumUpdated();

protected:
  /** The spectrum sink. */
  SpectrumProvider *_spectrum;
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

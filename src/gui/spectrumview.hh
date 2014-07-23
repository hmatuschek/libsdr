#ifndef __SDR_GUI_SPECTRUMVIEW_HH__
#define __SDR_GUI_SPECTRUMVIEW_HH__

#include <QWidget>
#include <QPen>

#include "sdr.hh"
#include "spectrum.hh"


namespace sdr {
namespace gui {

/** A simple widget to display a PSD with live update. */
class SpectrumView: public QWidget
{
Q_OBJECT

public:
  /** @param rrate Specifies the (approx) refreshrate of the FFT plot. */
  explicit SpectrumView(Spectrum *spectrum, QWidget *parent=0);

  inline size_t numXTicks() const { return _numXTicks; }
  inline void setNumXTicks(size_t N) { _numXTicks=N; update(); }

  inline size_t numYTicks() const { return _numYTicks; }
  inline void setNumYTicks(size_t N) { _numYTicks = N; }

  inline double mindB() const { return _mindB; }
  inline void setMindB(double mindB) { _mindB = mindB; }

  /// Destructor.
  virtual ~SpectrumView();

signals:
  void click(double f);

protected:
  /** Handles mouse clicks. */
  virtual void mouseReleaseEvent(QMouseEvent *evt);

  /**  Updates _plotArea on resize events. */
  virtual void resizeEvent(QResizeEvent *evt);
  /** Draws the spectrum. */
  virtual void paintEvent(QPaintEvent *evt);
  /** Draws the spectrum graph */
  void _drawGraph(QPainter &painter);
  /** Draw the axes, ticks and labels. */
  void _drawAxis(QPainter &painter);


protected:
  /** Holds a weak reference to the spectrum recorder object. */
  Spectrum *_spectrum;
  /// The font being used for axis labels
  QFont _axisFont;
  /// The plot area
  QRect _plotArea;
  /// Axis pen.
  QPen _axisPen;
  /// The pen used to draw the graph.
  QPen _graphPen;

  size_t _numXTicks;
  size_t _numYTicks;
  double _maxF;
  /** Lower bound of the PSD plot. */
  double _mindB;
};

}
}

#endif // __SDR_GUI_SPECTRUMVIEW_HH__

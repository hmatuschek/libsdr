#include "waterfallview.hh"
#include <QPainter>
#include <QPaintEvent>
#include <QDebug>

using namespace sdr;
using namespace sdr::gui;



/* ****************************************************************************************** *
 *  Implementation of ColorMap
 * ****************************************************************************************** */
ColorMap::ColorMap(double min, double max)
  : _min(min), _max(max)
{
  // pass...
}

ColorMap::~ColorMap() {
  // pass...
}

/* ****************************************************************************************** *
 *  Implementation of GrayScaleColorMap
 * ****************************************************************************************** */
GrayScaleColorMap::GrayScaleColorMap(double min, double max)
  : ColorMap(min, max)
{
  // pass...
}

GrayScaleColorMap::~GrayScaleColorMap() {
  // pass...
}

QColor
GrayScaleColorMap::map(const double &value) {
  int h = 255*value;
  return QColor(h,h,h);
}


/* ****************************************************************************************** *
 *  Implementation of LinearColorMap
 * ****************************************************************************************** */
LinearColorMap::LinearColorMap(const QVector<QColor> &colors, double min, double max)
  : ColorMap(min, max), _colors(colors)
{
  // pass...
}

LinearColorMap::~LinearColorMap() {
  // pass...
}

QColor
LinearColorMap::map(const double &value) {
  // Calc indices
  double upper = ceil(value*(_colors.size()-1));
  double lower = floor(value*(_colors.size()-1));
  int idx = int(lower);
  if (lower == upper) { return _colors[idx]; }
  double dv = upper-(value*(_colors.size()-1));
  double dr = dv * (_colors[idx].red()   - _colors[idx+1].red());
  double dg = dv * (_colors[idx].green() - _colors[idx+1].green());
  double db = dv * (_colors[idx].blue()  - _colors[idx+1].blue());
  return QColor(int(_colors[idx+1].red()+dr),
                int(_colors[idx+1].green()+dg),
                int(_colors[idx+1].blue()+db));
}


/* ****************************************************************************************** *
 *  Implementation of WaterFallView
 * ****************************************************************************************** */
WaterFallView::WaterFallView(SpectrumProvider *spectrum, size_t hist, Direction dir, QWidget *parent)
  : QWidget(parent), _spectrum(spectrum), _N(_spectrum->fftSize()), _M(hist), _dir(dir),
    _waterfall(_N,_M)
{
  switch (dir) {
  case BOTTOM_UP:
  case TOP_DOWN:
    setMinimumHeight(hist);
    break;
  case LEFT_RIGHT:
  case RIGHT_LEFT:
    setMinimumWidth(hist);
    break;
  }

  // Fill waterfall pixmap
  _waterfall.fill(Qt::black);
  // Create color map
  //_colorMap = new GrayScaleColorMap(-120, 0);
  QVector<QColor> colors; colors.reserve(4);
  colors << Qt::black << Qt::red << Qt::yellow << Qt::white;
  _colorMap = new LinearColorMap(colors, -60, 0);

  // Get notified once a new spectrum is available:
  QObject::connect(_spectrum, SIGNAL(spectrumUpdated()), this, SLOT(_onSpectrumUpdated()));
  QObject::connect(_spectrum, SIGNAL(spectrumConfigured()), this, SLOT(update()));
}

void
WaterFallView::_onSpectrumUpdated() {
  if (_waterfall.isNull() || (_M==0) || (_N==0)) { return; }

  QPainter painter(&_waterfall);
  // scroll pixmap one pixel up
  QRect target(0,0, _N, _M-1), source(0,1,_N,_M-1);
  painter.drawPixmap(target, _waterfall, source);

  // Draw new spectrum
  for (size_t i=0; i<_N; i++) {
    int idx = (_spectrum->fftSize()/2+i) % _spectrum->fftSize();
    double value;
    if ((TOP_DOWN == _dir) || (LEFT_RIGHT == _dir)) {
      value = 10*log10(_spectrum->spectrum()[_spectrum->fftSize()-1-idx])-10*log10(_N);
    } else {
      value = 10*log10(_spectrum->spectrum()[idx])-10*log10(_N);
    }
    // Reset NaNs
    if (value != value) { value = std::numeric_limits<double>::min(); }
    painter.setPen((*_colorMap)(value));
    painter.drawPoint(i, _M-1);
  }

  // Trigger update
  this->update();
}

void
WaterFallView::mouseReleaseEvent(QMouseEvent *evt) {
  // If event is accepted -> determine frequency
  if ((evt->pos().x() < 0) || (evt->pos().x() > this->width())) { return; }
  double f=0;

  if ((BOTTOM_UP == _dir) || (TOP_DOWN == _dir)) {
    double dfdx = _spectrum->sampleRate()/this->width();
    f = -_spectrum->sampleRate()/2 + dfdx*evt->pos().x();
    emit click(f);
  } else {
    double dfdx = _spectrum->sampleRate()/this->height();
    f = -_spectrum->sampleRate()/2 + dfdx*evt->pos().y();
    emit click(f);
  }

  // Forward to default impl:
  QWidget::mouseReleaseEvent(evt);

  // redraw
  this->update();
}

void
WaterFallView::paintEvent(QPaintEvent *evt)
{
  QWidget::paintEvent(evt);

  QPainter painter(this);

  painter.save();
  painter.setRenderHints(QPainter::SmoothPixmapTransform|QPainter::Antialiasing);
  // Assemble trafo
  QTransform trafo;
  switch (_dir) {
  case BOTTOM_UP:
    trafo.scale(this->width()/qreal(_N), this->height()/qreal(_M));
    break;
  case LEFT_RIGHT:
    trafo.scale(this->width()/qreal(_M), this->height()/qreal(_N));
    trafo.translate(_M,0);
    trafo.rotate(90);
    break;
  case TOP_DOWN:
    trafo.scale(this->width()/qreal(_N), this->height()/qreal(_M));
    trafo.translate(_N,_M);
    trafo.rotate(180);
    break;
  case RIGHT_LEFT:
    trafo.scale(this->width()/qreal(_M), this->height()/qreal(_N));
    trafo.translate(0,_N);
    trafo.rotate(270);
    break;
  }
  painter.setTransform(trafo);
  QRect exposedRect = painter.matrix().inverted()
      .mapRect(evt->rect())
      .adjusted(-1, -1, 1, 1);
  // the adjust is to account for half pixels along edges
  painter.drawPixmap(exposedRect, _waterfall, exposedRect);
  //painter.drawPixmap(0,0, _waterfall);
  painter.restore();
}





#include "waterfallview.hh"
#include <QPainter>
#include <QPaintEvent>

using namespace sdr;
using namespace sdr::gui;



/* ****************************************************************************************** *
 *  Implementation of ColorMap
 * ****************************************************************************************** */
ColorMap::ColorMap() {
  // pass...
}

ColorMap::~ColorMap() {
  // pass...
}

/* ****************************************************************************************** *
 *  Implementation of GrayScaleColorMap
 * ****************************************************************************************** */
GrayScaleColorMap::GrayScaleColorMap(double mindB)
  : ColorMap(), _mindB(mindB)
{
  // pass...
}

GrayScaleColorMap::~GrayScaleColorMap() {
  // pass...
}

QColor
GrayScaleColorMap::operator ()(const double &value) {
  if (value > 0) { return Qt::white; }
  if (value < _mindB) { return Qt::black; }
  int h = (255*(value-_mindB))/std::abs(_mindB);
  return QColor(h,h,h);
}


/* ****************************************************************************************** *
 *  Implementation of GrayScaleColorMap
 * ****************************************************************************************** */
WaterFallView::WaterFallView(Spectrum *spectrum, size_t height, QWidget *parent)
  : QWidget(parent), _spectrum(spectrum), _N(_spectrum->fftSize()), _M(height), _waterfall(_N,_M)
{
  // Fill waterfall pixmap
  _waterfall.fill(Qt::black);
  // Create color map
  _colorMap = new GrayScaleColorMap();

  // Get notified once a new spectrum is available:
  QObject::connect(_spectrum, SIGNAL(spectrumUpdated()), this, SLOT(_onSpectrumUpdated()));
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
    double value = 10*log10(_spectrum->spectrum()[i])-10*log10(_N);
    painter.setPen((*_colorMap)(value));
    painter.drawPoint(i, _M-1);
  }

  // Trigger update
  this->update();
}

void
WaterFallView::paintEvent(QPaintEvent *evt) {
  QPainter painter(this);
  painter.save();
  // Draw scaled pixmap
  painter.drawPixmap(evt->rect(), _waterfall.scaled(this->width(), this->height()), evt->rect());
  painter.restore();
}





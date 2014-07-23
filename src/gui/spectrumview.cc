#include "spectrumview.hh"
#include <QPaintEvent>
#include <QPainter>
#include <QPaintEngine>
#include <QResizeEvent>
#include <QFontMetrics>

using namespace sdr;
using namespace sdr::gui;

SpectrumView::SpectrumView(Spectrum *spectrum, QWidget *parent)
  : QWidget(parent), _spectrum(spectrum), _numXTicks(11), _numYTicks(6),
    _maxF(std::numeric_limits<double>::infinity()), _mindB(-60)
{
  // Assemble pens
  _axisPen = QPen(Qt::black);
  _axisPen.setWidth(3);
  _axisPen.setStyle(Qt::SolidLine);
  _graphPen = QPen(Qt::blue);
  _axisPen.setWidth(2);
  _axisPen.setStyle(Qt::SolidLine);

  // Connect to update signal
  QObject::connect(_spectrum, SIGNAL(spectrumUpdated()), this, SLOT(update()));
}

SpectrumView::~SpectrumView() {
  // pass...
}


void
SpectrumView::mouseReleaseEvent(QMouseEvent *evt) {
  // If event is accepted -> determine frequency
  if ((evt->pos().x() < _plotArea.left()) || (evt->pos().x() > _plotArea.right())) { return; }
  double f=0;

  if (_spectrum->isInputReal()) {
    double dfdx = _spectrum->sampleRate()/(2*_plotArea.width());
    f = dfdx * (evt->pos().x()-_plotArea.left());
  } else {
    double dfdx = _spectrum->sampleRate()/_plotArea.width();
      f = -_spectrum->sampleRate()/2 + dfdx * (evt->pos().x()-_plotArea.left());
  }

  emit click(f);

  // Forward to default impl:
  QWidget::mouseReleaseEvent(evt);
  // redraw
  update();
}


void
SpectrumView::resizeEvent(QResizeEvent *evt) {
  // First, forward to default impl.
  QWidget::resizeEvent(evt);

  // If resize event was accepted, recalc plot area
  if (evt->isAccepted()) {
    QSize ws = evt->size();
    QFontMetrics fm(_axisFont);
    int leftMargin = 15 + 6*fm.width("x");
    int topMargin = 10;
    int rightMargin = 3*fm.width("x");
    int bottomMargin = 15 + 2*fm.xHeight();
    _plotArea = QRect(
          leftMargin, topMargin,
          ws.width()-leftMargin-rightMargin,
          ws.height()-bottomMargin-topMargin);
  }
}

void
SpectrumView::paintEvent(QPaintEvent *evt) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.save();

  // Clip region to update
  painter.setClipRect(evt->rect());

  // Draw background
  painter.fillRect(QRect(0,0, this->width(), this->height()), QColor(Qt::white));

  _drawAxis(painter);

  _drawGraph(painter);

  painter.restore();
}


void
SpectrumView::_drawGraph(QPainter &painter) {
  // Draw spectrum
  painter.save();
  painter.setClipRect(_plotArea);
  painter.setPen(_graphPen);
  double height = _plotArea.height();
  double width  = _plotArea.width();

  if (_spectrum->isInputReal()) {
    double df = double(2*width)/_spectrum->fftSize();
    double dh = double(height)/_mindB;
    int xoff  = _plotArea.x();
    int yoff  = _plotArea.y();
    for (size_t i=1; i<_spectrum->fftSize()/2; i++) {
      int x1 = xoff+df*(i-1), x2 = xoff+df*(i);
      int y1 = yoff+dh*(10*log10(_spectrum->spectrum()[i-1])-10*log10(_spectrum->fftSize()));
      int y2 = yoff+dh*(10*log10(_spectrum->spectrum()[i])-10*log10(_spectrum->fftSize()));
      painter.drawLine(x1, y1, x2, y2);
    }
  } else {
    double df = double(width)/_spectrum->fftSize();
    double dh = double(height)/_mindB;
    int xoff  = _plotArea.x();
    int yoff  = _plotArea.y();
    for (size_t i=1; i<_spectrum->fftSize(); i++) {
      int idx1 = (_spectrum->fftSize()/2+i-1) % _spectrum->fftSize();
      int idx2 = (_spectrum->fftSize()/2+i) % _spectrum->fftSize();
      int x1 = xoff+df*(i-1), x2 = xoff+df*(i);
      int y1 = yoff+dh*(10*log10(_spectrum->spectrum()[idx1])-10*log10(_spectrum->fftSize()));
      int y2 = yoff+dh*(10*log10(_spectrum->spectrum()[idx2])-10*log10(_spectrum->fftSize()));
      painter.drawLine(x1,y1, x2,y2);
    }
  }
  painter.restore();
}


void
SpectrumView::_drawAxis(QPainter &painter) {
  painter.save();
  // Get some sizes
  double height = _plotArea.height();
  double width  = _plotArea.width();

  painter.setPen(_axisPen);

  // Axes
  painter.drawLine(_plotArea.topLeft(), _plotArea.bottomLeft());
  painter.drawLine(_plotArea.bottomLeft(), _plotArea.bottomRight());

  // draw y-ticks & labels
  double dh = double(height)/_mindB;
  double x = _plotArea.topLeft().x();
  double y = _plotArea.topLeft().y();
  double ddB = _mindB/(_numYTicks-1);
  QFontMetrics fm(_axisFont); QRect bb; double db=0;
  for (size_t i=0; i<_numYTicks; i++, db+=ddB) {
    QString label = QString("%1").arg(db);
    bb = fm.boundingRect(label);
    y = _plotArea.topLeft().y() + db*dh;
    bb.translate(x-8-bb.width(),y+fm.strikeOutPos());
    painter.drawLine(x-5,y, x,y);
    painter.drawText(bb, label);
  }

  // Draw x ticks & labels (real spectrum)
  if (_spectrum->isInputReal()) {
    double dx = double(width)/(_numXTicks-1);
    double maxF = std::min(_maxF, _spectrum->sampleRate()/2);
    double df = maxF/(_numXTicks-1);
    double x = _plotArea.bottomLeft().x();
    double y = _plotArea.bottomLeft().y();
    double f = 0;
    for (size_t i=0; i<11; i++, f +=df, x += dx) {
      QString label = QString("%1").arg(f);
      bb = fm.boundingRect(label);
      bb.translate(x-bb.width()/2,y+8+fm.overlinePos());
      painter.drawLine(x,y, x,y+5);
      painter.drawText(bb, label);
    }
  } else {
    double dx = double(width)/(_numXTicks-1);
    double df = _spectrum->sampleRate()/(_numXTicks-1);
    double x = _plotArea.bottomLeft().x();
    double y = _plotArea.bottomLeft().y();
    double f = -_spectrum->sampleRate()/2;
    for (size_t i=0; i<_numXTicks; i++, f +=df, x += dx) {
      QString label = QString("%1").arg(f);
      bb = fm.boundingRect(label);
      bb.translate(x-bb.width()/2,y+8+fm.overlinePos());
      painter.drawLine(x,y, x,y+5);
      painter.drawText(bb, label);
    }
  }
  painter.restore();
}

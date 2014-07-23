#ifndef __SDR_GUI_WATERFALLVIEW_HH__
#define __SDR_GUI_WATERFALLVIEW_HH__

#include <QWidget>
#include <QImage>
#include "spectrum.hh"


namespace sdr {
namespace gui {

class WaterFallView : public QWidget
{
  Q_OBJECT

public:
  explicit WaterFallView(Spectrum *spectrum, QWidget *parent = 0);

protected:
  Spectrum *_spectrum;
  QImage *_waterfall;
};

}
}

#endif // WATERFALLVIEW_HH

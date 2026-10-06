#ifndef BANDINDICATOR_H
#define BANDINDICATOR_H

#include <QWidget>
#include <QList>
#include <QColor>
#include "itubands.h"

// 30 px strip showing the area around the current frequency, centered on it.
// Bands in view are shaded like the chart band highlights; a dot marks the
// frequency in the SWR color.
//   - drag the dot: tune
//   - drag anywhere else: slide the view left/right
//   - click anywhere else: jump the frequency there
//   - wheel: zoom
// The view recenters whenever the frequency is set from outside (typed,
// stepped, band pick); its width follows the band the frequency is in.
class BandIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit BandIndicator(QWidget* parent = nullptr);

    void setBands(const QList<BandPreset>& bands);
    void setFrequencyKHz(double khz);
    void setDotColor(const QColor& color);

signals:
    void frequencyPicked(double khz);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    enum class Drag { None, Dot, Pan };

    void adaptSpan();
    double usableWidth() const;
    double xToKHz(double x) const;
    double kHzToX(double khz) const;
    void pick(double khz);

    QList<BandPreset> m_bands;
    double m_khz = 0;
    double m_center = 0;
    double m_span = 0;
    QColor m_dotColor;
    Drag m_drag = Drag::None;
    double m_pressX = 0;
    double m_pressCenter = 0;
    bool m_moved = false;
};

#endif // BANDINDICATOR_H

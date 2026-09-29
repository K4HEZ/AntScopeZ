#ifndef SWRCHART_H
#define SWRCHART_H

#include <QQuickPaintedItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// SWR-vs-frequency plot drawn with QPainter (Qt Graphs isn't available on Android).
// Y axis fixed 1..10 to match the desktop SWR chart.
class SwrChart : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList points READ points WRITE setPoints)
    // {"fromKHz","toKHz","label"} per band (e.g. BandPresets.bands); each
    // overlapping the plotted sweep is drawn as a translucent highlight,
    // matching the desktop's Band Highlighting (MainWindow::addBand()).
    Q_PROPERTY(QVariantList bands READ bands WRITE setBands)

public:
    explicit SwrChart(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;
    QVariantList points() const { return m_points; }
    void setPoints(const QVariantList& points);
    QVariantList bands() const { return m_bands; }
    void setBands(const QVariantList& bands);

private:
    QVariantList m_points;
    QVariantList m_bands;
};

#endif // SWRCHART_H

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

public:
    explicit SwrChart(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;
    QVariantList points() const { return m_points; }
    void setPoints(const QVariantList& points);

private:
    QVariantList m_points;
};

#endif // SWRCHART_H

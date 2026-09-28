#ifndef SMITHCHART_H
#define SMITHCHART_H

#include <QQuickPaintedItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Smith chart drawn with QPainter (Qt Graphs isn't available on Android).
// Grid circles are drawn whole and clipped to the unit circle, rather than
// point-sampling each arc.
class SmithChart : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList points READ points WRITE setPoints)
    Q_PROPERTY(double z0 READ z0 WRITE setZ0)

public:
    explicit SmithChart(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    QVariantList points() const { return m_points; }
    void setPoints(const QVariantList& points);
    double z0() const { return m_z0; }
    void setZ0(double v);

private:
    QVariantList m_points;
    double m_z0 = 50;
};

#endif // SMITHCHART_H

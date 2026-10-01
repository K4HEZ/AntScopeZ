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
    // Point marked with a dot; -1 for none. Tapping the chart emits
    // pointSelected() with the nearest trace point.
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex)

public:
    explicit SmithChart(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    QVariantList points() const { return m_points; }
    void setPoints(const QVariantList& points);
    double z0() const { return m_z0; }
    void setZ0(double v);
    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int v);

signals:
    void pointSelected(int index);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;

private:
    QPointF pointToScreen(const QVariant& point) const;
    void selectNear(const QPointF& pos);
    int m_selectedIndex = -1;
    QVariantList m_points;
    double m_z0 = 50;
};

#endif // SMITHCHART_H

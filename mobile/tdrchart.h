#ifndef TDRCHART_H
#define TDRCHART_H

#include <QElapsedTimer>
#include <QList>
#include <QTimer>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

// One TDR trace (impulse, step or impedance) against distance, drawn with
// QPainter. Dense traces are drawn as a min/max envelope per pixel column
// so narrow reflections aren't lost.
class TdrChart : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QList<double> values READ values WRITE setValues)
    // Distance between consecutive values, in `unit`.
    Q_PROPERTY(double xStep READ xStep WRITE setXStep)
    Q_PROPERTY(QString unit READ unit WRITE setUnit)
    // Label for the values themselves ("Ω", "ρ", "amp", ...).
    Q_PROPERTY(QString valueUnit READ valueUnit WRITE setValueUnit)
    // Zoom (>= 1) shows 1/zoom of the trace; viewSize/viewPosition drive a
    // ScrollBar the same way as SwrChart's.
    Q_PROPERTY(double zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(double viewSize READ viewSize NOTIFY viewChanged)
    Q_PROPERTY(double viewPosition READ viewPosition WRITE setViewPosition NOTIFY viewChanged)
    // Index marked by the cursor; -1 for none. Touch/drag emits pointSelected().
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex)

public:
    explicit TdrChart(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;
    QList<double> values() const { return m_values; }
    void setValues(const QList<double>& v);
    double xStep() const { return m_xStep; }
    void setXStep(double v);
    QString unit() const { return m_unit; }
    void setUnit(const QString& v);
    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int v);
    QString valueUnit() const { return m_valueUnit; }
    void setValueUnit(const QString& v);
    double zoom() const { return m_zoom; }
    double viewSize() const;
    double viewPosition() const;
    void setViewPosition(double v);

    Q_INVOKABLE void zoomIn() { setZoom(m_zoom * 2); }
    Q_INVOKABLE void zoomOut() { setZoom(m_zoom / 2); }
    Q_INVOKABLE void fit() { setZoom(1); }

signals:
    void pointSelected(int index);
    void viewChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;

private:
    QRectF plotRect() const;
    double visibleSpan() const; // in index steps
    void setZoom(double zoom);
    void setFirst(double first);
    void edgeScrollTick();
    void selectAt(qreal x);
    QList<double> m_values;
    double m_xStep = 1;
    QString m_unit;
    QString m_valueUnit;
    int m_selectedIndex = -1;
    double m_zoom = 1;
    double m_first = 0; // index at the left edge
    bool m_dragging = false;
    qreal m_dragX = 0;
    QTimer m_edgeTimer;
    QElapsedTimer m_edgeClock;
};

#endif // TDRCHART_H

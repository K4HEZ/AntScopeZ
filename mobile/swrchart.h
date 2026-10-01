#ifndef SWRCHART_H
#define SWRCHART_H

#include <QQuickPaintedItem>
#include <QElapsedTimer>
#include <QRectF>
#include <QTimer>
#include <QVector>
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
    // Point marked by the cursor line and dot; -1 for none. Touching or
    // dragging on the plot emits pointSelected() with the nearest point.
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex)
    // Minimum horizontal spacing between points. A scan with more points
    // than fit at this density shows a window of them; viewSize/viewPosition
    // (0..1, for a ScrollBar) describe and move that window.
    Q_PROPERTY(double minPixelsPerPoint READ minPixelsPerPoint WRITE setMinPixelsPerPoint)
    Q_PROPERTY(double viewSize READ viewSize NOTIFY viewChanged)
    Q_PROPERTY(double viewPosition READ viewPosition WRITE setViewPosition NOTIFY viewChanged)

public:
    explicit SwrChart(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;
    QVariantList points() const { return m_points; }
    void setPoints(const QVariantList& points);
    QVariantList bands() const { return m_bands; }
    void setBands(const QVariantList& bands);
    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int v);
    double minPixelsPerPoint() const { return m_minPx; }
    void setMinPixelsPerPoint(double v);
    double viewSize() const;
    double viewPosition() const;
    void setViewPosition(double v);

signals:
    void pointSelected(int index);
    void viewChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;

private:
    QRectF plotRect() const;
    void selectAt(qreal x);
    double visibleSpan() const; // in point-index units
    void setFirst(double first);
    double fqAt(double index) const;
    int m_selectedIndex = -1;
    double m_minPx = 5;
    double m_first = 0; // index of the leftmost visible point
    void edgeScrollTick();
    bool m_panning = false;
    bool m_dragging = false;       // marker drag in progress
    bool m_dragSelecting = false;  // inside selectAt()'s emit
    qreal m_dragX = 0;
    QTimer m_edgeTimer;
    QElapsedTimer m_edgeClock;
    qreal m_panStartX = 0;
    double m_panStartFirst = 0;
    QVector<double> m_fq;
    QVector<double> m_swr;
    QVariantList m_points;
    QVariantList m_bands;
};

#endif // SWRCHART_H

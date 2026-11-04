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
    // Wheel/trackpad-glide zoom support (mouse/trackpad only -- see
    // SwrPage.qml): keeps whatever index was under screenX fixed on screen
    // while the zoom changes, so it zooms exactly at the cursor. Also used
    // by a pinch gesture on a non-touch pointer (e.g. a trackpad's native
    // gesture channel); not used for touch -- see beginSelect()'s comment.
    Q_INVOKABLE void beginGesture(qreal screenX);
    Q_INVOKABLE void updateGesture(qreal scale, qreal screenX);
    Q_INVOKABLE void wheelZoom(qreal factor, qreal screenX);
    // Marker select/pan, driven by a QML PointHandler rather than the old
    // mousePressEvent()-style overrides. The intent was for a PinchHandler
    // to take the point over from this handler when a second finger
    // arrives, via Qt Quick's grab-negotiation (grabPermissions) -- in
    // principle this works (and does, for a trackpad's separate gesture
    // channel), but on a real touchscreen the single-finger grab in
    // practice still wins and a two-finger pinch never completes. So touch
    // pan/zoom doesn't go through PinchHandler at all; see SwrPage.qml's
    // ChartOverview strip and setViewWindow() below instead. This handler
    // is kept as-is (PointHandler, not the old mouse overrides) because
    // it's still the right tool for marker select on its own.
    Q_INVOKABLE void beginSelect(qreal x, qreal y);
    Q_INVOKABLE void updateSelect(qreal x);
    Q_INVOKABLE void endSelect();
    // Overview-strip support: jump straight to an arbitrary window
    // [position, position+size] (0..1 fractions of the full point range).
    // Unlike beginGesture()/wheelZoom(), which incrementally adjust from
    // wherever the view currently is, this is an absolute jump -- needed
    // because a scan's frequency range can span orders of magnitude (100
    // kHz to several GHz), where incremental pan/zoom (or edge-autoscroll)
    // would take many steps to get anywhere.
    Q_INVOKABLE void setViewWindow(qreal position, qreal size);

signals:
    void pointSelected(int index);
    void viewChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    QRectF plotRect() const;
    void selectAt(qreal x);
    double indexAt(qreal screenX) const; // fractional point index under a screen x
    double visibleSpan() const; // in point-index units
    void setFirst(double first);
    double fqAt(double index) const;
    int m_selectedIndex = -1;
    double m_minPx = 5;
    double m_gestureAnchorIndex = 0; // index held fixed under the gesture's screen position
    double m_gestureBaseMinPx = 5; // m_minPx at the start of the current pinch
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

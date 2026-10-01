#include "swrchart.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <cmath>
#include <QPainter>
#include <QVariantMap>

namespace {
const qreal kLeftMargin = 28;
const qreal kBottomMargin = 20;
}

SwrChart::SwrChart(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setImplicitHeight(220);
    m_edgeTimer.setInterval(16);
    connect(&m_edgeTimer, &QTimer::timeout, this, &SwrChart::edgeScrollTick);
}

void SwrChart::setPoints(const QVariantList& points)
{
    m_points = points;
    m_fq.resize(points.size());
    m_swr.resize(points.size());
    for (int i = 0; i < points.size(); ++i) {
        const QVariantMap p = points.at(i).toMap();
        m_fq[i] = p.value("fq").toDouble();
        m_swr[i] = p.value("swr").toDouble();
    }
    if (points.size() < 2)
        m_first = 0;
    setFirst(m_first);
    emit viewChanged();
    update();
}

void SwrChart::setBands(const QVariantList& bands)
{
    m_bands = bands;
    update();
}

QRectF SwrChart::plotRect() const
{
    return QRectF(kLeftMargin, 4, width() - kLeftMargin - 6, height() - kBottomMargin - 4);
}

// How many index steps the plot shows: everything if it fits at
// minPixelsPerPoint, otherwise a window.
double SwrChart::visibleSpan() const
{
    const int n = m_fq.size();
    if (n < 2)
        return 1;
    const double fits = plotRect().width() / qMax(0.5, m_minPx);
    return qBound(1.0, fits, double(n - 1));
}

void SwrChart::setFirst(double first)
{
    const int n = m_fq.size();
    const double maxFirst = n < 2 ? 0 : qMax(0.0, (n - 1) - visibleSpan());
    first = qBound(0.0, first, maxFirst);
    if (qFuzzyCompare(m_first + 1, first + 1))
        return;
    m_first = first;
    emit viewChanged();
    update();
}

double SwrChart::fqAt(double index) const
{
    const int n = m_fq.size();
    if (n == 0)
        return 0;
    index = qBound(0.0, index, double(n - 1));
    const int lo = int(index);
    const int hi = qMin(lo + 1, n - 1);
    return m_fq[lo] + (m_fq[hi] - m_fq[lo]) * (index - lo);
}

double SwrChart::viewSize() const
{
    const int n = m_fq.size();
    return n < 2 ? 1 : visibleSpan() / (n - 1);
}

double SwrChart::viewPosition() const
{
    const int n = m_fq.size();
    return n < 2 ? 0 : m_first / (n - 1);
}

void SwrChart::setViewPosition(double v)
{
    setFirst(v * (m_fq.size() - 1));
}

void SwrChart::setMinPixelsPerPoint(double v)
{
    v = qMax(0.5, v);
    if (qFuzzyCompare(m_minPx, v))
        return;
    m_minPx = v;
    setFirst(m_first);
    emit viewChanged();
    update();
}

void SwrChart::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.width() != oldGeometry.width()) {
        setFirst(m_first);
        emit viewChanged();
    }
}

void SwrChart::setSelectedIndex(int v)
{
    if (m_selectedIndex == v)
        return;
    m_selectedIndex = v;
    // Bring it into view when set from elsewhere (Smith slider, a finished
    // scan); not while dragging here, which scrolls via edgeScrollTick().
    if (!m_dragSelecting && v >= 0 && v < m_fq.size()) {
        const double span = visibleSpan();
        const double margin = span * 0.1;
        if (v < m_first - span || v > m_first + 2 * span)
            setFirst(v - span / 2); // far away: center it
        else if (v < m_first + margin)
            setFirst(v - margin);
        else if (v > m_first + span - margin)
            setFirst(v - span + margin);
    }
    update();
}

// Nearest point to x within the visible window.
void SwrChart::selectAt(qreal x)
{
    if (m_fq.size() < 2)
        return;
    const QRectF plot = plotRect();
    if (plot.width() <= 0)
        return;
    const double span = visibleSpan();
    const double idx = m_first + (x - plot.left()) / plot.width() * span;
    const int lo = int(std::ceil(m_first - 1e-9));
    const int hi = qMin(int(m_fq.size()) - 1, int(std::floor(m_first + span + 1e-9)));
    m_dragSelecting = true;
    emit pointSelected(qBound(lo, int(qRound(idx)), hi));
    m_dragSelecting = false;
}

// While dragging near (or past) a plot edge, scroll smoothly under the
// marker, faster the closer to / further past the edge.
void SwrChart::edgeScrollTick()
{
    const QRectF plot = plotRect();
    const qreal zone = 36;
    qreal depth = 0; // -1..1, sign = direction
    if (m_dragX < plot.left() + zone)
        depth = -qMin(1.0, (plot.left() + zone - m_dragX) / zone);
    else if (m_dragX > plot.right() - zone)
        depth = qMin(1.0, (m_dragX - (plot.right() - zone)) / zone);
    const qint64 ms = m_edgeClock.restart();
    if (depth == 0 || viewSize() >= 1 || plot.width() <= 0)
        return;
    const double pxPerPoint = plot.width() / visibleSpan();
    const double pxPerSec = 250;
    setFirst(m_first + depth * pxPerSec * (ms / 1000.0) / pxPerPoint);
    selectAt(m_dragX);
}

void SwrChart::mousePressEvent(QMouseEvent* event)
{
    // Keep the drag here; otherwise a vertical move hands it to the
    // enclosing Flickable and horizontal tracking stops.
    setKeepMouseGrab(true);
    setKeepTouchGrab(true);
    const QPointF pos = event->position();
    // A press on the frequency labels pans; on the plot it moves the marker.
    if (pos.y() > plotRect().bottom() && viewSize() < 1) {
        m_panning = true;
        m_panStartX = pos.x();
        m_panStartFirst = m_first;
        return;
    }
    m_dragging = true;
    m_dragX = pos.x();
    m_edgeClock.start();
    m_edgeTimer.start();
    selectAt(pos.x());
}

void SwrChart::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        const qreal w = plotRect().width();
        if (w > 0)
            setFirst(m_panStartFirst - (event->position().x() - m_panStartX) / w * visibleSpan());
        return;
    }
    m_dragX = event->position().x();
    selectAt(m_dragX);
}

void SwrChart::mouseReleaseEvent(QMouseEvent*)
{
    m_panning = false;
    m_dragging = false;
    m_edgeTimer.stop();
    setKeepMouseGrab(false);
    setKeepTouchGrab(false);
}

void SwrChart::mouseUngrabEvent()
{
    m_panning = false;
    m_dragging = false;
    m_edgeTimer.stop();
    setKeepMouseGrab(false);
    setKeepTouchGrab(false);
}

void SwrChart::paint(QPainter* painter)
{
    const QRectF rect(0, 0, width(), height());
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillRect(rect, Qt::white);

    const qreal leftMargin = kLeftMargin;
    const QRectF plot = plotRect();

    QPen gridPen(QColor(200, 200, 200));
    gridPen.setStyle(Qt::DotLine);
    QPen axisPen(Qt::black);

    QFont f = painter->font();
    f.setPointSize(9);
    painter->setFont(f);

    for (int swr = 1; swr <= 10; ++swr) {
        const qreal y = plot.bottom() - (swr - 1) / 9.0 * plot.height();
        painter->setPen(swr == 1 ? axisPen : gridPen);
        painter->drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter->setPen(axisPen);
        painter->drawText(QRectF(rect.left(), y - 8, leftMargin - 4, 16),
                           Qt::AlignRight | Qt::AlignVCenter, QString::number(swr));
    }
    painter->setPen(axisPen);
    painter->drawRect(plot);

    const int n = m_fq.size();
    if (n < 2)
        return;

    const double span = visibleSpan();
    const double minFq = fqAt(m_first);
    double maxFq = fqAt(m_first + span);
    if (maxFq <= minFq)
        maxFq = minFq + 1;

    auto fqLabel = [](double mhz) { return QString::number(mhz, 'f', 3); };
    painter->drawText(QRectF(plot.left() - 20, plot.bottom() + 2, 90, 16),
                       Qt::AlignLeft, fqLabel(minFq));
    painter->drawText(QRectF(plot.center().x() - 45, plot.bottom() + 2, 90, 16),
                       Qt::AlignHCenter, fqLabel((minFq + maxFq) / 2));
    painter->drawText(QRectF(plot.right() - 70, plot.bottom() + 2, 90, 16),
                       Qt::AlignRight, fqLabel(maxFq));

    auto toX = [&](double fq) { return plot.left() + (fq - minFq) / (maxFq - minFq) * plot.width(); };
    auto xOfIndex = [&](double i) { return plot.left() + (i - m_first) / span * plot.width(); };
    auto toY = [&](double swr) {
        const double clamped = qBound(1.0, swr, 10.0);
        return plot.bottom() - (clamped - 1.0) / 9.0 * plot.height();
    };

    painter->setClipRect(plot.adjusted(-1, -1, 1, 1));

    // Band highlighting -- same translucent-rectangle idea as the desktop's
    // MainWindow::addBand(), drawn for every band that overlaps the visible
    // window, clipped to it. Bands are in kHz; points' fq is in MHz.
    QFont bandFont = painter->font();
    bandFont.setPointSize(7);
    painter->setFont(bandFont);
    for (const QVariant& v : m_bands) {
        const QVariantMap b = v.toMap();
        const double bandFromMHz = b.value("fromKHz").toDouble() / 1000.0;
        const double bandToMHz = b.value("toKHz").toDouble() / 1000.0;
        if (bandToMHz < minFq || bandFromMHz > maxFq)
            continue;
        const qreal x1 = qBound(plot.left(), toX(qMax(bandFromMHz, minFq)), plot.right());
        const qreal x2 = qBound(plot.left(), toX(qMin(bandToMHz, maxFq)), plot.right());
        const QRectF bandRect(QPointF(x1, plot.top()), QPointF(x2, plot.bottom()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(50, 50, 150, 50));
        painter->drawRect(bandRect);
        // Label centered on the band, not clipped to it; kept inside the plot.
        const QString label = b.value("label").toString();
        const qreal tw = QFontMetricsF(bandFont).horizontalAdvance(label);
        const qreal lx = qBound(plot.left() + 1, bandRect.center().x() - tw / 2, plot.right() - tw - 1);
        painter->setPen(QColor(50, 50, 150, 200));
        painter->drawText(QRectF(lx, plot.top() + 1, tw + 2, 12), Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, label);
    }
    f.setPointSize(9);
    painter->setFont(f);

    // Only the visible slice, plus one point either side so the line runs to the edges.
    const int firstIdx = qMax(0, int(m_first) - 1);
    const int lastIdx = qMin(n - 1, int(m_first + span) + 2);
    QPolygonF polyline;
    int minIdx = -1;
    double minSwr = 1e9;
    for (int i = firstIdx; i <= lastIdx; ++i) {
        polyline << QPointF(xOfIndex(i), toY(m_swr[i]));
        if (m_swr[i] < minSwr) {
            minSwr = m_swr[i];
            minIdx = i;
        }
    }

    QPen curvePen(QColor(30, 180, 40));
    curvePen.setWidthF(2.5);
    curvePen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(curvePen);
    painter->drawPolyline(polyline);

    // Cursor on the selected point; falls back to the min-SWR point of the
    // window when nothing is selected.
    const int markIdx = (m_selectedIndex >= 0 && m_selectedIndex < n) ? m_selectedIndex : minIdx;
    if (markIdx >= 0) {
        const QPointF marker(xOfIndex(markIdx), toY(m_swr[markIdx]));
        QPen dash(QColor(120, 120, 120));
        dash.setStyle(Qt::DashLine);
        painter->setPen(dash);
        painter->drawLine(QPointF(marker.x(), plot.top()), QPointF(marker.x(), plot.bottom()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(30, 180, 40));
        painter->drawEllipse(marker, 5, 5);
    }
    painter->setClipping(false);
}

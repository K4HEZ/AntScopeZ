#include "tdrchart.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

namespace {
const qreal kLeftMargin = 40;
const qreal kBottomMargin = 20;
}

TdrChart::TdrChart(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setImplicitHeight(220);
    m_edgeTimer.setInterval(16);
    connect(&m_edgeTimer, &QTimer::timeout, this, &TdrChart::edgeScrollTick);
}

void TdrChart::setValues(const QList<double>& v)
{
    const bool resized = v.size() != m_values.size();
    m_values = v;
    if (resized) {
        m_zoom = 1;
        m_first = 0;
        emit viewChanged();
    }
    update();
}

void TdrChart::setValueUnit(const QString& v)
{
    m_valueUnit = v;
    update();
}

double TdrChart::visibleSpan() const
{
    const int n = m_values.size();
    return n < 2 ? 1 : (n - 1) / m_zoom;
}

double TdrChart::viewSize() const
{
    const int n = m_values.size();
    return n < 2 ? 1 : visibleSpan() / (n - 1);
}

double TdrChart::viewPosition() const
{
    const int n = m_values.size();
    return n < 2 ? 0 : m_first / (n - 1);
}

void TdrChart::setViewPosition(double v)
{
    setFirst(v * (m_values.size() - 1));
}

void TdrChart::setFirst(double first)
{
    const int n = m_values.size();
    first = qBound(0.0, first, n < 2 ? 0.0 : (n - 1) - visibleSpan());
    if (qFuzzyCompare(m_first + 1, first + 1))
        return;
    m_first = first;
    emit viewChanged();
    update();
}

// At least ~16 samples stay in view.
double TdrChart::maxZoom() const
{
    return qMax(1.0, (m_values.size() - 1) / 16.0);
}

void TdrChart::setZoom(double zoom)
{
    const int n = m_values.size();
    if (n < 2)
        return;
    zoom = qBound(1.0, zoom, maxZoom());
    if (qFuzzyCompare(m_zoom, zoom))
        return;
    // Anchor on the marker if it's in view (it stays at the same spot on
    // screen), else on the centre.
    const bool anchored = m_selectedIndex >= m_first && m_selectedIndex <= m_first + visibleSpan();
    const double anchorIndex = anchored ? m_selectedIndex : m_first + visibleSpan() / 2;
    const double anchorFrac = (anchorIndex - m_first) / visibleSpan();
    m_zoom = zoom;
    m_first = qBound(0.0, anchorIndex - anchorFrac * visibleSpan(), (n - 1) - visibleSpan());
    emit viewChanged();
    update();
}

void TdrChart::setXStep(double v)
{
    m_xStep = v;
    update();
}

void TdrChart::setUnit(const QString& v)
{
    m_unit = v;
    update();
}

void TdrChart::setSelectedIndex(int v)
{
    if (m_selectedIndex == v)
        return;
    m_selectedIndex = v;
    update();
}

QRectF TdrChart::plotRect() const
{
    return QRectF(kLeftMargin, 4, width() - kLeftMargin - 6, height() - kBottomMargin - 4);
}

void TdrChart::selectAt(qreal x)
{
    if (m_values.size() < 2)
        return;
    const QRectF plot = plotRect();
    if (plot.width() <= 0)
        return;
    const double idx = m_first + (x - plot.left()) / plot.width() * visibleSpan();
    const int lo = int(std::ceil(m_first - 1e-9));
    const int hi = qMin(int(m_values.size()) - 1, int(std::floor(m_first + visibleSpan() + 1e-9)));
    emit pointSelected(qBound(lo, int(qRound(idx)), hi));
}

// Dragging near (or past) a plot edge scrolls under the marker, faster the
// closer to / further past the edge.
void TdrChart::edgeScrollTick()
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

void TdrChart::mousePressEvent(QMouseEvent* event)
{
    // Same reasoning as SwrChart: keep the drag out of the enclosing Flickable.
    setKeepMouseGrab(true);
    setKeepTouchGrab(true);
    m_dragging = true;
    m_dragX = event->position().x();
    m_edgeClock.start();
    m_edgeTimer.start();
    selectAt(m_dragX);
}

void TdrChart::mouseMoveEvent(QMouseEvent* event)
{
    m_dragX = event->position().x();
    selectAt(m_dragX);
}

void TdrChart::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
    m_edgeTimer.stop();
    setKeepMouseGrab(false);
    setKeepTouchGrab(false);
}

void TdrChart::mouseUngrabEvent()
{
    m_dragging = false;
    m_edgeTimer.stop();
    setKeepMouseGrab(false);
    setKeepTouchGrab(false);
}

void TdrChart::paint(QPainter* painter)
{
    const QRectF rect(0, 0, width(), height());
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillRect(rect, Qt::white);

    const QRectF plot = plotRect();
    QFont f = painter->font();
    f.setPointSize(9);
    painter->setFont(f);
    painter->setPen(Qt::black);
    painter->drawRect(plot);

    const int n = m_values.size();
    if (n < 2)
        return;

    auto [minIt, maxIt] = std::minmax_element(m_values.begin(), m_values.end());
    double lo = *minIt, hi = *maxIt;
    if (hi - lo < 1e-9) {
        lo -= 0.5;
        hi += 0.5;
    }
    const double pad = (hi - lo) * 0.05;
    lo -= pad;
    hi += pad;

    auto toY = [&](double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };
    const double span = visibleSpan();
    auto toX = [&](double i) { return plot.left() + (i - m_first) / span * plot.width(); };

    // Zero line, and Y labels at the extremes.
    if (lo < 0 && hi > 0) {
        QPen zero(QColor(200, 200, 200));
        zero.setStyle(Qt::DotLine);
        painter->setPen(zero);
        painter->drawLine(QPointF(plot.left(), toY(0)), QPointF(plot.right(), toY(0)));
    }
    painter->setPen(Qt::black);
    painter->drawText(QRectF(0, plot.top() - 2, kLeftMargin - 4, 14), Qt::AlignRight, QString::number(hi, 'g', 3));
    painter->drawText(QRectF(0, plot.bottom() - 12, kLeftMargin - 4, 14), Qt::AlignRight, QString::number(lo, 'g', 3));
    if (!m_valueUnit.isEmpty())
        painter->drawText(QRectF(0, plot.center().y() - 7, kLeftMargin - 4, 14), Qt::AlignRight, m_valueUnit);

    const double xFrom = m_first * m_xStep;
    const double xTo = (m_first + span) * m_xStep;
    painter->drawText(QRectF(plot.left() - 10, plot.bottom() + 2, 80, 16), Qt::AlignLeft,
                       QString::number(xFrom, 'f', 1));
    painter->drawText(QRectF(plot.center().x() - 40, plot.bottom() + 2, 80, 16), Qt::AlignHCenter,
                       QString::number((xFrom + xTo) / 2, 'f', 1));
    painter->drawText(QRectF(plot.right() - 90, plot.bottom() + 2, 96, 16), Qt::AlignRight,
                       QString("%1 %2").arg(xTo, 0, 'f', 1).arg(m_unit));

    painter->setClipRect(plot.adjusted(-1, -1, 1, 1));
    QPen curve(QColor(30, 100, 200));
    curve.setWidthF(1.5);
    painter->setPen(curve);

    // Visible samples, one either side so the line reaches the edges.
    const int i0 = qMax(0, int(std::floor(m_first)));
    const int i1 = qMin(n - 1, int(std::ceil(m_first + span)));
    const int count = i1 - i0 + 1;
    const int columns = qMax(1, int(plot.width()));
    if (count <= columns * 2) {
        QPolygonF line;
        line.reserve(count);
        for (int i = i0; i <= i1; ++i)
            line << QPointF(toX(i), toY(m_values[i]));
        painter->drawPolyline(line);
    } else {
        // Min/max per pixel column.
        QPolygonF line;
        line.reserve(columns * 2);
        for (int c = 0; c < columns; ++c) {
            const int from = i0 + int(qint64(c) * count / columns);
            const int to = qMax(from + 1, i0 + int(qint64(c + 1) * count / columns));
            double cMin = m_values[from], cMax = m_values[from];
            for (int i = from; i < qMin(to, i1 + 1); ++i) {
                cMin = qMin(cMin, m_values[i]);
                cMax = qMax(cMax, m_values[i]);
            }
            const qreal x = plot.left() + c;
            line << QPointF(x, toY(cMin)) << QPointF(x, toY(cMax));
        }
        painter->drawPolyline(line);
    }

    if (m_selectedIndex >= 0 && m_selectedIndex < n && m_selectedIndex >= m_first
        && m_selectedIndex <= m_first + span) {
        const QPointF marker(toX(m_selectedIndex), toY(m_values[m_selectedIndex]));
        QPen dash(QColor(120, 120, 120));
        dash.setStyle(Qt::DashLine);
        painter->setPen(dash);
        painter->drawLine(QPointF(marker.x(), plot.top()), QPointF(marker.x(), plot.bottom()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(200, 30, 30));
        painter->drawEllipse(marker, 5, 5);
    }
    painter->setClipping(false);
}

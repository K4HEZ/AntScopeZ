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
}

void TdrChart::setValues(const QList<double>& v)
{
    m_values = v;
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
    const double idx = (x - plot.left()) / plot.width() * (m_values.size() - 1);
    emit pointSelected(qBound(0, int(qRound(idx)), int(m_values.size()) - 1));
}

void TdrChart::mousePressEvent(QMouseEvent* event)
{
    // Same reasoning as SwrChart: keep the drag out of the enclosing Flickable.
    setKeepMouseGrab(true);
    setKeepTouchGrab(true);
    selectAt(event->position().x());
}

void TdrChart::mouseMoveEvent(QMouseEvent* event)
{
    selectAt(event->position().x());
}

void TdrChart::mouseReleaseEvent(QMouseEvent*)
{
    setKeepMouseGrab(false);
    setKeepTouchGrab(false);
}

void TdrChart::mouseUngrabEvent()
{
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
    auto toX = [&](double i) { return plot.left() + i / (n - 1) * plot.width(); };

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

    const double span = (n - 1) * m_xStep;
    painter->drawText(QRectF(plot.left() - 10, plot.bottom() + 2, 80, 16), Qt::AlignLeft, "0");
    painter->drawText(QRectF(plot.center().x() - 40, plot.bottom() + 2, 80, 16), Qt::AlignHCenter,
                       QString::number(span / 2, 'f', 1));
    painter->drawText(QRectF(plot.right() - 90, plot.bottom() + 2, 96, 16), Qt::AlignRight,
                       QString("%1 %2").arg(span, 0, 'f', 1).arg(m_unit));

    painter->setClipRect(plot.adjusted(-1, -1, 1, 1));
    QPen curve(QColor(30, 100, 200));
    curve.setWidthF(1.5);
    painter->setPen(curve);

    const int columns = qMax(1, int(plot.width()));
    if (n <= columns * 2) {
        QPolygonF line;
        line.reserve(n);
        for (int i = 0; i < n; ++i)
            line << QPointF(toX(i), toY(m_values[i]));
        painter->drawPolyline(line);
    } else {
        // Min/max per pixel column.
        QPolygonF line;
        line.reserve(columns * 2);
        for (int c = 0; c < columns; ++c) {
            const int from = int(qint64(c) * n / columns);
            const int to = qMax(from + 1, int(qint64(c + 1) * n / columns));
            double cMin = m_values[from], cMax = m_values[from];
            for (int i = from; i < qMin(to, n); ++i) {
                cMin = qMin(cMin, m_values[i]);
                cMax = qMax(cMax, m_values[i]);
            }
            const qreal x = plot.left() + c;
            line << QPointF(x, toY(cMin)) << QPointF(x, toY(cMax));
        }
        painter->drawPolyline(line);
    }

    if (m_selectedIndex >= 0 && m_selectedIndex < n) {
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

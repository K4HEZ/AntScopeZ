#include "swrchart.h"

#include <QPainter>
#include <QVariantMap>

SwrChart::SwrChart(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setImplicitHeight(220);
}

void SwrChart::setPoints(const QVariantList& points)
{
    m_points = points;
    update();
}

void SwrChart::paint(QPainter* painter)
{
    const QRectF rect(0, 0, width(), height());
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillRect(rect, Qt::white);

    const qreal leftMargin = 28;
    const qreal bottomMargin = 20;
    const QRectF plot(rect.left() + leftMargin, rect.top() + 4,
                       rect.width() - leftMargin - 6, rect.height() - bottomMargin - 4);

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

    if (m_points.size() < 2)
        return;

    const double minFq = m_points.first().toMap().value("fq").toDouble();
    double maxFq = m_points.last().toMap().value("fq").toDouble();
    if (maxFq <= minFq)
        maxFq = minFq + 1;

    auto fqLabel = [](double mhz) { return QString::number(mhz, 'f', 3); };
    painter->drawText(QRectF(plot.left() - 20, plot.bottom() + 2, 90, 16),
                       Qt::AlignLeft, fqLabel(minFq));
    painter->drawText(QRectF(plot.center().x() - 45, plot.bottom() + 2, 90, 16),
                       Qt::AlignHCenter, fqLabel((minFq + maxFq) / 2));
    painter->drawText(QRectF(plot.right() - 70, plot.bottom() + 2, 90, 16),
                       Qt::AlignRight, fqLabel(maxFq));

    auto toPoint = [&](double fq, double swr) {
        const qreal x = plot.left() + (fq - minFq) / (maxFq - minFq) * plot.width();
        const double clamped = qBound(1.0, swr, 10.0);
        const qreal y = plot.bottom() - (clamped - 1.0) / 9.0 * plot.height();
        return QPointF(x, y);
    };

    QPolygonF polyline;
    int minIdx = -1;
    double minSwr = 1e9;
    for (int i = 0; i < m_points.size(); ++i) {
        const QVariantMap p = m_points.at(i).toMap();
        const double fq = p.value("fq").toDouble();
        const double swr = p.value("swr").toDouble();
        polyline << toPoint(fq, swr);
        if (swr < minSwr) {
            minSwr = swr;
            minIdx = i;
        }
    }

    QPen curvePen(QColor(30, 180, 40));
    curvePen.setWidthF(2.5);
    curvePen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(curvePen);
    painter->drawPolyline(polyline);

    if (minIdx >= 0) {
        const QPointF marker = polyline.at(minIdx);
        QPen dash(QColor(120, 120, 120));
        dash.setStyle(Qt::DashLine);
        painter->setPen(dash);
        painter->drawLine(QPointF(marker.x(), plot.top()), QPointF(marker.x(), plot.bottom()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(30, 180, 40));
        painter->drawEllipse(marker, 4, 4);
    }
}

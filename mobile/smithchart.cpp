#include "smithchart.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariantMap>

#include <rfmath.h>

namespace {
// Normalized resistance/reactance rings drawn on every Smith chart.
const double kResistanceRings[] = {0.2, 0.5, 1.0, 2.0, 5.0};
const double kReactanceRings[] = {0.2, 0.5, 1.0, 2.0, 5.0};
// Constant-SWR circles, centered at the origin, |Gamma| = (SWR-1)/(SWR+1).
const double kSwrRings[] = {1.5, 2.0, 3.0, 5.0};
}

SmithChart::SmithChart(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setImplicitHeight(260);
}

void SmithChart::setPoints(const QVariantList& points)
{
    m_points = points;
    update();
}

void SmithChart::setZ0(double v)
{
    if (qFuzzyCompare(m_z0, v))
        return;
    m_z0 = v;
    update();
}

void SmithChart::paint(QPainter* painter)
{
    const QRectF rect(0, 0, width(), height());
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillRect(rect, Qt::white);

    const qreal margin = 8;
    const qreal side = qMin(rect.width(), rect.height()) - 2 * margin;
    if (side <= 0)
        return;
    const QPointF center = rect.center();
    const qreal radius = side / 2;

    // Gamma-plane (|Gamma| <= 1) -> screen; +reactance drawn above the axis.
    auto toScreen = [&](double gx, double gy) {
        return QPointF(center.x() + gx * radius, center.y() - gy * radius);
    };

    QPainterPath clip;
    clip.addEllipse(center, radius, radius);
    painter->setClipPath(clip);

    QPen gridPen(QColor(150, 150, 150));
    painter->setBrush(Qt::NoBrush);

    // Constant-resistance circles: center (r/(r+1), 0), radius 1/(r+1).
    for (double r : kResistanceRings) {
        const double gr = 1.0 / (r + 1.0);
        painter->setPen(gridPen);
        painter->drawEllipse(toScreen(r / (r + 1.0), 0), gr * radius, gr * radius);
    }

    // Constant-reactance arcs: center (1, +-1/x), radius 1/x; clipped to the unit circle.
    for (double x : kReactanceRings) {
        const double gr = 1.0 / x;
        for (int sign : {1, -1}) {
            painter->setPen(gridPen);
            painter->drawEllipse(toScreen(1.0, sign * gr), gr * radius, gr * radius);
        }
    }

    // Zero-reactance diameter and outer boundary.
    painter->setPen(gridPen);
    painter->drawLine(toScreen(-1, 0), toScreen(1, 0));
    painter->setPen(QPen(Qt::black, 1.5));
    painter->drawEllipse(center, radius, radius);

    QPen swrPen(QColor(210, 210, 210));
    for (double swr : kSwrRings) {
        const double gr = (swr - 1.0) / (swr + 1.0);
        painter->setPen(swrPen);
        painter->drawEllipse(center, gr * radius, gr * radius);
    }

    painter->setClipping(false);
    QFont labelFont = painter->font();
    labelFont.setPointSize(8);
    painter->setFont(labelFont);
    painter->setPen(Qt::black);
    for (double swr : kSwrRings) {
        const double gr = (swr - 1.0) / (swr + 1.0);
        const QPointF top = toScreen(0, gr);
        painter->drawText(QRectF(top.x() - 25, top.y() - 16, 50, 14),
                           Qt::AlignHCenter | Qt::AlignBottom, QString("%1:1").arg(swr));
    }
    painter->setClipPath(clip);

    if (m_points.size() < 2 || m_z0 <= 0) {
        painter->setClipping(false);
        return;
    }

    QPolygonF polyline;
    for (const QVariant& v : m_points) {
        const QVariantMap p = v.toMap();
        const double rNorm = p.value("r").toDouble() / m_z0;
        const double xNorm = p.value("x").toDouble() / m_z0;
        double gx6 = 0, gy6 = 0;
        RfMath::smithPoint(rNorm, xNorm, gx6, gy6);
        polyline << toScreen(gx6 / 6.0, gy6 / 6.0);
    }

    QPen curvePen(QColor(30, 180, 40));
    curvePen.setWidthF(2.5);
    curvePen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(curvePen);
    painter->drawPolyline(polyline);
    painter->setClipping(false);
}

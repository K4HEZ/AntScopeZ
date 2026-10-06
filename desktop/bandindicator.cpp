#include "bandindicator.h"
#include "appconfig.h"
#include <QPainter>
#include <QPolygonF>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <cmath>

static const int kMargin = 10;     // keeps the dot clear of the rounded ends
static const int kDotRadius = 6;
static const int kGrabSlop = 5;    // extra pixels around the dot that still grab it

BandIndicator::BandIndicator(QWidget* parent) :
    QWidget(parent),
    m_dotColor(Qt::gray)
{
    setFixedHeight(30);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
}

void BandIndicator::setBands(const QList<BandPreset>& bands)
{
    m_bands = bands;
    adaptSpan();
    update();
}

// Set from outside (typed, stepped, band pick): recenter. The echo of our
// own pick() comes back here too; ignore it so dragging doesn't recenter.
void BandIndicator::setFrequencyKHz(double khz)
{
    if (std::fabs(khz - m_khz) < 0.0015)
        return;
    m_khz = khz;
    m_center = khz;
    adaptSpan();
    update();
}

void BandIndicator::setDotColor(const QColor& color)
{
    m_dotColor = color;
    update();
}

// The view is as wide as the band the frequency is in (padded by the Band
// margin, as the charts show it); outside every band, keep the last width.
void BandIndicator::adaptSpan()
{
    for (const BandPreset& b : m_bands) {
        if (m_khz >= b.fromKHz && m_khz <= b.toKHz) {
            double lo, hi;
            ItuBands::widen(b.fromKHz, b.toKHz, AppConfig::get().bandMarginPercent, lo, hi);
            if (hi > lo) {
                m_span = hi - lo;
                return;
            }
        }
    }
    if (m_span <= 0)
        m_span = qMax(1.0, m_khz * 0.1);
}

double BandIndicator::usableWidth() const
{
    return qMax(1.0, width() - 2.0 * kMargin);
}

double BandIndicator::xToKHz(double x) const
{
    return m_center - m_span / 2 + (x - kMargin) / usableWidth() * m_span;
}

double BandIndicator::kHzToX(double khz) const
{
    return kMargin + (khz - (m_center - m_span / 2)) / m_span * usableWidth();
}

void BandIndicator::pick(double khz)
{
    khz = qMax(0.0, khz);
    m_khz = khz;
    update();
    emit frequencyPicked(khz);
}

void BandIndicator::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(palette().color(QPalette::Mid));
    p.setBrush(palette().color(QPalette::Base));
    p.drawRoundedRect(r, 4, 4);
    if (m_span <= 0)
        return;

    double lo = m_center - m_span / 2;
    double hi = m_center + m_span / 2;

    // Band highlights, same blue as the charts'.
    QFont small = font();
    small.setPointSize(8);
    p.setFont(small);
    QFontMetrics fm(small);
    for (const BandPreset& b : m_bands) {
        if (b.toKHz < lo || b.fromKHz > hi)
            continue;
        double x1 = qMax(kHzToX(b.fromKHz), r.left() + 1);
        double x2 = qMin(kHzToX(b.toKHz), r.right() - 1);
        QRectF bandRect(x1, r.top() + 1, x2 - x1, r.height() - 2);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(50, 50, 150, 90));
        p.drawRect(bandRect);
        if (!b.label.isEmpty() && bandRect.width() > fm.horizontalAdvance(b.label) + 6) {
            p.setPen(palette().color(QPalette::Text));
            p.drawText(bandRect, Qt::AlignHCenter | Qt::AlignTop, b.label);
        }
    }

    // View ends, in MHz.
    p.setPen(palette().color(QPalette::PlaceholderText));
    QRectF textRect = r.adjusted(4, 0, -4, -1);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignBottom, QString::number(qMax(0.0, lo) / 1000.0, 'f', 3));
    p.drawText(textRect, Qt::AlignRight | Qt::AlignBottom, QString::number(hi / 1000.0, 'f', 3));

    // Frequency marker. Out of view (after sliding), a filled arrow at the
    // edge points toward it.
    double x = kHzToX(m_khz);
    double cy = r.center().y();
    QPen outline(palette().color(QPalette::Text), 1.5);
    if (x < kMargin || x > width() - kMargin) {
        bool left = x < kMargin;
        double ex = left ? r.left() + 3 : r.right() - 3;
        double dir = left ? 1 : -1;
        QPolygonF arrow;
        arrow << QPointF(ex, cy) << QPointF(ex + dir * 10, cy - 8) << QPointF(ex + dir * 10, cy + 8);
        p.setPen(outline);
        p.setBrush(m_dotColor);
        p.drawPolygon(arrow);
        return;
    }
    p.setPen(QPen(palette().color(QPalette::Text), 1));
    p.drawLine(QPointF(x, r.top() + 2), QPointF(x, r.bottom() - 2));
    p.setPen(outline);
    p.setBrush(m_dotColor);
    p.drawEllipse(QPointF(x, cy), kDotRadius, kDotRadius);
}

void BandIndicator::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || m_span <= 0)
        return;
    double x = e->position().x();
    m_pressX = x;
    m_pressCenter = m_center;
    m_moved = false;
    m_drag = std::fabs(x - kHzToX(m_khz)) <= kDotRadius + kGrabSlop ? Drag::Dot : Drag::Pan;
    setCursor(m_drag == Drag::Dot ? Qt::SizeHorCursor : Qt::ClosedHandCursor);
}

void BandIndicator::mouseMoveEvent(QMouseEvent* e)
{
    double x = e->position().x();
    if (m_drag == Drag::Dot) {
        m_moved = true;
        pick(xToKHz(x));
    } else if (m_drag == Drag::Pan) {
        if (std::fabs(x - m_pressX) > 3)
            m_moved = true;
        if (m_moved) {
            m_center = m_pressCenter - (x - m_pressX) / usableWidth() * m_span;
            update();
        }
    } else {
        bool onDot = std::fabs(x - kHzToX(m_khz)) <= kDotRadius + kGrabSlop;
        setCursor(onDot ? Qt::SizeHorCursor : Qt::OpenHandCursor);
        QToolTip::showText(e->globalPosition().toPoint(),
                           tr("%1 MHz -- click to tune here, drag to slide the view, wheel to zoom")
                               .arg(xToKHz(x) / 1000.0, 0, 'f', 4), this);
    }
}

void BandIndicator::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    if (m_drag == Drag::Pan && !m_moved)
        pick(xToKHz(m_pressX)); // a click, not a slide
    m_drag = Drag::None;
    setCursor(Qt::OpenHandCursor);
}

void BandIndicator::wheelEvent(QWheelEvent* e)
{
    if (m_span <= 0)
        return;
    double x = e->position().x();
    double under = xToKHz(x);
    double factor = std::pow(0.9, e->angleDelta().y() / 120.0); // wheel up zooms in
    m_span = qBound(0.5, m_span * factor, 1.0e7);
    // Keep the frequency under the cursor where it was.
    m_center = under - (x - kMargin) / usableWidth() * m_span + m_span / 2;
    update();
    e->accept();
}

#include "popupindicator.h"
#include <QPainter>
#include <QApplication>
#include <QGuiApplication>
#include <QDateTime>
#include <QDebug>

// TEMPORARY instrumentation (2026-09-05) -- root-caused now (see
// showIndicator()'s own comment) via a live run's timestamped [BUSY] log
// lined up against the wire-level chunk timing. Kept for one more
// confirmation pass; remove once the QGuiApplication::sync() fix below is
// confirmed live.
#define POPUP_INDICATOR_DEBUG(msg) \
    qDebug().noquote() << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << "[BUSY]" << msg


PopUpIndicator* PopUpIndicator::m_popUpIndicator = nullptr;

PopUpIndicator::PopUpIndicator(QWidget *parent)
    : PopUp(parent)
{
    m_popUpIndicator = this;
    setBackgroundColor(Qt::red);
    setPenColor(Qt::red);
}

void PopUpIndicator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

//    QRect roundedRect;
//    roundedRect.setX(rect().x() + 5);
//    roundedRect.setY(rect().y() + 5);
//    roundedRect.setWidth(rect().width() - 10);
//    roundedRect.setHeight(rect().height() - 10);
    painter.setBrush(QBrush(Qt::red));
    painter.setPen(Qt::red);

    int R = rect().width()/2-2;
    painter.drawEllipse(rect().center(), R, R);
}

void PopUpIndicator::showIndicator(QWidget* parent)
{
    POPUP_INDICATOR_DEBUG("showIndicator() called, wasNull=" << (m_popUpIndicator == nullptr));
    if (m_popUpIndicator == nullptr)
        m_popUpIndicator = new PopUpIndicator(parent);
    QPoint pt = m_popUpIndicator->parentWidget()->mapToGlobal(QPoint(10, 10));
    m_popUpIndicator->move(pt.x(), pt.y());
    m_popUpIndicator->show();
    POPUP_INDICATOR_DEBUG("showIndicator() after show(): isVisible=" << m_popUpIndicator->isVisible()
        << "pos=" << pt << "size=" << m_popUpIndicator->size());
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Root-caused 2026-09-05 via a live [BUSY]-tagged debug run: on a
    // >255-point NanoVNA V2/LiteVNA64 scan, NanovnaV2Analyzer::
    // processFifoChunk() burns ~20ms/point (255 points -> ~5s, confirmed
    // live) in one single synchronous call -- each point's emitPoint()
    // triggers a full chart replot inline, with no event-loop turn between
    // points for the whole chunk. isVisible() was already true the entire
    // time (confirmed live too) -- show() had genuinely been called and
    // Qt's own bookkeeping agreed the widget was shown -- but nothing had
    // forced the *window system* to actually composite/paint it yet.
    // QApplication::processEvents() alone only dispatches whatever's
    // already queued in Qt's own event queue; it doesn't wait for a round
    // trip to the X11/Wayland compositor, and that round trip's result
    // never arrived before the 5-second freeze began, starving it for the
    // freeze's entire duration. The window only ever got its first real
    // on-screen paint in the brief gap between chunks -- and, being a
    // "frozen" (non-repainting) window from then on, kept showing that
    // last-painted frame right through the second chunk's own freeze,
    // which is what made it look like it "started working" at the chunk
    // boundary. QGuiApplication::sync() is the documented way to force
    // that round trip to actually complete before returning, so the first
    // paint can never again land inside a freeze regardless of how long
    // one particular chunk's synchronous burst turns out to be.
    QApplication::processEvents();
    QGuiApplication::sync();
    POPUP_INDICATOR_DEBUG("showIndicator() after sync(): isVisible=" << m_popUpIndicator->isVisible());
}

void PopUpIndicator::hideIndicator(QWidget* parent)
{
    POPUP_INDICATOR_DEBUG("hideIndicator() called, wasNull=" << (m_popUpIndicator == nullptr));
    if (m_popUpIndicator == nullptr)
        m_popUpIndicator = new PopUpIndicator(parent);
    QApplication::restoreOverrideCursor();
    // Was hideAnimation() -- PopUp's inherited 1000ms opacity fade-out;
    // QWidget::hide() only actually runs once that animation finishes
    // (PopUp::hide()'s getPopupOpacity()==0.0 check, fired off the
    // animation's own finished signal). Same class of problem as show()'s
    // fade-in just below: a timer-driven animation needs the event loop
    // to advance, and can't be trusted to ever finish if a new scan's own
    // freeze starts before it does -- same live 2026-09-05 finding, fixed
    // the same way. Hide immediately instead.
    m_popUpIndicator->QWidget::hide();
}

void PopUpIndicator::setIndicatorVisible(bool visible)
{
    if (visible)
        showIndicator();
    else
        hideIndicator();
}

void PopUpIndicator::show()
{
    // Was a 150ms opacity fade-in (0->1), like PopUp::show()'s. That
    // animation only advances via timer ticks, which need the event loop
    // running -- but showIndicator() (see its own comment) is typically
    // called right before a scan's synchronous per-point burst freezes
    // the event loop almost immediately. The fade would get a few ticks
    // in and then freeze partway, leaving the dot visibly dim for that
    // whole freeze -- confirmed live 2026-09-05 ("dim for the first
    // segment, bright for the next"): the animation only got to finish
    // reaching full opacity in the brief idle gap between segments.
    // Full opacity immediately avoids depending on the event loop for
    // this widget's own correctness at all.
    setWindowOpacity(1.0);
    QWidget::show();
}

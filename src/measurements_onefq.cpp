#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"

extern bool g_developerMode;
extern QMap<QString, QString> g_mapTabPlotNames;
extern int g_maxMeasurements; // defined in measurements.cpp
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original measurements.cpp (still in
// measurements.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of Measurements.

void Measurements::showOneFqWidget(QWidget* _parent, int _dots)
{
    bool enteringOneFqMode = !m_oneFqMode;
    m_oneFqMode = true;

    if (m_graphHintBox != nullptr)
        m_graphHintBox->setVisible(false);

    if (enteringOneFqMode) {
        m_oneFqSavedHints = QPair<bool,bool>(m_graphHintEnabled, m_graphBriefHintEnabled);
        m_graphHintEnabled = false;
        m_graphBriefHintEnabled = false;
        createOneFqDisplayWidgets(_parent, _dots);
    }
}

void Measurements::createOneFqDisplayWidgets(QWidget* parent, int dots)
{
    m_oneFqWidget = new OneFqWidget(dots, parent);
    connect(m_oneFqWidget, &OneFqWidget::canceled, this, &Measurements::hideOneFqWidget);
    connect(m_oneFqWidget, &OneFqWidget::styleToggleRequested, this, &Measurements::toggleOneFqDisplayStyle);

    m_oneFqBigReadout = new OneFqBigReadout(parent);
    m_oneFqBigReadout->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_oneFqBigReadout, &OneFqBigReadout::closing, this, &Measurements::onOneFqBigReadoutClosing);
    connect(m_oneFqBigReadout, &OneFqBigReadout::styleToggleRequested, this, &Measurements::toggleOneFqDisplayStyle);

    updateOneFqDisplayVisibility();
}

void Measurements::updateOneFqDisplayVisibility()
{
    if (m_oneFqWidget)
        m_oneFqWidget->setVisible(m_oneFqDisplayStyle == OneFqDisplayStyle::Detailed);
    if (m_oneFqBigReadout)
        m_oneFqBigReadout->setVisible(m_oneFqDisplayStyle == OneFqDisplayStyle::BigReadout);
}

void Measurements::destroyOneFqDisplayWidgets()
{
    if (m_oneFqWidget) {
        // No explicit disconnect needed -- OneFqWidget has no closeEvent/
        // reject() of its own that could fire something re-entrant during
        // hide()/delete, and ~QObject() disconnects everything involving
        // tmp anyway. An earlier version called the wildcard tmp->
        // disconnect() here defensively; harmless in effect, but Qt warns
        // ("wildcard call disconnects from destroyed signal") because a
        // signal-wildcard disconnect also severs whatever's listening to
        // tmp's own destroyed() (Qt's own accessibility-bridge bookkeeping
        // among them, given this widget's history -- see this member's
        // declaration comment) -- worth avoiding, not just silencing.
        OneFqWidget* tmp = m_oneFqWidget;
        m_oneFqWidget = nullptr;
        tmp->hide();
        delete tmp;
    }
    if (m_oneFqBigReadout) {
        OneFqBigReadout* tmp = m_oneFqBigReadout;
        m_oneFqBigReadout = nullptr;
        // Precise, not wildcard: only the one connection that actually
        // needs severing before close() below (so it can't re-enter and
        // end One-Fq mode out from under this teardown) -- see the
        // OneFqWidget branch above for why a broad disconnect() is worth
        // avoiding here even though it "worked".
        disconnect(tmp, &OneFqBigReadout::closing, this, &Measurements::onOneFqBigReadoutClosing);
        tmp->close(); // Qt::WA_DeleteOnClose (set in createOneFqDisplayWidgets()) handles deletion
    }
}

void Measurements::updateOneFqWidget(GraphData& _data)
{
    if (m_oneFqWidget == nullptr && m_oneFqBigReadout == nullptr)
        return;
    // Both widgets exist and are fed for the whole One-Fq session (see
    // m_oneFqWidget's declaration comment in measurements.h) -- only one
    // is visible at a time, but keeping both current means a style
    // toggle never shows a stale value.
    if (m_oneFqWidget)
        m_oneFqWidget->addData(_data);
    if (m_oneFqBigReadout)
        m_oneFqBigReadout->addData(_data);

    if(m_smithTracer == NULL)
    {
        m_smithTracer = new QCPItemEllipse(m_smithWidget);
        m_smithTracer->setAntialiased(true);
        QPen pen;
        //pen.setColor(QColor(250,30,20,180));
        pen.setColor(Qt::magenta);
        pen.setWidth(4);
        m_smithTracer->setPen(pen);
    }

    double ptX = _data.ptX;
    double ptY = _data.ptY;

    //ptX = -1.05612;
    //ptY = 2.91823 ;

    // _data.ptX/ptY are already Smith-chart plot coordinates (see
    // Measurements::NormRXtoSmithPoint(), RhoReal*6/RhoImag*6 -- the
    // commented-out hardcoded test values right above, e.g. -1.05612, are
    // in that same ~-6..6 range, not pixel-sized). Running them through
    // pixelToCoord() treated a plot coordinate as a raw pixel position --
    // wrong regardless of outcome, and confirmed via core dump
    // (2026-08-20) to occasionally divide by an axis rect height of 0
    // (transient layout state right as this floating widget first opens),
    // producing inf -> NaN once QCPItemEllipse::draw() converts it back to
    // pixels, aborting on Qt's own qSaturateRound() assert. No conversion
    // needed at all -- setCoords() already expects plot coordinates.

    m_smithTracer->topLeft->setCoords(ptX-0.1, ptY+0.1);
    m_smithTracer->bottomRight->setCoords(ptX+0.1, ptY-0.1);
    m_smithWidget->replot();

}

void Measurements::endOneFqMode()
{
    if (!m_oneFqMode)
        return;
    m_oneFqMode = false;
    m_isContinuing = false;
    m_graphHintEnabled = m_oneFqSavedHints.first;
    m_graphBriefHintEnabled = m_oneFqSavedHints.second;
    showHideHints();
    emit oneFqCanceled();
}

void Measurements::hideOneFqWidget(bool)
{
    if (!m_oneFqMode)
        return;
    endOneFqMode();
    destroyOneFqDisplayWidgets();
}

void Measurements::toggleOneFqDisplayStyle()
{
    if (!m_oneFqMode)
        return;
    m_oneFqDisplayStyle = (m_oneFqDisplayStyle == OneFqDisplayStyle::Detailed)
                               ? OneFqDisplayStyle::BigReadout
                               : OneFqDisplayStyle::Detailed;
    updateOneFqDisplayVisibility();
}

void Measurements::onOneFqBigReadoutClosing()
{
    // BigReadout is already closing/tearing itself down (title-bar X, or
    // Esc while it has focus) -- end the whole session and take down its
    // sibling OneFqWidget, but don't touch BigReadout itself again here;
    // its own Qt::WA_DeleteOnClose (set in createOneFqDisplayWidgets())
    // handles that.
    if (!m_oneFqMode)
        return;
    endOneFqMode();
    if (m_oneFqWidget) {
        // No explicit disconnect needed here either -- see the matching
        // comment in destroyOneFqDisplayWidgets().
        OneFqWidget* tmp = m_oneFqWidget;
        m_oneFqWidget = nullptr;
        tmp->hide();
        delete tmp;
    }
    m_oneFqBigReadout = nullptr;
}

void Measurements::on_mainWindowMinimized(bool minimized)
{
    if (!m_oneFqMode)
        return;
    // Only the currently-visible style needs to track this -- the other
    // one is already hidden (inactive), and blindly setVisible()ing it
    // too would wrongly show it back on restore.
    if (m_oneFqDisplayStyle == OneFqDisplayStyle::Detailed) {
        if (m_oneFqWidget)
            m_oneFqWidget->setVisible(!minimized);
    } else if (m_oneFqBigReadout) {
        if (minimized)
            m_oneFqBigReadout->showMinimized();
        else
            m_oneFqBigReadout->showNormal();
    }
}

void Measurements::on_newMeasurementOneFq(QWidget* parent, qint64 fq, qint32 dots)
{
    m_interrupted = false;
    Q_UNUSED (fq)
    showOneFqWidget(parent, dots);
}

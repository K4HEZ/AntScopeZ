#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "popupindicator.h"
#include "analyzer/customanalyzer.h"
#include "analyzer/nanovna_analyzer.h"
#include "Notification.h"
#include "glwidget.h"
#include "CustomPlot.h"
#include "selectdevicedialog.h"
#include "printmulti.h"
#include "style.h"
#include "filedialog.h"
#include <QWindow>
#include <cmath>

extern QString appendSpaces(const QString& number);
extern bool g_developerMode; // see main.cpp
extern bool g_extendedChartZoom; // see mainwindow.cpp
extern bool g_usbOnly;
extern int g_maxMeasurements; // see measurements.cpp
extern QMap<QString, QString> g_mapTabPlotNames; // see mainwindow.cpp
extern void setAbsoluteFqMaximum();
extern bool g_bAA55modeNewProtocol;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original mainwindow.cpp (still in
// mainwindow.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of MainWindow.

void MainWindow::mouseWheel_swr(QWheelEvent * e)
{   //v4_(04/12)
    // QCustomPlot::wheelEvent() emits mouseWheel() *before* forwarding the
    // event to the axis rect that actually applies the zoom (see that
    // function's own doc comment, qcustomplot.cpp) -- reversed from 1.3.1,
    // where el->wheelEvent() ran first and the signal fired after, so this
    // slot always saw the already-zoomed range. Reading
    // m_swrWidget->xAxis->range() synchronously here on a plain wheel tick
    // (X-only zoom, see setRangeZoom(Qt::Horizontal) in setWidgetsSettings())
    // now gets the range from *before* this tick's zoom -- one tick behind
    // the chart, which repaints with the new range moments later on the
    // same event. Start/Stop and the sibling widgets' xAxis would lag by
    // exactly one wheel click (fixed on the next tick, reading what this
    // one just set -- "sorta sync up" once you keep scrolling). Deferred
    // via singleShot(0, ...) so this reads the range after QCustomPlot has
    // finished applying the zoom (still perceived as instantaneous -- same
    // UI event, next spin of the event loop).
    QTimer::singleShot(0, this, [this]() {
        double from  = m_swrWidget->xAxis->range().lower;
        double to = m_swrWidget->xAxis->range().upper;
        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_phaseWidget->xAxis->setRange(m_swrWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_swrWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_swrWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_swrWidget->xAxis->range());
    });

    if (e->modifiers() == Qt::ControlModifier)
    {
        if(m_measurements)
        {
            QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
        }
        bool noerror=true;
        bool zoomin =false;
        if( e->angleDelta().y() > 0)
            zoomin =true;

        QCPRange range = m_swrWidget->yAxis->range();

        double length = range.size();
        if ((length >= 10 && (!zoomin)) || (length <= 0.1 && zoomin))
            noerror=false;//  return;

        if (!g_extendedChartZoom && (length <= (0.4)) && (zoomin))
            noerror=false;//  return;
        if(noerror){
            double lower = range.lower - 0.02;
            double upper = range.upper;
            double delta = 0.1;
            if(length >= 5)
            {
                delta = 1;
            } else if (length >= 2.5) {
                delta = 0.5;
            } else if (length >= 1) {
                delta = 0.1;
            }

            bool change_upper =false;
            double y = m_swrWidget->yAxis->pixelToCoord( e->position().y()); //focus zoom on point
            if((lower+(length/2))>y)
              change_upper =true;
            //--------------
            if(zoomin)
            {
                delta = -delta;
                if(change_upper)
                {
                    upper += delta ;
                }else{
                    lower -= delta ;
                }
            }else{
                upper += delta ;
                lower -= delta ;
            }
            if (upper >= MAX_SWR)
                upper = MAX_SWR+0.01;

             if (lower <= 0.92)
                 lower = 0.92;
             m_swrWidget->yAxis->setRangeUpper(upper);
             m_swrWidget->yAxis->setRangeLower(lower);
            //-----customize---
            QPen pen=m_swrWidget->yAxis->grid()->pen();
            if(length<=5){
                pen.setWidth(2);
                m_swrWidget->yAxis->grid()->setPen(pen);
                m_swrWidget->yAxis->grid()->setSubGridVisible(true);

             }else{
               pen.setWidth(1);
               m_swrWidget->yAxis->grid()->setPen(pen);
               m_swrWidget->yAxis->grid()->setSubGridVisible(false);

            }

                     //------------
            QTimer::singleShot(5, m_markers, SLOT(redraw()));
        }
    }

    emit rescale();
    replotY_swr();
}


void MainWindow::replotY_swr()
{
    // Used to blank the Y tick labels below 1 (not a physically meaningful
    // SWR value) after mouse pan/zoom, the same one-off hack Measurements::
    // replot() also had for the general redraw path (see its comment,
    // measurements_redraw.cpp) -- both removed in the 2.x port (2026-08-25)
    // in favor of SwrAxisTicker (CustomPlot.h), which blanks the same
    // labels as part of computing them in the first place, on every
    // replot regardless of what triggered it. Left as a no-op rather than
    // removing this function and its several call sites (mainwindow_mouse.cpp,
    // mainwindow_shortcuts.cpp, the mousePress/mouseRelease connections in
    // mainwindow.cpp) -- smaller, lower-risk diff for the same effect.
}


void MainWindow::mouseMove_swr(QMouseEvent *e)
{
    m_isMouseClick = false;
    double x = m_swrWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_swrWidget->yAxis->pixelToCoord(e->pos().y());
    // See mouseMove_smith()'s comment -- same degenerate-axis-range NaN
    // guard, explicit here too rather than relying on the range comparisons
    // below happening to be NaN-safe on their own.
    if (!std::isfinite(x) || !std::isfinite(y))
        return;

    double from;
    double to;
    if(!m_isRange)
    {
        from = getFqFrom();
        to = getFqTo();
    }else
    {
        from = getFqFrom() - getFqTo();
        to = getFqFrom() + getFqTo();
    }
    if((x >= from) && (x <= to))
    {
        if(y >= m_swrWidget->yAxis->range().lower && y <= m_swrWidget->yAxis->range().upper)
        {
            QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
            if(!list.isEmpty())
            {
                QTableWidgetItem * item = list.at(0);
                emit newCursorFq(x, item->row(), QCursor::pos().x(), QCursor::pos().y());
            }
        }
        else if (m_measurements)
        {
            m_measurements->hideGraphCursor();
        }
    }
    else if (m_measurements)
    {
        m_measurements->hideGraphCursor();
    }
    if (e->buttons() & Qt::LeftButton)
    {
        if(!m_isRange)
        {
            setFqFrom(getFqFrom());
            setFqTo(getFqTo());
        }else
        {
            setFqFrom((getFqTo() + getFqFrom())/2);
            setFqTo((getFqTo() - getFqFrom())/2);
        }

        m_phaseWidget->xAxis->setRange(m_swrWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_swrWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_swrWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_swrWidget->xAxis->range());
    }
     replotY_swr();
}

void MainWindow::mouseWheel_phase(QWheelEvent * e)
{
    Q_UNUSED(e);

    // See mouseWheel_swr()'s comment -- deferred so this reads the range
    // after QCustomPlot has actually applied this tick's zoom.
    QTimer::singleShot(0, this, [this]() {
        double from  = m_phaseWidget->xAxis->range().lower;
        double to = m_phaseWidget->xAxis->range().upper;
        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((from+to)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        emit rescale();
    });
}

void MainWindow::mouseMove_phase(QMouseEvent *e)
{
    m_isMouseClick = false;
    double x = m_phaseWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_phaseWidget->yAxis->pixelToCoord(e->pos().y());
    // See mouseMove_smith()'s comment -- same degenerate-axis-range NaN
    // guard, explicit here too rather than relying on the range comparisons
    // below happening to be NaN-safe on their own.
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    double from;
    double to;
    if(!m_isRange)
    {
        from = getFqFrom();
        to = getFqTo();
    }else
    {
        from = getFqFrom() - getFqTo();
        to = getFqFrom() + getFqTo();
    }
    if((x >= from) && (x <= to))
    {
        if(y >= m_phaseWidget->yAxis->range().lower && y <= m_phaseWidget->yAxis->range().upper)
        {
            QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
            if(!list.isEmpty())
            {
                QTableWidgetItem * item = list.at(0);
                emit newCursorFq(x, item->row(), QCursor::pos().x(), QCursor::pos().y());
            }
        }
        else if (m_measurements)
        {
            m_measurements->hideGraphCursor();
        }
    }
    else if (m_measurements)
    {
        m_measurements->hideGraphCursor();
    }
    if (e->buttons() & Qt::LeftButton)
    {
        if(!m_isRange)
        {
            setFqFrom(getFqFrom());
            setFqTo(getFqTo());
        }else
        {
            setFqFrom((getFqTo() + getFqFrom())/2);
            setFqTo((getFqTo() - getFqFrom())/2);
        }
        m_swrWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
    }
}

void MainWindow::mouseWheel_rs(QWheelEvent * e)
{
    static int state = 1;
    // See mouseWheel_swr()'s comment -- deferred so this reads the range
    // after QCustomPlot has actually applied this tick's zoom.
    QTimer::singleShot(0, this, [this]() {
        double from  = m_rsWidget->xAxis->range().lower;
        double to = m_rsWidget->xAxis->range().upper;
        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((from+to)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rsWidget->xAxis->range());
    });

    if (e->modifiers() == Qt::ControlModifier)
    {
        int val;
        if(e->angleDelta().y() < 0)
        {
            if(g_extendedChartZoom || state <= 19)
            {
                ++state;
                val = state*80;
                m_rsWidget->yAxis->setRangeLower(-val);
                m_rsWidget->yAxis->setRangeUpper(val);
                m_rsWidget->replot();
                if(m_markers)
                {
                    QTimer::singleShot(5, m_markers, SLOT(redraw()));
                }
                if(m_measurements)
                {
                    QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
                }
            }
        }else
        {
            if(state > 1)
            {
                --state;
                val = state*80;
                m_rsWidget->yAxis->setRangeLower(-val);
                m_rsWidget->yAxis->setRangeUpper(val);
                m_rsWidget->replot();
                if(m_markers)
                {
                    QTimer::singleShot(5, m_markers, SLOT(redraw()));
                }
                if(m_measurements)
                {
                    QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
                }
            }
        }
    }
    emit rescale();
}

void MainWindow::mouseMove_rs(QMouseEvent *e)
{
    m_isMouseClick = false;
    double x = m_rsWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_rsWidget->yAxis->pixelToCoord(e->pos().y());
    // See mouseMove_smith()'s comment -- same degenerate-axis-range NaN
    // guard, explicit here too rather than relying on the range comparisons
    // below happening to be NaN-safe on their own.
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    int from;
    int to;
    if(!m_isRange)
    {
        from = getFqFrom();
        to = getFqTo();
    }else
    {
        from = getFqFrom() - getFqTo();
        to = getFqFrom() + getFqTo();
    }
    if((x >= from) && (x <= to))
    {
        if(y >= m_rsWidget->yAxis->range().lower && y <= m_rsWidget->yAxis->range().upper)
        {
            QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
            if(!list.isEmpty())
            {
                QTableWidgetItem * item = list.at(0);
                emit newCursorFq(x, item->row(), QCursor::pos().x(), QCursor::pos().y());
            }
        }
        else if (m_measurements)
        {
            m_measurements->hideGraphCursor();
        }
    }
    else if (m_measurements)
    {
        m_measurements->hideGraphCursor();
    }
    if (e->buttons() & Qt::LeftButton)
    {
        if(!m_isRange)
        {
            setFqFrom(getFqFrom());
            setFqTo(getFqTo());
        }else
        {
            setFqFrom((getFqTo() + getFqFrom())/2);
            setFqTo((getFqTo() - getFqFrom())/2);
        }
        m_swrWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rsWidget->xAxis->range());
    }
}

void MainWindow::mouseWheel_rp(QWheelEvent *e)
{
    static quint32 state = 1;
    // See mouseWheel_swr()'s comment -- deferred so this reads the range
    // after QCustomPlot has actually applied this tick's zoom.
    QTimer::singleShot(0, this, [this]() {
        double from  = m_rpWidget->xAxis->range().lower;
        double to = m_rpWidget->xAxis->range().upper;
        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((from+to)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rpWidget->xAxis->range());
    });

    if (e->modifiers() == Qt::ControlModifier)
    {
        if(e->angleDelta().y() < 0)
        {
            if(g_extendedChartZoom || state <= 19)
            {
                ++state;
                int val = state*80;
                m_rpWidget->yAxis->setRangeLower(-val);
                m_rpWidget->yAxis->setRangeUpper(val);
                m_rpWidget->replot();
                if(m_markers)
                {
                    QTimer::singleShot(5, m_markers, SLOT(redraw()));
                }
                if(m_measurements)
                {
                    QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
                }
            }
        }else
        {
            if(state > 1)
            {
                --state;
                int val = state*80;
                m_rpWidget->yAxis->setRangeLower(-val);
                m_rpWidget->yAxis->setRangeUpper(val);
                m_rpWidget->replot();
                if(m_markers)
                {
                    QTimer::singleShot(5, m_markers, SLOT(redraw()));
                }
                if(m_measurements)
                {
                    QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
                }
            }
        }
    }
    emit rescale();
}

void MainWindow::mouseMove_rp(QMouseEvent *e)
{
    m_isMouseClick = false;
    double x = m_rpWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_rpWidget->yAxis->pixelToCoord(e->pos().y());
    // See mouseMove_smith()'s comment -- same degenerate-axis-range NaN
    // guard, explicit here too rather than relying on the range comparisons
    // below happening to be NaN-safe on their own.
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    double from;
    double to;
    if(!m_isRange)
    {
        from = getFqFrom();
        to = getFqTo();
    }else
    {
        from = getFqFrom() - getFqTo();
        to = getFqFrom() + getFqTo();
    }
    if((x >= from) && (x <= to))
    {
        if(y >= m_rpWidget->yAxis->range().lower && y <= m_rpWidget->yAxis->range().upper)
        {
            QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
            if(!list.isEmpty())
            {
                QTableWidgetItem * item = list.at(0);
                emit newCursorFq(x, item->row(), QCursor::pos().x(), QCursor::pos().y());
            }
        }
        else if (m_measurements)
        {
            m_measurements->hideGraphCursor();
        }
    }
    else if (m_measurements)
    {
        m_measurements->hideGraphCursor();
    }
    if (e->buttons() & Qt::LeftButton)
    {
        if(!m_isRange)
        {
            setFqFrom(getFqFrom());
            setFqTo(getFqTo());
        }else
        {
            setFqFrom((getFqTo() + getFqFrom())/2);
            setFqTo((getFqTo() - getFqFrom())/2);
        }
        m_swrWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rpWidget->xAxis->range());
    }
}

void MainWindow::mouseWheel_rl(QWheelEvent *e)
{
    // See mouseWheel_swr()'s comment -- deferred so this reads the range
    // after QCustomPlot has actually applied this tick's zoom.
    QTimer::singleShot(0, this, [this]() {
        double from  = m_rlWidget->xAxis->range().lower;
        double to = m_rlWidget->xAxis->range().upper;
        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((from+to)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rlWidget->xAxis->range());
    });

    if (e->modifiers() == Qt::ControlModifier)
    {
        if(m_measurements)
        {
            QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
        }
        if(e->angleDelta().y() < 0)
        {
            if(g_extendedChartZoom || m_rlZoomState <= 9)
            {
                ++m_rlZoomState;
                m_rlWidget->yAxis->setRangeUpper(m_rlZoomState*5);
                m_rlWidget->yAxis->setRangeLower(0);
                m_rlWidget->replot();
            }
        }else
        {
            int limit = g_extendedChartZoom ? 1 : SWR_ZOOM_LIMIT;
            if(m_rlZoomState > limit)
            {
                --m_rlZoomState;
                m_rlWidget->yAxis->setRangeUpper(m_rlZoomState*5);
                m_rlWidget->yAxis->setRangeLower(0);
                m_rlWidget->replot();
            }
        }
        QTimer::singleShot(5, m_markers, SLOT(redraw()));
    }
    emit rescale();
}

void MainWindow::mouseMove_rl(QMouseEvent *e)
{
    m_isMouseClick = false;
    double x = m_rlWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_rlWidget->yAxis->pixelToCoord(e->pos().y());
    // See mouseMove_smith()'s comment -- same degenerate-axis-range NaN
    // guard, explicit here too rather than relying on the range comparisons
    // below happening to be NaN-safe on their own.
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    double from;
    double to;
    if(!m_isRange)
    {
        from = getFqFrom();
        to = getFqTo();
    }else
    {
        from = getFqFrom() - getFqTo();
        to = getFqFrom() + getFqTo();
    }
    if((x >= from) && (x <= to))
    {
        double lo = m_rlWidget->yAxis->range().lower;
        double up = m_rlWidget->yAxis->range().upper;
        if(y >= lo && y <= up)
        {
            QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
            if(!list.isEmpty())
            {
                QTableWidgetItem * item = list.at(0);
                emit newCursorFq(x, item->row(), QCursor::pos().x(), QCursor::pos().y());
            }
        }
        else if (m_measurements)
        {
            m_measurements->hideGraphCursor();
        }
    }
    else if (m_measurements)
    {
        m_measurements->hideGraphCursor();
    }
    if (e->buttons() & Qt::LeftButton)
    {
        if(!m_isRange)
        {
            setFqFrom(getFqFrom());
            setFqTo(getFqTo());
        }else
        {
            setFqFrom((getFqTo() + getFqFrom())/2);
            setFqTo((getFqTo() - getFqFrom())/2);
        }
        m_swrWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rlWidget->xAxis->range());
    }
}

void MainWindow::mouseWheel_tdr(QWheelEvent* e)
{
    static int state = 0;
    //qDebug() << "MainWindow::mouseWheel_tdr: state" << state << ", lo " << m_tdrWidget->yAxis->range().lower << ", up " << m_tdrWidget->yAxis->range().upper;
    if (e->modifiers() == Qt::ControlModifier)
    {
//        if(m_measurements)
//        {
//            QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
//        }
//        double up = m_tdrWidget->yAxis->range().upper;
//        double lo = m_tdrWidget->yAxis->range().lower;

        if(e->angleDelta().y() < 0)
        {
            if(state <= 30)
            {
                ++state;
//                m_tdrWidget->yAxis->setRangeUpper(up+0.1);//(up + up*0.1 );
//                m_tdrWidget->yAxis->setRangeLower(lo-0.1);//(lo - lo*0.1);
                m_tdrWidget->yAxis->scaleRange(1.1, 0);
                m_tdrWidget->replot();
            }
        } else {
            if(state > -30)
            {
                --state;
//                m_tdrWidget->yAxis->setRangeUpper(up-0.1);//(up - up*0.1 );
//                m_tdrWidget->yAxis->setRangeLower(lo+0.1);//(lo + lo*0.1);
                m_tdrWidget->yAxis->scaleRange(0.9, 0);
                m_tdrWidget->replot();
            }
        }
        QTimer::singleShot(5, m_markers, SLOT(redraw()));
        //emit rescale();
    }
    else
    {
        // Plain scroll (no Ctrl) zooms the X-axis (distance) -- not via
        // any handler in this file, but QCustomPlot's own built-in
        // iRangeZoom interaction, which has no concept of "1000m is a sane
        // ceiling for TDR." Left unclamped, repeated scroll-out reaches
        // absurd values (e.g. 10000000, six-digit-plus meters) with
        // nothing to claw it back -- see on_pressCtrlZero()'s tab_tdr fix.
        // Clamp back down immediately unless the user's explicitly asked
        // for unlimited zoom. Fixed 2026-08-20.
        //
        // Deferred via singleShot(0, ...) since 2026-08-25: QCustomPlot
        // 2.x's wheelEvent() emits mouseWheel() *before* forwarding to the
        // axis rect that actually applies the zoom (reversed from 1.3.1 --
        // see mouseWheel_swr()'s comment), so reading xAxis->range() here
        // synchronously always saw the range from *before* this tick's
        // zoom -- this clamp was permanently one tick behind, silently
        // letting every scroll-out tick through unclamped.
        QTimer::singleShot(0, this, [this]() {
            if (!g_extendedChartZoom && m_tdrWidget->xAxis->range().upper > 1000)
            {
                m_tdrWidget->xAxis->setRangeUpper(1000);
                m_tdrWidget->replot();
            }
        });
    }
}

void MainWindow::mouseMove_tdr(QMouseEvent * e)
{
    double x = m_tdrWidget->xAxis->pixelToCoord(e->pos().x());
    // See mouseMove_smith()'s comment -- same degenerate-axis-range NaN
    // guard, explicit here too rather than relying on the range comparison
    // below happening to be NaN-safe on its own.
    if (!std::isfinite(x))
        return;
    if( (x >= m_tdrWidget->xAxis->range().lower) && (x <= m_tdrWidget->xAxis->range().upper))
    {
        QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
        if(!list.isEmpty())
        {
            QTableWidgetItem * item = list.at(0);
            emit newCursorFq(x, item->row(), QCursor::pos().x(), QCursor::pos().y());
        }
    }
    else if (m_measurements)
    {
        m_measurements->hideGraphCursor();
    }
}

void MainWindow::mouseMove_s21(QMouseEvent * e)
{
    // QPointF pos = e->pos();
    double x = m_s21Widget->xAxis->pixelToCoord(e->pos().x());
    // See mouseMove_smith()'s comment -- same degenerate-axis-range NaN
    // guard, explicit here too rather than relying on the range comparison
    // below happening to be NaN-safe on its own.
    if (!std::isfinite(x))
        return;
    if( (x >= m_s21Widget->xAxis->range().lower) && (x <= m_s21Widget->xAxis->range().upper))
    {
        QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
        if(!list.isEmpty())
        {
            QTableWidgetItem * item = list.at(0);
            emit newCursorFq(x, item->row(), QCursor::pos().x(), QCursor::pos().y());
        }
    }
    else if (m_measurements)
    {
        m_measurements->hideGraphCursor();
    }
}

void MainWindow::mouseMove_smith(QMouseEvent * e)
{
    double x = m_smithWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_smithWidget->yAxis->pixelToCoord(e->pos().y());
    // Every other mouseMove_*() in this file guards its pixelToCoord()
    // result with a range comparison before use -- incidentally NaN-safe
    // too (any comparison against NaN is false), since a degenerate axis
    // range (lower == upper, e.g. right after connecting/reconnecting
    // before a scan has plotted anything) makes pixelToCoord() return NaN.
    // This one had no such guard at all. Not the confirmed crash site (see
    // onCreateMarker()'s and measurements_onefq.cpp's own comments for
    // those) -- Measurements::on_newCursorSmithPos() happens to only ever
    // use x/y in the same kind of fail-safe comparison, falling back to a
    // real data index rather than ever using them as a coordinate directly
    // -- but that's an incidental property of its current implementation,
    // not a guarantee, and every sibling handler is explicit about this
    // instead of relying on it. Hardening to match, both for consistency
    // and so this doesn't silently become exploitable if that function
    // ever changes.
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
    if(!list.isEmpty())
    {
        QTableWidgetItem * item = list.at(0);
        emit newCursorSmithPos( x, y, item->row());
    }
}

void MainWindow::mouseWheel_user(QWheelEvent * e)
{
    static int state = 1;
    // See mouseWheel_swr()'s comment -- deferred so this reads the range
    // after QCustomPlot has actually applied this tick's zoom.
    QTimer::singleShot(0, this, [this]() {
        double from  = m_userWidget->xAxis->range().lower;
        double to = m_userWidget->xAxis->range().upper;
        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((from+to)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_userWidget->xAxis->range());
    });

    if (e->modifiers() == Qt::ControlModifier)
    {
        int val;
        if(e->angleDelta().y() < 0)
        {
            if(g_extendedChartZoom || state <= 19)
            {
                ++state;
                val = state*80;
                m_userWidget->yAxis->setRangeLower(-val);
                m_userWidget->yAxis->setRangeUpper(val);
                m_userWidget->replot();
                if(m_markers)
                {
                    QTimer::singleShot(5, m_markers, SLOT(redraw()));
                }
                if(m_measurements)
                {
                    QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
                }
            }
        }else
        {
            if(state > 1)
            {
                --state;
                val = state*80;
                m_userWidget->yAxis->setRangeLower(-val);
                m_userWidget->yAxis->setRangeUpper(val);
                m_userWidget->replot();
                if(m_markers)
                {
                    QTimer::singleShot(5, m_markers, SLOT(redraw()));
                }
                if(m_measurements)
                {
                    QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
                }
            }
        }
    }
    emit rescale();
}

void MainWindow::mouseMove_user(QMouseEvent *e)
{
    m_isMouseClick = false;
    double x = m_userWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_userWidget->yAxis->pixelToCoord(e->pos().y());
    // See mouseMove_smith()'s comment -- same degenerate-axis-range NaN
    // guard, explicit here too rather than relying on the range comparisons
    // below happening to be NaN-safe on their own.
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    int from;
    int to;
    if(!m_isRange)
    {
        from = getFqFrom();
        to = getFqTo();
    }else
    {
        from = getFqFrom() - getFqTo();
        to = getFqFrom() + getFqTo();
    }
    if((x >= from) && (x <= to))
    {
        if(y >= m_userWidget->yAxis->range().lower && y <= m_userWidget->yAxis->range().upper)
        {
            QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
            if(!list.isEmpty())
            {
                QTableWidgetItem * item = list.at(0);
                emit newCursorFq(x, item->row(), QCursor::pos().x(), QCursor::pos().y());
            }
        }
    }
    if (e->buttons() & Qt::LeftButton)
    {
        if(!m_isRange)
        {
            setFqFrom(getFqFrom());
            setFqTo(getFqTo());
        }else
        {
            setFqFrom((getFqTo() + getFqFrom())/2);
            setFqTo((getFqTo() - getFqFrom())/2);
        }
        m_swrWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_userWidget->xAxis->range());
    }
}


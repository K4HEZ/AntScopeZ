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

extern QString appendSpaces(const QString& number);
extern bool g_developerMode; // see main.cpp
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

void MainWindow::on_pressF1 ()
{
    ui->tabWidget->setCurrentIndex(0);
}

void MainWindow::on_pressF2 ()
{
    ui->tabWidget->setCurrentIndex(1);
}

void MainWindow::on_pressF3 ()
{
    ui->tabWidget->setCurrentIndex(2);
}

void MainWindow::on_pressF4 ()
{
    ui->tabWidget->setCurrentIndex(3);
}

void MainWindow::on_pressF5 ()
{
    ui->tabWidget->setCurrentIndex(4);
}

void MainWindow::on_pressF6 ()
{
    ui->tabWidget->setCurrentIndex(5);
}

void MainWindow::on_pressF7 ()
{
    ui->tabWidget->setCurrentIndex(6);
}

void MainWindow::on_pressEsc ()
{
    ui->singleStart->setChecked(false);
    ui->continuousStartBtn->setChecked(false);
    m_bInterrupted = true;

    m_measurements->hideOneFqWidget();
    m_measurements->interrupt();
    emit stopMeasure();
}

void MainWindow::on_pressF9 ()
{
    if (!ui->singleStart->isEnabled())
        return;
    emit on_singleStart_clicked();
}

void MainWindow::on_pressF10 ()
{
    if (!ui->continuousStartBtn->isEnabled())
        return;
    emit on_continuousStartBtn_clicked(!ui->continuousStartBtn->isChecked());
    ui->continuousStartBtn->setChecked(!ui->continuousStartBtn->isChecked());
}

void MainWindow::on_pressDelete ()
{
    emit on_measurmentsDeleteBtn_clicked();
}

void MainWindow::on_pressPlus ()
{
    QString str = ui->tabWidget->currentWidget()->objectName();
    if( str == "tab_swr")
    {
        double from = m_swrWidget->xAxis->getRangeLower();
        double to = m_swrWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band -= band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_swrWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_swrWidget->xAxis->setRangeUpper(to);

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
        m_s21Widget->xAxis->setRange(m_swrWidget->xAxis->range());
        if (g_developerMode) {
            m_userWidget->xAxis->setRange(m_swrWidget->xAxis->range());
        }
        m_swrWidget->replot();
         replotY_swr();
    }else if(str == "tab_phase")
    {
        double from = m_phaseWidget->xAxis->getRangeLower();
        double to = m_phaseWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band -= band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_phaseWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_phaseWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_phaseWidget->xAxis->range());
        if (g_developerMode) {
            m_userWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        }
        m_phaseWidget->replot();
    }else if(str == "tab_rs")
    {
        double from = m_rsWidget->xAxis->getRangeLower();
        double to = m_rsWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band -= band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_rsWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_rsWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_rsWidget->xAxis->range());
        if (g_developerMode) {
            m_userWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        }
        m_rsWidget->replot();
    }else if(str == "tab_rp")
    {
        double from = m_rpWidget->xAxis->getRangeLower();
        double to = m_rpWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band -= band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_rpWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_rpWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_rpWidget->xAxis->range());
        if (g_developerMode) {
            m_userWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        }
        m_rpWidget->replot();
    }else if(str == "tab_rl")
    {
        double from = m_rlWidget->xAxis->getRangeLower();
        double to = m_rlWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band -= band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_rlWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_rlWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_userWidget->xAxis->range());
        if (g_developerMode) {
            m_userWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        }
        m_rlWidget->replot();
    }else if(str == "tab_tdr")
    {
        double from = m_tdrWidget->xAxis->getRangeLower();
        double to = m_tdrWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band -= band/10;
        double center = (from + to)/2;

        if((center - band) > 0)
        {
            m_tdrWidget->xAxis->setRangeLower(center - band);
        }
        if ((center + band) < 10000)
        {
            m_tdrWidget->xAxis->setRangeUpper(center + band);
        }
        m_tdrWidget->replot();
    }else if(str == "tab_s21")
    {
        double from = m_s21Widget->xAxis->getRangeLower();
        double to = m_s21Widget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band -= band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_s21Widget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_s21Widget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_s21Widget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_s21Widget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_s21Widget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_s21Widget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_s21Widget->xAxis->range());
        if (g_developerMode) {
            m_userWidget->xAxis->setRange(m_s21Widget->xAxis->range());
        }
        m_s21Widget->replot();
    }else if(str == "tab_user")
    {
        double from = m_userWidget->xAxis->getRangeLower();
        double to = m_userWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band -= band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_rlWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_userWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_userWidget->xAxis->range());
        m_userWidget->replot();
    }
}

void MainWindow::on_pressCtrlPlus ()
{    
    QString str = ui->tabWidget->currentWidget()->objectName();
    if( str == "tab_swr")
    {
        QPointF pos = rect().center();
        QPoint delta = QPoint(0,1);
        Qt::ScrollPhase phase = Qt::NoScrollPhase;        QWheelEvent event(pos, pos, delta, delta, Qt::NoButton, Qt::ControlModifier, phase, false);
        mouseWheel_swr(&event);
//        int limit = g_developerMode ? 1 : SWR_ZOOM_LIMIT;
//        if(m_swrZoomState > limit)
//        {
//            m_swrZoomState = m_swrZoomState - m_swrZoomState/10;
//            m_swrWidget->yAxis->setRangeUpper(m_swrZoomState+0.02);
//            m_swrWidget->yAxis->setRangeLower(MIN_SWR);
//            m_swrWidget->replot();
//            if(m_markers)
//            {
//                QTimer::singleShot(5, m_markers, SLOT(redraw()));
//            }
//            if(m_measurements)
//            {
//                QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
//            }
//        }

    }else if(str == "tab_phase")
    {
    }else if(str == "tab_rs")
    {
        if(m_rsZoomState > 1)
        {
            --m_rsZoomState;
            int val = m_rsZoomState*80;
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
    }else if(str == "tab_rp")
    {
        if(m_rpZoomState > 1)
        {
            --m_rpZoomState;
            int val = m_rpZoomState*80;
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
    }else if(str == "tab_rl")
    {
        int limit = g_developerMode ? 1 : SWR_ZOOM_LIMIT;
        if(m_rlZoomState > limit)
        {
            --m_rlZoomState;
            m_rlWidget->yAxis->setRangeUpper(m_rlZoomState*5);
            m_rlWidget->yAxis->setRangeLower(0);
            if(m_markers)
            {
                QTimer::singleShot(5, m_markers, SLOT(redraw()));
            }
            if(m_measurements)
            {
                QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
            }
        }
    }else if(str == "tab_tdr")
    {
        m_tdrWidget->replot();
    }else if(str == "tab_s21")
    {
        int limit = g_developerMode ? 1 : SWR_ZOOM_LIMIT;
        if(m_s21ZoomState > limit)
        {
            --m_s21ZoomState;
            m_s21Widget->yAxis->setRangeUpper(m_s21ZoomState*5);
            m_s21Widget->yAxis->setRangeLower(0);
            if(m_markers)
            {
                QTimer::singleShot(5, m_markers, SLOT(redraw()));
            }
            if(m_measurements)
            {
                QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
            }
        }
    }else if(str == "tab_user")
    {
        if(m_userZoomState > 1)
        {
            --m_userZoomState;
            int val = m_userZoomState*80;
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

void MainWindow::on_pressMinus ()
{
    QString str = ui->tabWidget->currentWidget()->objectName();
    if( str == "tab_swr")
    {
        double from = m_swrWidget->xAxis->getRangeLower();
        double to = m_swrWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band += band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_swrWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_swrWidget->xAxis->setRangeUpper(to);

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
        m_swrWidget->replot();
         replotY_swr();
    }else if(str == "tab_phase")
    {
        double from = m_phaseWidget->xAxis->getRangeLower();
        double to = m_phaseWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band += band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_phaseWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_phaseWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_phaseWidget->replot();
    }else if(str == "tab_rs")
    {
        double from = m_rsWidget->xAxis->getRangeLower();
        double to = m_rsWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band += band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_rsWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_rsWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rsWidget->replot();
    }else if(str == "tab_rp")
    {
        double from = m_rpWidget->xAxis->getRangeLower();
        double to = m_rpWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band += band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_rpWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_rpWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rpWidget->replot();
    }else if(str == "tab_rl")
    {
        double from = m_rlWidget->xAxis->getRangeLower();
        double to = m_rlWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band += band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_rlWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_rlWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rlWidget->replot();
    }else if(str == "tab_tdr")
    {
        double from = m_tdrWidget->xAxis->getRangeLower();
        double to = m_tdrWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band += band/10;
        double center = (from + to)/2;

        if((center - band) > 0)
        {
            m_tdrWidget->xAxis->setRangeLower(center - band);
        }
        if ((center + band) < 10000)
        {
            m_tdrWidget->xAxis->setRangeUpper(center + band);
        }
        m_tdrWidget->replot();
    }else if(str == "tab_s21")
    {
        double from = m_s21Widget->xAxis->getRangeLower();
        double to = m_s21Widget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band += band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_s21Widget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_s21Widget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_s21Widget->replot();
    }else if(str == "tab_user")
    {
        double from = m_userWidget->xAxis->getRangeLower();
        double to = m_userWidget->xAxis->getRangeUpper();
        double band = (to - from)/2;
        band += band/10;
        double center = (from + to)/2;

        if((center - band) > ABSOLUTE_MIN_FQ)
        {
            from = center - band;
        }else
        {
            from = ABSOLUTE_MIN_FQ;
        }
        m_userWidget->xAxis->setRangeLower(from);

        if ((center + band) < ABSOLUTE_MAX_FQ)
        {
            to = center + band;
        }else
        {
            to = ABSOLUTE_MAX_FQ;
        }
        m_userWidget->xAxis->setRangeUpper(to);

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_userWidget->replot();

    }
}

void MainWindow::on_pressCtrlMinus ()
{
    QString str = ui->tabWidget->currentWidget()->objectName();
    if( str == "tab_swr")
    {
        QPointF pos = rect().center();
        QPoint delta = QPoint(0,-1);
        Qt::ScrollPhase phase = Qt::NoScrollPhase;
        QWheelEvent event(pos, pos, delta, delta, Qt::NoButton, Qt::ControlModifier, phase, false);

        mouseWheel_swr(&event);
        mouseWheel_swr(&event);
    }else if(str == "tab_phase")
    {
//        m_phaseWidget->replot();
    }else if(str == "tab_rs")
    {
        if(m_rsZoomState < 19)
        {
            ++m_rsZoomState;
            int val = m_rsZoomState*80;
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
    }else if(str == "tab_rp")
    {
        if(m_rpZoomState < 19)
        {
            ++m_rpZoomState;
            int val = m_rpZoomState*80;
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
    }else if(str == "tab_rl")
    {
        if(m_rlZoomState <= 9)
        {
            ++m_rlZoomState;
            m_rlWidget->yAxis->setRangeUpper(m_rlZoomState*5);
            m_rlWidget->yAxis->setRangeLower(0);
            m_rlWidget->replot();
            if(m_markers)
            {
                QTimer::singleShot(5, m_markers, SLOT(redraw()));
            }
            if(m_measurements)
            {
                QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
            }
        }
    }else if(str == "tab_tdr")
    {
        m_tdrWidget->replot();
    }else if(str == "tab_s21")
    {
        if(m_s21ZoomState <= 9)
        {
            ++m_s21ZoomState;
            m_s21Widget->yAxis->setRangeUpper(m_s21ZoomState*5);
            m_s21Widget->yAxis->setRangeLower(0);
            m_s21Widget->replot();
            if(m_markers)
            {
                QTimer::singleShot(5, m_markers, SLOT(redraw()));
            }
            if(m_measurements)
            {
                QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
            }
        }
    }else if(str == "tab_user")
    {
        if(m_userZoomState < 19)
        {
            ++m_userZoomState;
            int val = m_userZoomState*80;
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

void MainWindow::on_pressCtrlZero()
{
    QString str = ui->tabWidget->currentWidget()->objectName();
    if( str == "tab_swr")
    {
        if (g_developerMode) {

            //m_swrZoomState = 10;
            //m_swrWidget->yAxis->setRangeUpper(m_swrZoomState+0.02);
            m_swrWidget->yAxis->setRangeUpper(10.02);
            m_swrWidget->yAxis->setRangeLower(MIN_SWR);
            m_swrWidget->replot();
             replotY_swr();
            if(m_markers)
            {
                QTimer::singleShot(5, m_markers, SLOT(redraw()));
            }
            if(m_measurements)
            {
                QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
            }
        }
    }else if(str == "tab_phase")
    {
//        m_phaseWidget->replot();
    }else if(str == "tab_rs")
    {
        int val = m_rsZoomState*80;
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
    }else if(str == "tab_rp")
    {
        int val = m_rpZoomState*80;
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
    }else if(str == "tab_rl")
    {
        m_rlWidget->yAxis->setRangeUpper(m_rlZoomState*5);
        m_rlWidget->yAxis->setRangeLower(0);
        m_rlWidget->replot();
        if(m_markers)
        {
            QTimer::singleShot(5, m_markers, SLOT(redraw()));
        }
        if(m_measurements)
        {
            QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
        }
    }else if(str == "tab_rl")
    {
        m_rlWidget->yAxis->setRangeUpper(m_rlZoomState*5);
        m_rlWidget->yAxis->setRangeLower(0);
        m_rlWidget->replot();
        if(m_markers)
        {
            QTimer::singleShot(5, m_markers, SLOT(redraw()));
        }
        if(m_measurements)
        {
            QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
        }
    }else if(str == "tab_s21")
    {
        m_s21Widget->yAxis->setRangeUpper(m_s21ZoomState*5);
        m_s21Widget->yAxis->setRangeLower(0);
        m_s21Widget->replot();
        if(m_markers)
        {
            QTimer::singleShot(5, m_markers, SLOT(redraw()));
        }
        if(m_measurements)
        {
            QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
        }
    }else if(str == "tab_tdr")
    {
        m_tdrWidget->replot();
    }else if(str == "tab_user")
    {
        int val = m_userZoomState*80;
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

void MainWindow::on_pressLeft()
{
    QString str = ui->tabWidget->currentWidget()->objectName();
    if( str == "tab_swr")
    {
        double from = m_swrWidget->xAxis->getRangeLower();
        double to = m_swrWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((from - diff) >= ABSOLUTE_MIN_FQ)
        {
            from -= diff;
            to -= diff;
            m_swrWidget->xAxis->setRangeLower(from);
            m_swrWidget->xAxis->setRangeUpper(to);
        }else
        {
            from = ABSOLUTE_MIN_FQ;
            m_swrWidget->xAxis->setRangeLower(from);
        }

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
        m_s21Widget->xAxis->setRange(m_swrWidget->xAxis->range());
        m_swrWidget->replot();
         replotY_swr();
    }else if(str == "tab_phase")
    {
        double from = m_phaseWidget->xAxis->getRangeLower();
        double to = m_phaseWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((from - diff) >= ABSOLUTE_MIN_FQ)
        {
            from -= diff;
            to -= diff;
            m_phaseWidget->xAxis->setRangeLower(from);
            m_phaseWidget->xAxis->setRangeUpper(to);
        }else
        {
            from = ABSOLUTE_MIN_FQ;
            m_phaseWidget->xAxis->setRangeLower(from);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_phaseWidget->replot();
    }else if(str == "tab_rs")
    {
        double from = m_rsWidget->xAxis->getRangeLower();
        double to = m_rsWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((from - diff) >= ABSOLUTE_MIN_FQ)
        {
            from -= diff;
            to -= diff;
            m_rsWidget->xAxis->setRangeLower(from);
            m_rsWidget->xAxis->setRangeUpper(to);
        }else
        {
            from = ABSOLUTE_MIN_FQ;
            m_rsWidget->xAxis->setRangeLower(from);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rsWidget->replot();
    }else if(str == "tab_rp")
    {
        double from = m_rpWidget->xAxis->getRangeLower();
        double to = m_rpWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((from - diff) >= ABSOLUTE_MIN_FQ)
        {
            from -= diff;
            to -= diff;
            m_rpWidget->xAxis->setRangeLower(from);
            m_rpWidget->xAxis->setRangeUpper(to);
        }else
        {
            from = ABSOLUTE_MIN_FQ;
            m_rpWidget->xAxis->setRangeLower(from);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rpWidget->replot();
    }else if(str == "tab_rl")
    {
        double from = m_rlWidget->xAxis->getRangeLower();
        double to = m_rlWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((from - diff) >= ABSOLUTE_MIN_FQ)
        {
            from -= diff;
            to -= diff;
            m_rlWidget->xAxis->setRangeLower(from);
            m_rlWidget->xAxis->setRangeUpper(to);
        }else
        {
            from = ABSOLUTE_MIN_FQ;
            m_rlWidget->xAxis->setRangeLower(from);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rlWidget->replot();
    }else if(str == "tab_s21")
    {
        double from = m_s21Widget->xAxis->getRangeLower();
        double to = m_s21Widget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((from - diff) >= ABSOLUTE_MIN_FQ)
        {
            from -= diff;
            to -= diff;
            m_s21Widget->xAxis->setRangeLower(from);
            m_s21Widget->xAxis->setRangeUpper(to);
        }else
        {
            from = ABSOLUTE_MIN_FQ;
            m_s21Widget->xAxis->setRangeLower(from);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_s21Widget->replot();
    }else if(str == "tab_tdr")
    {
        double from = m_tdrWidget->xAxis->getRangeLower();
        double to = m_tdrWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((from - diff) >= 0)
        {
            m_tdrWidget->xAxis->setRangeLower(from - diff);
            m_tdrWidget->xAxis->setRangeUpper(to - diff);
        }else
        {
            m_tdrWidget->xAxis->setRangeLower(0);
        }
        m_tdrWidget->replot();
    }else if(str == "tab_user")
    {
        double from = m_userWidget->xAxis->getRangeLower();
        double to = m_userWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((from - diff) >= ABSOLUTE_MIN_FQ)
        {
            from -= diff;
            to -= diff;
            m_userWidget->xAxis->setRangeLower(from);
            m_userWidget->xAxis->setRangeUpper(to);
        }else
        {
            from = ABSOLUTE_MIN_FQ;
            m_userWidget->xAxis->setRangeLower(from);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_userWidget->xAxis->range());
        m_userWidget->replot();
    }
}

void MainWindow::on_pressRight()
{
    QString str = ui->tabWidget->currentWidget()->objectName();
    if( str == "tab_swr")
    {
        double from = m_swrWidget->xAxis->getRangeLower();
        double to = m_swrWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((to + diff) <= ABSOLUTE_MAX_FQ)
        {
            from += diff;
            to += diff;
            m_swrWidget->xAxis->setRangeLower(from);
            m_swrWidget->xAxis->setRangeUpper(to);
        }else
        {
            to = ABSOLUTE_MAX_FQ;
            m_swrWidget->xAxis->setRangeUpper(to);
        }

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
        m_s21Widget->xAxis->setRange(m_swrWidget->xAxis->range());
        m_swrWidget->replot();
         replotY_swr();
    }else if(str == "tab_phase")
    {
        double from = m_phaseWidget->xAxis->getRangeLower();
        double to = m_phaseWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((to + diff) <= ABSOLUTE_MAX_FQ)
        {
            from += diff;
            to += diff;
            m_phaseWidget->xAxis->setRangeLower(from);
            m_phaseWidget->xAxis->setRangeUpper(to);
        }else
        {
            to = ABSOLUTE_MAX_FQ;
            m_phaseWidget->xAxis->setRangeUpper(to);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_phaseWidget->xAxis->range());
        m_phaseWidget->replot();
    }else if(str == "tab_rs")
    {
        double from = m_rsWidget->xAxis->getRangeLower();
        double to = m_rsWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((to + diff) <= ABSOLUTE_MAX_FQ)
        {
            from += diff;
            to += diff;
            m_rsWidget->xAxis->setRangeLower(from);
            m_rsWidget->xAxis->setRangeUpper(to);
        }else
        {
            to = ABSOLUTE_MAX_FQ;
            m_rsWidget->xAxis->setRangeUpper(to);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_rsWidget->xAxis->range());
        m_rsWidget->replot();
    }else if(str == "tab_rp")
    {
        double from = m_rpWidget->xAxis->getRangeLower();
        double to = m_rpWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((to + diff) <= ABSOLUTE_MAX_FQ)
        {
            from += diff;
            to += diff;
            m_rpWidget->xAxis->setRangeLower(from);
            m_rpWidget->xAxis->setRangeUpper(to);
        }else
        {
            to = ABSOLUTE_MAX_FQ;
            m_rpWidget->xAxis->setRangeUpper(to);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_rpWidget->xAxis->range());
        m_rpWidget->replot();
    }else if(str == "tab_rl")
    {
        double from = m_rlWidget->xAxis->getRangeLower();
        double to = m_rlWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((to + diff) <= ABSOLUTE_MAX_FQ)
        {
            from += diff;
            to += diff;
            m_rlWidget->xAxis->setRangeLower(from);
            m_rlWidget->xAxis->setRangeUpper(to);
        }else
        {
            to = ABSOLUTE_MAX_FQ;
            m_rlWidget->xAxis->setRangeUpper(to);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_s21Widget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rlWidget->replot();
    }else if(str == "tab_s21")
    {
        double from = m_s21Widget->xAxis->getRangeLower();
        double to = m_s21Widget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((to + diff) <= ABSOLUTE_MAX_FQ)
        {
            from += diff;
            to += diff;
            m_s21Widget->xAxis->setRangeLower(from);
            m_s21Widget->xAxis->setRangeUpper(to);
        }else
        {
            to = ABSOLUTE_MAX_FQ;
            m_s21Widget->xAxis->setRangeUpper(to);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_rlWidget->xAxis->range());
        m_s21Widget->replot();
    }else if(str == "tab_tdr")
    {
        double from = m_tdrWidget->xAxis->getRangeLower();
        double to = m_tdrWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((to + diff) <= 10000)
        {
            m_tdrWidget->xAxis->setRangeLower(from + diff);
            m_tdrWidget->xAxis->setRangeUpper(to + diff);
        }else
        {
            m_tdrWidget->xAxis->setRangeUpper(10000);
        }
        m_tdrWidget->replot();
    }else if(str == "tab_user")
    {
        double from = m_userWidget->xAxis->getRangeLower();
        double to = m_userWidget->xAxis->getRangeUpper();
        double diff = (to - from)/10;

        if((to + diff) <= ABSOLUTE_MAX_FQ)
        {
            from += diff;
            to += diff;
            m_userWidget->xAxis->setRangeLower(from);
            m_userWidget->xAxis->setRangeUpper(to);
        }else
        {
            to = ABSOLUTE_MAX_FQ;
            m_userWidget->xAxis->setRangeUpper(to);
        }

        if(!m_isRange)
        {
            setFqFrom(from);
            setFqTo(to);
        }else
        {
            setFqFrom((to+from)/2);
            setFqTo((to-from)/2);
        }

        m_swrWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_phaseWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rpWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rsWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_rlWidget->xAxis->setRange(m_userWidget->xAxis->range());
        m_userWidget->replot();
    }
}

void MainWindow::on_pressCtrlC ()
{
    QCustomPlot* plot = nullptr;
    QString str = ui->tabWidget->currentWidget()->objectName();
    if( str == "tab_swr")
    {
        plot = m_swrWidget;
    }else if(str == "tab_phase")
    {
        plot = m_phaseWidget;
    }else if(str == "tab_rs")
    {
        plot = m_rsWidget;
    }else if(str == "tab_rp")
    {
        plot = m_rpWidget;
    }else if(str == "tab_rl")
    {
        plot = m_rlWidget;
    }else if(str == "tab_s21")
    {
        plot = m_s21Widget;
    }else if(str == "tab_tdr")
    {
        plot = m_tdrWidget;
    }else if(str == "tab_smith")
    {
        resizeWnd();
        plot = m_smithWidget;
    }else if(str == "tab_user")
    {
        plot = m_userWidget;
    }

    QPixmap pixmap = (plot != nullptr) ? plot->grab() : ui->tabWidget->currentWidget()->grab();

    QPainter painter(&pixmap);
    QPixmap logo(":/new/prefix1/logo_watermark.png");
    painter.drawPixmap(10, 10, logo);

    QFont font = painter.font();
    font.setFamily("Courier New");
    painter.setFont(font);

    QString text = "RigExpert AntScope: antenna and cable analysis software";
    painter.setPen(qRgb(0x55, 0x7b, 0xce));
    QRect bound = painter.boundingRect(pixmap.rect(), Qt::AlignBottom|Qt::AlignRight, text);
    painter.drawText(bound.left()-2, bound.bottom()-1, text);

    QClipboard *pClipboard = QApplication::clipboard();
    pClipboard->setPixmap(pixmap);
}


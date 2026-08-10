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

void MainWindow::mouseWheel_swr(QWheelEvent * e)
{   //v4_(04/12)
    double from  = m_swrWidget->xAxis->getRangeLower();
    double to = m_swrWidget->xAxis->getRangeUpper();
    //return;
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

        if (!g_developerMode && (length <= (0.4)) && (zoomin))
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
{ bool replotneed =false;
    //------------overwrite lables Y
     QVector<double> yScale       =m_swrWidget->yAxis->tickVector();
     QVector<QString> yScaleLabels=m_swrWidget->yAxis->tickVectorLabels();
     for (int i=0; i < yScale.count(); ++ i){
        if(yScale[i]<1){
            yScaleLabels[i]=" ";
            replotneed=true;
        }
     }
     if(replotneed){
     m_swrWidget->yAxis->setAutoTickLabels(false);
     m_swrWidget->yAxis->setTickVectorLabels(yScaleLabels);
     m_swrWidget->replot();//Y_labl
     m_swrWidget->yAxis->setAutoTickLabels(true);
     }
    //--------------------

}


void MainWindow::mouseMove_swr(QMouseEvent *e)
{
    m_isMouseClick = false;
    double x = m_swrWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_swrWidget->yAxis->pixelToCoord(e->pos().y());

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

    double from  = m_phaseWidget->xAxis->getRangeLower();
    double to = m_phaseWidget->xAxis->getRangeUpper();
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
}

void MainWindow::mouseMove_phase(QMouseEvent *e)
{
    m_isMouseClick = false;
    double x = m_phaseWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_phaseWidget->yAxis->pixelToCoord(e->pos().y());
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
    double from  = m_rsWidget->xAxis->getRangeLower();
    double to = m_rsWidget->xAxis->getRangeUpper();
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

    if (e->modifiers() == Qt::ControlModifier)
    {
        int val;
        if(e->angleDelta().y() < 0)
        {
            if(g_developerMode || state <= 19)
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
    double from  = m_rpWidget->xAxis->getRangeLower();
    double to = m_rpWidget->xAxis->getRangeUpper();
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

    if (e->modifiers() == Qt::ControlModifier)
    {
        if(e->angleDelta().y() < 0)
        {
            if(g_developerMode || state <= 19)
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
    double from  = m_rlWidget->xAxis->getRangeLower();
    double to = m_rlWidget->xAxis->getRangeUpper();
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

    if (e->modifiers() == Qt::ControlModifier)
    {
        if(m_measurements)
        {
            QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
        }
        if(e->angleDelta().y() < 0)
        {
            if(g_developerMode || m_rlZoomState <= 9)
            {
                ++m_rlZoomState;
                m_rlWidget->yAxis->setRangeUpper(m_rlZoomState*5);
                m_rlWidget->yAxis->setRangeLower(0);
                m_rlWidget->replot();
            }
        }else
        {
            int limit = g_developerMode ? 1 : SWR_ZOOM_LIMIT;
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
    //qDebug() << "MainWindow::mouseWheel_tdr: state" << state << ", lo " << m_tdrWidget->yAxis->getRangeLower() << ", up " << m_tdrWidget->yAxis->getRangeUpper();
    if (e->modifiers() == Qt::ControlModifier)
    {
//        if(m_measurements)
//        {
//            QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
//        }
//        double up = m_tdrWidget->yAxis->getRangeUpper();
//        double lo = m_tdrWidget->yAxis->getRangeLower();

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
}

void MainWindow::mouseMove_tdr(QMouseEvent * e)
{
    double x = m_tdrWidget->xAxis->pixelToCoord(e->pos().x());
    if( (x >= m_tdrWidget->xAxis->range().lower) && (x <= m_tdrWidget->xAxis->range().upper))
    {
        QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
        if(!list.isEmpty())
        {
            QTableWidgetItem * item = list.at(0);
            emit newCursorFq(x, item->row(), e->pos().x(), e->pos().y());
        }
    }
}

void MainWindow::mouseMove_s21(QMouseEvent * e)
{
    // QPointF pos = e->pos();
    double x = m_s21Widget->xAxis->pixelToCoord(e->pos().x());
    if( (x >= m_s21Widget->xAxis->range().lower) && (x <= m_s21Widget->xAxis->range().upper))
    {
        QList <QTableWidgetItem *> list = ui->tableWidget_measurments->selectedItems();
        if(!list.isEmpty())
        {
            QTableWidgetItem * item = list.at(0);
            emit newCursorFq(x, item->row(), e->pos().x(), e->pos().y());
        }
    }
}

void MainWindow::mouseMove_smith(QMouseEvent * e)
{
    double x = m_smithWidget->xAxis->pixelToCoord(e->pos().x());
    double y = m_smithWidget->yAxis->pixelToCoord(e->pos().y());
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
    double from  = m_userWidget->xAxis->getRangeLower();
    double to = m_userWidget->xAxis->getRangeUpper();
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

    if (e->modifiers() == Qt::ControlModifier)
    {
        int val;
        if(e->angleDelta().y() < 0)
        {
            if(g_developerMode || state <= 19)
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


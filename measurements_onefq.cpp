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
    m_oneFqMode = true;
    if (m_graphHint != nullptr)
        m_graphHint->focusHide();
//    delete m_oneFqWidget;
//    m_oneFqWidget = new OneFqWidget(_dots, _parent);
    if (m_oneFqWidget == nullptr) {
        m_oneFqWidget = new OneFqWidget(_dots, _parent);
        connect(m_oneFqWidget, &OneFqWidget::canceled, this, &Measurements::hideOneFqWidget);
        connect(m_oneFqWidget, &OneFqWidget::udpReceived, this, [=](QString cmd, qreal data) {
            extern MainWindow* g_mainWindow;
            //g_mainWindow->on_pressEsc();
            g_mainWindow->analyzer()->on_stopMeasure();
            g_mainWindow->analyzer()->on_measureOneFq(g_mainWindow, quint64(data*1000), _dots);
            m_oneFqWidget->needBroadcast(data*1000);
        });
        m_oneFqWidget->show();
        m_oneFqWidget->saveHintFlags(QPair<bool,bool>(m_graphHintEnabled, m_graphBriefHintEnabled));
        m_graphHintEnabled = false;
        m_graphBriefHintEnabled = false;
    }
//    m_oneFqWidget->saveHintFlags(QPair<bool,bool>(m_graphHintEnabled, m_graphBriefHintEnabled));
//    m_graphHintEnabled = false;
//    m_graphBriefHintEnabled = false;

//    connect(m_oneFqWidget, &OneFqWidget::canceled, this, &Measurements::hideOneFqWidget);
//    connect(m_oneFqWidget, &OneFqWidget::udpReceived, this, [=](QString cmd, qreal data) {
//        extern MainWindow* g_mainWindow;
//        g_mainWindow->on_pressEsc();
//        g_mainWindow->on_startOneFq(quint64(data*1000), _dots);
//    });
//    m_oneFqWidget->show();
}

void Measurements::updateOneFqWidget(GraphData& _data)
{
    if (m_oneFqWidget == nullptr)
        return;
    m_oneFqWidget->addData(_data);
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

    ptX = m_smithWidget->xAxis->pixelToCoord(ptX);
    ptY = m_smithWidget->yAxis->pixelToCoord(ptY);

    m_smithTracer->topLeft->setCoords(ptX-0.1, ptY+0.1);
    m_smithTracer->bottomRight->setCoords(ptX+0.1, ptY-0.1);
    m_smithWidget->replot();

}

void Measurements::hideOneFqWidget(bool)
{
    m_oneFqMode = false;
    OneFqWidget* tmp = m_oneFqWidget;
    if (tmp != nullptr) {
        m_isContinuing = false;
        m_oneFqWidget = nullptr;
        QPair<bool,bool> hints = tmp->resoreHintFlags();
        m_graphHintEnabled = hints.first;
        m_graphBriefHintEnabled = hints.second;
        disconnect(tmp);
        tmp->hide();
        showHideHints();
        emit oneFqCanceled();
        delete tmp;
    }
}

void Measurements::on_newMeasurementOneFq(QWidget* parent, qint64 fq, qint32 dots)
{
    m_interrupted = false;
    Q_UNUSED (fq)
    showOneFqWidget(parent, dots);
}



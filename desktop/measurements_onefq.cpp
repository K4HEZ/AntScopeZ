#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"

extern QMap<QString, QString> g_mapTabPlotNames;
extern int g_maxMeasurements; // defined in measurements.cpp
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original measurements.cpp (still in
// measurements.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of Measurements.

// One-Fq (Tuning mode) readings never become measurements: on_newData()
// hands each one straight to publishOneFqData().
void Measurements::publishOneFqData(GraphData& _data)
{
    emit oneFqData(_data);

    if(m_smithTracer == NULL)
    {
        m_smithTracer = new QCPItemEllipse(m_smithWidget);
        m_smithTracer->setAntialiased(true);
        QPen pen;
        pen.setColor(Qt::magenta);
        pen.setWidth(4);
        m_smithTracer->setPen(pen);
    }

    // _data.ptX/ptY are already Smith-chart plot coordinates (see
    // Measurements::NormRXtoSmithPoint()) -- no pixelToCoord() conversion;
    // that treated a plot coordinate as a pixel and crashed on a zero-height
    // axis rect (confirmed via core dump 2026-08-20).
    m_smithTracer->topLeft->setCoords(_data.ptX-0.1, _data.ptY+0.1);
    m_smithTracer->bottomRight->setCoords(_data.ptX+0.1, _data.ptY-0.1);
    m_smithWidget->replot();
}

void Measurements::endOneFqMode()
{
    if (!m_oneFqMode)
        return;
    m_oneFqMode = false;
    m_measurements.stopContinuing();
    emit oneFqCanceled();
}

void Measurements::stopOneFq(bool)
{
    endOneFqMode();
}

void Measurements::on_newMeasurementOneFq(QWidget*, qint64 fq, qint32 dots)
{
    Q_UNUSED (fq)
    Q_UNUSED (dots)
    m_measurements.clearInterrupted();
    m_oneFqMode = true;
}

#include "testresultplot.h"
#include <calibration.h>
#include <qcustomplot.h>
#include <QVector>

// Advisory-only reference bands -- there's no pass/fail, the user judges
// the scatter against these themselves (see the "failed test" discussion:
// SWR/RL-derived thresholds didn't actually agree with these Ohm values,
// so Ohms is the one number this is gated on, informally, by eye).
// Provisional pending real hardware testing.
static const double kExcellentBandOhms = 0.25;
static const double kAcceptableBandOhms = 1.0;

// Spans the full plot width (ptAxisRectRatio) at a fixed Ohm level
// (ptPlotCoords) regardless of the data's actual frequency range.
static void addBand(QCustomPlot* plot, double levelOhms, const QColor& color)
{
    QCPItemRect* rect = new QCPItemRect(plot);
    rect->setPen(QPen(Qt::transparent));
    rect->setBrush(QBrush(color));
    rect->topLeft->setTypeX(QCPItemPosition::ptAxisRectRatio);
    rect->topLeft->setTypeY(QCPItemPosition::ptPlotCoords);
    rect->topLeft->setCoords(0, levelOhms);
    rect->bottomRight->setTypeX(QCPItemPosition::ptAxisRectRatio);
    rect->bottomRight->setTypeY(QCPItemPosition::ptPlotCoords);
    rect->bottomRight->setCoords(1, -levelOhms);
}

QCustomPlot* createTestResultPlot()
{
    QCustomPlot* plot = new QCustomPlot();
    plot->setMinimumHeight(220);
    plot->xAxis->setLabel(QObject::tr("Frequency (MHz)"));
    plot->yAxis->setLabel(QObject::tr("Error (Ω)"));
    plot->legend->setVisible(true);

    // Wider/looser band drawn first, narrower/tighter band drawn on top.
    addBand(plot, kAcceptableBandOhms, QColor(230, 200, 60, 60));
    addBand(plot, kExcellentBandOhms, QColor(80, 200, 80, 90));

    QCPGraph* rGraph = plot->addGraph();
    rGraph->setName(QObject::tr("ΔR (resistance)"));
    rGraph->setPen(QPen(QColor(220, 60, 60)));
    rGraph->setLineStyle(QCPGraph::lsNone);
    rGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));

    QCPGraph* xGraph = plot->addGraph();
    xGraph->setName(QObject::tr("ΔX (reactance)"));
    xGraph->setPen(QPen(QColor(60, 90, 220)));
    xGraph->setLineStyle(QCPGraph::lsNone);
    xGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));

    return plot;
}

void populateTestResultPlot(QCustomPlot* plot, Calibration* calibration, double z0)
{
    int n = calibration->testResultCount();
    QVector<double> freqs(n), dR(n), dX(n);
    for (int i = 0; i < n; ++i) {
        freqs[i] = calibration->testResultFqMHz(i);
        dR[i] = calibration->testResultR(i) - z0;
        dX[i] = calibration->testResultX(i);
    }
    plot->graph(0)->setData(freqs, dR);
    plot->graph(1)->setData(freqs, dX);
    plot->rescaleAxes();
    plot->replot();
}

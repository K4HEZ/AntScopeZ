#include "measurements.h"
#include "markermath.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"
#include "Notification.h"

extern QMap<QString, QString> g_mapTabPlotNames;
extern int g_maxMeasurements; // defined in measurements.cpp
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original measurements.cpp (still in
// measurements.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of Measurements.

void Measurements::setCableVelFactor(double value)
{
    m_cableVelFactor = value;
}
//------------------------------------------------------------------------------
void Measurements::setCableResistance(double value)
{
    m_cableResistance = value;
}
//------------------------------------------------------------------------------
void Measurements::setCableLossConductive(double value)
{
    m_cableLossConductive = value;
}
//------------------------------------------------------------------------------
void Measurements::setCableLossDielectric(double value)
{
    m_cableLossDielectric = value;
}
//------------------------------------------------------------------------------
void Measurements::setCableLossFqMHz(double value)
{
    m_cableLossFqMHz = value;
}
//------------------------------------------------------------------------------
void Measurements::setCableLossUnits(int value)
{
    m_cableLossUnits = value;
}
//------------------------------------------------------------------------------
void Measurements::setCableLossAtAnyFq(bool value)
{
    m_cableLossAtAnyFq = value;
}
//------------------------------------------------------------------------------
void Measurements::setCableLength(double value)
{
    m_cableLength = value;
}
//------------------------------------------------------------------------------
void Measurements::setCableFarEndMeasurement(int value)
{
    m_farEndMeasurement = value;
}

// Cable-corrected series for every measurement that has cable add/subtract
// on (its own mode and cable model), built from its calibrated points when
// it shows OSL.
void Measurements::calcFarEnd(bool _incrementally)
{
    int count = m_measurements.length();
    int i = _incrementally ? (count-1) : 0;
    for( ; i < count; ++i)
    {
        int mode = rowCable(i);
        if (mode == 0)
            continue;
        const QVector<RawData>& data = rowOsl(i) ? m_measurements.at(i).dataRXCalib : m_measurements.at(i).dataRX;
        int dataCount = data.length();
        if (! _incrementally)
        {
            measurement& fe = (mode == 1) ? m_farEndMeasurementsSub[i] : m_farEndMeasurementsAdd[i];
            fe.dataRX.clear();
            fe.swrGraph.clear();
            fe.rlGraph.clear();
            fe.rsrGraph.clear();
            fe.rsxGraph.clear();
            fe.rszGraph.clear();
            fe.rprGraph.clear();
            fe.rpxGraph.clear();
            fe.rpzGraph.clear();
            fe.phaseGraph.clear();
            fe.rhoGraph.clear();
            fe.smithGraph.clear();
        }
        int ii = _incrementally ? (dataCount-1) : 0;
        for( ; ii < dataCount; ++ii)
        {
            calcFarEnd(data.at(ii), i);
        }
    }
}

RawData Measurements::calcFarEnd(const RawData& data, int idx, bool refreshGraphs)
{
    const measurement& m = m_measurements[idx];
    return calcFarEnd(data, idx, refreshGraphs, m.corrections.cableMode, m.corrections.cable);
}

RawData Measurements::calcFarEnd(const RawData& data, int idx, bool refreshGraphs,
                                 int mode, const RfMath::CableParams& cable)
{
    RawData da = data;

    double fq = data.fq;
    double R = data.r;
    double X = data.x;

    QCPGraphData qdata;
    qdata.key = fq*1000;

    // A measurement loaded with a cable correction already in it isn't
    // corrected again.
    bool baked = idx >= 0 && idx < m_measurements.size() && m_measurements[idx].applied.cableMode != 0;
    if (!baked) {
        Complex zin = RfMath::cableTransform(fq, R, X, cable, mode==1);
        R = zin.real();
        X = zin.imag();
    }

    RawData transformed = data;
    transformed.r = R;
    transformed.x = X;
    MarkerMath::ChartPoint cp = MarkerMath::chartPoint(transformed, m_Z0, MarkerMath::Series::FarEnd, 1, 0);

    if (qIsNaN(R) || (R<0.001) ) {R = 0.01;}
    if (qIsNaN(X)) {X = 0;}

    da.r = R;
    da.x = X;
    QList <measurement>& _farEndMeasurements = (mode==1)
            ? m_farEndMeasurementsSub
            : m_farEndMeasurementsAdd;

    _farEndMeasurements[idx].dataRX.append(da);

    if (refreshGraphs) {
        qdata.value = cp.swr;
        _farEndMeasurements[idx].swrGraph.add(qdata);
        qdata.value = cp.rl;
        _farEndMeasurements[idx].rlGraph.add(qdata);
        qdata.value = cp.r;
        _farEndMeasurements[idx].rsrGraph.add(qdata);
        qdata.value = cp.x;
        _farEndMeasurements[idx].rsxGraph.add(qdata);
        qdata.value = cp.z;
        _farEndMeasurements[idx].rszGraph.add(qdata);
        qdata.value = cp.rpar;
        _farEndMeasurements[idx].rprGraph.add(qdata);
        qdata.value = cp.xpar;
        _farEndMeasurements[idx].rpxGraph.add(qdata);
        qdata.value = cp.zpar;
        _farEndMeasurements[idx].rpzGraph.add(qdata);
        qdata.value = cp.phase;
        _farEndMeasurements[idx].phaseGraph.add(qdata);
        qdata.value = cp.rho;
        _farEndMeasurements[idx].rhoGraph.add(qdata);

        double pointX,pointY;
        RfMath::smithPoint(R/m_Z0, X/m_Z0, pointX, pointY);
        int len = _farEndMeasurements[idx].dataRX.length();
        _farEndMeasurements[idx].smithGraph.add(QCPCurveData(len, pointX, pointY));
    }
    return da;
}

bool Measurements::rowOsl(int row) const
{
    const measurement& m = m_measurements.at(row);
    return m.corrections.osl && m.hasCalibrated();
}

int Measurements::rowCable(int row) const
{
    return m_measurements.at(row).corrections.cableMode;
}

bool Measurements::shownOsl(int number)
{
    return rowOsl(m_measurements.length()-1 - number);
}

const QVector<RawData>* Measurements::shownFarEnd(int number)
{
    int row = m_measurements.length()-1 - number;
    switch (rowCable(row)) {
    case 1: return &m_farEndMeasurementsSub[row].dataRX;
    case 2: return &m_farEndMeasurementsAdd[row].dataRX;
    default: return nullptr;
    }
}

Corrections Measurements::scanCorrections() const
{
    Corrections c = currentCorrections();
    c.osl = c.osl && m_calibration != nullptr && m_calibration->getCalibrationPerformed();
    if (c.cableMode == 0)
        c.cable = RfMath::CableParams();
    return c;
}

bool Measurements::canRemoveBuiltIn(int row) const
{
    const measurement& m = m_measurements.at(row);
    return !m.applied.any() || !m.asReceived.isEmpty();
}

bool Measurements::canApplyOsl(int row, QString* why) const
{
    const measurement& m = m_measurements.at(row);
    if (m.applied.osl) {
        if (why) *why = tr("OSL calibration is built into this measurement's saved points.");
        return false;
    }
    if (!canRemoveBuiltIn(row)) {
        if (why) *why = tr("This measurement's original points aren't available.");
        return false;
    }
    if (m.hasCalibrated())
        return true; // taken with a calibration; uses that one
    if (m_calibration == nullptr || !m_calibration->getCalibrationPerformed()) {
        if (why) *why = tr("No OSL calibration exists for the connected analyzer.");
        return false;
    }
    if (!m.analyzerSerial.isEmpty() && m.analyzerSerial != m_calibration->getSerial()) {
        if (why) *why = tr("It was taken with a different analyzer (%1).").arg(m.analyzerSerial);
        return false;
    }
    return true;
}

void Measurements::applyCorrections(int row, const Corrections& c, bool asCopy)
{
    if (row < 0 || row >= m_measurements.size())
        return;

    int target = row;
    if (asCopy) {
        measurement src = m_measurements.at(row);
        on_newMeasurement(src.name + tr(" (corrected)"), src.qint64From, src.qint64To, src.qint64Dots);
        target = m_measurements.size() - 1; // (if the list was full, the oldest row went; src is a copy)
        measurement& dst = m_measurements[target];
        dst.dataRX = src.dataRX;
        dst.dataRXCalib = src.dataRXCalib;
        dst.applied = src.applied;
        dst.asReceived = src.asReceived;
        dst.analyzerSerial = src.analyzerSerial;
        dst.dataSParam = src.dataSParam;
        m_measurements.complete();
    }

    measurement& m = m_measurements[target];
    // Start over from the original points when something built in has to go.
    bool removeBuiltIn = (m.applied.osl && !c.osl) || (m.applied.cableMode != 0 && c.cableMode != m.applied.cableMode);
    if (removeBuiltIn && !m.asReceived.isEmpty()) {
        m.dataRX = m.asReceived;
        m.asReceived.clear();
        m.applied = Corrections();
        m.dataRXCalib.clear();
    }

    Corrections on = c;
    if (m.applied.osl)
        on.osl = false; // already in the points
    if (m.applied.cableMode != 0)
        on.cableMode = 0;
    if (on.osl && !m.hasCalibrated())
        m.recalibrate(m_Z0, m_calibration);
    m.corrections = on;
    m.dirty = true;

    rebuildRowGraphs(target);
    calcFarEnd(false);
    refreshCorrectionsCell(target);
    if (m_tableWidget != nullptr && m_tableWidget->item(target, COL_POINTS) != nullptr)
        m_tableWidget->item(target, COL_POINTS)->setText(pointsCellText(m));
    on_redrawGraphs();
    if (m_currentTab != "tab_tdr")
        redrawTDR(target);
}

void Measurements::rebuildRowGraphs(int row)
{
    measurement& m = m_measurements[row];
    for (QCPGraphDataContainer* g : {&m.swrGraph, &m.rsrGraph, &m.rsxGraph, &m.rszGraph, &m.rprGraph,
                                     &m.rpxGraph, &m.rpzGraph, &m.rlGraph, &m.phaseGraph, &m.rhoGraph,
                                     &m.swrGraphCalib, &m.rsrGraphCalib, &m.rsxGraphCalib, &m.rszGraphCalib,
                                     &m.rprGraphCalib, &m.rpxGraphCalib, &m.rpzGraphCalib, &m.rlGraphCalib,
                                     &m.phaseGraphCalib, &m.rhoGraphCalib})
        g->clear();
    m.smithGraph.clear();
    m.smithGraphView.clear();
    m.smithGraphCalib.clear();
    m.smithGraphViewCalib.clear();

    bool haveCalib = m.dataRXCalib.size() == m.dataRX.size();
    double prevSwr = MAX_SWR, prevRl = 0;
    for (int k = 0; k < m.dataRX.size(); ++k) {
        const RawData& p = m.dataRX.at(k);
        QCPGraphData d;
        d.key = p.fq*1000;
        MarkerMath::ChartPoint cp = MarkerMath::chartPoint(p, m_Z0, MarkerMath::Series::Raw, prevSwr, prevRl);
        prevSwr = cp.swr;
        prevRl = cp.rl;
        d.value = cp.swr;   m.swrGraph.add(d);
        d.value = cp.r;     m.rsrGraph.add(d);
        d.value = cp.x;     m.rsxGraph.add(d);
        d.value = cp.z;     m.rszGraph.add(d);
        d.value = cp.rpar;  m.rprGraph.add(d);
        d.value = cp.xpar;  m.rpxGraph.add(d);
        d.value = cp.zpar;  m.rpzGraph.add(d);
        d.value = cp.rl;    m.rlGraph.add(d);
        d.value = cp.phase; m.phaseGraph.add(d);
        d.value = cp.rho;   m.rhoGraph.add(d);

        double R = p.r, X = p.x;
        if (qIsNaN(R) || (R<0.001)) R = 0.01;
        if (qIsNaN(X)) X = 0;
        double sx, sy;
        RfMath::smithPoint(R/m_Z0, X/m_Z0, sx, sy);
        m.smithGraph.add(QCPCurveData(k, sx, sy));
        m.smithGraphView.add(QCPCurveData(qMax(0, k*2 - 1), sx, sy));

        if (!haveCalib)
            continue;
        const RawData& c = m.dataRXCalib.at(k);
        MarkerMath::ChartPoint cc = MarkerMath::chartPoint(c, m_Z0, MarkerMath::Series::Calibrated, cp.rawSwr, cp.rawRl);
        d.value = cc.swr;   m.swrGraphCalib.add(d);
        d.value = cc.r;     m.rsrGraphCalib.add(d);
        d.value = cc.x;     m.rsxGraphCalib.add(d);
        d.value = cc.z;     m.rszGraphCalib.add(d);
        d.value = cc.rpar;  m.rprGraphCalib.add(d);
        d.value = cc.xpar;  m.rpxGraphCalib.add(d);
        d.value = cc.zpar;  m.rpzGraphCalib.add(d);
        d.value = cc.rl;    m.rlGraphCalib.add(d);
        d.value = cc.phase; m.phaseGraphCalib.add(d);
        d.value = cc.rho;   m.rhoGraphCalib.add(d);
        double cR = c.r, cX = c.x;
        if (qIsNaN(cR) || (cR<0.001)) cR = 0.01;
        if (qIsNaN(cX)) cX = 0;
        RfMath::smithPoint(cR/m_Z0, cX/m_Z0, sx, sy);
        m.smithGraphCalib.add(QCPCurveData(k, sx, sy));
        m.smithGraphViewCalib.add(QCPCurveData(qMax(0, k*2 - 1), sx, sy));
    }
}

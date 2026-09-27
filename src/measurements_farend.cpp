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

void Measurements::calcFarEnd(bool _incrementally)
{
    //if(m_calibration != NULL)
    {
        int count = m_measurements.length();
        int dataCount;
        QVector <RawData> data;
        int i = _incrementally ? (count-1) : 0;
        for( ; i < count; ++i)
        {
            if(m_calibration != nullptr && m_calibration->getCalibrationEnabled())
            {
                dataCount = m_measurements.at(i).dataRXCalib.length();
                data = m_measurements.at(i).dataRXCalib;
            }else
            {
                dataCount = m_measurements.at(i).dataRX.length();
                data = m_measurements.at(i).dataRX;
            }
            if (! _incrementally)
            {
                if(m_farEndMeasurement==1) // subtract cable
                {
                    m_farEndMeasurementsSub[i].dataRX.clear();
                    m_farEndMeasurementsSub[i].swrGraph.clear();
                    m_farEndMeasurementsSub[i].rlGraph.clear();
                    m_farEndMeasurementsSub[i].rsrGraph.clear();
                    m_farEndMeasurementsSub[i].rsxGraph.clear();
                    m_farEndMeasurementsSub[i].rszGraph.clear();
                    m_farEndMeasurementsSub[i].rprGraph.clear();
                    m_farEndMeasurementsSub[i].rpxGraph.clear();
                    m_farEndMeasurementsSub[i].rpzGraph.clear();
                    m_farEndMeasurementsSub[i].phaseGraph.clear();
                    m_farEndMeasurementsSub[i].rhoGraph.clear();
                    m_farEndMeasurementsSub[i].smithGraph.clear();
                }else if(m_farEndMeasurement==2) // add cable
                {
                    m_farEndMeasurementsAdd[i].dataRX.clear();
                    m_farEndMeasurementsAdd[i].swrGraph.clear();
                    m_farEndMeasurementsAdd[i].rlGraph.clear();
                    m_farEndMeasurementsAdd[i].rsrGraph.clear();
                    m_farEndMeasurementsAdd[i].rsxGraph.clear();
                    m_farEndMeasurementsAdd[i].rszGraph.clear();
                    m_farEndMeasurementsAdd[i].rprGraph.clear();
                    m_farEndMeasurementsAdd[i].rpxGraph.clear();
                    m_farEndMeasurementsAdd[i].rpzGraph.clear();
                    m_farEndMeasurementsAdd[i].phaseGraph.clear();
                    m_farEndMeasurementsAdd[i].rhoGraph.clear();
                    m_farEndMeasurementsAdd[i].smithGraph.clear();
                }
            }
            int ii = _incrementally ? (dataCount-1) : 0;
            for( ; ii < dataCount; ++ii)
            {
                calcFarEnd(data.at(ii), i);
            }
        }
    }
}

RawData Measurements::calcFarEnd(const RawData& data, int idx, bool refreshGraphs)
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
        Complex zin = RfMath::cableTransform(fq, R, X, cableParams(), m_farEndMeasurement==1);
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
    QList <measurement>& _farEndMeasurements = (m_farEndMeasurement==1)
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



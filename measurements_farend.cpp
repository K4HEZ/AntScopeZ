#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"
#include "Notification.h"

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

    double Rpar;
    double Xpar;

    double fq = data.fq;
    double R = data.r;
    double X = data.x;

    QCPData qdata;
    qdata.key = fq*1000;

    double Klen = 1;
    switch (m_cableLossUnits)
    {
    case 0: Klen = 1; break;
    case 1: Klen = 1*100.0; break;
    case 2: Klen = 1/FEETINMETER; break;
    case 3: Klen = 1/FEETINMETER*100.0; break;
    }

    Complex Zload = Complex( R, X);

    double dMatchedLossDb;  // Note that K1/K2 are in dB/100 ft
    if(!m_cableLossAtAnyFq)
        dMatchedLossDb = m_cableLossConductive*Klen*sqrt(fq) + m_cableLossDielectric*Klen*fq;
    else
        dMatchedLossDb = m_cableLossConductive*Klen + m_cableLossDielectric*Klen;


#define NEPER 8.68588963806504        // = 20 / Ln(10)

    double Alpha = dMatchedLossDb / 100.0 / NEPER; // Nepers (attenuation) per foot
    double Beta = (2*M_PI * fq) / (SPEEDOFLIGHT*FEETINMETER/1000000.0 * m_cableVelFactor); // Radians (phase constant) per foot

    double Alphal = Alpha * m_cableLength;
    double Betal = Beta * m_cableLength;

    da.r = R;
    da.x = X;

    if(m_farEndMeasurement==1) // subtract cable
    {
        Alphal = -Alphal;
        Betal = -Betal;
    }
    if (m_cableLossUnits==0)
    {
        Alphal *= FEETINMETER;
        Betal *= FEETINMETER;
    }

    Complex Sinh_gl = Complex( cos(Betal) * sinh(Alphal), sin(Betal) * cosh(Alphal) );
    Complex Cosh_gl = Complex( cos(Betal) * cosh(Alphal), sin(Betal) * sinh(Alphal) );

    Complex Zo = Complex(m_cableResistance, -m_cableResistance * (Alpha / Beta));

    Complex ZIZL = Zo * ( (Zload*Cosh_gl + Zo*Sinh_gl) /  (Zo*Cosh_gl + Zload*Sinh_gl) );

    R = ZIZL.real();
    if(R<0.0001)
        R = 0.0001;
    X = ZIZL.imag();

    Rpar = R*(1+X*X/R/R);
    Xpar = X*(1+R*R/X/X);

    if (qIsNaN(R) || (R<0.001) ) {R = 0.01;}
    if (qIsNaN(X)) {X = 0;}

    double Rnorm = R/m_Z0;
    double Xnorm = X/m_Z0;

    double Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
    double RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
    double RhoImag = 2*Xnorm/Denom;

    double RhoPhase = atan2(RhoImag, RhoReal) / M_PI * 180.0;
    double RhoMod = sqrt(RhoReal*RhoReal+RhoImag*RhoImag);

    double swr=1;
    double rl=0;
    computeSWR(fq, getZ0(), R, X, &swr, &rl);

    da.r = R;
    da.x = X;
    QList <measurement>& _farEndMeasurements = (m_farEndMeasurement==1)
            ? m_farEndMeasurementsSub
            : m_farEndMeasurementsAdd;

    _farEndMeasurements[idx].dataRX.append(da);

    if (refreshGraphs) {
        qdata.value = swr;
        _farEndMeasurements[idx].swrGraph.insert(qdata.key,qdata);

        qdata.value = rl;
        _farEndMeasurements[idx].rlGraph.insert(qdata.key,qdata);

        qdata.value = R;
        _farEndMeasurements[idx].rsrGraph.insert(qdata.key,qdata);
        qdata.value = X;
        _farEndMeasurements[idx].rsxGraph.insert(qdata.key,qdata);
        qdata.value = computeZ(R, X);
        _farEndMeasurements[idx].rszGraph.insert(qdata.key,qdata);

        qdata.value = Rpar;
        _farEndMeasurements[idx].rprGraph.insert(qdata.key,qdata);
        qdata.value = Xpar;
        _farEndMeasurements[idx].rpxGraph.insert(qdata.key,qdata);
        qdata.value = computeZ(R, X);
        _farEndMeasurements[idx].rpzGraph.insert(qdata.key,qdata);

        qdata.value = RhoPhase;
        _farEndMeasurements[idx].phaseGraph.insert(qdata.key,qdata);
        qdata.value = RhoMod;
        _farEndMeasurements[idx].rhoGraph.insert(qdata.key,qdata);

        double pointX,pointY;
        NormRXtoSmithPoint(R/m_Z0, X/m_Z0, pointX, pointY);
        int len = _farEndMeasurements[idx].dataRX.length();
        _farEndMeasurements[idx].smithGraph.insert(len, QCPCurveData(len, pointX, pointY));
    }
    return da;
}

void Measurements::on_exportCableSettings(QString _description)
{
    // m_measurements.size()-1 is -1 with no scans yet, which silently
    // wraps to 4294967295 once it's passed into
    // Export::setMeasurements()'s quint32 number parameter -- Export::
    // suggestedPath() then feeds that straight into getMeasurement() with
    // no bounds check, an out-of-bounds QList access that crashes as soon
    // as the Export dialog needs a suggested filename. Guard here instead
    // of chasing it downstream.
    if (m_measurements.isEmpty()) {
        Notification::showMessage(tr("No measurement to export -- run a scan first."), m_tableWidget);
        return;
    }

    Export* exportDialog = new Export(m_tableWidget);
    exportDialog->setAttribute(Qt::WA_DeleteOnClose);
    exportDialog->setWindowTitle(tr("Export"));
    exportDialog->setMeasurements(this, m_measurements.size()-1, true, _description);

    exportDialog->exec();
}



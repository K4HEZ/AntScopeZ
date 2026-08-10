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

void Measurements::startAutocalibrateProgress(QWidget* _parent, int _dots)
{
    delete m_autoCalibrateProgressDlg;

    m_autoCalibrateProgressDlg = new ProgressDlg(_parent);
    m_autoCalibrateProgressDlg->setWindowModality(Qt::WindowModal);
    m_autoCalibrateProgressDlg->setValue(0);
    m_autoCalibrateProgressDlg->setProgressData(0, _dots, 1);
    m_autoCalibrateProgressDlg->updateActionInfo(tr("Auto calibration"));
    m_autoCalibrateProgressDlg->updateStatusInfo(tr("please wait ...."));
    m_autoCalibrateProgressDlg->setCancelable();
    connect(m_autoCalibrateProgressDlg, &ProgressDlg::canceled, this, &Measurements::interrupt);
    m_autoCalibrateProgressDlg->show();
    QApplication::processEvents();
}

void Measurements::stopAutocalibrateProgress()
{
    if (m_autoCalibrateProgressDlg != nullptr)
    {
        m_autoCalibrateProgressDlg->hide();
        m_autoCalibrateProgressDlg->deleteLater();
        m_autoCalibrateProgressDlg = nullptr;
    }
}

void Measurements::updateAutocalibrateProgress(int _dots, QString _msg)
{
    if (m_autoCalibrateProgressDlg == nullptr)
        return;
    m_autoCalibrateProgressDlg->setValue(_dots);
    m_autoCalibrateProgressDlg->updateStatusInfo(QString(tr("Iteration %1. %2"))
                                       .arg(_dots)
                                       .arg(_msg));
}

QPair<double, double> Measurements::autoCalibrate()
{
    m_settings->beginGroup("Auto-calibration");
    double cable_length_min = m_settings->value("cable_length_min", 0).toDouble();
    double cable_length_max = m_settings->value("cable_length_max", 0.02).toDouble();
    double cable_length_steps = m_settings->value("cable_length_steps", 100).toDouble();
    double cable_res_min = m_settings->value("cable_res_min", 20).toDouble();
    double cable_res_max = m_settings->value("cable_res_max", 40).toDouble();
    double cable_res_steps = m_settings->value("cable_res_steps", 100).toDouble();
    m_settings->endGroup();

    startAutocalibrateProgress(nullptr, cable_length_steps * cable_res_steps);
    // backup settings
    double _cableResistance = m_cableResistance;
    double _cableLength = m_cableLength;

    if (m_autoCalibration == 1) {
        m_cableLossConductive = 0;
        m_cableLossDielectric = 0;
        m_cableLossFqMHz = 1;
        m_farEndMeasurement = 1;
        m_cableLossAtAnyFq = 1;
        m_cableLossUnits = 0;
        m_cableVelFactor = 1;


        double dBestLength = cable_length_min;
        double dBestResistance = cable_res_max;
        double dBestDistance = 100000000000.0;

        double dBestMaxSwrValue = 0;
        double dBestMaxSwrFq = 0;

        double dSqrDist = 0;
        double Rswr = getZ0();

        //bKeypressDetected = FALSE;

        int total = 0;
        double leStep = (cable_length_max-cable_length_min)/cable_length_steps;
        double reStep = (cable_res_max-cable_res_min)/cable_res_steps;
        for (double dLen = cable_length_min; dLen < cable_length_max; dLen += leStep)
        {
            total++;
            m_cableLength = dLen;

            for (double dRes = cable_res_min; dRes < cable_res_max; dRes += reStep)
            {
                total++;
                m_cableResistance = dRes;

                dSqrDist = 0;
                double dMaxSwrValue = 0;
                double dMaxSwrFq = 0;

                int count = getMeasurementLength();
                if (count == 0) {
                    return QPair<double, double>(_cableResistance, _cableLength);
                }
                measurement* mm = getMeasurement(count-1);
                m_farEndMeasurementsSub[count-1].dataRX.clear();
                const QVector<RawData>& data = mm->dataRX;

//                double dSqrDist0 = 0;
                for (int i=0; i<data.size(); i++) {
                    const RawData& inData = data.at(i);
                    if (inData.fq*1000 > 10000)
                    {
                        RawData outData = calcFarEnd(inData, count-1, true);
                        double R = outData.r;
                        double X = outData.x;

                        double Gre = (R*R-Rswr*Rswr+X*X)/((R+Rswr)*(R+Rswr)+X*X);
                        double Gim = (2*Rswr*X)/((R+Rswr)*(R+Rswr)+X*X);

                        dSqrDist += Gre*Gre+Gim*Gim;

                        double tmpSWR;
                        int ret = computeSWR(inData.fq*1000.0, getZ0(), R, X, &tmpSWR, nullptr);
                        if (ret != 0) {

//                            if (i == 0) {
//                                dSqrDist0 = tmpSWR*tmpSWR;
//                                dSqrDist = dSqrDist0;
//                            }
//                            else
//                            {
//                                dSqrDist += (tmpSWR - dSqrDist0)*(tmpSWR - dSqrDist0);
//                            }

                            double tmpFQ = inData.fq;
                            if (tmpSWR > dMaxSwrValue)
                            {
                                dMaxSwrValue = tmpSWR;
                                dMaxSwrFq = tmpFQ;
                            }
                        }
                    }
                }

                bool update = false;
                if (dSqrDist<dBestDistance)
                {
                    update = true;
                    dBestDistance = dSqrDist;
                    dBestLength = dLen;
                    dBestResistance = dRes;

                    on_redrawGraphs();
                    QApplication::processEvents();

                    dBestMaxSwrValue = dMaxSwrValue;
                    dBestMaxSwrFq = dMaxSwrFq;

                    QString msg = QString("Connector compensation: SWR=%1 at Fq=%2 MHz: Rcable=%3, Lcable=%4")
                            .arg(dBestMaxSwrValue)
                            .arg(dBestMaxSwrFq)
                            .arg(m_cableResistance)
                            .arg(m_cableLength);
                    updateAutocalibrateProgress(total, msg);
                }
                if (update || ((total%100)==0)) {
                    QString msg = QString("Connector compensation: SWR=%1 at Fq=%2 MHz: Rcable=%3, Lcable=%4")
                            .arg(dBestMaxSwrValue)
                            .arg(dBestMaxSwrFq)
                            .arg(m_cableResistance)
                            .arg(m_cableLength);
                    updateAutocalibrateProgress(total, msg);
                }
                QApplication::processEvents();
                if (m_interrupted)
                    break;
            }

            QApplication::processEvents();
            if (m_interrupted)
                break;
        }

        if (m_interrupted) {
            m_interrupted = false;
            stopAutocalibrateProgress();
            return QPair<double, double>(_cableResistance, _cableLength);;
        }

        m_cableLength = dBestLength;
        m_cableResistance = dBestResistance;

        stopAutocalibrateProgress();
        on_redrawGraphs();
    }

    m_interrupted = false;
    QPair<double, double> result(m_cableResistance, m_cableLength);

    // restore settings
    //m_cableResistance = _cableResistance;
    //m_cableLength = _cableLength;

    setAutoCalibration(0);
    return result;
}


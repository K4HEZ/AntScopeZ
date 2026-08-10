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

int Measurements::calcTdrDist(QVector<RawData> *data)
{
    if (data == nullptr || data->length() == 0)
        return 0;

    int asize = data->length();

    double minfq = data->at(0).fq;
    if ( minfq > 0.1 )
    {
        return 0; // Wrong fq
    }

    double maxfq = data->at(asize-1).fq;
    int iTdrFftSize = 0;

    int i;
    for (i=0; ; i++)
    {
        iTdrFftSize = (1<<i);
        if ( (iTdrFftSize/2) >= (asize-1) )
            break;

        if (i==14)
            return 0; // bug
    }

    iTdrFftSize *= 8;

    if (iTdrFftSize > TDR_MAXARRAY)
            return 0; // bug

    double tdrResolution = 1.0/(maxfq-minfq)/4*299792458*m_cableVelFactor / (iTdrFftSize/2) * (asize-1);

    double tdrRange = tdrResolution*iTdrFftSize/1000000;

    if (!m_measureSystemMetric)
        tdrRange *= FEETINMETER;

    return tdrRange;
}

int Measurements::CalcTdr(QVector <RawData> *data)
{
    if (data == nullptr || data->length() == 0)
        return 0;

    int asize = data->length();

    if (asize < (int)m_tdrDots)
    {
        return 0;
    }

    double minfq = data->at(0).fq;
    if ( minfq > 0.1 )
    {
        return 0; // Wrong fq
    }

    double maxfq = data->at(asize-1).fq;

    if (asize < 2 || maxfq <= minfq)
    {
        // A single data point (the norm for the first tick of a live scan,
        // e.g. when TDR is one of the joined "Multi" views and gets redrawn
        // after every incoming point) makes maxfq==minfq, so 1.0/(maxfq-minfq)
        // below is +-inf; that gets multiplied by the trailing *(asize-1),
        // which is 0 in this same case, giving inf*0 == NaN. That NaN then
        // flows into the axis range and graph data and crashes QCustomPlot's
        // internal qRound() the next time it renders. Bail out instead --
        // the caller (redrawTDR) already needs to tolerate a 0 return since
        // the checks above it can do the same.
        return 0;
    }

    int m_iTdrFftSize = 0;

    int i;
    for (i=0; ; i++)
    {
        m_iTdrFftSize = (1<<i);
        if ( (m_iTdrFftSize/2) >= (asize-1) )
            break;

        if (i==14)
            return 0; // bug
    }

    m_iTdrFftSize *= 8;

    if (m_iTdrFftSize > TDR_MAXARRAY)
            return 0; // bug

    m_tdrResolution = 1.0/(maxfq-minfq)/4*299792458*m_cableVelFactor / (m_iTdrFftSize/2) * (asize-1);

    m_tdrRange = m_tdrResolution*m_iTdrFftSize/1000000;

    if (!m_measureSystemMetric)
        m_tdrRange *= FEETINMETER;


    float *TdrReal = new float[TDR_MAXARRAY];
    float *TdrImag = new float[TDR_MAXARRAY];

#define Rdevice 50.0

    for (i=0; i<=m_iTdrFftSize/2; i++)
    {
        double R=0;
        double X=0;
        double Gre=0;
        double Gim=0;
        double FQ=0;
        if (i < asize)
        {
            FQ = data->at(i).fq;
            R = data->at(i).r;
            X = data->at(i).x;

            Gre = (R*R-Rdevice*Rdevice+X*X)/((R+Rdevice)*(R+Rdevice)+X*X);
            Gim = (2*Rdevice*X)/((R+Rdevice)*(R+Rdevice)+X*X);

            if ( i==0)
            {
                double m_dFarEndImpedance = 50;
                Gre = (m_dFarEndImpedance-Rdevice)/(m_dFarEndImpedance+Rdevice);
                Gim = 0;
            }

#define KP (1.0/0.53836)
                double k = 0.53836-0.46146*cos(M_PI+M_PI*i/(asize-1));

                TdrReal[i] = Gre*m_iTdrFftSize/asize/2.0*k*KP;
                TdrImag[i] = Gim*m_iTdrFftSize/asize/2.0*k*KP;
        }
        else
        {
            TdrReal[i] = 0;
            TdrImag[i] = 0;
        }
    }

// Interpolate zero frequency

#define BDR 1
    for (i=0; i<BDR; i++)
    {
        double newreal = sqrt(TdrReal[BDR]*TdrReal[BDR]+TdrImag[BDR]*TdrImag[BDR]);

        if (TdrReal[BDR] < 0)
            TdrReal[i] = -newreal;
        else
            TdrReal[i] = newreal;

        TdrImag[i] = 0;

    }

// Mirror
    for (i=1; i<m_iTdrFftSize/2; i++)
    {
        TdrReal[m_iTdrFftSize-i] = TdrReal[i];
        TdrImag[m_iTdrFftSize-i] = -TdrImag[i];
    }
    TdrReal[m_iTdrFftSize/2] = 0;
    TdrImag[m_iTdrFftSize/2] = 0;

    FFT(TdrReal, TdrImag, m_iTdrFftSize, 1/*Inverse*/);	// Inverse FFT

    double ig = 0;
    for (i=0; i<m_iTdrFftSize; i++)
    {
        double Amp = TdrReal[i];
        if((Amp > 0.015) || (Amp < -0.015))
        {
            m_pdTdrImp[i] = Amp;
            ig += Amp/2/(((double)m_iTdrFftSize)/asize/2);
        }else
        {
            m_pdTdrImp[i] = 0;
        }

        m_pdTdrStep[i] = ig;

        double Z = m_Z0*(1+ig)/(1-ig);
        Z = (Z < 0) ? 0 : Z;
        m_pdTdrZ[i] = (Z > VALUE_LIMIT) ? VALUE_LIMIT : Z;
    }

    delete[] TdrReal;
    delete[] TdrImag;
    return m_iTdrFftSize;
}

void Measurements::FFT(float real[], float imag[], int length, int Inverse)
{
    double wreal, wpreal, wimag, wpimag, theta;
    double tempreal, tempimag, tempwreal, direction;

    int Addr, Position, Mask, BitRevAddr, PairAddr;
    int m, k;

    direction = -1.0;		// direction of rotating phasor for FFT

    if(Inverse)
        direction = 1.0;	// direction of rotating phasor for IFFT

    //  bit-reverse the addresses of both the real and imaginary arrays
    //  real[0..length-1] and imag[0..length-1] are the paired complex numbers

    for (Addr=0; Addr<length; Addr++)
    {
        // Derive Bit-Reversed Address
        BitRevAddr = 0;
        Position = length >> 1;
        Mask = Addr;
        while (Mask)
        {
            if(Mask & 1)
                BitRevAddr += Position;
            Mask >>= 1;
            Position >>= 1;
        }

        if (BitRevAddr > Addr)				// Swap
        {
            double s;
            s = real[BitRevAddr];			// real part
            real[BitRevAddr] = real[Addr];
            real[Addr] = s;
            s = imag[BitRevAddr];			// imaginary part
            imag[BitRevAddr] = imag[Addr];
            imag[Addr] = s;
        }
    }

    // FFT, IFFT Kernel

    for (k=1; k < length; k <<= 1)
    {
        theta = direction * M_PI / (double)k;
        wpimag = sin(theta);
        wpreal = cos(theta);
        wreal = 1.0;
        wimag = 0.0;

        for (m=0; m < k; m++)
        {
            for (Addr = m; Addr < length; Addr += (k*2))
            {
                PairAddr = Addr + k;

                tempreal = wreal * (double)real[PairAddr] - wimag * (double)imag[PairAddr];
                tempimag = wreal * (double)imag[PairAddr] + wimag * (double)real[PairAddr];


                real[PairAddr] = (double)real[Addr] - tempreal;
                imag[PairAddr] = (double)imag[Addr] - tempimag;
                real[Addr] += tempreal;
                imag[Addr] += tempimag;
            }
            tempwreal = wreal;
            wreal = wreal * wpreal - wimag * wpimag;
            wimag = wimag * wpreal + tempwreal * wpimag;
        }
    }

    if(Inverse)							// Normalize the IFFT coefficients
        for(int i=0; i<length; i++)
        {
            real[i] /= (double)length;
            imag[i] /= (double)length;
        }
}

int Measurements::CalcTdr2(QVector <RawData> *data)
{
    Q_UNUSED(data);
    // not used
    return 0;
}

qint16 Measurements::DTF_FindRadix2Length(qint16 length, int *log2N)
{
    Q_UNUSED(length);
    Q_UNUSED(log2N);
    // not used
    return 0;
}

void  Measurements::FFT2(double *Rdat, double *Idat, int N, int LogN, int Ft_Flag)
{
    Q_UNUSED(Rdat);
    Q_UNUSED(Idat);
    Q_UNUSED(N);
    Q_UNUSED(LogN);
    Q_UNUSED(Ft_Flag);

}


void Measurements::startTDRProgress(QWidget* _parent, int _dots)
{
    m_tdrDots = _dots;
    delete m_tdrProgressDlg;

    m_tdrProgressDlg = new ProgressDlg(_parent);
    m_tdrProgressDlg->setWindowModality(Qt::WindowModal);
    m_tdrProgressDlg->setValue(0);
    m_tdrProgressDlg->setProgressData(0, _dots, 1);
    m_tdrProgressDlg->updateActionInfo(tr("TDR measuring"));
    m_tdrProgressDlg->updateStatusInfo(tr("please wait ...."));
    m_tdrProgressDlg->setCancelable();
    connect(m_tdrProgressDlg, &ProgressDlg::canceled, this, &Measurements::measurementCanceled);
    m_tdrProgressDlg->show();
}

void Measurements::stopTDRProgress()
{
    if (m_tdrProgressDlg != nullptr)
    {
        m_tdrProgressDlg->hide();
        delete m_tdrProgressDlg;
        m_tdrProgressDlg = nullptr;
    }
    on_redrawGraphs();
}

void Measurements::updateTDRProgress(int dots)
{
    if (m_tdrProgressDlg != nullptr) {
        //if ((dots%10) == 0)
        {
            m_tdrProgressDlg->setValue(dots);
            m_tdrProgressDlg->updateStatusInfo(QString(tr("processed %1 dots, from %2")).arg(dots).arg(m_tdrDots));
        }
    }
}

void Measurements::redrawTDR(int _index)
{
    m_tdrZRange = 0;
    int mode = m_farEndMeasurement;
    int begin = _index < 0 ? 0 : _index;
    int end = _index < 0 ? m_measurements.length() : (_index+1);
    for (int index=begin; index<end; index++) {
        measurement& mm = (mode == 1)
                ? m_farEndMeasurementsSub[index]
                : ( (mode == 2) ? m_farEndMeasurementsAdd[index] : m_measurements[index] );

        int len = CalcTdr(m_calibration->getCalibrationEnabled()
                      ? &mm.dataRXCalib
                      : &mm.dataRX);
        if (len <= 0)
        {
            // CalcTdr() returns 0 for several "not enough/valid data yet"
            // cases (too few points, wrong fq, FFT size out of range). The
            // code below unconditionally divides m_tdrRange by len, which
            // would itself be a division by zero (0/0 == NaN when m_tdrRange
            // is still its initial 0) -- skip this measurement's TDR redraw
            // instead of feeding that into the axis/graphs.
            continue;
        }
        m_tdrWidget->xAxis->setRangeUpper(m_tdrRange);
        m_tdrWidget->xAxis->setRangeMax(m_tdrRange);
        double step = m_tdrRange/len;
        mm.tdrImpGraph.clear();
        mm.tdrStepGraph.clear();
        mm.tdrZGraph.clear();
        mm.tdrImpGraphFeet.clear();
        mm.tdrStepGraphFeet.clear();
        mm.tdrZGraphFeet.clear();
        for(int i = 0; i < len; ++i)
        {
            double x = i;
            QCPData data;
            data.key = x*step;
            data.value = m_pdTdrImp[i];
            mm.tdrImpGraph.insert(data.key,data);
            data.value = m_pdTdrStep[i];
            mm.tdrStepGraph.insert(data.key,data);
            data.value = m_pdTdrZ[i];
            mm.tdrZGraph.insert(data.key,data);

            QCPData dataFeet;
            dataFeet.key = x*step;
            dataFeet.value = m_pdTdrImp[i];
            mm.tdrImpGraphFeet.insert(dataFeet.key,dataFeet);
            dataFeet.value = m_pdTdrStep[i];
            mm.tdrStepGraphFeet.insert(dataFeet.key,dataFeet);
            dataFeet.value = m_pdTdrZ[i];
            mm.tdrZGraphFeet.insert(dataFeet.key,dataFeet);

            m_tdrZRange = m_measureSystemMetric ? qMax(m_tdrZRange, data.value) : qMax(m_tdrZRange, dataFeet.value);
        }
        m_tdrWidget->graph(index*3+1)->setData(
                        m_measureSystemMetric ? &mm.tdrImpGraph : &mm.tdrImpGraphFeet, true);
        m_tdrWidget->graph(index*3+2)->setData(
                        m_measureSystemMetric ? &mm.tdrStepGraph : &mm.tdrStepGraphFeet, true);
        m_tdrWidget->graph(index*3+3)->setData(
                        m_measureSystemMetric ? &mm.tdrZGraph : &mm.tdrZGraphFeet, true);
    } // for ( index )
    // m_tdrZRange is reset to 0 at the top of this function and only raised
    // inside the per-measurement loop above, which is skipped entirely
    // (continue) whenever CalcTdr() had no valid TDR data for that
    // measurement -- e.g. every call during a normal frequency-band scan,
    // since CalcTdr() rejects any data that doesn't start near DC. Setting
    // yAxis2 to [0, 0*1.05] == [0, 0] unconditionally collapses it to a
    // zero-size range; QCPAxis::coordToPixel() then divides by
    // mRange.size() (== 0) whenever it maps a value of 0 (the axis's own
    // lower bound, hit on essentially every replot), giving 0/0 == NaN and
    // crashing the next qRound() on that pixel coordinate. Only touch the
    // axis when we actually have a new, real range to show.
    if (m_tdrZRange > 0)
    {
        m_tdrWidget->yAxis2->setRangeUpper(m_tdrZRange*1.05);
        m_tdrWidget->yAxis2->setRangeLower(0);
    }
    extern MainWindow* g_mainWindow;
    g_mainWindow->m_tdrZRange = m_tdrZRange;

    replot();
}


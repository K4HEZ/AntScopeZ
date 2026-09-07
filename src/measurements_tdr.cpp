#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"
#include "../analyzer/qcpgraphdatahelpers.h"

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

    // See calcTdrEstimateRaw() -- this used to duplicate CalcTdr()'s FFT-size/
    // resolution/range math inline; both now share one implementation. Its
    // asize<2||maxfq<=minfq NaN guard is a small behavior addition here (this
    // function never had it before, unlike CalcTdr()), closing the same
    // latent single-point-measurement crash CalcTdr() was already guarded
    // against.
    return (int)calcTdrEstimateRaw(asize, minfq, maxfq, m_cableVelFactor, m_measureSystemMetric).unambiguousRange;
}

// See the declaration in measurements.h for what unambiguousRange/resolution/
// chartStep each mean and how they were verified.
Measurements::TdrEstimate Measurements::calcTdrEstimateRaw(int asize, double minfqMHz, double maxfqMHz,
                                                             double velFactor, bool metric)
{
    TdrEstimate est;

    if (asize < 2 || maxfqMHz <= minfqMHz)
        return est; // NaN guard -- see CalcTdr()'s original comment on why (0-width/0-length data)

    int fftSize = 0;
    for (int i=0; ; i++)
    {
        fftSize = (1<<i);
        if ( (fftSize/2) >= (asize-1) )
            break;

        if (i==14)
            return TdrEstimate(); // bug
    }

    fftSize *= 8;

    if (fftSize > TDR_MAXARRAY)
        return TdrEstimate(); // bug

    double bw = maxfqMHz - minfqMHz;

    double chartStep = 1.0/bw/4*299792458*velFactor / (fftSize/2) * (asize-1);
    double range = chartStep*fftSize/1000000;
    // c x VF / (2 x BW), BW in Hz (maxfqMHz/minfqMHz are MHz, hence *1e6) --
    // independent of asize/fftSize, unlike chartStep/range above.
    double resolution = (299792458.0 * velFactor) / (2.0 * bw * 1.0e6);

    if (!metric)
    {
        range *= FEETINMETER;
        resolution *= FEETINMETER;
    }

    est.unambiguousRange = range;
    est.resolution = resolution;
    est.chartStep = chartStep;
    est.fftSize = fftSize;
    return est;
}

Measurements::TdrEstimate Measurements::calcTdrEstimate(int dots, double topFreqMHz, double velFactor, bool metric)
{
    return calcTdrEstimateRaw(dots + 1, 0.0, topFreqMHz, velFactor, metric);
}

// Modified Bessel function of the first kind, order 0 -- series expansion,
// only needed for the Kaiser window below. Accurate to well under the
// window's own precision needs across the beta range a Kaiser window
// picker would realistically expose (0-20ish); no external math library
// pulled in for one function.
static double besselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    double xx = x*x/4.0;
    for (int k = 1; k <= 25; k++)
    {
        term *= xx/((double)k*(double)k);
        sum += term;
        if (term < 1e-12*sum)
            break;
    }
    return sum;
}

// Combined window-shape-and-normalization coefficient CalcTdr() multiplies
// each frequency bin's reflection coefficient by before the inverse FFT.
// i=0..n-1 maps onto the *falling half* (x=0.5..1.0) of a standard
// full-length window -- full gain at DC (i=0), tapering toward that
// window's minimum at the top of the measured band (i=n-1). That's
// deliberate, not a simplification: a TDR sweep only has one truncation
// edge to taper (the top of the measured band) -- the low edge is the
// spectrum's real DC start, not an artifact -- unlike a typical centered
// FFT window, which tapers both edges of data that's truncated on both
// sides. This mapping reproduces the original hardcoded Hamming formula
// exactly (case TdrWindow::Hamming below is algebraically identical to the
// pre-2026-08-21 inline code).
//
// Normalization: each case (Kaiser aside) divides by that window's own
// additive/DC-offset coefficient ("a0"), matching the original Hamming
// code's own KP=1/0.53836 convention -- an approximation (not a rigorous
// coherent-gain correction), kept for the sake of not changing today's
// Hamming output. Kaiser's own formula already peaks at exactly 1.0 at
// x=0.5 (i=0), so it needs no extra normalization.
double Measurements::tdrWindowCoeff(TdrWindow type, double beta, int i, int n)
{
    double x = (n > 1) ? (0.5 + 0.5*(double)i/(double)(n-1)) : 0.5;

    switch (type)
    {
    case TdrWindow::Rectangular:
        return 1.0;

    case TdrWindow::Hann:
        return (0.5 - 0.5*cos(2*M_PI*x)) / 0.5;

    case TdrWindow::Blackman:
        return (0.42 - 0.5*cos(2*M_PI*x) + 0.08*cos(4*M_PI*x)) / 0.42;

    case TdrWindow::Kaiser:
    {
        double arg = 1.0 - (2.0*x-1.0)*(2.0*x-1.0);
        if (arg < 0)
            arg = 0;
        return besselI0(beta*sqrt(arg)) / besselI0(beta);
    }

    case TdrWindow::Hamming:
    default:
        return (0.53836 - 0.46146*cos(2*M_PI*x)) / 0.53836;
    }
}

int Measurements::CalcTdr(QVector <RawData> *data)
{
    if (data == nullptr || data->length() == 0)
        return 0;

    int asize = data->length();

    // m_tdrDots is the dot count Tools > TDR Measurement's own scan was
    // started with (startTDRProgress()) -- this guard's real job is
    // refusing to FFT a TDR-tool scan that was canceled before collecting
    // as many points as it was asked for (stopTDRProgress() always
    // redraws once on the way out, canceled or not). stopTDRProgress()
    // resets m_tdrDots back to 0 right after, specifically so this can't
    // outlive that one scan: found 2026-08-26 that without the reset, any
    // later CalcTdr() call -- a normal scan, or even just switching to the
    // TDR tab -- silently returned 0 forever after, since a normal scan's
    // point count (~100) is routinely below the TDR tool's own minimum
    // (TDR_MINPOINTS = 200), regardless of that later data's own frequency
    // range being perfectly valid.
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

    // A single data point (the norm for the first tick of a live scan, e.g.
    // when TDR is one of the joined "Multi" views and gets redrawn after
    // every incoming point) makes maxfq==minfq, so 1.0/(maxfq-minfq) inside
    // calcTdrEstimateRaw() would be +-inf, multiplied by a trailing
    // *(asize-1) that's 0 in this same case, giving inf*0 == NaN -- which
    // would flow into the axis range and graph data and crash QCustomPlot's
    // internal qRound() the next time it renders. calcTdrEstimateRaw() bails
    // out (fftSize stays 0) in exactly this case; the caller (redrawTDR)
    // already needs to tolerate a 0 return since the checks above it can do
    // the same.
    TdrEstimate est = calcTdrEstimateRaw(asize, minfq, maxfq, m_cableVelFactor, m_measureSystemMetric);
    if (est.fftSize == 0)
        return 0; // bug, or not enough/valid data yet -- see calcTdrEstimateRaw()

    int m_iTdrFftSize = est.fftSize;
    m_tdrResolution = est.chartStep;
    m_tdrRange = est.unambiguousRange;

    int i;

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

            double k = tdrWindowCoeff(m_tdrWindowType, m_tdrKaiserBeta, i, asize);

            TdrReal[i] = Gre*m_iTdrFftSize/asize/2.0*k;
            TdrImag[i] = Gim*m_iTdrFftSize/asize/2.0*k;
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
    // on_redrawGraphs() only redraws whichever tab is currently visible
    // (see its own tab_swr/tab_phase/.../tab_tdr dispatch) -- fine for a
    // normal frequency-sweep scan, since the user is necessarily looking at
    // *some* chart tab while one runs. TdrScanPanel (Tools > TDR
    // Measurement) breaks that assumption: it's deliberately tab-
    // independent (see the tdr-scan-rework-plan memory -- "replaces the old
    // tab-implicit trigger"), so a scan run from there with any *other* tab
    // selected left tdrImpGraph/tdrStepGraph/tdrZGraph never (re)populated
    // for the just-finished measurement. findTdrPeak() then read an empty
    // container and TdrScanPanel::refreshResult() (connected to the same
    // measurementComplete() signal, right after this call returns) always
    // showed "-- (run a TDR scan first)" no matter what was actually
    // scanned. Confirmed 2026-08-25.
    if (m_currentTab != "tab_tdr")
        redrawTDR();

    // Reset now that both redraw calls above (which need this scan's own
    // dots target for CalcTdr()'s canceled-early guard, see its comment)
    // are done with it. m_tdrDots has no other reader once this scan is
    // over (only CalcTdr()'s guard and updateTDRProgress()'s status text,
    // both scoped to this one scan) -- left set, it silently blocked every
    // later CalcTdr() call (a normal scan, or just switching to the TDR
    // tab) for the rest of the session, since a normal scan's point count
    // is routinely below whatever this TDR-tool scan asked for. Found
    // 2026-08-26.
    m_tdrDots = 0;
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

void Measurements::redrawTDR(int _index, bool resetRange)
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
        if (resetRange)
        {
            // setRangeMax(m_tdrRange) used to follow here too -- removed in
            // the 2.x port (2026-08-25), see Print::setRange()'s comment
            // (print.cpp): not real QCustomPlot API, and set to exactly the
            // value setRangeUpper() just applied on the line above anyway.
            m_tdrWidget->xAxis->setRangeUpper(m_tdrRange);
        }
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
            QCPGraphData data;
            data.key = x*step;
            data.value = m_pdTdrImp[i];
            mm.tdrImpGraph.add(data);
            data.value = m_pdTdrStep[i];
            mm.tdrStepGraph.add(data);
            data.value = m_pdTdrZ[i];
            mm.tdrZGraph.add(data);

            QCPGraphData dataFeet;
            dataFeet.key = x*step;
            dataFeet.value = m_pdTdrImp[i];
            mm.tdrImpGraphFeet.add(dataFeet);
            dataFeet.value = m_pdTdrStep[i];
            mm.tdrStepGraphFeet.add(dataFeet);
            dataFeet.value = m_pdTdrZ[i];
            mm.tdrZGraphFeet.add(dataFeet);

            m_tdrZRange = m_measureSystemMetric ? qMax(m_tdrZRange, data.value) : qMax(m_tdrZRange, dataFeet.value);
        }
        m_tdrWidget->graph(index*3+1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measureSystemMetric ? mm.tdrImpGraph : mm.tdrImpGraphFeet));
        m_tdrWidget->graph(index*3+2)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measureSystemMetric ? mm.tdrStepGraph : mm.tdrStepGraphFeet));
        m_tdrWidget->graph(index*3+3)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measureSystemMetric ? mm.tdrZGraph : mm.tdrZGraphFeet));
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

// Note: the "is this actually a reflection, or just noise" check (against
// CalcTdr()'s own 0.015 noise floor) intentionally isn't done here -- it
// stays in the caller (TdrScanPanel), same as it did in the now-merged
// TDRAnalysisDialog, since it only affects how the result is *displayed*
// ("No reflection above noise floor"), not whether a peak was technically
// found.
Measurements::TdrPeak Measurements::findTdrPeak(bool metric, double localVf)
{
    TdrPeak p;
    if (isEmpty())
        return p;

    // Same most-recent-measurement selection as
    // MarkerComparisonDialog::qFactorAt() -- see cableVelFactor()'s comment
    // and last()'s own comment for why this has to be 0, not
    // getMeasurementLength()-1.
    int mostRecent = 0;
    measurement* mm;
    switch (getFarEndMeasurement()) {
    case 1: mm = getMeasurementSub(mostRecent); break;
    case 2: mm = getMeasurementAdd(mostRecent); break;
    default: mm = last(); break;
    }
    if (mm == nullptr)
        return p;

    QCPGraphDataContainer& impMap = metric ? mm->tdrImpGraph : mm->tdrImpGraphFeet;
    // QCPGraphDataContainer has no .keys() (2026-08-25 QCustomPlot 2.x
    // port) -- it's already a sorted-by-key (ascending distance) sequence
    // with native index access, so walk it directly. bestKey/keys.at(i)
    // lookups against impMap *itself* (this loop) use impMap.at(i) rather
    // than a separate key lookup, since the index is already known; the
    // stepMap/zMap lookups further down are genuine cross-container
    // exact-key lookups and use graphValueAt() instead.
    if (impMap.isEmpty())
        return p;

    double bestKey = impMap.at(0)->key;
    double bestAmp = impMap.at(0)->value;
    int bestIndex = 0;
    for (int i = 1; i < impMap.size(); ++i) {
        double amp = impMap.at(i)->value;
        if (qAbs(amp) > qAbs(bestAmp)) {
            bestAmp = amp;
            bestKey = impMap.at(i)->key;
            bestIndex = i;
        }
    }

    // Impedance is read from the *step* response (tdrStepGraph/tdrZGraph),
    // not at the same key as the impulse peak above. CalcTdr()'s Z is
    // Z0*(1+ig)/(1-ig), where ig is a *running, cumulative* integral of the
    // reflection response ("step response," the classic TDR technique) --
    // it only reaches its true, settled value some distance *after* a
    // reflection's leading edge, not exactly at the impulse response's own
    // peak. Reading Z at bestKey directly gave a partial, transitional
    // value (confirmed 2026-08-21: a genuinely open 13ft cable read
    // "≈101 Ω" -- nowhere near VALUE_LIMIT=9999, the ceiling a real open
    // should approach). Fixed by searching forward from the impulse peak
    // for where the step response itself reaches its own largest
    // magnitude -- that's where it's actually settled -- and reading Z
    // there instead. Distance/amplitude above still use the impulse peak,
    // which is the right signal for *locating* and classifying (open vs.
    // short) a reflection; only the Ohms reading needed to move.
    QCPGraphDataContainer& stepMap = metric ? mm->tdrStepGraph : mm->tdrStepGraphFeet;
    QCPGraphDataContainer& zMap = metric ? mm->tdrZGraph : mm->tdrZGraphFeet;
    double zKey = bestKey;
    double bestStep = graphValueAt(stepMap, bestKey);
    for (int i = bestIndex + 1; i < impMap.size(); ++i) {
        double candidateKey = impMap.at(i)->key;
        double step = graphValueAt(stepMap, candidateKey);
        if (qAbs(step) > qAbs(bestStep)) {
            bestStep = step;
            zKey = candidateKey;
        }
    }
    p.impedanceOhms = graphValueAt(zMap, zKey);

    // The stored key is a distance computed with whatever velocity factor
    // was active when redrawTDR() last ran (cableVelFactor()). Distance is
    // linear in velocity factor (see chartStep's formula in
    // calcTdrEstimateRaw()), so rescaling to localVf is exact and doesn't
    // need re-running the FFT -- only re-plotting would.
    double globalVf = cableVelFactor();
    double ratio = (globalVf > 0 && localVf > 0) ? (localVf / globalVf) : 1.0;

    p.found = true;
    p.distance = bestKey * ratio;
    p.amplitude = bestAmp;

    double lastKey = impMap.at(impMap.size()-1)->key;
    p.nearRangeEdge = (lastKey > 0 && bestKey >= 0.95 * lastKey);

    return p;
}


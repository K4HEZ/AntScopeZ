#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"
#include "qcpgraphdatahelpers.h"

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

    // See TdrMath::estimateRaw() -- this used to duplicate CalcTdr()'s FFT-size/
    // resolution/range math inline; both now share one implementation. Its
    // asize<2||maxfq<=minfq NaN guard is a small behavior addition here (this
    // function never had it before, unlike CalcTdr()), closing the same
    // latent single-point-measurement crash CalcTdr() was already guarded
    // against.
    return (int)TdrMath::estimateRaw(asize, minfq, maxfq, m_cableVelFactor, m_measureSystemMetric).unambiguousRange;
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

    TdrMath::Result tdr = TdrMath::compute(*data, m_cableVelFactor, m_measureSystemMetric,
                                           m_tdrWindowType, m_tdrKaiserBeta, m_Z0);
    if (tdr.fftSize == 0)
        return 0;
    m_tdrRange = tdr.range;
    m_tdrImp = tdr.impulse;
    m_tdrStep = tdr.step;
    m_tdrZ = tdr.impedance;
    return tdr.fftSize;
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

        // Cable-corrected points already include calibration (see calcFarEnd()).
        bool calib = (mode != 1 && mode != 2) && m_calibration->getCalibrationEnabled();
        int len = CalcTdr(calib ? &mm.dataRXCalib : &mm.dataRX);
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
            data.value = m_tdrImp[i];
            mm.tdrImpGraph.add(data);
            data.value = m_tdrStep[i];
            mm.tdrStepGraph.add(data);
            data.value = m_tdrZ[i];
            mm.tdrZGraph.add(data);

            QCPGraphData dataFeet;
            dataFeet.key = x*step;
            dataFeet.value = m_tdrImp[i];
            mm.tdrImpGraphFeet.add(dataFeet);
            dataFeet.value = m_tdrStep[i];
            mm.tdrStepGraphFeet.add(dataFeet);
            dataFeet.value = m_tdrZ[i];
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
    // TdrMath::estimateRaw()), so rescaling to localVf is exact and doesn't
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


#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "popupindicator.h"
#include "analyzer/customanalyzer.h"
#include "analyzer/nanovna_analyzer.h"
#include "Notification.h"
#include "glwidget.h"
#include "CustomPlot.h"
#include "selectdevicedialog.h"
#include "printmulti.h"
#include "style.h"
#include "filedialog.h"
#include <QWindow>

extern QString appendSpaces(const QString& number);
extern bool g_usbOnly;
extern int g_maxMeasurements; // see measurements.cpp
extern int g_pointsWarnThreshold; // see mainwindow.cpp
extern QMap<QString, QString> g_mapTabPlotNames; // see mainwindow.cpp
extern void setAbsoluteFqMaximum();
extern bool g_bAA55modeNewProtocol;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original mainwindow.cpp (still in
// mainwindow.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of MainWindow.

// Pre-scan gate for Single/Continuous -- see declaration (mainwindow.h)
// for why TDR and Calibration don't go through this.
bool MainWindow::confirmScanPoints(int dots)
{
    if (dots <= g_pointsWarnThreshold)
        return true;

    return g_showMessageBox(this, QMessageBox::Warning, tr("Large scan"),
        tr("This scan will request %1 points, above your configured "
           "warning threshold of %2 (Settings > General). Large point "
           "counts can take a long time to complete. Continue?")
            .arg(dots).arg(g_pointsWarnThreshold),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Ok;
}

void MainWindow::on_singleStart_clicked()
{
    m_measurements->setContinuous(false);

    if (isMeasuring())
    {
        m_bInterrupted = true;
        // Same as on_pressEsc()'s equivalent call -- this is the other
        // user-facing way to stop a running scan (re-clicking Single), and
        // needs the same "stop accepting further points" flag Measurements::
        // on_newData()/on_newS21Data()/on_newUserData() now check. Missing
        // here before: only Esc set it, so stopping via this button instead
        // left leftover in-flight points (several devices, e.g. BLE, have
        // no real wire abort -- see BleAnalyzer::stopMeasure()) still
        // landing in m_measurements.last() same as any other point.
        m_measurements->interrupt();
        emit stopMeasure();
        ui->singleStart->setChecked(false);
        ui->fullBtn->setEnabled(true);
        ui->fullBtn->setChecked(true);
        ui->continuousStartBtn->setChecked(false);
        // One Fq mode isn't gated by g_developerMode (see the trigger just
        // below, and on_measurementComplete()'s own comment) -- cleaning
        // it up on stop shouldn't be either, or the floating widget is
        // left orphaned when stopping via Single without the flag set.
        m_measurements->hideOneFqWidget();
        // on_startOneFq() disables these four unconditionally when One Fq
        // mode starts; nothing on any stop path re-enabled them until now
        // -- confirmed 2026-08-20 (stayed disabled after Esc/stop).
        ui->actionExport->setEnabled(true);
        return;
    }

    if (!confirmScanPoints(m_dotsNumber))
        return;

    ui->singleStart->setChecked(true);
    ui->continuousStartBtn->setChecked(false);
    ui->fullBtn->setEnabled(false);
    ui->fullBtn->setChecked(false);

    //if (g_developerMode)
    {
        ui->singleStart->setChecked(true);
        quint64 fqFrom = ui->lineEdit_fqFrom->text().remove(' ').toLongLong();
        quint64 fqTo = ui->lineEdit_fqTo->text().remove(' ').toLongLong();
        bool oneFq = m_isRange ? (fqTo==0) : (fqTo==fqFrom);
        if (oneFq) {
            on_startOneFq(fqFrom, m_dotsNumber, false);
            return;
        }
    }


    double start;
    double stop;

    AnalyzerParameters* param = AnalyzerParameters::current();
    qint64 minFreq = param == nullptr ? 100 : param->minFq().toULongLong();
    qint64 maxFreq = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong();

    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            QString strMin = ca->minFq();
            minFreq = strMin.toULongLong();
            QString strMax = ca->maxFq();
            maxFreq = strMax.toULongLong();
        }
        getEnteredFq(start, stop);
    } else {
        getEnteredFq(start, stop);
        if (m_fqRestrict)
        {
            AnalyzerParameters::normalizeFq(start, stop);
            if(!m_isRange)
            {
                setFqFrom(start);
                setFqTo(stop);
            }else
            {
                setFqTo((stop-start)/2);
                setFqFrom((stop+start)/2);
            }
        }
    }

    if(m_fqRestrict && (stop > static_cast<double>(maxFreq)))
    {
        stop = maxFreq;
        if(!m_isRange)
        {
            setFqFrom(start);
            setFqTo(stop);
        }else
        {
            setFqTo((stop-start)/2);
            setFqFrom((stop+start)/2);
        }
    }
    if (m_fqRestrict) {
        if((start > static_cast<double>(maxFreq)) || (start < static_cast<double>(minFreq)))
        {
            start = minFreq;
            if(!m_isRange)
            {
                setFqFrom(start);
            }else
            {
                setFqTo((stop-start)/2);
                setFqFrom((stop+start)/2);
            }
        }
    }
    QCPRange range(start, stop);
    m_swrWidget->xAxis->setRange(range);
    m_phaseWidget->xAxis->setRange(range);
    m_rsWidget->xAxis->setRange(range);
    m_rpWidget->xAxis->setRange(range);
    m_rlWidget->xAxis->setRange(range);
    m_s21Widget->xAxis->setRange(range);
#if USER_DEFINED_FEATURE
    m_userWidget->xAxis->setRange(range);
#endif
    m_settings->beginGroup("MainWindow");
    if (!m_isRange) {
        m_settings->setValue("rangeLower", start);
        m_settings->setValue("rangeUpper", stop);
    } else {
        m_settings->setValue("rangeLower", (stop-start)/2);
        m_settings->setValue("rangeUpper", (stop+start)/2);
    }
    m_settings->setValue("dotsNumber", m_dotsNumber);
    m_settings->endGroup();

    // TDR no longer has a special case here -- Single now behaves the same
    // on every tab, including TDR (falls through to the generic emit
    // measure() below). TDR scans are triggered exclusively through
    // TdrScanPanel's own Single/Continuous buttons now (see
    // on_tdrScanRequested()) -- this also retires a real pre-existing unit
    // bug this branch had (ABSOLUTE_MAX_FQ and CustomAnalyzer's minFq()/
    // maxFq() are kHz, used here as if they were already Hz -- 1000x too
    // small whenever no device was connected or Custom Analyzer was
    // active; found 2026-08-21 debugging TdrScanPanel's own frequency-limit
    // wiring, see the tdr-scan-rework-plan memory).
    if(ui->tabWidget->currentWidget()->objectName() == "tab_user")
    {
        emit measureUser(start*1000, stop*1000, m_dotsNumber);
    }
    else if(ui->tabWidget->currentWidget()->objectName() == "tab_s21")
    {
        emit measureS21(start*1000, stop*1000, m_dotsNumber);
    }
    else
    {
        emit measure(start*1000, stop*1000, m_dotsNumber);
    }
    ui->actionExport->setEnabled(true);

    dtStartMeasurement = QDateTime::currentDateTime();
}

void MainWindow::on_continuousStartBtn_clicked(bool checked)
{
    if (isMeasuring())
    {
        m_bInterrupted = true;
        // See on_singleStart_clicked()'s identical call for why.
        m_measurements->interrupt();
        emit stopMeasure();
        ui->singleStart->setChecked(false);
        ui->continuousStartBtn->setChecked(false);
        ui->fullBtn->setEnabled(true);
        ui->fullBtn->setChecked(true);
        m_isContinuos = false;
        m_measurements->setContinuous(false);

        // Same reasoning as on_singleStart_clicked()'s stop path -- One Fq
        // mode isn't gated by g_developerMode, so cleaning it up on stop
        // shouldn't be either.
        m_measurements->hideOneFqWidget();
        ui->actionExport->setEnabled(true);
        return;
    }
    if(ui->tabWidget->currentWidget()->objectName() == "tab_tdr") {
        ui->continuousStartBtn->setChecked(false);
        return;
    }

    if (checked && !confirmScanPoints(m_dotsNumber)) {
        ui->continuousStartBtn->setChecked(false);
        return;
    }

    // Not gated by g_developerMode -- One Fq mode itself isn't (matches
    // on_singleStart_clicked()'s equivalent check). Scoped to `checked`
    // (true only for a genuine user press of Continuous) rather than
    // g_developerMode -- that was the actual bug: on_measurementComplete()
    // calls this function with checked=false to stop One Fq mode after
    // each batch, and this block used to fire on *that* call too (nothing
    // here ever looked at `checked`), silently restarting instead of
    // stopping. With g_developerMode on, that produced a self-sustaining
    // restart loop; with it off, this whole block was skipped, so it fell
    // through to a normal (and useless -- zero-width-range) continuous
    // scan instead. Confirmed 2026-08-20.
    if (checked) {
        quint64 fqFrom = ui->lineEdit_fqFrom->text().remove(' ').toLongLong();
        quint64 fqTo = ui->lineEdit_fqTo->text().remove(' ').toLongLong();
        bool oneFq = m_isRange ? (fqTo==0) : (fqTo==fqFrom);
        if (oneFq) {
            ui->continuousStartBtn->setChecked(true);
            on_startOneFq(fqFrom, m_dotsNumber, true);
            return;
        }
    }

    ui->singleStart->setChecked(false);
    m_isContinuos = checked;
    m_analyzer->setContinuos(m_isContinuos);
    if(m_isContinuos)
    {
        m_bInterrupted = false;
        double start;
        double stop;

        // 20210423
        //start = getFqFrom();
        //stop = getFqTo();
        getEnteredFq(start, stop);

    AnalyzerParameters* param = AnalyzerParameters::current();
    qint64 minFreq = param == nullptr ? 100 : param->minFq().toULongLong();
    qint64 maxFreq = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong();
        if (CustomAnalyzer::customized()) {
            CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
            if (ca != nullptr) {
                minFreq = ca->minFq().toULongLong();
                maxFreq = ca->maxFq().toULongLong();
            }
        } else {
            AnalyzerParameters::normalizeFq(start, stop);
        }

        if(m_fqRestrict && (stop > static_cast<double>(maxFreq)))
        {
            stop = maxFreq;
            if(!m_isRange)
            {
                setFqTo(stop);
            }else
            {
                setFqTo((stop-start)/2);
            }
        }
        if (m_fqRestrict) {
            if((start > static_cast<double>(maxFreq)) || (start < static_cast<double>(minFreq)))
            {
                start = minFreq;
                if(!m_isRange)
                {
                    setFqFrom(start);
                }else
                {
                    setFqFrom((stop+start)/2);
                }
            }
        }
        QCPRange range(start, stop);
        m_swrWidget->xAxis->setRange(range);
        m_phaseWidget->xAxis->setRange(range);
        m_rsWidget->xAxis->setRange(range);
        m_rpWidget->xAxis->setRange(range);
        m_rlWidget->xAxis->setRange(range);
        m_s21Widget->xAxis->setRange(range);
#if USER_DEFINED_FEATURE
        m_userWidget->xAxis->setRange(range);
#endif

        m_settings->beginGroup("MainWindow");
        if (!m_isRange) {
            m_settings->setValue("rangeLower", start);
            m_settings->setValue("rangeUpper", stop);
        } else {
            m_settings->setValue("rangeLower", (stop-start)/2);
            m_settings->setValue("rangeUpper", (stop+start)/2);
        }
        m_settings->setValue("dotsNumber", m_dotsNumber);
        m_settings->endGroup();

        if(ui->tabWidget->currentWidget()->objectName() == "tab_user")
        {
            emit measureUser(start*1000, stop*1000, m_dotsNumber);
        } else {
            emit measure(start*1000, stop*1000, m_dotsNumber);
        }
        ui->actionExport->setEnabled(true);
    }else
    {
        m_bInterrupted = true;
        m_analyzer->setContinuos(false);
    }
    m_measurements->setContinuous(m_isContinuos);
}

void MainWindow::on_startOneFq(quint64 _fq, int _dots, bool _continuous)
{
    // Always requests exactly one point on the wire now (FRX1), looping
    // here instead of asking the device for one big FRX<N> batch -- see
    // on_measurementComplete()'s continuation logic. _dots is the target
    // iteration count for Single mode (ignored, loops forever, for
    // Continuous); m_oneFqRemaining tracks progress toward it across each
    // loop's completion callback. Reused for both the initial trigger and
    // every subsequent loop iteration.
    m_isContinuos = _continuous;
    m_analyzer->setContinuos(m_isContinuos);
    m_measurements->setContinuous(m_isContinuos);
    m_bInterrupted = false;
    m_oneFqFreq = _fq;
    m_oneFqRemaining = _dots;

    emit measureOneFq(this, _fq*1000, 1);

    ui->actionExport->setEnabled(false);
}

// TdrScanPanel::scanRequested() -- see m_isTdrScanning's comment in
// mainwindow.h. No Continuous mode (removed 2026-08-21, see
// tdr-scan-rework-plan memory) -- single-shot only, so nothing here needs
// to remember what to re-request later.
void MainWindow::on_tdrScanRequested(qint64 topFreqKHz, int dots, TdrWindow window, double beta, double velFactor)
{
    if (isMeasuring())
        return; // shouldn't happen -- TdrScanPanel::setScanning() disables the button/re-entry, but don't rely on that alone

    dots = qBound((int)TDR_MINPOINTS, dots, (int)TDR_MAXPOINTS);

    m_measurements->setTdrWindowType(window);
    m_measurements->setTdrKaiserBeta(beta);

    // velFactor now genuinely drives this scan's own distance calculation
    // (merged in from TDRAnalysisDialog 2026-08-21 -- see
    // tdr-scan-rework-plan memory for why this is scoped/restored rather
    // than just left applied: Measurements::m_cableVelFactor is also read
    // by feedline-loss compensation elsewhere, measurements_farend.cpp, so
    // leaving it overridden would silently disturb that). Restored in
    // on_measurementComplete()'s m_isTdrScanning branch right after
    // CalcTdr()/redrawTDR() finish using it -- safe as a plain save/apply/
    // restore now that TDR has no Continuous mode (single, deterministic
    // completion, nothing else running in between).
    m_tdrSavedVelFactor = m_measurements->cableVelFactor();
    m_measurements->setCableVelFactor(velFactor);

    m_isTdrScanning = true;
    m_bInterrupted = false;

    // Same min-frequency logic setFrequencyLimits() callers use (see
    // MainWindow::on_actionTDRMeasurement_triggered()) -- TDR always starts
    // near DC (see CalcTdr()'s own "Wrong fq" guard), so only the top
    // frequency is ever user-adjustable; the bottom always comes from the
    // device/Custom Analyzer's own real minimum.
    AnalyzerParameters* param = AnalyzerParameters::current();
    qint64 minFqKHz = param == nullptr ? 100 : param->minFq().toULongLong();
    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr)
            minFqKHz = ca->minFq().toULongLong();
    }

    // Everything below sets up "scan in progress" state and MUST run
    // before emit measure() -- measure()/measurementComplete() are plain
    // (non-queued) same-thread connections (see mainwindow.cpp's connect()
    // calls -- Qt::QueuedConnection is explicitly commented out there), so
    // when there's no device to actually start an async scan,
    // AnalyzerPro::on_measure() falls straight through to on_stopMeasure(),
    // which emits measurementComplete() *synchronously, reentrantly*,
    // before emit measure() below even returns -- MainWindow::
    // on_measurementComplete()'s m_isTdrScanning branch runs and finalizes
    // (resets m_isTdrScanning, re-enables the panel) *while this function
    // is still on the call stack*. Setting up the progress dialog/disabled
    // controls *after* emit measure() (as this originally did) meant that
    // reentrant finalize ran first, and the progress dialog got shown
    // *afterward* with nothing left to ever close it -- confirmed
    // 2026-08-21 (had to kill the process; Cancel/Esc did nothing because
    // m_isMeasuring was already false by the time either fired). Doing the
    // setup first means the reentrant case finalizes it correctly instead.
    m_measurements->startTDRProgress(this, dots);
    if (m_tdrScanDialog != nullptr)
        m_tdrScanDialog->panel()->setScanning(true);

    ui->actionExport->setEnabled(false);

    emit measure(minFqKHz*1000, topFreqKHz*1000, dots);
}

// TdrScanDialog::closing() -- see mainwindow.h's comment. No-op if nothing's
// actually running (the dialog can close any time, scanning or not).
void MainWindow::on_tdrStopRequested()
{
    if (!isMeasuring())
        return;
    m_bInterrupted = true;
    emit stopMeasure();
    if (m_tdrScanDialog != nullptr)
        m_tdrScanDialog->panel()->setScanning(false);
    ui->actionExport->setEnabled(true);
}

// AnalyzerPro::drainingChanged() -- see AnalyzerPro::m_isDraining's own
// comment. Disabling every scan-triggering control here (not just the one
// the user actually clicked) is deliberate: a second command landing on
// the wire while the device is still mid-drain is exactly the kind of
// desync that produced real protocol garbage earlier in this project's
// history (a mismatched device profile mid-stream).
void MainWindow::onAnalyzerDrainingChanged(bool draining)
{
    ui->singleStart->setEnabled(!draining);
    ui->continuousStartBtn->setEnabled(!draining);
    ui->fullBtn->setEnabled(!draining);
    if (m_tdrScanDialog != nullptr)
        m_tdrScanDialog->panel()->setScanning(draining);
}

void MainWindow::onAnalyzerStatusMessageChanged(const QString& text)
{
    m_statusLabel->setText(text);
}

void MainWindow::on_measurementComplete()
{
    // A stitched (multi-segment) sweep's analyzer backend fires this exact
    // signal once per individual segment on its way to being stitched into
    // one continuous result, not just on the truly final one -- without
    // this guard, the *first* segment's completion alone would run every
    // finalize step below (UI resets, setIsMeasuring(false), etc.), and
    // AnalyzerPro::on_newData()'s own "!m_isMeasuring -> ignore as stale
    // leftover data" guard would then silently discard every subsequent
    // segment's points for the rest of the sweep. Confirmed live
    // 2026-09-03: a NanoVNA V2 sweep split across two stitched segments
    // only ever showed the first segment's data on the chart. See
    // AnalyzerPro::isStitchedSweepComplete()'s own comment.
    if (m_analyzer != nullptr && !m_analyzer->isStitchedSweepComplete())
        return;

    // TdrScanPanel-triggered scan -- see m_isTdrScanning's comment in
    // mainwindow.h for why this can't be inferred from the current tab. No
    // Continuous mode (removed 2026-08-21) -- every TDR scan finalizes
    // here, nothing to re-trigger.
    //
    // Deliberately checked *before* the NANO early-return below: a NanoVNA-
    // type connection still emits this exact (non-Nano) measurementComplete()
    // signal when a scan is stopped early -- AnalyzerPro::on_stopMeasure()'s
    // "if (wasMeasuring) emit measurementComplete();" -- and a NanoVNA scan's
    // own normal completion (on_measurementCompleteNano()) never touches
    // m_isTdrScanning at all. If this ran after the NANO check, a TDR scan
    // against a NanoVNA device could never finalize either way: the progress
    // dialog (frameless, window-modal, Esc deliberately disabled -- see
    // ProgressDlg::reject()) and the panel's Scan button would both stay
    // stuck with no UI path left to recover. Confirmed live 2026-09-03
    // against the NanoVNA emulator -- had to kill the process.
    if (m_isTdrScanning) {
        m_measurements->stopTDRProgress();
        m_measurements->on_measurementComplete();
        m_isTdrScanning = false;
        m_bInterrupted = true;
        m_analyzer->setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
        if (m_tdrScanDialog != nullptr)
            m_tdrScanDialog->panel()->setScanning(false);
        ui->actionExport->setEnabled(true);
        // Restore the velocity factor on_tdrScanRequested() overrode --
        // deferred, not done right here, because TdrScanPanel::refreshResult()
        // is *also* connected to this same measurementComplete() signal
        // (connected after this slot, so it runs after this returns) and
        // needs cableVelFactor() to still read the just-used override when
        // it computes the fresh peak's rescale ratio (see
        // Measurements::findTdrPeak()'s comment) -- restoring synchronously
        // here would make that ratio wrong for the scan that just ran.
        // QTimer::singleShot(0, ...) runs after every same-signal listener
        // (this slot and refreshResult() both) has already finished, so it
        // sees the right value at the right time without needing to know
        // what else is listening.
        {
            double savedVf = m_tdrSavedVelFactor;
            QTimer::singleShot(0, this, [this, savedVf]() {
                m_measurements->setCableVelFactor(savedVf);
            });
        }
        return;
    }

    // NanoVNA-family (both classic ASCII and V2/binary) finalizes through
    // on_measurementCompleteNano() instead, driven by the backend's own
    // completeMeasurement() signal -- see AnalyzerPro::connectSignals()'s
    // completeMeasurement lambda. This early-return only ever checked NANO,
    // not NANOV2 (added later as its own enum value, see redeviceinfo.h),
    // so a V2/binary scan fell through all the way to this function's own
    // autoPlaceAtLowestSwr() call below *and* on_measurementCompleteNano()'s
    // -- two markers placed per scan instead of one. Classic ASCII never
    // showed this because it actually matched NANO and returned here.
    // Confirmed live 2026-09-04 against the NanoVNA emulator's binary
    // profile.
    if (m_analyzer->connectionType() == ReDeviceInfo::NANO ||
        m_analyzer->connectionType() == ReDeviceInfo::NANOV2)
        return;
    // One Fq mode (Start==Stop or Range==0) isn't gated by g_developerMode
    // -- it's reachable in the shipped build regardless. Every wire request
    // is a single FRX1 now (see on_startOneFq()), so "one batch" here means
    // one point -- looping (Single: m_oneFqRemaining more times; Continuous:
    // forever) happens app-side by re-triggering on_startOneFq() from here,
    // not by asking the device for a bigger batch.
    //
    // m_measurements.last() note (why this can't just fall through to the
    // normal Continuous-scan completion path below): One Fq's on_newData()
    // never adds anything to m_measurements (it short-circuits straight to
    // updateOneFqWidget()), so Measurements::on_continueMeasurement()'s
    // m_measurements.last() would assert on an empty list. Confirmed via
    // coredumpctl/gdb backtrace, 2026-08-20.
    if (m_measurements->isOneFqMode()) {
        if (!m_bInterrupted && (m_isContinuos || m_oneFqRemaining > 1)) {
            int remaining = m_isContinuos ? 0 : m_oneFqRemaining - 1;
            on_startOneFq(m_oneFqFreq, remaining, m_isContinuos);
            return;
        }
        on_continuousStartBtn_clicked(false);
        return;
    }

//{ TODO should be checked for autoclibration
    int autoCalibration = m_measurements->getAutoCalibration();
    if (autoCalibration != 0) {
        m_measurements->stopAutocalibrateProgress();
        autoCalibrate();
    } else {
        m_measurements->stopTDRProgress();
    }
//}

    m_tdrWidget->xAxis->setRangeLower(0);

    QTimer::singleShot(5, m_markers, SLOT(redraw()));
    if(m_isContinuos)
    {
        ui->singleStart->setChecked(false);

        double start;
        double stop;

        // 20210423
        //start = getFqFrom();
        //stop = getFqTo();
        getEnteredFq(start, stop);

    AnalyzerParameters* param = AnalyzerParameters::current();
    qint64 minFreq = param == nullptr ? 100 : param->minFq().toULongLong();
    qint64 maxFreq = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong();
        if (CustomAnalyzer::customized()) {
            CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
            if (ca != nullptr) {
                minFreq = ca->minFq().toULongLong();
                maxFreq = ca->maxFq().toULongLong();
            }
        } else {
            AnalyzerParameters::normalizeFq(start, stop);
        }
        if(m_fqRestrict && (stop > static_cast<double>(maxFreq)))
        {
            stop = maxFreq;
            if(!m_isRange)
            {
                setFqTo(maxFreq);
            }else
            {
                setFqTo((stop-start)/2);
            }
        }
        if(m_fqRestrict) {
            if((start > static_cast<double>(maxFreq)) || (start < static_cast<double>(minFreq)))
            {
                start = minFreq;
                if(!m_isRange)
                {
                    setFqFrom(start);
                }else
                {
                    setFqFrom((stop+start)/2);
                }
            }
        }
        QCPRange range(start, stop);
        m_swrWidget->xAxis->setRange(range);
        m_phaseWidget->xAxis->setRange(range);
        m_rsWidget->xAxis->setRange(range);
        m_rpWidget->xAxis->setRange(range);
        m_rlWidget->xAxis->setRange(range);
        m_s21Widget->xAxis->setRange(range);
#if USER_DEFINED_FEATURE
        m_userWidget->xAxis->setRange(range);
#endif
        if (!m_bInterrupted)
        {
            emit measureContinuous(start*1000, stop*1000, m_dotsNumber);
        } else {
            m_bInterrupted = true;
            // Was missing -- unlike the Single-scan branch just below (see
            // its own identical call), this path never told Measurements
            // the scan was actually done, so the Points column kept
            // showing whatever count it had at the last mid-scan redraw
            // instead of the real final one. Only a *later* scan's own
            // fresh table rebuild (on_newMeasurement()) happened to paper
            // over it. Not autoPlaceAtLowestSwr() -- that's deliberately
            // single/full-scan only, see its own comment.
            m_measurements->on_measurementComplete();
            m_analyzer->setContinuos(false);
            m_analyzer->setIsMeasuring(false);
            PopUpIndicator::setIndicatorVisible(false);
            ui->continuousStartBtn->setChecked(false);
        }
        m_measurements->setContinuous(m_isContinuos);
    } else {
        ui->singleStart->setChecked(false);
        ui->continuousStartBtn->setChecked(false);
        ui->fullBtn->setEnabled(true);
        ui->fullBtn->setChecked(true);
        m_measurements->on_measurementComplete();
        m_markers->autoPlaceAtLowestSwr();
        m_bInterrupted = true;
        ui->actionExport->setEnabled(true);
        m_analyzer->setContinuos(false);
        m_analyzer->setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
    }

    // Fixes issue #33: Measurements::replot() only replots m_currentTab
    // while a scan streams in, so every other tab's QCustomPlot keeps a
    // stale pixel<->coordinate mapping until something replots it -- the
    // "brief params under cursor" hint would compute garbage (or nothing)
    // on those tabs until the user's own mouse movement there happened to
    // trigger a replot. Catch every tab up once here, now that the scan is
    // fully done.
    foreach (QCustomPlot *plot, m_mapWidgets) {
        // graph(0) on every tab is the live "current scan position" tick
        // (setWidgetsSettings()'s white QPen(255,255,255,150); fed via
        // setData(x,y) as each point streams in -- see the comment on
        // Measurements::replot() for the XOR-buffer mechanism). Nothing
        // ever cleared it once a scan finished, so it stayed drawn at
        // wherever the last point was.
        if (plot->graphCount() > 0) {
            plot->graph(0)->data()->clear();
        }
        plot->replot();
    }
}

void MainWindow::on_measurementCompleteNano()
{
    // See the identical guard/comment at the top of on_measurementComplete()
    // -- same reasoning, this is the *other* completion signal a stitched
    // NanoVNA-family sweep's intermediate segments fire on their way
    // through.
    if (m_analyzer != nullptr && !m_analyzer->isStitchedSweepComplete())
        return;

    // TdrScanPanel-triggered scan -- see the matching comment and TDR
    // finalize block in on_measurementComplete(). That function's own
    // m_isTdrScanning check only ever fires when Stop is clicked mid-scan
    // (AnalyzerPro::on_stopMeasure()'s synthesized measurementComplete());
    // a TDR scan against a NanoVNA device that completes *normally* comes
    // through here instead (measurementCompleteNano(), not
    // measurementComplete()), so this has to be duplicated rather than
    // shared -- same reason the graph(0)-clear fix a few lines down is
    // duplicated too.
    if (m_isTdrScanning) {
        m_measurements->stopTDRProgress();
        m_measurements->on_measurementComplete();
        m_isTdrScanning = false;
        m_bInterrupted = true;
        m_analyzer->setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
        if (m_tdrScanDialog != nullptr)
            m_tdrScanDialog->panel()->setScanning(false);
        ui->actionExport->setEnabled(true);
        // See the identical restore-after-listeners comment in
        // on_measurementComplete()'s own TDR block.
        {
            double savedVf = m_tdrSavedVelFactor;
            QTimer::singleShot(0, this, [this, savedVf]() {
                m_measurements->setCableVelFactor(savedVf);
            });
        }
        return;
    }

//{ TODO should be checked for autoclibration
    int autoCalibration = m_measurements->getAutoCalibration();
    if (autoCalibration != 0) {
        m_measurements->stopAutocalibrateProgress();
        autoCalibrate();
    } else {
        m_measurements->stopTDRProgress();
    }
//}
    m_tdrWidget->xAxis->setRangeLower(0);
    QTimer::singleShot(5, m_markers, SLOT(redraw()));
    if(m_isContinuos)
    {
        ui->singleStart->setChecked(false);
        if (!m_bInterrupted)
        {
            // frequencies were saved by nanjkAnalyzer
            emit measureContinuous(0, 0, m_dotsNumber);
        } else {
            m_bInterrupted = true;
            // See the identical fix/comment in on_measurementComplete()'s
            // own Continuous-interrupted branch.
            m_measurements->on_measurementComplete();
            m_analyzer->setContinuos(false);
            m_analyzer->setIsMeasuring(false);
            PopUpIndicator::setIndicatorVisible(false);
            ui->continuousStartBtn->setChecked(false);
        }
        m_measurements->setContinuous(m_isContinuos);
    } else { // single mode
        ui->singleStart->setChecked(false);
        ui->continuousStartBtn->setChecked(false);
        m_measurements->on_measurementComplete();
        m_markers->autoPlaceAtLowestSwr();
        m_bInterrupted = true;
        ui->actionExport->setEnabled(true);
        m_analyzer->setContinuos(false);
        m_analyzer->setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
    }

    // Fixes issue #33: see the matching comment in on_measurementComplete().
    // This is the NANO analyzer's own end-of-scan path -- on_measurementComplete()
    // returns immediately for NANO connections (see its own early return
    // above) and never reaches its graph(0) clear, so that fix has to be
    // duplicated here rather than shared.
    foreach (QCustomPlot *plot, m_mapWidgets) {
        if (plot->graphCount() > 0) {
            plot->graph(0)->data()->clear();
        }
        plot->replot();
    }
}

void MainWindow::updateGraph ()
{
    QCustomPlot* plot = nullptr;
    try {
        QString str = ui->tabWidget->currentWidget()->objectName();
        if( str == "tab_swr")
        {
            plot = m_swrWidget;
        }else if(str == "tab_phase")
        {
            plot = m_phaseWidget;
        }else if(str == "tab_rs")
        {
            plot = m_rsWidget;
        }else if(str == "tab_rp")
        {
            plot = m_rpWidget;
        }else if(str == "tab_rl")
        {
            plot = m_rlWidget;
        }else if(str == "tab_s21")
        {
            plot = m_s21Widget;
        }else if(str == "tab_tdr")
        {
            plot = m_tdrWidget;
        }else if(str == "tab_smith")
        {
            resizeWnd();
            plot = m_smithWidget;
        }else if(str == "tab_user")
        {
            plot = m_userWidget;
        }
#ifndef NO_MULTITAB
        else if(str == "tab_multi") {
            for (int idx=0; idx<m_multiTabData.tabs.size(); idx++) {
                QString tab_name = m_multiTabData.tabs[idx];
                QString plot_name = g_mapTabPlotNames[tab_name];
                m_mapWidgets[plot_name]->replot();
            }
            return;
        }
#endif
    } catch(...) {
        return;
    }
    if (plot != nullptr)
        plot->replot();
}

void MainWindow::on_1secTimerTick()
{
    QString str = ui->tabWidget->currentWidget()->objectName();
    if(str == "tab_tdr" || str == "tab_smith")
    {
        m_measurements->hideGraphBriefHint();
        return;
    }
    // Was manually re-deriving m_swrWidget's global bounding box by adding
    // this->geometry() + ui->tabWidget's offset + m_swrWidget's offset --
    // which skips the offset of m_swrWidget's own parent tab page *within*
    // tabWidget (the tab bar's height and the tab widget's frame border).
    // That missing offset shifted the computed box's top edge up into where
    // the tab bar actually is, so the cursor still read as "inside" while
    // hovering the tab bar itself -- the hint would never be told to hide
    // while the mouse was up there, including while clicking a tab (issue:
    // tab clicks sometimes not registering, worst on TDR/Smith since they
    // also force hideGraphBriefHint() above, adding more show/hide churn
    // right at the tab bar). mapToGlobal() walks the full parent chain
    // correctly instead of re-deriving it by hand.
    QRect plotRect(m_swrWidget->mapToGlobal(QPoint(0, 0)), m_swrWidget->size());
    if (plotRect.contains(QCursor::pos()))
    {
        m_measurements->showHideHints();
    }else
    {
        m_measurements->hideGraphBriefHint();
    }
}

void MainWindow::on_presssCtrlAltShiftM()
{
#if CALIBRATION_DEBUG_TOOLS
    if (!ui->singleStart->isEnabled())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    clearAllMeasurements();

    m_measurements->setAutoCalibration(1);

    QString cmd = "cals\r";
    if (!m_analyzer->sendCommand(cmd)) {
        return;
    }
    QCoreApplication::processEvents();
    QThread::sleep(2);

    cmd = "calt\r";
    if (!m_analyzer->sendCommand(cmd)) {
        return;
    }
    QCoreApplication::processEvents();
    QThread::sleep(2);

    m_measurements->setFarEndMeasurement(0);
    onFullRange(true);
    m_dotsNumber = 200;
    // QString style = "QPushButton:checked{"
    //         "background-color: rgb(255, 1, 52);}";
    // ui->singleStart->setStyleSheet(style);

    on_singleStart_clicked();
    QApplication::processEvents();
#endif
}

void MainWindow::autoCalibrate()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QPair<double, double> calibr = m_measurements->autoCalibrate(); // <CableResistance, CableLength>
    QString cmd = QString("calrl%1,%2\r")
            .arg((double)calibr.first, 0, 'f', 8, QLatin1Char(' '))
            .arg((double)calibr.second, 0, 'f', 8, QLatin1Char(' '));
    m_analyzer->sendCommand(cmd);

    // QString style = "QPushButton:checked{"
    //         "background-color: rgb(0, 178, 90);}";
    // ui->singleStart->setStyleSheet(style);
    QApplication::restoreOverrideCursor();

    QString notify = QString("Autocalibration: CableResistance=%1, CableLength=%2")
            .arg((double)calibr.first, 0, 'f', 8, QLatin1Char(' '))
            .arg((double)calibr.second, 0, 'f', 8, QLatin1Char(' '));
    QRect rn(0, 0, rect().width(), 40);
    Notification::showMessage(notify, QColor(Qt::white), rn, 5000, ui->tabWidget->currentWidget());
    return;

}

void MainWindow::onMeasurementError()
{
    QApplication::beep();
    //showErrorPopup(tr("Measurement ERROR!"), 2000);
    on_pressEsc();
}

// Was a Notification::showMessage() banner -- fixed geometry (a 40px-tall
// strip sized for one-line transient confirmations like the autocalibration
// notice, see on_autocalibrateClick()), which clipped anything sentence-
// length, and auto-dismissed after 5s whether the user had read it or not.
// A real message a user needs to act on (check the cable, check whether
// something else has the device open) gets a real, standard-looking error
// dialog instead -- QMessageBox, shown non-modally (show(), not exec()) so
// it doesn't block the rest of the app, dismissed by the user's own OK
// click rather than a timer. WA_DeleteOnClose + the QPointer in
// m_analyzerErrorBox mean a second error arriving while one's already up
// (repeated watchdog fires, several stitched segments timing out in a row)
// updates and re-raises the same box instead of stacking duplicates.
void MainWindow::onAnalyzerError(const QString& error)
{
    // Plain "YYYYMMDD-HHMMSS: " prefix -- not tied to any locale/date
    // format setting, since this is meant to line up with Debug-yyyyMMdd.log
    // filenames/entries (debuglog.cpp) if this ever grows into writing these
    // to that same log too (not done yet -- out of scope for now, this is
    // just groundwork so the timestamp's already in the right shape).
    QString stamped = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ": " + error;

    if (m_analyzerErrorBox) {
        m_analyzerErrorBox->setText(stamped);
        m_analyzerErrorBox->raise();
        m_analyzerErrorBox->activateWindow();
        return;
    }

    QMessageBox* box = new QMessageBox(QMessageBox::Warning, tr("Analyzer Error"),
                                        stamped, QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowModality(Qt::NonModal);
    m_analyzerErrorBox = box;
    box->show();
}

void MainWindow::on_presssCtrlAltShiftN()
{
#if CALIBRATION_DEBUG_TOOLS
    if (!ui->singleStart->isEnabled())
        return;

    connect(m_analyzer, &AnalyzerPro::updateAutocalibrate5, this, [this](int _dots, QString _msg){
        if (_msg.contains("START")) {
            m_measurements->startAutocalibrateProgress(this, _dots);
            m_measurements->progressDlg()->updateActionInfo("Adjustment of signal scaling factor");
            m_measurements->progressDlg()->setCancelable(false);
            m_measurements->progressDlg()->setValue(0);
        } else {
            int _max = m_measurements->progressDlg()->maxValue();
            m_measurements->progressDlg()->setValue(_max - _dots);
            m_measurements->progressDlg()->updateStatusInfo(QString(tr("Remains %1").arg(_dots)));
        }
    });
    QObject::connect(m_analyzer, &AnalyzerPro::stopAutocalibrate5, this, [this]() {
        QObject::disconnect(m_analyzer, &AnalyzerPro::stopAutocalibrate5, this, nullptr);
        QObject::disconnect(m_analyzer, &AnalyzerPro::updateAutocalibrate5, this, nullptr);
        m_measurements->stopAutocalibrateProgress();
    });

    m_analyzer->setParseState(WAIT_CALFIVEKOHM_START);
    m_analyzer->sendCommand("CALFIVEKOHM\r");
#endif
}



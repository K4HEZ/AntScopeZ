#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "analyzer/customanalyzer.h"

// Tools menu (mainwindow.ui's menuTools) lives here, matching this
// project's existing tier-1 split of mainwindow.cpp by feature area.

void MainWindow::on_actionMarkerComparison_triggered()
{
    if (m_markerComparisonDialog == nullptr) {
        m_markerComparisonDialog = new MarkerComparisonDialog(m_markers, m_measurements, m_measureSystemMetric, this);
        m_markerComparisonDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_markerComparisonDialog, &QObject::destroyed, this, [this](){
            m_markerComparisonDialog = nullptr;
        });
        // Tracks a live Continuous scan the same way the chart/markers
        // themselves do -- see MarkerComparisonDialog::refresh(). Torn down
        // automatically when m_markerComparisonDialog is destroyed
        // (WA_DeleteOnClose above), no manual disconnect needed.
        connect(m_analyzer, &AnalyzerPro::measurementComplete,
                m_markerComparisonDialog, &MarkerComparisonDialog::refresh);
        // NanoVNA connections never fire measurementComplete() at a real
        // completion (see the identical fix/comment for TdrScanPanel::
        // refreshResult() further down in this file, on_actionTDRMeasurement_
        // triggered()) -- without this, the combos/values here never updated
        // live for a NanoVNA scan while this dialog was open. Fixes #39.
        connect(m_analyzer, &AnalyzerPro::measurementCompleteNano,
                m_markerComparisonDialog, &MarkerComparisonDialog::refresh);
        // A marker added/removed on the plots doesn't produce a new sweep
        // (no measurementComplete above), so the combos need their own hook
        // to pick it up without the user having to reopen this dialog.
        connect(m_markers, &Markers::markersChanged,
                m_markerComparisonDialog, &MarkerComparisonDialog::refresh);
    }
    m_markerComparisonDialog->refresh();
    if (!m_markerComparisonDialog->isVisible())
        m_markerComparisonDialog->show();
    m_markerComparisonDialog->raise();
    m_markerComparisonDialog->activateWindow();
}

// Tools > TDR Measurement -- covers both scan setup and (merged in from the
// now-retired TDRAnalysisDialog, 2026-08-21) post-scan peak analysis. Same
// non-modal, single-instance tool-dialog shape as MarkerComparisonDialog.
void MainWindow::on_actionTDRMeasurement_triggered()
{
    if (m_tdrScanDialog == nullptr) {
        m_tdrScanDialog = new TdrScanDialog(this);
        m_tdrScanDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_tdrScanDialog, &QObject::destroyed, this, [this](){
            m_tdrScanDialog = nullptr;
        });
        connect(m_tdrScanDialog->panel(), &TdrScanPanel::scanRequested,
                this, &MainWindow::on_tdrScanRequested);
        // Safety net, not the panel's own Stop button (there isn't one --
        // no Continuous mode, see TdrScanPanel's class comment) -- makes
        // sure closing this dialog mid-scan (Esc, the window's close
        // button) actually stops the scan instead of leaving it running
        // headless with no UI left to stop it (confirmed 2026-08-21: Esc
        // hid the dialog but a no-device scan kept looping, flooding the
        // measurement list).
        connect(m_tdrScanDialog, &TdrScanDialog::closing,
                this, &MainWindow::on_tdrStopRequested);
        // Live, no rescan -- windowChanged() only re-plots already-captured
        // data with the newly chosen window (see
        // Measurements::setTdrWindowType()'s comment). Harmless/no-op if
        // nothing's been scanned yet (redrawTDR() tolerates that, see its
        // own guards). resetRange=false -- window choice never changes
        // m_tdrRange (only dots/bandwidth/velocity factor do), so there's
        // no reason to snap back to the full range and undo whatever the
        // user had zoomed/panned to (reported 2026-08-21: rescaled to the
        // full ~325ft range on every window change).
        connect(m_tdrScanDialog->panel(), &TdrScanPanel::windowChanged,
                this, [this](TdrWindow window, double beta) {
            m_measurements->setTdrWindowType(window);
            m_measurements->setTdrKaiserBeta(beta);
            m_measurements->redrawTDR(-1, false);
        });
        // Recomputes the Result groupbox (peak distance/reflection type/
        // reverse-solve) opportunistically after *any* scan, not just ones
        // triggered from this panel -- same broad refresh
        // TDRAnalysisDialog::refresh() used to do. MainWindow's own
        // on_measurementComplete()/on_measurementCompleteNano() (connected
        // in MainWindow's constructor, long before this dialog ever exists)
        // always run first and are what actually populate the TDR data this
        // reads, same ordering guarantee TDRAnalysisDialog::refresh() relied
        // on. Both signals, not just measurementComplete() -- a NanoVNA-type
        // connection's *normal* scan completion only ever emits
        // measurementCompleteNano() (see AnalyzerPro's own connectSignals()
        // lambda); measurementComplete() there only fires if a scan was
        // stopped early. Without this second connection, the Result
        // groupbox never refreshed after a normally-completed NanoVNA TDR
        // scan -- only after one that was manually stopped.
        connect(m_analyzer, &AnalyzerPro::measurementComplete,
                m_tdrScanDialog->panel(), &TdrScanPanel::refreshResult);
        // NanoVNA connections never fire measurementComplete() at a real
        // completion (MainWindow::on_measurementComplete() early-returns for
        // NANO; the actual signal is measurementCompleteNano(), driven by
        // the device's own "ch>" prompt) -- without this, refreshResult()
        // was never invoked after a NanoVNA TDR scan, so the Result groupbox
        // (peak distance/reflection type) stayed blank even though
        // stopTDRProgress()'s own redrawTDR() call correctly populated the
        // chart itself. Confirmed issue #30 (follow-up finding), 2026-09-05.
        // Same "connected after MainWindow's own on_measurementCompleteNano()
        // slot" ordering guarantee as the measurementComplete connection
        // above -- MainWindow's constructor connects its own slot to this
        // signal long before m_tdrScanDialog exists.
        connect(m_analyzer, &AnalyzerPro::measurementCompleteNano,
                m_tdrScanDialog->panel(), &TdrScanPanel::refreshResult);
        // "Use this velocity factor" (reverse-solve) -- apply it as
        // Settings > Cable's Custom velocity factor (never Preset -- a
        // reverse-solved value doesn't correspond to any named cable), and
        // refresh the TDR chart's own distance axis immediately rather than
        // waiting for the next scan. If Settings happens to already be
        // open, push the same values into it live too (safe now that
        // m_settingsDialog always gets reliably nulled on close -- see the
        // destroyed() connect in on_actionSettings_triggered()) instead of
        // leaving it showing stale numbers until closed and reopened.
        // Moved here verbatim from the now-retired TDRAnalysisDialog.
        //
        // R0/conductive+dielectric loss/units/frequency reset to the same
        // "Ideal 50-Ohm cable" convention Settings > Cable's own built-in
        // presets use (see cables.txt) rather than being left alone --
        // leaving them alone would silently keep whichever *other* cable's
        // real R0/loss figures happened to be showing from the last
        // Preset selected, which reads as "known" but is actually a
        // mismatched guess. TDR only ever solves for velocity factor, so
        // everything else genuinely is unknown here; 50 Ohm/no loss is an
        // honest "not modeled" default instead of a misleading borrowed one.
        connect(m_tdrScanDialog->panel(), &TdrScanPanel::applyVelocityFactorAsCustom,
                this, [this](double vf) {
            m_cableVelFactor = vf;
            m_cableResistance = 50.0;
            m_cableLossConductive = 0.0;
            m_cableLossDielectric = 0.0;
            m_cableLossUnits = 0;
            m_cableLossAtAnyFq = true;
            m_cableIsPreset = false;
            m_measurements->setCableVelFactor(vf);
            m_measurements->setCableResistance(m_cableResistance);
            m_measurements->setCableLossConductive(m_cableLossConductive);
            m_measurements->setCableLossDielectric(m_cableLossDielectric);
            m_measurements->setCableLossUnits(m_cableLossUnits);
            m_measurements->setCableLossAtAnyFq(m_cableLossAtAnyFq);
            m_measurements->redrawTDR();

            if (m_settingsDialog != nullptr) {
                m_settingsDialog->setCableVelFactor(m_cableVelFactor);
                m_settingsDialog->setCableResistance(m_cableResistance);
                m_settingsDialog->setCableLossConductive(m_cableLossConductive);
                m_settingsDialog->setCableLossDielectric(m_cableLossDielectric);
                m_settingsDialog->setCableLossUnits(m_cableLossUnits);
                m_settingsDialog->setCableLossAtAnyFq(m_cableLossAtAnyFq);
                // Last -- switching to Custom re-enables/unlocks the fields
                // (see Settings::updateCableEditability()), so the values
                // above need to already be in place first.
                m_settingsDialog->setCableIsPreset(false);
            }
        });
        m_tdrScanDialog->panel()->setMeasurements(m_measurements);
        // Seeds the Single button's enabled state for whatever the
        // connection already is (this dialog can be opened well after
        // connect/disconnect happened) -- see setConnected()'s comment.
        // Kept in sync afterward by on_analyzerNameFound()/
        // on_deviceDisconnected().
        m_tdrScanDialog->panel()->setConnected(m_analyzerConnected);
    }

    // AnalyzerParameters::minFq()/maxFq(), CustomAnalyzer::minFq()/maxFq(),
    // and ABSOLUTE_MAX_FQ are all already kHz (see ABSOLUTE_MAX_FQ's own
    // comment in mainwindow_frequency.cpp, and Settings copying param's
    // minFq()/maxFq() straight into the Custom Analyzer fields with no
    // scaling) -- matching TdrScanPanel's own kHz convention (see
    // mainwindow.ui's "Frequency, kHz" label) directly, no *1000/1000
    // conversion needed here. (The now-retired TDR branch of
    // on_singleStart_clicked() this was originally modeled on multiplied
    // AnalyzerParameters' value by 1000 to feed emit measure(), which wants
    // Hz -- a different consumer with a different unit; don't reuse that
    // logic verbatim for a kHz-native consumer like this one.)
    AnalyzerParameters* param = AnalyzerParameters::current();
    qint64 minFqKHz = param == nullptr ? 100 : param->minFq().toULongLong();
    qint64 maxFqKHz = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong();
    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            minFqKHz = ca->minFq().toULongLong();
            maxFqKHz = ca->maxFq().toULongLong();
        }
    }
    m_tdrScanDialog->panel()->setFrequencyLimits(minFqKHz, maxFqKHz);
    m_tdrScanDialog->panel()->setVelocityFactor(m_measurements->cableVelFactor());
    m_tdrScanDialog->panel()->setMeasureSystemMetric(m_measureSystemMetric);

    if (!m_tdrScanDialog->isVisible())
        m_tdrScanDialog->show();
    m_tdrScanDialog->raise();
    m_tdrScanDialog->activateWindow();
}

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tdranalysisdialog.h"

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

void MainWindow::on_actionTDRAnalysis_triggered()
{
    if (m_tdrAnalysisDialog == nullptr) {
        m_tdrAnalysisDialog = new TDRAnalysisDialog(m_measurements, m_measureSystemMetric, this);
        m_tdrAnalysisDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_tdrAnalysisDialog, &QObject::destroyed, this, [this](){
            m_tdrAnalysisDialog = nullptr;
        });
        // See TDRAnalysisDialog::refresh()'s own comment for why this
        // ordering (MainWindow::on_measurementComplete(), connected in
        // MainWindow's constructor, always runs first and is what actually
        // populates the TDR data this reads) is safe to rely on.
        connect(m_analyzer, &AnalyzerPro::measurementComplete,
                m_tdrAnalysisDialog, &TDRAnalysisDialog::refresh);
        // "Use this velocity factor" -- apply it as Settings > Cable's
        // Custom velocity factor (never Preset -- a reverse-solved value
        // doesn't correspond to any named cable), and refresh the TDR
        // chart's own distance axis immediately rather than waiting for
        // the next scan. If Settings happens to already be open, push the
        // same values into it live too (safe now that m_settingsDialog
        // always gets reliably nulled on close -- see the destroyed()
        // connect in on_actionSettings_triggered()) instead of leaving it
        // showing stale numbers until closed and reopened.
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
        connect(m_tdrAnalysisDialog, &TDRAnalysisDialog::applyVelocityFactorAsCustom,
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
    }
    m_tdrAnalysisDialog->refresh();
    if (!m_tdrAnalysisDialog->isVisible())
        m_tdrAnalysisDialog->show();
    m_tdrAnalysisDialog->raise();
    m_tdrAnalysisDialog->activateWindow();
}

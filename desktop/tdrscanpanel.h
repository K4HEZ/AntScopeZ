#ifndef TDRSCANPANEL_H
#define TDRSCANPANEL_H

#include <QWidget>
#include "measurements.h"
#include "cablecatalog.h"

namespace Ui {
class TdrScanPanel;
}

// Content widget for the TDR scan-setup rework (cable type/velocity
// factor + top-frequency+points controls, a live unambiguous-range/
// resolution preview, a window-function picker, and -- merged in
// 2026-08-21 from the now-retired TDRAnalysisDialog -- post-scan peak
// analysis and a reverse velocity-factor solve) -- deliberately a plain
// QWidget, not baked into a QDialog, so it can be hosted in a modeless
// TdrScanDialog today and moved into a docked QGroupBox later (mirroring
// the graph-hint-box's own floating->docked history -- see
// MainWindow::setGraphHintWidgets()'s comment) without a rewrite. See the
// tdr-scan-rework-plan memory for the full design and the formulas'
// derivation/verification.
//
// Owns none of the scan-triggering logic itself -- it only ever reports
// what the user asked for via scanRequested()/windowChanged()/
// applyVelocityFactorAsCustom(). The caller (MainWindow) decides what to
// actually do with that.
//
// No Continuous mode (removed 2026-08-21) -- TDR's real use cases are all
// "make a change, then re-scan and check," not "watch it live while
// continuously adjusting" the way SWR tuning is, and every bug found
// during this rework was specific to Continuous (a with-no-device
// reentrant tight loop flooding the measurement list, and closing this
// panel's dialog not actually stopping the background loop). One
// "TDR Scan" button instead.
class TdrScanPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TdrScanPanel(QWidget* parent = nullptr);
    ~TdrScanPanel();

    // Device limits can change between opens (Custom Analyzer toggled,
    // analyzer connected/disconnected) -- call again whenever they might
    // have. minFqKHz is clamped up to TDR_MIN_FREQUENCY regardless of what's
    // passed in (see measurements.h's comment on why TDR needs a floor here).
    void setFrequencyLimits(qint64 minFqKHz, qint64 maxFqKHz);

    // Seeds velocityFactorEdit from Settings > Cable's current value.
    // Unlike before this merge, this field's value now genuinely drives
    // the next TDR scan (see scanRequested()'s comment) -- it's not
    // preview-only anymore. Call each time the panel becomes visible.
    void setVelocityFactor(double vf);
    void setMeasureSystemMetric(bool metric);
    // Needed for the post-scan Result groupbox (findTdrPeak()) and the
    // "Use this velocity factor" apply path. Call once, when the panel/
    // dialog is created.
    void setMeasurements(Measurements* measurements);

    // Disables the controls that would build an inconsistent request
    // mid-scan.
    void setScanning(bool scanning);

    // No analyzer connection -> a scan request would just sit there (or, if
    // the app's own reentrant no-device short-circuit fires -- see
    // MainWindow::on_tdrScanRequested()'s comment -- finalize instantly
    // with nothing real measured, which used to be the only thing stopping
    // the progress dialog and quietly left the Result box showing stale
    // data from whatever was last actually scanned). This app's other scan
    // triggers (singleStart/continuousStartBtn/fullBtn) are disabled the
    // same way in MainWindow::on_analyzerNameFound()/on_deviceDisconnected()
    // -- this panel's button had no equivalent wiring since it's a
    // separately-created, on-demand dialog. Call once at creation (with
    // whatever the connection state already is) and again on every
    // connect/disconnect. Confirmed 2026-08-25.
    void setConnected(bool connected);

public slots:
    // Connect to the analyzer's measurementComplete() the same way
    // TDRAnalysisDialog::refresh() used to be -- recomputes the Result
    // groupbox (peak distance/reflection type/reverse-solve) from
    // whatever TDR data currently exists, opportunistically after *any*
    // scan (not just ones triggered from this panel), same as before the
    // merge. No-op if nothing's been scanned yet.
    void refreshResult();

signals:
    // topFreqKHz/dots/window/beta/velFactor: whatever the controls
    // currently hold. Matches the rest of the app's kHz convention for
    // frequency fields (see mainwindow.ui's "Frequency, kHz" label) -- the
    // caller applies the same *1000-to-Hz conversion on_singleStart_clicked()
    // already does for every other tab before calling emit measure().
    // velFactor now genuinely applies to this scan (temporarily -- see
    // MainWindow::on_tdrScanRequested()'s comment on why that doesn't
    // disturb feedline-loss compensation elsewhere).
    void scanRequested(qint64 topFreqKHz, int dots, TdrWindow window, double beta, double velFactor);
    // Emitted live, on every combobox change -- no scan needed, the trace
    // is recomputed from already-captured data (see
    // Measurements::setTdrWindowType()'s comment).
    void windowChanged(TdrWindow window, double beta);
    // "Use this velocity factor" (reverse-solve) -- see applyVfButton's
    // tooltip. Never emitted just from editing velocityFactorEdit or
    // picking a cableTypeCombo preset, which stay local to this panel.
    void applyVelocityFactorAsCustom(double vf);

private slots:
    void onTopFreqSliderChanged(int value);
    void onTopFreqEditChanged();
    void onDotsSliderChanged(int value);
    void onDotsEditChanged();
    void onVelocityFactorEdited();
    void onWindowComboChanged(int index);
    void onScanClicked();
    void onCableTypeChanged(int index);
    void onApplyVfClicked();

private:
    Ui::TdrScanPanel* ui;
    Measurements* m_measurements = nullptr;
    bool m_measureSystemMetric = true;
    qint64 m_minFqKHz = TDR_MIN_FREQUENCY;
    qint64 m_maxFqKHz = TDR_MIN_FREQUENCY;
    QList<CableSpec> m_cables;
    // Both feed tdrSingleButton's enabled state (updateScanButtonEnabled())
    // -- neither setScanning() nor setConnected() alone should ever
    // re-enable it out from under the other.
    bool m_scanning = false;
    bool m_connected = false;
    // Last value refreshResult() solved for in the reverse-VF calculator --
    // onApplyVfClicked() copies this into velocityFactorEdit rather than
    // re-parsing calculatedVfLabel's formatted text. 0 when nothing valid
    // has been solved yet (see applyVfButton's enabled state, kept in sync).
    double m_lastCalculatedVf = 0;

    void updateEstimateLabels();
    void updateWindowExplanation();
    void updateScanButtonEnabled();
    void populateCableTypeCombo();
    double velocityFactor() const;
    static TdrWindow windowForComboIndex(int index);
};

#endif // TDRSCANPANEL_H

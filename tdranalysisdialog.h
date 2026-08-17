#ifndef TDRANALYSISDIALOG_H
#define TDRANALYSISDIALOG_H

#include <QDialog>
#include <measurements.h>
#include "cablecatalog.h"

namespace Ui {
class TDRAnalysisDialog;
}

// Tools -> TDR Analysis: same non-modal, single-instance tool-dialog shape
// as MarkerComparisonDialog (see MainWindow::on_actionTDRAnalysis_triggered()).
// Reads the most recent TDR scan's already-computed reflection trace
// (Measurements::redrawTDR() -- see measurements_tdr.cpp) and reports the
// distance to its strongest reflection plus whether that reflection looks
// like an open or a short, using a velocity factor kept local to this
// dialog (defaults from, but never writes back to, Settings > Cable) so
// trying a different VF here doesn't disturb feedline-loss compensation
// elsewhere. Also offers the reverse direction: given a known physical
// cable length, solve for the velocity factor that would make the
// displayed length match it.
class TDRAnalysisDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TDRAnalysisDialog(Measurements* measurements, bool measureSystemMetric,
                                QWidget* parent = nullptr);
    ~TDRAnalysisDialog();

signals:
    // Emitted only by onApplyCalculatedVf() (the "Use this velocity
    // factor" button) -- never just from editing velocityFactorEdit or
    // picking a cableTypeCombo preset, which stay local to this dialog.
    // MainWindow applies it as Settings > Cable's Custom velocity factor
    // (see MainWindow::on_actionTDRAnalysis_triggered()) -- a reverse-
    // solved value doesn't correspond to any named preset, so it's always
    // Custom, never an attempt to guess a matching preset.
    void applyVelocityFactorAsCustom(double vf);

public slots:
    // Connected to the analyzer's measurementComplete() the same way
    // MarkerComparisonDialog::refresh() is (see
    // MainWindow::on_actionTDRAnalysis_triggered()) -- MainWindow's own
    // on_measurementComplete() slot is connected first (in its
    // constructor, long before this dialog ever exists) and is what
    // actually calls Measurements::redrawTDR() via stopTDRProgress(), so
    // by the time this fires for the same signal the TDR data is already
    // current. Continuous scans never reach here -- TDR has no Continuous
    // mode (see MainWindow::on_continuousStartBtn_clicked()).
    void refresh();

private slots:
    void recompute();
    void onCableTypeChanged(int index);
    void onApplyCalculatedVf();

private:
    struct Peak {
        bool found = false;
        double distance = 0; // in the dialog's current unit (ft or m)
        double amplitude = 0; // signed -- see Markers/CalcTdr's Gre-based reflection amplitude
        // True if the peak sits in the last ~5% of this scan's unambiguous
        // range -- the real reflection may be farther out than this scan
        // can resolve. See findPeak()'s comment.
        bool nearRangeEdge = false;
    };

    Ui::TDRAnalysisDialog *ui;
    Measurements* m_measurements;
    bool m_measureSystemMetric;
    QList<CableSpec> m_cables;
    // Last value recompute() solved for in the reverse-VF calculator --
    // onApplyCalculatedVf() copies this into velocityFactorEdit rather than
    // re-parsing calculatedVfLabel's formatted text. 0 when nothing valid
    // has been solved yet (see applyVfButton's enabled state, kept in sync).
    double m_lastCalculatedVf = 0;

    void populateCableTypeCombo();
    // Strongest reflection in the most recent TDR scan's trace, in
    // whatever unit the trace itself is stored in (ft or m, matching
    // m_measureSystemMetric -- same choice redrawTDR() makes for the
    // chart). Distance comes back already rescaled from
    // Measurements::cableVelFactor() (whatever velocity factor was active
    // when that scan's data was computed) to whatever this dialog's own
    // velocityFactorEdit currently holds -- see the .cpp for why that
    // rescale is valid without re-running the FFT.
    Peak findPeak() const;
};

#endif // TDRANALYSISDIALOG_H

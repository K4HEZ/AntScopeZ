#ifndef MARKERCOMPARISONDIALOG_H
#define MARKERCOMPARISONDIALOG_H

#include <QDialog>
#include <markers.h>
#include <measurements.h>

namespace Ui {
class MarkerComparisonDialog;
}

// Settings -> Themes-style non-modal, single-instance tool dialog (see
// MainWindow::on_actionMarkerComparison_triggered()). Reads two already-
// placed markers -- "Current (dip)" and "Target (desired)" -- shows the
// difference between them (frequency, SWR, RL, R, X), Q factor and
// equivalent L/C off the Current marker, and estimates how much to trim
// (or add) to a simple antenna to move its resonance from one to the
// other. Everything here is derived math, not a measurement of its own --
// it only reads values Markers/Measurements already have.
class MarkerComparisonDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MarkerComparisonDialog(Markers* markers, Measurements* measurements,
                                     bool measureSystemMetric, QWidget *parent = nullptr);
    ~MarkerComparisonDialog();

public slots:
    // Repopulates the marker combos from Markers' current list (preserving
    // the current selection where still valid) and recomputes every
    // derived field. Connected to the analyzer's measurementComplete()
    // signal (see MainWindow::on_actionMarkerComparison_triggered()) so
    // this dialog tracks a live Continuous scan the same way the chart
    // itself does, not just markers being added/removed.
    void refresh();

private slots:
    void recompute();

private:
    enum AntennaType {
        QuarterWaveVertical,
        HalfWaveDipole,
        FullWave
    };

    // Trim-estimate display unit -- independent of the "Current length"
    // field's unit (which stays tied to the app's global Metric/Imperial
    // setting). A whole antenna's length is naturally feet/meters-scale;
    // a trim amount is naturally a much finer unit (inches/cm) regardless
    // of what unit you happened to enter the length in.
    enum TrimUnit {
        Feet,
        Inches,
        Meters,
        Cm
    };

    struct MarkerReadout {
        bool valid = false;
        double frequency = 0; // kHz
        double swr = 0;
        double rl = 0;
        double r = 0;
        double x = 0;
        double l = 0; // nH
        double c = 0; // pF
    };

    Ui::MarkerComparisonDialog *ui;
    Markers* m_markers;
    Measurements* m_measurements;
    bool m_measureSystemMetric;

    void populateMarkerCombos();
    MarkerReadout readoutForMarker(int markerNumber);
    // 2:1-SWR-crossing bandwidth around centerFq, searched in the most
    // recent measurement's SWR data; returns 0 if no crossing is found on
    // one or both sides (e.g. the whole visible sweep stays under 2:1).
    double qFactorAt(double centerFq);
    // Classic ham rule-of-thumb nominal-length constant (feet, at 1 MHz)
    // for the given antenna type -- see markercomparisondialog.cpp for the
    // actual values and their sourcing/caveats.
    static double nominalLengthConstantFeet(AntennaType type);
    // All trim math is done internally in feet; these convert to/describe
    // whatever TrimUnit the trimUnitsCombo has selected for display.
    static double feetToTrimUnit(double feet, TrimUnit unit);
    static QString trimUnitSuffix(TrimUnit unit);
};

#endif // MARKERCOMPARISONDIALOG_H

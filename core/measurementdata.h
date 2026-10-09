#ifndef MEASUREMENTDATA_H
#define MEASUREMENTDATA_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <analyzer/analyzerparameters.h>
#include "rfmath.h"

class Calibration;

// AntScopeZ corrections applied to a set of points. On MeasurementData
// (`applied`) it means already baked into its points (a file saved that way),
// so they aren't applied a second time.
struct Corrections
{
    bool osl = false;          // AntScopeZ OSL calibration
    int cableMode = 0;         // 0 none, 1 cable subtracted, 2 cable added
    RfMath::CableParams cable; // the cable model, when cableMode != 0
    double cableLossFqMHz = 1; // frequency the loss figures are given at
    bool any() const { return osl || cableMode != 0; }

    // "OSL · cable −12.0 ft", or "none".
    QString summary(bool metric) const;
    // Short tag for a list column: "OSL", "−C", "+C", "OSL −C"; empty for none.
    QString tag() const;
    // Longer text for a tooltip, including the cable model.
    QString details(bool metric) const;
};

// What a measurement was taken for. Each app mode works with its own kind.

// One measurement's real data, UI-free. The GUI's `measurement` (measurement.h)
// adds its plot caches on top of this.
struct MeasurementData
{
    QString name;
    // Stable identity assigned once at creation (Measurements::
    // nextSerialNumber()) -- covers every measurement, scan or loaded file
    // alike, and is what both the Measurements table's own "#" column and
    // the Markers panel's "#" column show. Replaced an earlier "01>"/"02>"
    // prefix baked into scan names for the same purpose (removed 2026-09-17
    // -- it only ever covered scans, not loaded files, and its own counter
    // could disagree with this one once both existed side by side).
    int serialNumber = 0;
    // True if this measurement's on-disk copy (if any) doesn't match what's
    // in memory -- defaults true (a live scan), set false on load from a
    // file (Measurements::loadData()/importData()) or a successful save
    // (any format -- Export's format buttons all funnel through Measurements::
    // clearDirty()), set true again on rename (Measurements::
    // renameMeasurement()). Not affected by changing a measurement's color
    // (display preference, not data) or by a cancelled save. Shown as a
    // trailing " *" in the Points column (Measurements::pointsCellText()).
    bool dirty = true;
    qint64 qint64From;
    qint64 qint64To;
    qint64 qint64Dots;
    void set(qint64 _qint64From, qint64 _qint64To, qint64 _qint64Dots) {
        qint64From = _qint64From; qint64To = _qint64To; qint64Dots =_qint64Dots;
    }

    QVector <S21Data> dataS21;
    QVector <SParamPoint> dataSParam; // real complex 2-port data, from .s2p import -- see SParamPoint's own comment
    QVector <RawData> dataRX;
    QVector <RawData> dataRXCalib; // dataRX with OSL calibration applied (when performed)
    QVector <UserData> dataUser;
    QStringList fieldsUser;
    Corrections applied;
    // Corrections shown on top of the points: taken from the settings when
    // the scan ran, or changed later per measurement. `applied` + these =
    // what the measurement shows (its tag).
    Corrections corrections;
    QString analyzerSerial;       // analyzer that took it, if known
    // The analyzer's original points when dataRX isn't them (a file saved
    // with corrections built in); empty otherwise.
    QVector<RawData> asReceived;

    // Rebuilds dataRXCalib from dataRX with `calibration` (which must have one
    // performed). For applying OSL after the fact.
    void recalibrate(double Z0, Calibration* calibration);

    // Everything in what's shown: built in plus on top.
    Corrections shownCorrections() const;
    // The points as shown (with `corrections` applied).
    QVector<RawData> shownPoints() const
    {
        return exportPoints(corrections.osl, corrections.cableMode, corrections.cable);
    }
    // The analyzer's original points, if known.
    const QVector<RawData>& originalPoints() const
    {
        return asReceived.isEmpty() && !applied.any() ? dataRX : asReceived;
    }

    // A calibrated version of every point exists (or is built in).
    bool hasCalibrated() const
    {
        return applied.osl || (!dataRX.isEmpty() && dataRXCalib.size() == dataRX.size());
    }
    // The points with the requested corrections applied, skipping any
    // already built in. cableMode as in Corrections.
    QVector<RawData> exportPoints(bool osl, int cableMode, const RfMath::CableParams& cable) const;

    // Stores one analyzer point at `index` (replacing it if `replace` and it
    // exists -- Continuous re-sweeps -- else appending), plus its
    // OSL-calibrated copy in dataRXCalib when `calibration` has one
    // performed. Returns true and fills `calibrated` in that case. Points
    // that already have OSL applied (applied.osl) are their own calibrated copy.
    bool addPoint(const RawData& raw, int index, bool replace, double Z0,
                  Calibration* calibration, RawData* calibrated = nullptr);
};

#endif // MEASUREMENTDATA_H

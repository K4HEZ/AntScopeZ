#ifndef MEASUREMENTDATA_H
#define MEASUREMENTDATA_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <analyzer/analyzerparameters.h>

class Calibration;

// Corrections already baked into a measurement's points -- set for files
// saved that way, so they aren't applied a second time.
struct Corrections
{
    bool osl = false;          // AntScopeZ OSL calibration
    int cableMode = 0;         // 0 none, 1 cable subtracted, 2 cable added
    double cableLengthFeet = 0;
    bool any() const { return osl || cableMode != 0; }
};

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

    // Stores one analyzer point at `index` (replacing it if `replace` and it
    // exists -- Continuous re-sweeps -- else appending), plus its
    // OSL-calibrated copy in dataRXCalib when `calibration` has one
    // performed. Returns true and fills `calibrated` in that case. Points
    // that already have OSL applied (applied.osl) are their own calibrated copy.
    bool addPoint(const RawData& raw, int index, bool replace, double Z0,
                  Calibration* calibration, RawData* calibrated = nullptr);
};

#endif // MEASUREMENTDATA_H

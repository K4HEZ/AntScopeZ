#ifndef MEASUREMENTFILES_H
#define MEASUREMENTFILES_H

#include <QList>
#include <QString>
#include <QVector>
#include <float.h>
#include <analyzer/analyzerparameters.h>
#include "measurementdata.h"

// UI-free reading/writing of measurement files: .asd, Touchstone
// .s1p/.s2p, R/X .csv and .nwl. Callers own dialogs, progress and plotting.
namespace MeasurementFiles {

enum class ReadError {
    None,
    Silent,      // failed; the app never reported these, so callers don't either
    CannotOpen,  // .asd only: "Couldn't open saved file."
    TooShort,    // .asd only: "The saved file is too short."
};

struct ReadResult {
    ReadError error = ReadError::Silent;
    Corrections applied; // as recorded in the file; none if not recorded
    bool ok() const { return error == ReadError::None; }
    QVector<RawData> raw;
    QList<SParamPoint> sparams; // 2-port Touchstone only
    double fqMinMHz = DBL_MAX;
    double fqMaxMHz = 0;
};

ReadResult readAsd(const QString& path);
ReadResult readTouchstone(const QString& path); // .s1p and .s2p
ReadResult readCsv(const QString& path);
ReadResult readNwl(const QString& path);

// Appends ".asd" if missing. False if the file can't be opened. `applied`
// is recorded in the file.
bool writeAsd(QString path, const QVector<RawData>& data, const Corrections& applied = Corrections());
// Picks the format from the extension (.s1p/.csv/.nwl); other names are
// ignored. type (.s1p only): 0 Z RI, 1 S RI, 2 S MA, 3 S DB. `applied` is
// recorded in .s1p comments (CSV/NWL stay plain for other tools).
void writeRaw(const QString& path, int type, const QVector<RawData>& data,
              double z0, const QString& description,
              const Corrections& applied = Corrections());
// 2-port Touchstone; type 0 RI, 1 MA, 2 DB. Always "R 50".
bool writeTouchstone2Port(const QString& path, int type, const QList<SParamPoint>& points,
                          const QString& description);

// Display name for an imported file: its file name.
QString nameFromPath(const QString& path);

}

#endif // MEASUREMENTFILES_H

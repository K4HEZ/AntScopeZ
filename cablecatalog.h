#ifndef CABLECATALOG_H
#define CABLECATALOG_H

#include <QString>
#include <QList>

// One entry from cables.txt (see that file's own header comment for the
// full field list/units) -- just the fields TDRAnalysisDialog actually
// needs (name, characteristic impedance, velocity factor). Settings'
// own cable picker (Settings::openCablesFile()/m_cablesList) parses the
// same file independently and keeps the loss-related fields this doesn't
// bother with; the two aren't unified (yet) to avoid touching Settings'
// already-working parsing for this.
struct CableSpec {
    QString name;
    double r0 = 50.0;
    double velocityFactor = 0.66;
};

class CableCatalog {
public:
    // path is normally Settings::programDataPath("cables.txt"). Always
    // returns the same 4 built-in "Ideal N-Ohm cable" entries Settings'
    // own picker starts with, even if path can't be opened -- callers
    // don't need to special-case a missing/unreadable file.
    static QList<CableSpec> load(const QString& path);
};

#endif // CABLECATALOG_H

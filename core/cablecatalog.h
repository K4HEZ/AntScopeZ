#ifndef CABLECATALOG_H
#define CABLECATALOG_H

#include <QString>
#include <QList>
#include <QStringList>

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

    // Names containing every whitespace-separated word of filter,
    // case-insensitively; all of them if filter is blank.
    static QStringList filterNames(const QStringList& names, const QString& filter);
    // Moves name to the front of recents, capped at maxRecent.
    static void pushRecent(QStringList& recents, const QString& name, int maxRecent = kMaxRecent);
    // recents without names missing from names, capped at maxRecent.
    static QStringList pruneRecents(const QStringList& recents, const QStringList& names,
                                    int maxRecent = kMaxRecent);

    static constexpr int kMaxRecent = 5;
};

#endif // CABLECATALOG_H

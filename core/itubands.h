#ifndef ITUBANDS_H
#define ITUBANDS_H

#include <QList>
#include <QMap>
#include <QString>

struct BandPreset {
    double fromKHz = 0;
    double toKHz = 0;
    QString label; // e.g. "20m"
};

// Parses the shared/itu-regions-defaults.txt format ([Region Name]
// sections, each followed by "from, to, label" lines in kHz) into region
// name -> its bands, in file order. UI-free so any front end can use it
// without the desktop's QSettings-backed BandsMap (MainWindow::loadBands()
// in desktop/mainwindow_presets_bands.cpp, which this doesn't replace --
// that one also layers in the user's own itu-regions.txt edits via
// EditBandsDialog; out of scope here).
namespace ItuBands {
QMap<QString, QList<BandPreset>> parse(const QString& text);

// Widens [fromKHz, toKHz] by padding *each side* with percent% (0-100) of
// the range's own width -- e.g. a 2000kHz-wide band at 100% grows by 2000
// on each side, becoming 6000kHz wide. Clamped to >= 0.
void widen(double fromKHz, double toKHz, double percent, double& outFromKHz, double& outToKHz);
}

#endif // ITUBANDS_H

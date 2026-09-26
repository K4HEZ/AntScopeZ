#ifndef APPCONFIG_H
#define APPCONFIG_H

#include "rfmath.h"

// Absolute, non-user-facing ceiling -- how many points the app will ever
// attempt for a single scan, full stop. Not what the Points field/slider
// actually clamps to day-to-day; that's g_pointsMax (mainwindow.cpp), the
// user-configurable "Scanning points maximum" (Settings > General), which
// itself can't be set above this. Originally a single hardcoded constant
// (POINTS_TEST_MAX) used directly by both the slider's clamp and its
// runtime maximum while testing how many points a real device would
// actually return (see the Measurements table's "Points" column) -- kept
// as the outer bound once that testing turned into a real three-tier
// feature: this ceiling, the user's practical max, and a separate warn-
// above-this-many-points threshold (also g_-global, also General tab).
#define POINTS_MAX 10000

class QSettings;

// App-wide settings, UI-free. Meant to become the single settings module;
// the rest of MainWindow's g_ globals move in here as the core split goes on.
class AppConfig
{
public:
    static AppConfig& get();

    // Persisted, ini [Settings] group.

    // Max points per device request; bigger scans are stitched from several
    // sweeps (AnalyzerPro::buildStitchSegments()). Range 50-POINTS_MAX.
    int analyzerMaxPoints = 1000;
    // Seconds without a data point before the scan watchdog gives up.
    int analyzerTimeoutSec = 8;
    // Stop mid-scan by reconnecting instead of draining leftover points
    // (AnalyzerPro::beginReconnectDrain()).
    bool reconnectToDrain = false;
    // https + cert verification for RigExpert requests. Issue #14.
    bool useTls = true;
    int maxMarkers = 5;               // Settings > Markers
    bool autoMarkerAtLowestSwr = true; // marker at the lowest SWR after a Single scan

    // Persisted, ini [MainWindow] group.
    bool measureSystemMetric = true;
    double systemImpedance = 50; // Z0

    // Persisted, ini [Cable] group (Settings > Cable).
    RfMath::CableParams cable;
    double cableLossFqMHz = 1;
    int cableIndex = 0;
    bool cableIsPreset = false;
    int farEndMeasurement = 0; // 0 off, 1 subtract cable, 2 add cable

    // Startup only, not persisted (command line / platform, main.cpp).
    bool usbOnly = false;
    bool raspbian = false;
    bool aa55NewProtocol = false;

    // Read/write the persisted fields; begins/ends the groups itself.
    void load(QSettings& settings);
    void save(QSettings& settings) const;

private:
    AppConfig() = default;
};

#endif // APPCONFIG_H

#ifndef APPCONFIG_H
#define APPCONFIG_H

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

    // Startup only, not persisted (command line / platform, main.cpp).
    bool usbOnly = false;
    bool raspbian = false;
    bool aa55NewProtocol = false;

    // Read/write the persisted fields; begins/ends the group itself.
    void load(QSettings& settings);
    void save(QSettings& settings) const;

private:
    AppConfig() = default;
};

#endif // APPCONFIG_H

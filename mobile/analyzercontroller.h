#ifndef ANALYZERCONTROLLER_H
#define ANALYZERCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class AnalyzerPro;
class BleAnalyzer;
class MeasurementSession;
struct RawData;

// QML-facing wrapper over the core: BLE search/connect and a single scan.
class AnalyzerController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QStringList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY connectedChanged)
    Q_PROPERTY(bool measuring READ measuring NOTIFY measuringChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    // One map per point: fq (MHz), swr, rl, r, x, z, rpar, xpar, zpar, rhoPhase.
    Q_PROPERTY(QVariantList points READ points NOTIFY pointsChanged)
    Q_PROPERTY(int minSwrIndex READ minSwrIndex NOTIFY pointsChanged)
    // True while startLive()'s single-frequency measurement is repeating.
    Q_PROPERTY(bool liveMode READ liveMode NOTIFY liveModeChanged)

    // Scan settings, set from SettingsPage and used by scan().
    Q_PROPERTY(double fromKHz READ fromKHz WRITE setFromKHz NOTIFY fromKHzChanged)
    Q_PROPERTY(double toKHz READ toKHz WRITE setToKHz NOTIFY toKHzChanged)
    Q_PROPERTY(int sweepPoints READ sweepPoints WRITE setSweepPoints NOTIFY sweepPointsChanged)
    Q_PROPERTY(double z0 READ z0 WRITE setZ0 NOTIFY z0Changed)

public:
    explicit AnalyzerController(QObject* parent = nullptr);

    QStringList devices() const { return m_deviceNames; }
    bool searching() const { return m_searching; }
    bool connected() const { return m_connected; }
    QString deviceName() const { return m_deviceName; }
    bool measuring() const { return m_measuring; }
    QString status() const { return m_status; }
    QVariantList points() const { return m_points; }
    int minSwrIndex() const { return m_minSwrIndex; }
    bool liveMode() const { return m_liveMode; }

    double fromKHz() const { return m_fromKHz; }
    double toKHz() const { return m_toKHz; }
    int sweepPoints() const { return m_sweepPoints; }
    double z0() const { return m_z0; }
    void setFromKHz(double v);
    void setToKHz(double v);
    void setSweepPoints(int v);
    void setZ0(double v);

    Q_INVOKABLE void search();
    Q_INVOKABLE void connectTo(int index);
    Q_INVOKABLE void disconnectAnalyzer();
    Q_INVOKABLE void scan();
    // Repeating single-frequency measurement, chained off each completion
    // (not a timer) until stop() is called. For the All Parameters page.
    Q_INVOKABLE void startLive(double fqKHz);
    Q_INVOKABLE void stop();
    // Writes the current scan as a 1-port Touchstone (.s1p) file to the
    // app's private storage, then (Android only) opens the system Share
    // sheet for it via a FileProvider content:// URI. Desktop just saves.
    Q_INVOKABLE void shareTouchstone();

signals:
    void devicesChanged();
    void searchingChanged();
    void connectedChanged();
    void measuringChanged();
    void statusChanged();
    void pointsChanged();
    void fromKHzChanged();
    void toKHzChanged();
    void sweepPointsChanged();
    void z0Changed();
    void liveModeChanged();

private:
    void startSearch();
    void requestLivePoint();
    void setStatus(const QString& text);
    void setSearching(bool on);
    void setConnected(bool on);
    void setMeasuring(bool on);
    void addPoint(const RawData& raw);

    AnalyzerPro* m_analyzer = nullptr;
    MeasurementSession* m_session = nullptr;
    BleAnalyzer* m_ble = nullptr; // searching; handed to m_analyzer on connect

    QStringList m_deviceNames;
    QStringList m_deviceAddresses;
    bool m_searching = false;
    bool m_connected = false;
    QString m_deviceName;
    bool m_measuring = false;
    QString m_status;
    QVariantList m_points;
    int m_minSwrIndex = -1;
    bool m_liveMode = false;
    double m_liveFqKHz = 0;

    double m_fromKHz = 14000;
    double m_toKHz = 14350;
    int m_sweepPoints = 50;
    double m_z0 = 50;
};

#endif // ANALYZERCONTROLLER_H

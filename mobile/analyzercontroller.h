#ifndef ANALYZERCONTROLLER_H
#define ANALYZERCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
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
    // True while connectUsb() is retrying (e.g. waiting on the Android USB
    // permission dialog).
    Q_PROPERTY(bool connectingUsb READ connectingUsb NOTIFY connectingUsbChanged)
    // Serial ports with a recognised analyzer, filled by searchSerial().
    Q_PROPERTY(QStringList serialDevices READ serialDevices NOTIFY serialDevicesChanged)
    Q_PROPERTY(bool serialSearched READ serialSearched NOTIFY serialDevicesChanged)

    // Filled when a device connects. Range is kHz, 0 if unknown.
    Q_PROPERTY(QString deviceModel READ deviceModel NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceSerial READ deviceSerial NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceInterface READ deviceInterface NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceProtocol READ deviceProtocol NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString devicePort READ devicePort NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceFirmware READ deviceFirmware NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceLicense READ deviceLicense NOTIFY deviceInfoChanged)
    Q_PROPERTY(double deviceMinKHz READ deviceMinKHz NOTIFY deviceInfoChanged)
    Q_PROPERTY(double deviceMaxKHz READ deviceMaxKHz NOTIFY deviceInfoChanged)

    // Point shown in the SWR/Smith parameter grids; -1 if no scan yet.
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap selectedPoint READ selectedPoint NOTIFY selectedIndexChanged)
    // Latest Live Data reading; kept apart from points so a live session
    // doesn't wipe the scan.
    Q_PROPERTY(QVariantMap livePoint READ livePoint NOTIFY livePointChanged)

    // Frequency limits. The effective range is the device range (if
    // useDeviceRange) intersected with the absolute limits, which always
    // apply. Absolute limits in kHz.
    Q_PROPERTY(bool useDeviceRange READ useDeviceRange WRITE setUseDeviceRange NOTIFY limitsChanged)
    Q_PROPERTY(double absMinKHz READ absMinKHz WRITE setAbsMinKHz NOTIFY limitsChanged)
    Q_PROPERTY(double absMaxKHz READ absMaxKHz WRITE setAbsMaxKHz NOTIFY limitsChanged)
    Q_PROPERTY(double limitMinKHz READ limitMinKHz NOTIFY limitsChanged)
    Q_PROPERTY(double limitMaxKHz READ limitMaxKHz NOTIFY limitsChanged)

    // SWR chart: minimum px between points before it scrolls (Settings).
    Q_PROPERTY(double chartMinPxPerPoint READ chartMinPxPerPoint WRITE setChartMinPxPerPoint NOTIFY chartMinPxPerPointChanged)

    // Scan settings, set from ScanPage and used by scan().
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
    bool connectingUsb() const { return m_connectingUsb; }

    QStringList serialDevices() const { return m_serialNames; }
    bool serialSearched() const { return m_serialSearched; }
    QString deviceModel() const { return m_deviceModel; }
    QString deviceInterface() const { return m_deviceInterface; }
    QString deviceProtocol() const { return m_deviceProtocol; }
    QString devicePort() const { return m_devicePort; }
    QString deviceFirmware() const { return m_deviceFirmware; }
    QString deviceLicense() const { return m_deviceLicense; }
    QString deviceSerial() const { return m_deviceSerial; }
    double deviceMinKHz() const { return m_deviceMinKHz; }
    double deviceMaxKHz() const { return m_deviceMaxKHz; }

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int v);
    QVariantMap selectedPoint() const;
    QVariantMap livePoint() const { return m_livePoint; }

    bool useDeviceRange() const { return m_useDeviceRange; }
    double absMinKHz() const { return m_absMinKHz; }
    double absMaxKHz() const { return m_absMaxKHz; }
    double limitMinKHz() const;
    double limitMaxKHz() const;
    void setUseDeviceRange(bool v);
    void setAbsMinKHz(double v);
    void setAbsMaxKHz(double v);
    // Clamps a frequency to the effective range.
    Q_INVOKABLE double clampKHz(double kHz) const;

    double chartMinPxPerPoint() const { return m_chartMinPx; }
    void setChartMinPxPerPoint(double v);

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
    // RigExpert Match over USB-HID -- no live device list like BLE's
    // search()/connectTo(); HidAnalyzer enumerates+opens synchronously
    // itself, retried here (not by HidAnalyzer's own checkTimerTick(),
    // which only fires if AppConfig::get().usbOnly is set -- a shared
    // desktop setting this deliberately doesn't touch) until connected or
    // cancelUsbConnect() is called, since the first attempt typically just
    // triggers Android's USB permission dialog rather than connecting.
    Q_INVOKABLE void connectUsb();
    Q_INVOKABLE void cancelUsbConnect();
    // NanoVNA (classic/V2) and RigExpert COM units: list detected ports,
    // then connect to one by index, same selection rules as the desktop's
    // SelectDeviceDialog::onApply().
    Q_INVOKABLE void searchSerial();
    Q_INVOKABLE void connectSerial(int index);
    Q_INVOKABLE void disconnectAnalyzer();
    Q_INVOKABLE void scan();
    // Repeating single-frequency measurement, chained off each completion
    // (not a timer) until stop() is called. For the Live Data page.
    Q_INVOKABLE void startLive(double fqKHz);
    Q_INVOKABLE void stop();
    // Writes the current scan as a 1-port Touchstone (.s1p) file to the
    // app's private storage, then (Android only) opens the system Share
    // sheet for it via a FileProvider content:// URI. Desktop just saves.
    Q_INVOKABLE void shareTouchstone();

signals:
    void serialDevicesChanged();
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
    void connectingUsbChanged();
    void deviceInfoChanged();
    void selectedIndexChanged();
    void livePointChanged();
    void limitsChanged();
    void chartMinPxPerPointChanged();

private:
    void refreshDeviceInfo();
    QVariantMap makePoint(const RawData& raw) const;
    void selectAfterScan();
    void reclampRange();
    void startSearch();
    void requestLivePoint();
    void queueLivePoint();
    void attemptUsbConnect();
    void setConnectingUsb(bool on);
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
    bool m_connectingUsb = false;
    QTimer* m_usbRetryTimer = nullptr;

    struct SerialEntry {
        int type; // ReDeviceInfo::InterfaceType
        QString name;
        QString port;
    };
    QList<SerialEntry> m_serialEntries;
    QStringList m_serialNames;
    bool m_serialSearched = false;

    QString m_deviceModel;
    QString m_deviceInterface;
    QString m_deviceProtocol;
    QString m_devicePort;
    QString m_deviceFirmware;
    QString m_deviceLicense;
    QString m_deviceSerial;
    double m_deviceMinKHz = 0;
    double m_deviceMaxKHz = 0;

    int m_selectedIndex = -1;
    double m_selectedFqMHz = -1; // survives a rescan so the selection stays put
    QVariantMap m_livePoint;
    double m_liveBestDeltaMHz = -1;
    int m_liveFailures = 0;
    bool m_startingMeasure = false;

    double m_chartMinPx = 5;
    bool m_useDeviceRange = true;
    double m_absMinKHz = 100;
    double m_absMaxKHz = 10000000;

    double m_fromKHz = 14000;
    double m_toKHz = 14350;
    int m_sweepPoints = 50;
    double m_z0 = 50;
};

#endif // ANALYZERCONTROLLER_H

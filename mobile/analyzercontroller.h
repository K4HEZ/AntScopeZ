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
    // One map per point: fq (MHz), swr, rl, r, x, z.
    Q_PROPERTY(QVariantList points READ points NOTIFY pointsChanged)
    Q_PROPERTY(int minSwrIndex READ minSwrIndex NOTIFY pointsChanged)

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

    Q_INVOKABLE void search();
    Q_INVOKABLE void connectTo(int index);
    Q_INVOKABLE void disconnectAnalyzer();
    Q_INVOKABLE void scan(double fromKHz, double toKHz, int points);
    Q_INVOKABLE void stop();

signals:
    void devicesChanged();
    void searchingChanged();
    void connectedChanged();
    void measuringChanged();
    void statusChanged();
    void pointsChanged();

private:
    void startSearch();
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
    double m_z0 = 50;
};

#endif // ANALYZERCONTROLLER_H

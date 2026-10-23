#ifndef VNAAIR_ANALYZER_H
#define VNAAIR_ANALYZER_H

#include <QObject>
#include <QBluetoothDeviceInfo>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QVector>
#include <complex>
#include "baseanalyzer.h"

// VNA Air (https://vnaair.com) -- a third-party ESP32-S3 USB-host/BLE
// bridge that lets a USB-only NanoVNA talk BLE to a phone/PC, closing
// AntScopeZ's iOS/NanoVNA gap (Apple disallows generic USB access to a
// non-MFi accessory; BLE needs no MFi certification). Deliberately a
// separate BaseAnalyzer, not folded into BleAnalyzer: this is a
// fundamentally different wire protocol (JSON over chunked GATT
// notifications, with its own op-code/framing/calibration state
// machine), not RigExpert's fixed 20-byte binary packets -- and the
// vendor is actively iterating on it independently of anything
// RigExpert's own BLE units do (a binary payload variant was mentioned
// as "in the works" on a 2026-10-22 call). See
// research/vnaair-protocol.md (gitignored, deliberately never published
// -- see that file's own header for why) for the full protocol writeup
// this is built from; the vendor's own emailed spec is authoritative
// there over anything reverse-engineered from their web client.
//
// Scaffold status (2026-10-23): Status/Probe/Sweep only -- the part of
// the protocol that's stable and fully spec'd. Log and Calibration are
// deliberately not implemented yet: both are optional per the vendor's
// spec (absent/older firmware just hides the corresponding UI on their
// own client, same convention this follows), and there's no real VNA
// Air hardware in hand yet to validate the connect/sweep path against,
// let alone calibration's much larger state machine. No AntScopeZ UI
// wires this class in yet either -- the planned "VNA Air" radio button
// in SelectDeviceDialog, kept deliberately separate from the existing
// BLE radio button/BleAnalyzer pairing, is a following step.
class VnaAirAnalyzer : public BaseAnalyzer
{
    Q_OBJECT

public:
    explicit VnaAirAnalyzer(QObject *parent = nullptr);
    ~VnaAirAnalyzer() override;

    bool connectAnalyzer() override;
    void disconnectAnalyzer() override;
    // Neither a real wire-level abort -- same reasoning as
    // NanovnaAnalyzer/NanovnaV2Analyzer/BleAnalyzer, see BaseAnalyzer's
    // own comment on this method. VNA Air's Sweep is a single
    // request/response round trip with no "stop" op in the spec, and a
    // sweep already running on the NanoVNA completes on the bridge's own
    // side regardless (its result is just discarded on BLE disconnect).
    bool stopCommandAbortsDevice() const override { return false; }
    // BLE, same reasoning as BleAnalyzer: reconnecting is slow/unreliable
    // over BLE, so always drain by counting leftover points instead.
    bool supportsReconnectDrain() const override { return false; }

    QList<QBluetoothDeviceInfo>& devices() { return m_devices; }
    bool scanning() const;

public slots:
    void searchAnalyzer() override;
    void startMeasure(qint64 fqFrom_hz, qint64 fqTo_hz, int dotsNumber, bool frx = true) override;
    void startMeasureOneFq(qint64 fqFrom_hz, int dotsNumber, bool frx = true) override;
    void stopMeasure() override;

signals:
    // 1 = scan started, 0 = scan finished -- same convention as
    // BleAnalyzer::scanningChanged(), so SelectDeviceDialog's existing
    // BLE-scan UI wiring pattern carries over with minimal changes.
    void scanningChanged(int state);
    void devicesChanged(const QBluetoothDeviceInfo& info);
    // Status's "usb" state (starting/disconnected/probing/ready/error)
    // and "detail" (instrument identity once ready, or an error reason).
    // Not consumed by any UI yet -- this scaffold just tracks and
    // forwards it for whenever a connection-status readout gets wired up.
    void bridgeStatusChanged(const QString& usb, const QString& detail);

private slots:
    void addDevice(const QBluetoothDeviceInfo& device);
    void scanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void scanFinished();

    void serviceDiscovered(const QBluetoothUuid& gatt);
    void serviceScanDone();
    void serviceStateChanged(QLowEnergyService::ServiceState s);
    void characteristicChanged(const QLowEnergyCharacteristic& c, const QByteArray& value);
    void controllerError(QLowEnergyController::Error error);

private:
    void connectToDevice(const QString& address);
    void requestSweep(qint64 fqFrom_hz, qint64 fqTo_hz, int points);
    void handleStatusNotification(const QByteArray& value);
    void handleSweepNotification(const QByteArray& value);
    void parseSweepJson(const QByteArray& json);
    void finishSweep(); // mirrors NanovnaV2Analyzer::finishSweep() -- completeMeasurement(), then re-arm if continuous
    void emitPoint(double fqMHz, std::complex<double> s11, std::complex<double> s21);

    QBluetoothDeviceDiscoveryAgent* m_deviceDiscoveryAgent;
    QList<QBluetoothDeviceInfo> m_devices;

    QLowEnergyController* m_control = nullptr;
    QLowEnergyService* m_service = nullptr;

    QString m_usbState;
    QString m_detail;

    // Sweep response reassembly, keyed by the bridge's own sweep id (u16
    // LE at frame offset 2 -- see parseSweepJson()'s own comment). A
    // fresh id/count pair always resets this rather than ever merging two
    // in-flight responses, matching the vendor spec's own reassembly
    // rule.
    quint16 m_sweepId = 0;
    quint16 m_sweepChunkCount = 0;
    QVector<QByteArray> m_sweepChunks;
    QVector<bool> m_sweepChunkReceived; // separate from m_sweepChunks: a genuinely empty fragment is valid payload, not "not yet arrived"
    bool m_sweepIdKnown = false;

    // Last-requested range/points, for continuous mode's re-arm -- mirrors
    // NanovnaV2Analyzer's m_fqFrom/m_fqTo/m_dotsNumber.
    qint64 m_fqFrom = 0;
    qint64 m_fqTo = 0;
    int m_dotsNumber = 0;

    // TODO (needs real hardware + the 101-point cap confirmed in
    // practice): a single Sweep request is capped at 101 points per the
    // vendor spec. AnalyzerPro's existing segmented-scan stitching
    // (see NanovnaV2Analyzer's own comment on this) handles exceeding
    // that across *multiple* startMeasure() calls, but a single call
    // asking for more than 101 points in one go is silently downsampled
    // to 101 (same range, coarser resolution) rather than rejected --
    // revisit once there's a concrete case that actually hits this.
    static constexpr int kMaxSweepPoints = 101;
};

#endif // VNAAIR_ANALYZER_H

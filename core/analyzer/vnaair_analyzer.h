#ifndef VNAAIR_ANALYZER_H
#define VNAAIR_ANALYZER_H

#include <QObject>
#include <QBluetoothDeviceInfo>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QVector>
#include <QStringList>
#include <complex>
#include "baseanalyzer.h"

// One entry of a Calibration "status"/"engine" reply's slotMap -- see
// research/vnaair-protocol.md's Calibration section and "Slot records".
// known=false means that array index was JSON null: either genuinely
// empty, or (per the vendor spec) a record that belongs to a different
// NanoVNA board than the one currently connected -- the bridge can't
// tell those two cases apart, nor can we.
struct VnaAirCalSlot
{
    bool known = false;
    qint64 startHz = 0;
    qint64 stopHz = 0;
    int points = 0;
    qint64 savedAt = 0; // unix seconds, 0 == unknown
    QString source; // "nanovna" or "bridge"
    int mask = -1; // Status's "m" flag mask read back after the save/load, -1 == null (read failed)
};

// VNA Air (https://vnaair.com) -- a third-party ESP32-S3 USB-host/BLE
// bridge that lets a USB-only NanoVNA talk BLE to a phone/PC, closing
// AntScopeZ's iOS/NanoVNA gap (Apple disallows generic USB access to a
// non-MFi accessory; BLE needs no MFi certification). Deliberately a
// separate BaseAnalyzer, not folded into BleAnalyzer: this is a
// fundamentally different wire protocol (JSON over chunked GATT
// notifications, with its own op-code/framing/calibration state
// machine), not RigExpert's fixed 20-byte binary packets -- and the
// vendor is actively iterating on it independently of anything
// RigExpert's own BLE units do (build 176, 2026-10-26, added LiteVNA/
// NanoVNA-V2 USB-binary-protocol analyzer support, fully transparent
// over this same BLE wire format -- no change needed here). See
// research/vnaair-protocol.md (gitignored, deliberately never published
// -- see that file's own header for why) for the full protocol writeup
// this is built from; the vendor's own emailed spec is authoritative
// there over anything reverse-engineered from their web client.
//
// Scaffold status (2026-11-03): Status/Probe/Sweep, plus Calibration's
// read/select side -- status/slotMap query, recall, off, engine select.
// None of it is validated against real hardware yet (none in hand as of
// this writing; expected in a couple of days). Calibration's *capture*
// side (begin/standard/done/verify/save -- actually running a fresh
// on-device/bridge calibration into a slot) is deliberately not
// implemented yet: see research/calibration-redesign.md for why that's
// a separate design question (whether it needs its own wizard UI or can
// reuse desktop/calibrationwizarddialog.*'s, which is built around
// Calibration's (core/calibration.h) own very different, file-based,
// locally-computed OSL state machine). Log is also still unimplemented,
// same reasoning as before. SelectDeviceDialog already has a "VNA Air"
// radio button wired to this class (ReDeviceInfo::VNAAIR, see
// AnalyzerPro::createDevice()); the connect/Status/Sweep path is wired
// end to end, just unverified against a real bridge.
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

    // True once the Calibration characteristic (UUID ...0005) was found
    // during service discovery -- optional per the vendor spec, absent on
    // older firmware. UI should hide/grey the calibration controls when
    // false the same way the vendor's own client does, once that gating
    // is wired up (deliberately not done yet -- see Settings' own comment
    // on this).
    bool calibrationSupported() const { return m_calibrationSupported; }
    // Cached from the last "ok" status/engine/recall/off reply that
    // carried them -- see calibrationSlotsUpdated(). Empty/-1 before the
    // first query ever completes.
    const QVector<VnaAirCalSlot>& calibrationSlots() const { return m_calSlots; }
    int calibrationActiveSlot() const { return m_calActiveSlot; } // -1 == none recalled ("off")
    QString calibrationEngine() const { return m_calEngine; }
    QStringList calibrationEngines() const { return m_calEngines; }

public slots:
    void searchAnalyzer() override;
    void startMeasure(qint64 fqFrom_hz, qint64 fqTo_hz, int dotsNumber, bool frx = true) override;
    void startMeasureOneFq(qint64 fqFrom_hz, int dotsNumber, bool frx = true) override;
    void stopMeasure() override;

    // Calibration op 1 ("status") -- refreshes slots/slotMap/cal.n/engine.
    // Called once automatically on connect (see serviceStateChanged()) and
    // safe to call again any time to re-poll (e.g. after a slot might have
    // changed on the NanoVNA's own touchscreen -- the vendor spec's own
    // "clients present records as labels, not as facts" caveat).
    void queryCalibrationStatus();
    // Calibration op 9 ("recall") -- loads slot into the active calibration.
    // Not an AntScopeZ-side "apply": the bridge/NanoVNA corrects its own
    // sweeps from here on, same as if done from the device's own UI. Error
    // vocabulary (session_active, invalid, empty_slot, failed, ...) comes
    // back via calibrationOpError(), not a return value -- this is a BLE
    // round trip, not synchronous.
    void recallCalibrationSlot(int slot);
    // Calibration op 7 ("off") -- the combo's explicit "No calibration"
    // entry. Uncorrected/raw sweeps from here on.
    void clearCalibration();
    // Calibration op 10 ("engine"): 0 = vnaair (bridge-computed), 1 =
    // device (NanoVNA's own on-board cal, relayed). Only on allow-listed
    // NanoVNA firmware -- calibrationEngines() lists what's actually
    // offered before calling this. Refused with "session_active" if a
    // begin/standard/done capture session is open (not implemented yet,
    // so that can't happen through this class today).
    void setCalibrationEngine(int engine);

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

    // Fires once, right after service discovery, whether or not the
    // Calibration characteristic was actually found.
    void calibrationSupportedChanged(bool supported);
    // Fires after any "ok" status/engine/recall/off reply that carried
    // slot/engine info (not every op returns every field -- see
    // research/vnaair-protocol.md's Calibration "Keys, in emitted order"
    // table). slots/activeSlot/engine/engines mirror the matching
    // calibration*() accessors at the moment this fires.
    void calibrationSlotsUpdated(const QVector<VnaAirCalSlot>& slotList, int activeSlot,
                                  const QString& engine, const QStringList& engines);
    // A standard/verify/done/recall/save "running" notification -- deadlineMs
    // is the vendor spec's own worst-case timeout for this op, for client-
    // side progress animation. Not consumed by any UI yet (no capture-flow
    // UI exists), but status/recall/off/engine can still take long enough
    // (recall's own deadline is 14s) that a future UI will want this.
    void calibrationOpRunning(const QString& op, int deadlineMs);
    // Final "error" reply to a Calibration op, or "unsupported" synthesized
    // locally when the characteristic doesn't exist at all. error is one
    // of the vendor spec's own vocabulary (timeout busy usb no_session
    // session_active invalid unsupported failed empty_slot not_finalized
    // range_changed session_lost save_unverified).
    void calibrationOpError(const QString& op, const QString& error);

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

    void sendCalibrationRequest(quint8 op, const QByteArray& body);
    void handleCalibrationNotification(const QByteArray& value);
    void parseCalibrationJson(const QByteArray& json);
    static QString calibrationOpName(quint8 op);

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

    bool m_calibrationSupported = false;

    // Calibration notification reassembly -- same "VC"-framed chunking as
    // Sweep's own m_sweep*/handleSweepNotification(), just a separate set
    // of buffers since the two characteristics' notification streams are
    // independent and can interleave. Keyed by the Calibration request's
    // own requestId (the spec calls this the "stream id"), not a sweep id.
    quint16 m_calStreamId = 0;
    quint16 m_calChunkCount = 0;
    QVector<QByteArray> m_calChunks;
    QVector<bool> m_calChunkReceived;
    bool m_calStreamIdKnown = false;

    // Monotonic requestId for outgoing Calibration writes (op table's u16
    // requestId field). One request in flight at a time is assumed --
    // nothing in this scaffold queues a second Calibration op before the
    // first's final notification arrives.
    quint16 m_calRequestIdCounter = 0;

    // Cached from the last reply that carried them -- see
    // calibrationSlotsUpdated()'s own comment for which ops that is.
    QVector<VnaAirCalSlot> m_calSlots;
    int m_calActiveSlot = -1;
    QString m_calEngine;
    QStringList m_calEngines;
};

#endif // VNAAIR_ANALYZER_H

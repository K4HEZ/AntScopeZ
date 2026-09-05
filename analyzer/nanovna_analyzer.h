#ifndef NANOVNA_ANALYZER_H
#define NANOVNA_ANALYZER_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QSerialPortInfo>
#include <QStringList>
#include <QTimer>
#include <qdebug.h>
#include <math.h>
#include <complex>
#include "baseanalyzer.h"

#ifndef _NO_WINDOWS_
#include <windows.h>
#endif

#include "analyzerparameters.h"
#include "devinfo/redeviceinfo.h"

#define NANOVNA_VID 0x0483
#define NANOVNA_PID 0x5740

class NanovnaAnalyzer : public BaseAnalyzer
{
    Q_OBJECT

    enum {
        WAIT_NANO_NO=100,
        WAIT_NANO_VER,
        WAIT_NANO_VER_COMPLETE,
        WAIT_NANO_SWEEP,
        WAIT_NANO_FQ,
        WAIT_NANO_DATA,
        WAIT_NANO_DATA_S21,     // fallback sequence's "data 1" pass (real S21, paired with m_s11Buffer)
        WAIT_NANO_SCAN_PROBE,   // one-time capability probe: bare "scan" -> "scan?" (unsupported) or "usage: scan ..." (ascii supported)
        WAIT_NANO_SCAN_ASCII,   // fast-path "scan ... <mask>" with text reply (also used for the ascii leg of the binary upgrade check)
        WAIT_NANO_SCAN_BINARY   // fast-path "scan ... <mask|BINARY>" with raw binary reply -- byte-counted, not line-scanned
    };

    // What the connected firmware's "scan" command (see NanoVNA-D's
    // main.c cmd_scan()) turned out to support, probed once per
    // connection. Every tier ends up feeding the exact same SParamPoint
    // pipeline -- this only changes how many round trips and which wire
    // format is used to fill it.
    enum class ScanSupport {
        Unknown,        // not probed yet -- behaves like Unsupported until the probe resolves
        Unsupported,    // no "scan" command at all -- use sweep/frequencies/data 0/data 1
        AsciiOnly,      // "scan" exists, text reply confirmed usable
        AsciiAndBinary  // "scan" exists and a binary-mode reply round-tripped correctly
    };

public:
    explicit NanovnaAnalyzer(QObject *parent = 0);
    ~NanovnaAnalyzer();
    bool openComPort(const QString& portName, quint32 portSpeed=115200);
    void closeComPort();

    qint64 sendData(QString data);
    QSerialPort* comport() { return m_comPort; }
    RawData toRawData(QString& s1p);
    virtual bool refreshConnection();
    virtual bool connectAnalyzer();
    virtual void disconnectAnalyzer();
    // No real wire-level abort in this protocol -- see BaseAnalyzer's own
    // comment on this method.
    bool stopCommandAbortsDevice() const override { return false; }

    static void detectPorts();
    static QList<QSerialPortInfo> availablePorts() { return m_listNanovnaPorts; }
    static int portsCount() { return m_listNanovnaPorts.size(); }
    static bool isConnected() { return m_isConnected; }

    // Human-readable description of what this connection's "scan"
    // capability probe settled on -- for MainWindow's connection-info
    // status label. Returns a plain string rather than exposing the
    // private ScanSupport enum itself.
    QString scanCapabilityDescription() const;

private:
    QSerialPort * m_comPort;
    QStringList m_comAvailables;
    QByteArray m_incomingBuffer;
    QList <QString> m_stringList;

    volatile bool m_analyzerPresent;
    qint64 m_fqFrom;
    qint64 m_fqTo;
    int m_dotsNumber;
    QString m_serialPortName;

    qint32 parse (QByteArray arr);
    bool waitAnswer();
    QList<QString> m_listFQ;
    int m_fqCursor = 0; // walks m_listFQ non-destructively -- the fallback sequence needs to pair it against two channels (data 0 then data 1) per sweep/segment

    // S11 values captured during the current sweep/segment's first pass,
    // kept around so they can be paired with S21 once it arrives (from
    // the fallback sequence's "data 1" pass) into a real SParamPoint.
    QVector<std::complex<double>> m_s11Buffer;
    static std::complex<double> parseReIm(const QString& line);
    static std::complex<double> impedanceFromReflection(std::complex<double> gamma); // 50 ohm reference, shared by toRawData() and emitPoint()
    static SParamPoint makeSParamPoint(double fqMHz, std::complex<double> s11, std::complex<double> s21); // NanoVNA-family is forward-only (no S12/S22) -- shared by the WAIT_NANO_DATA_S21 fallback pass and emitPoint()

    // "scan" command capability probe/state -- see ScanSupport's comment.
    ScanSupport m_scanSupport = ScanSupport::Unknown;
    bool m_scanCapabilityProbeInProgress = false; // WAIT_NANO_SCAN_PROBE stage (bare "scan"), see probeScanCapability()
    bool m_scanBinaryProbeInProgress = false;      // WAIT_NANO_SCAN_BINARY stage, see probeBinaryScanSupport()
    QTimer* m_scanProbeTimeoutTimer = nullptr; // shared by both probe stages above -- only one is ever in flight at a time
    // startMeasure()'s handshake-retry (see its own comment) -- a real
    // QTimer rather than QTimer::singleShot() so stopMeasure() can actually
    // cancel a pending retry. See issue #27.
    QTimer* m_measureRetryTimer = nullptr;
    void probeScanCapability();
    void probeBinaryScanSupport();
    void onScanProbeTimeout();
    void finishMeasurementSegment(); // shared "ch>"-seen completion logic, same for every tier

    // Binary "scan" reply framing (NanoVNA-D main.c cmd_scan(),
    // SCAN_MASK_BINARY branch): 2-byte mask + 2-byte point count header,
    // then fixed-size per-point records (freq_t + 2 floats for S11 [+ 2
    // floats for S21 if requested]). This can't reuse the existing
    // "\r\n"-line-scanning parse() at all -- binary payload bytes can
    // legitimately equal 0x0D/0x0A -- so the expected total length is
    // tracked explicitly and decoding is only attempted once that many
    // bytes have arrived in m_incomingBuffer.
    int m_binaryExpectedBytes = -1; // -1 == still waiting for the 4-byte header
    quint16 m_binaryMask = 0;
    quint16 m_binaryPoints = 0;
    quint16 m_binarySentMask = 0;   // what we asked for, to sanity-check the header echoed back
    quint16 m_binarySentPoints = 0;
    qint32 parseBinaryScan();       // consumes from m_incomingBuffer directly; returns bytes consumed (0 == not enough yet)
    // The exact "scan ...\r\n" command most recently sent for a binary-mode
    // request (probe or real) -- the device echoes it back verbatim before
    // its real reply, and parseBinaryScan() needs to strip that echo before
    // reading the header. Cleared once checked; see parseBinaryScan()'s own
    // comment. Issues #27/#41.
    QByteArray m_lastScanCommand;

    void startFallbackSweep();               // classic sweep/frequencies/data 0 [/data 1] sequence
    void startScanSweep(bool useBinary);      // single "scan <from> <to> <points> <mask>" fast path
    void parseAsciiScanLine(const QString& line);
    void emitPoint(double fqMHz, std::complex<double> s11, std::complex<double> s21); // scan tiers always have both together

    static QList<QSerialPortInfo> m_listNanovnaPorts;
    static bool m_isConnected;

signals:
    //void analyzerFound (QString);
    //void analyzerDisconnected();
    //void analyzerDataStringArrived(QString);
    //void analyzerScreenshotDataArrived(QByteArray);
    //void signalFullInfo(QString str);
    //void signalMeasurementError();
    //void newData(rawData);
    void dataReceived(QString msg);
    // NOTE: deliberately no NanovnaAnalyzer::completeMeasurement() signal
    // here anymore -- it used to be redeclared in this class, which
    // silently hid BaseAnalyzer::completeMeasurement() (plain C++ name
    // hiding: two distinct signals, confirmed via generated moc code to
    // have different QMetaObject::activate indices). AnalyzerPro's own
    // connectSignals() only ever listens for BaseAnalyzer's, so this
    // class's version never actually reached measurementCompleteNano().
    // Scan completion itself was never affected -- that's driven by
    // AnalyzerPro::on_newData()'s shared point counter -- so removing the
    // shadowing redeclaration is a safe, incidental fix: emit
    // completeMeasurement() from this class now really is
    // BaseAnalyzer::completeMeasurement().

public slots:
    void dataArrived();
    void searchAnalyzer();
    void startMeasure(qint64 fqFrom, qint64 fqTo, int dotsNumber, bool frx=true);
    void stopMeasure();
    void checkAnalyzer();
    void makeScreenshot();
    void on_screenshotComplete();
    void on_measurementComplete();
    void on_changedSerialPort(QString portName, int analyzerIndex);
    void versionRequest();
    void portClosed();
};

#endif // NANOVNA_ANALYZER_H

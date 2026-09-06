#ifndef NANOVNA_V2_ANALYZER_H
#define NANOVNA_V2_ANALYZER_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QTimer>
#include <complex>
#include "baseanalyzer.h"
#include "analyzerparameters.h"
#include "devinfo/redeviceinfo.h"

// NanoVNA V2 / SAA-2 / LiteVNA64's binary register+FIFO protocol --
// completely different from classic NanoVNA/H/H4's ASCII shell
// (NanovnaAnalyzer): opcode+address commands, no "\r\n" line framing at
// all. Verified against two independent real-world clients: NanoVNASaver
// (github.com/NanoVNA-Saver/nanovna-saver, GPL-3.0-or-later -- compatible
// with AntScopeZ's own GPLv3-or-later combined-work status) and libxavna
// (github.com/nanovna-v2/NanoVNA-QT, GPL-2.0-only -- read for cross-
// reference only, deliberately NOT incorporated: GPLv2-only isn't
// compatible with a GPLv3-or-later combined work, see
// THIRD-PARTY-LICENSES.md's Qt section for the identical reasoning applied
// elsewhere in this project). This file is an independent implementation
// from the protocol facts both confirmed, not a port of either -- also a
// better structural fit regardless, since both reference clients use
// blocking/synchronous serial I/O while every AntScopeZ analyzer backend
// (this one included) is async, QSerialPort::readyRead()-driven.
#define NANOVNA_V2_VID 0x04B4
#define NANOVNA_V2_PID 0x0008

class NanovnaV2Analyzer : public BaseAnalyzer
{
    Q_OBJECT

    enum {
        WAIT_V2_NO = 200,
        WAIT_V2_VERSION,  // reading hw/fw version registers (0xF0/0xF2/0xF3/0xF4) right after connect
        WAIT_V2_FIFO,     // mid-sweep: reading FIFO records, byte-counted not line-scanned
    };

public:
    explicit NanovnaV2Analyzer(QObject *parent = 0);
    ~NanovnaV2Analyzer();

    bool openComPort(const QString& portName, quint32 portSpeed = 115200);
    void closeComPort();

    virtual bool refreshConnection();
    virtual bool connectAnalyzer();
    virtual void disconnectAnalyzer();
    // No real wire-level abort in this protocol -- see BaseAnalyzer's own
    // comment on this method.
    bool stopCommandAbortsDevice() const override { return false; }

    static void detectPorts();
    static QList<QSerialPortInfo> availablePorts() { return m_listNanovnaV2Ports; }
    static int portsCount() { return m_listNanovnaV2Ports.size(); }

private:
    QSerialPort* m_comPort;
    QByteArray m_incomingBuffer;

    qint64 m_fqFrom = 0;
    qint64 m_fqTo = 0;
    qint64 m_stepHz = 0;
    int m_dotsNumber = 0;

    quint8 m_hwVariant = 0;
    quint8 m_hwRevision = 0;
    quint8 m_fwMajor = 0;
    quint8 m_fwMinor = 0;

    // FIFO read bookkeeping for the in-flight sweep -- how many of the
    // requested points are still outstanding, and how many bytes the
    // *current* READFIFO request's reply will be (byte-counted, same
    // reasoning as NanovnaAnalyzer::parseBinaryScan(): a fixed-size binary
    // payload can legitimately contain any byte value, so there's nothing
    // to line-scan for here).
    int m_pointsRemaining = 0;
    int m_fifoExpectedBytes = -1;

    QTimer* m_connectTimeoutTimer = nullptr;

    void sendCommandBytes(const QByteArray& data);
    void writeRegister8(quint8 addr, quint8 value);
    void writeRegister16(quint8 addr, quint16 value);
    void writeRegister64(quint8 addr, quint64 value);

    void beginVersionProbe();
    void onConnectTimeout();
    void requestFifoChunk();  // issues one READFIFO for min(255, m_pointsRemaining) points
    void processFifoChunk();  // consumes exactly m_fifoExpectedBytes worth of 32-byte records from m_incomingBuffer
    void finishSweep();       // shared "sweep fully read" completion logic, mirrors NanovnaAnalyzer::finishMeasurementSegment()
    void emitPoint(double fqMHz, std::complex<double> s11, std::complex<double> s21); // mirrors NanovnaAnalyzer::emitPoint() -- same RawData r/x formula, same SParamPoint shape

    static QList<QSerialPortInfo> m_listNanovnaV2Ports;

signals:
    void dataReceived(QString msg);

public slots:
    void dataArrived();
    void startMeasure(qint64 fqFrom, qint64 fqTo, int dotsNumber, bool frx=true);
    void stopMeasure();
    void on_measurementComplete();
    void portClosed();
};

#endif // NANOVNA_V2_ANALYZER_H

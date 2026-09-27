#include "nanovna_v2_analyzer.h"
#include <qserialport.h>
#include <QtEndian>
#include <cmath>
#include "debuglog.h"

namespace {

// Binary protocol opcodes (see nanovna_v2_analyzer.h's class comment for
// where these were verified from).
constexpr quint8 CMD_NOP = 0x00;
constexpr quint8 CMD_READ = 0x10;
constexpr quint8 CMD_READFIFO = 0x18;
constexpr quint8 CMD_WRITE = 0x20;
constexpr quint8 CMD_WRITE2 = 0x21;
constexpr quint8 CMD_WRITE8 = 0x23;

constexpr quint8 ADDR_SWEEP_START = 0x00;
constexpr quint8 ADDR_SWEEP_STEP = 0x10;
constexpr quint8 ADDR_SWEEP_POINTS = 0x20;
constexpr quint8 ADDR_SWEEP_VALS_PER_FREQ = 0x22;
constexpr quint8 ADDR_VALUES_FIFO = 0x30;
constexpr quint8 ADDR_DEVICE_VARIANT = 0xF0;
constexpr quint8 ADDR_HARDWARE_REVISION = 0xF2;
constexpr quint8 ADDR_FW_MAJOR = 0xF3;
constexpr quint8 ADDR_FW_MINOR = 0xF4;

// int32 fwd_re, fwd_im, rev0_re, rev0_im, rev1_re, rev1_im (24 bytes) +
// int16 freq_index (2 bytes) + 6 pad bytes, per point.
constexpr int FIFO_RECORD_SIZE = 32;

} // namespace

// static members
QList<QSerialPortInfo> NanovnaV2Analyzer::m_listNanovnaV2Ports;

NanovnaV2Analyzer::NanovnaV2Analyzer(QObject *parent) : BaseAnalyzer(parent)
{
    m_type = ReDeviceInfo::NANOV2;
    m_comPort = new QSerialPort(this);
}

NanovnaV2Analyzer::~NanovnaV2Analyzer()
{
    if (m_comPort->isOpen())
        m_comPort->close();
    delete m_comPort;
    m_comPort = nullptr;
}

bool NanovnaV2Analyzer::openComPort(const QString& portName, quint32 portSpeed)
{
    if (m_comPort->isOpen()) {
        disconnect(m_comPort, SIGNAL(readyRead()), this, SLOT(dataArrived()));
        m_comPort->close();
    }
    m_comPort->setPortName(portName);
    m_comPort->setBaudRate(portSpeed); // meaningless over USB-CDC, kept for parity with NanovnaAnalyzer
    m_comPort->setFlowControl(QSerialPort::NoFlowControl);
    m_comPort->setDataBits(QSerialPort::Data8);
    m_comPort->setParity(QSerialPort::NoParity);
    m_comPort->setStopBits(QSerialPort::OneStop);

    connect(m_comPort, SIGNAL(readyRead()), this, SLOT(dataArrived()));
    connect(m_comPort, &QSerialPort::aboutToClose, this, &NanovnaV2Analyzer::portClosed);
    bool result = m_comPort->open(QSerialPort::ReadWrite);
    return result;
}

void NanovnaV2Analyzer::closeComPort()
{
    if (m_comPort != nullptr && m_comPort->isOpen()) {
        disconnect(m_comPort, SIGNAL(readyRead()), this, SLOT(dataArrived()));
        m_comPort->close();
        emit analyzerDisconnected();
    }
}

void NanovnaV2Analyzer::sendCommandBytes(const QByteArray& data)
{
    DebugLog::nanovnaTx(data);
    m_comPort->write(data);
}

void NanovnaV2Analyzer::writeRegister8(quint8 addr, quint8 value)
{
    QByteArray cmd;
    cmd.append(char(CMD_WRITE));
    cmd.append(char(addr));
    cmd.append(char(value));
    sendCommandBytes(cmd);
}

void NanovnaV2Analyzer::writeRegister16(quint8 addr, quint16 value)
{
    const quint16 le = qToLittleEndian(value);
    QByteArray cmd;
    cmd.append(char(CMD_WRITE2));
    cmd.append(char(addr));
    cmd.append(reinterpret_cast<const char*>(&le), sizeof(le));
    sendCommandBytes(cmd);
}

void NanovnaV2Analyzer::writeRegister64(quint8 addr, quint64 value)
{
    const quint64 le = qToLittleEndian(value);
    QByteArray cmd;
    cmd.append(char(CMD_WRITE8));
    cmd.append(char(addr));
    cmd.append(reinterpret_cast<const char*>(&le), sizeof(le));
    sendCommandBytes(cmd);
}

bool NanovnaV2Analyzer::connectAnalyzer()
{
    AnalyzerParameters* analyzer = AnalyzerParameters::byIndex(SelectionParameters::selected.modelIndex);
    if (analyzer == nullptr)
        return false;

    QString _serialPortName = SelectionParameters::selected.id;
    bool connected = openComPort(_serialPortName);

    // Same "let the device settle after the port opens" delay
    // NanovnaAnalyzer::checkAnalyzer() uses before its own first command --
    // matching that precedent rather than guessing a shorter one, given the
    // exact race that delay was protecting against (first-command-after-
    // connect, see nanovna_analyzer.cpp's startMeasure() comment) is a
    // property of USB-CDC devices in general, not specific to the classic
    // ASCII shell.
    QTimer::singleShot(2000, this, [this]() { beginVersionProbe(); });

    return connected;
}

void NanovnaV2Analyzer::disconnectAnalyzer()
{
}

bool NanovnaV2Analyzer::refreshConnection()
{
    return connectAnalyzer();
}

void NanovnaV2Analyzer::beginVersionProbe()
{
    // 8x NOP -- protocol reset, matches both reference clients' own
    // connect-time init sequence (a fresh connection may follow another
    // program that left a partial command in flight).
    sendCommandBytes(QByteArray(8, char(CMD_NOP)));

    // Four single-byte register reads in one burst -- device variant,
    // hardware revision, firmware major, firmware minor -- expects exactly
    // 4 reply bytes back in that order.
    QByteArray cmd;
    cmd.append(char(CMD_READ)); cmd.append(char(ADDR_DEVICE_VARIANT));
    cmd.append(char(CMD_READ)); cmd.append(char(ADDR_HARDWARE_REVISION));
    cmd.append(char(CMD_READ)); cmd.append(char(ADDR_FW_MAJOR));
    cmd.append(char(CMD_READ)); cmd.append(char(ADDR_FW_MINOR));
    sendCommandBytes(cmd);

    m_incomingBuffer.clear();
    setParseState(WAIT_V2_VERSION);

    if (m_connectTimeoutTimer == nullptr) {
        m_connectTimeoutTimer = new QTimer(this);
        m_connectTimeoutTimer->setSingleShot(true);
        connect(m_connectTimeoutTimer, &QTimer::timeout, this, &NanovnaV2Analyzer::onConnectTimeout);
    }
    m_connectTimeoutTimer->start(3000);
}

void NanovnaV2Analyzer::onConnectTimeout()
{
    if (getParseState() != WAIT_V2_VERSION)
        return; // already resolved

    // No version reply in time -- either not really a V2-family device
    // (shouldn't happen, this class is only ever instantiated for a
    // VID/PID-matched port -- see detectPorts()) or the device is wedged.
    // AnalyzerPro's own top-level connection handling treats a silent
    // connectAnalyzer() the same as any other "analyzer not found" case, so
    // just reset state rather than leaving the port half-initialized.
    m_incomingBuffer.clear();
    setParseState(WAIT_V2_NO);
}

void NanovnaV2Analyzer::dataArrived()
{
    QByteArray ar = m_comPort->readAll();
    m_incomingBuffer += ar;
    DebugLog::nanovnaRx(ar);

    if (getParseState() == WAIT_V2_VERSION) {
        if (m_incomingBuffer.size() < 4)
            return; // wait for the rest of the version reply

        m_hwVariant = quint8(m_incomingBuffer.at(0));
        m_hwRevision = quint8(m_incomingBuffer.at(1));
        m_fwMajor = quint8(m_incomingBuffer.at(2));
        m_fwMinor = quint8(m_incomingBuffer.at(3));
        m_incomingBuffer.remove(0, 4);

        if (m_connectTimeoutTimer != nullptr)
            m_connectTimeoutTimer->stop();
        setParseState(WAIT_V2_NO);

        // LiteVNA64's own detector (NanoVNASaver's is_lite_vna_64()) treats
        // hw==fw==2.2.0 exactly as the signature; anything else that still
        // cleared the "real V2 reply" bar reads as a generic V2/SAA-2.
        const bool isLiteVna64 =
            (m_hwVariant == 2 && m_hwRevision == 2 && m_fwMajor == 2 && m_fwMinor == 2);
        AnalyzerParameters* param =
            AnalyzerParameters::byName(isLiteVna64 ? "LiteVNA64" : "NanoVNA V2");
        if (param != nullptr)
            emit analyzerFound(param->index());
        return;
    }

    if (getParseState() == WAIT_V2_FIFO) {
        if (m_fifoExpectedBytes < 0 || m_incomingBuffer.size() < m_fifoExpectedBytes)
            return; // full chunk hasn't arrived yet
        processFifoChunk();
        return;
    }
}

void NanovnaV2Analyzer::startMeasure(qint64 fqFrom, qint64 fqTo, int dotsNumber, bool frx)
{
    Q_UNUSED(frx)

    if (getParseState() != WAIT_V2_NO) {
        // Same first-use race NanovnaAnalyzer::startMeasure() guards
        // against -- a scan request landing while the connect-time version
        // probe is still in flight. Retry shortly rather than dropping it;
        // AnalyzerPro's own scan-timeout watchdog is the backstop if the
        // device is genuinely wedged.
        QTimer::singleShot(50, this, [this, fqFrom, fqTo, dotsNumber, frx]() {
            startMeasure(fqFrom, fqTo, dotsNumber, frx);
        });
        return;
    }

    if (dotsNumber < 1)
        dotsNumber = 1;

    m_fqFrom = fqFrom;
    m_fqTo = fqTo;
    m_dotsNumber = dotsNumber;
    m_stepHz = (fqTo - fqFrom) / dotsNumber;
    m_isMeasuring = true;

    // dotsNumber+1 points, both endpoints inclusive -- matches
    // NanovnaAnalyzer's own convention (see its startScanSweep()'s
    // comment), which AnalyzerPro's stitching logic already assumes
    // regardless of which analyzer backend is actually in use.
    const quint16 points = quint16(dotsNumber + 1);

    writeRegister64(ADDR_SWEEP_START, quint64(fqFrom));
    writeRegister64(ADDR_SWEEP_STEP, quint64(m_stepHz));
    writeRegister16(ADDR_SWEEP_POINTS, points);
    writeRegister16(ADDR_SWEEP_VALS_PER_FREQ, 1);
    writeRegister8(ADDR_VALUES_FIFO, 0); // clear FIFO -- signals "a fresh sweep is about to be read"

    m_pointsRemaining = int(points);
    requestFifoChunk();
}

void NanovnaV2Analyzer::requestFifoChunk()
{
    const int count = std::min(255, m_pointsRemaining); // count is a single byte on the wire
    QByteArray cmd;
    cmd.append(char(CMD_READFIFO));
    cmd.append(char(ADDR_VALUES_FIFO));
    cmd.append(char(quint8(count)));
    sendCommandBytes(cmd);

    m_incomingBuffer.clear();
    m_fifoExpectedBytes = count * FIFO_RECORD_SIZE;
    setParseState(WAIT_V2_FIFO);
}

void NanovnaV2Analyzer::processFifoChunk()
{
    const int count = m_fifoExpectedBytes / FIFO_RECORD_SIZE;
    const int pointsAlreadyDelivered = (m_dotsNumber + 1) - m_pointsRemaining;

    const char* p = m_incomingBuffer.constData();
    for (int i = 0; i < count; ++i) {
        const qint32 fwdRe = qFromLittleEndian<qint32>(p + 0);
        const qint32 fwdIm = qFromLittleEndian<qint32>(p + 4);
        const qint32 rev0Re = qFromLittleEndian<qint32>(p + 8);
        const qint32 rev0Im = qFromLittleEndian<qint32>(p + 12);
        const qint32 rev1Re = qFromLittleEndian<qint32>(p + 16);
        const qint32 rev1Im = qFromLittleEndian<qint32>(p + 20);
        const quint16 freqIndex = qFromLittleEndian<quint16>(p + 24);
        p += FIFO_RECORD_SIZE;

        const std::complex<double> fwd(fwdRe, fwdIm);
        std::complex<double> s11(0.0, 0.0);
        std::complex<double> s21(0.0, 0.0);
        if (std::abs(fwd) > 0.0) {
            s11 = std::complex<double>(rev0Re, rev0Im) / fwd;
            s21 = std::complex<double>(rev1Re, rev1Im) / fwd;
        } // else: leave both zero rather than dividing by zero -- a genuinely bad/empty reference read, not a real measurement

        // freqIndex is the device's own record of which sweep point this
        // is (records aren't guaranteed delivered in order -- same
        // reasoning as the reference clients), not just this chunk's
        // loop counter.
        const double freqHz = double(m_fqFrom) + double(m_stepHz) * freqIndex;
        emitPoint(freqHz * 0.000001, s11, s21);
    }

    m_incomingBuffer.remove(0, m_fifoExpectedBytes);
    m_pointsRemaining -= count;
    m_fifoExpectedBytes = -1;
    Q_UNUSED(pointsAlreadyDelivered)

    if (m_pointsRemaining > 0) {
        requestFifoChunk();
    } else {
        finishSweep();
    }
}

void NanovnaV2Analyzer::emitPoint(double fqMHz, std::complex<double> s11, std::complex<double> s21)
{
    // Mirrors NanovnaAnalyzer::emitPoint() exactly -- same RawData r/x
    // formula, same SParamPoint shape (S12/S22 zeroed: this hardware family
    // only ever measures forward S11+S21 in one sweep, same as classic
    // NanoVNA/H/H4).
    if (fqMHz <= 0)
        return;

    const double re = s11.real(), im = s11.imag();
    RawData raw;
    raw.fq = fqMHz;
    raw.r = (1 - re * re - im * im) / ((1 - re) * (1 - re) + im * im) * 50;
    raw.x = (2 * im) / ((1 - re) * (1 - re) + im * im) * 50;
    emit newData(raw);

    SParamPoint sp;
    sp.fq = fqMHz;
    sp.s11 = s11;
    sp.s12 = std::complex<double>(0, 0);
    sp.s21 = s21;
    sp.s22 = std::complex<double>(0, 0);
    emit newSParamPoint(sp);
}

void NanovnaV2Analyzer::finishSweep()
{
    setParseState(WAIT_V2_NO);
    emit completeMeasurement();
    if (getContinuos()) {
        startMeasure(m_fqFrom, m_fqTo, m_dotsNumber);
    } else {
        setIsMeasuring(false);
        setContinuos(false);
    }
}

void NanovnaV2Analyzer::stopMeasure()
{
    m_isMeasuring = false;
}

void NanovnaV2Analyzer::on_measurementComplete()
{
    setIsMeasuring(false);
}

void NanovnaV2Analyzer::portClosed()
{
}

void NanovnaV2Analyzer::detectPorts()
{
    NanovnaV2Analyzer::m_listNanovnaV2Ports.clear();
    const QList<QSerialPortInfo> listPorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : listPorts) {
        if (info.vendorIdentifier() == NANOVNA_V2_VID && info.productIdentifier() == NANOVNA_V2_PID) {
            NanovnaV2Analyzer::m_listNanovnaV2Ports << info;
        }
    }
}

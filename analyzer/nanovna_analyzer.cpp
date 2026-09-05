#include "nanovna_analyzer.h"
#include <qserialport.h>
#include <QMessageBox>
#include <cstring>
#include "debuglog.h"

// static members
QList<QSerialPortInfo> NanovnaAnalyzer::m_listNanovnaPorts;
bool NanovnaAnalyzer::m_isConnected = false;

NanovnaAnalyzer::NanovnaAnalyzer(QObject *parent) : BaseAnalyzer(parent),
    m_analyzerPresent(false)
{
    m_type = ReDeviceInfo::NANO;
    m_comPort = new QSerialPort(this);

    //qDebug() << "NanovnaAnalyzer::NanovnaAnalyzer";
    //QTimer::singleShot(5000, this, SLOT(searchAnalyzer()));
}

NanovnaAnalyzer::~NanovnaAnalyzer()
{
    if(m_comPort->isOpen())
    {
        m_comPort->close();
    }
    delete m_comPort;
    m_comPort = NULL;
    //qDebug() << "NanovnaAnalyzer::~NanovnaAnalyzer";
}

bool NanovnaAnalyzer::openComPort(const QString& portName, quint32 portSpeed)
{
    if(m_comPort->isOpen())
    {
        disconnect(m_comPort, SIGNAL(readyRead()), this, SLOT(dataArrived()));
        m_comPort->close();
    }
    m_comPort->setPortName(portName);
    m_comPort->setBaudRate(portSpeed);//QSerialPort::Baud38400);
    m_comPort->setFlowControl(QSerialPort::NoFlowControl);
    m_comPort->setDataBits(QSerialPort::Data8);
    m_comPort->setParity(QSerialPort::NoParity);
    m_comPort->setStopBits(QSerialPort::OneStop);

    connect(m_comPort, SIGNAL(readyRead()), this, SLOT(dataArrived()));
    connect(m_comPort, &QSerialPort::aboutToClose, this, &NanovnaAnalyzer::portClosed);
    bool result = m_comPort->open(QSerialPort::ReadWrite);
    if (!result) {
        QString str = m_comPort->errorString();
        //qDebug() << "comAnalyzer::openComPort: " << portName << " " << str << " [" << m_comPort->error() << "]";
        // TODO show dialog
        // ...
    }
    m_isConnected = result;
    return result;
}

void NanovnaAnalyzer::closeComPort()
{
    if(m_comPort != NULL)
    {
        if(m_comPort->isOpen())
        {
            disconnect(m_comPort, SIGNAL(readyRead()), this, SLOT(dataArrived()));
            m_comPort->close();
            m_isConnected = false;
            emit analyzerDisconnected();
        }
    }
}

void NanovnaAnalyzer::dataArrived()
{
    QByteArray ar = m_comPort->readAll();
    m_incomingBuffer += ar;

    DebugLog::nanovnaRx(ar);
    //qDebug() << "NANO dataArrived: " << QString::fromLatin1(ar);

    if (getParseState() == WAIT_NANO_SCAN_BINARY) {
        qint32 count = parseBinaryScan();
        if (count == 0)
            return; // full frame hasn't arrived yet -- wait for more bytes
        m_incomingBuffer.remove(0, count);
        // parseBinaryScan() always leaves the state machine in
        // WAIT_NANO_NO once it has consumed a frame (success or bail-out),
        // so fall through to the normal text parser for anything left in
        // the buffer this same call (typically the shell's "ch> " prompt
        // that follows right after the binary payload) instead of leaving
        // it unread until more bytes happen to arrive later.
    }

    int count = parse(m_incomingBuffer);
    m_incomingBuffer.remove(0, count);
}

qint32 NanovnaAnalyzer::parse (QByteArray arr)
{
    int ret = 0;

    QString str(arr);
    int pos=0;
    while((pos=str.indexOf("\r\n")) != -1 || str =="ch> ") {
        QString data = str.left(pos);
        ret += data.length()+2;
        str = str.mid(pos+2);
        if (data.trimmed().isEmpty())
            continue;

        //qDebug() << "parse: state   " << getParseState() << data;

        if (getParseState() == WAIT_NANO_VER) {
            if (data.contains("info"))
                setParseState(WAIT_NANO_VER_COMPLETE);
        } else if (getParseState() == WAIT_NANO_VER_COMPLETE) {
            if (data.contains("ch>")) {
                // Used to fire a throwaway dummy "sweep" here just to
                // finish the handshake. Replaced with a real capability
                // probe: does this firmware's shell have the newer
                // single-command "scan" (and does a binary-mode reply
                // round-trip)? Every measurement tier below feeds the same
                // SParamPoint pipeline either way -- this only decides how
                // the bytes get fetched.
                probeScanCapability();
                return arr.size();
            } else {
                if (data.contains("Board:")) {
                    QString board = data.replace("Board:", "");
                    AnalyzerParameters* param = AnalyzerParameters::byName("NanoVNA");
                    if (param != nullptr)
                        emit analyzerFound(param->index());
                }
            }
        } else if (getParseState() == WAIT_NANO_SCAN_PROBE) {
            if (data.contains("usage: scan")) {
                // "scan" exists -- provisional AsciiOnly, upgraded to
                // AsciiAndBinary below if the binary check pans out.
                m_scanSupport = ScanSupport::AsciiOnly;
            } else if (data.startsWith("scan?")) {
                m_scanSupport = ScanSupport::Unsupported;
            } else if (data.contains("ch>")) {
                // Resolved one way or another -- probeBinaryScanSupport()
                // below reuses m_scanProbeTimeoutTimer for its own stage
                // (its start() call replaces this pending shot), and the
                // WAIT_NANO_NO fallthrough stops it explicitly itself.
                m_scanCapabilityProbeInProgress = false;
                if (m_scanSupport == ScanSupport::AsciiOnly) {
                    probeBinaryScanSupport();
                    return arr.size();
                }
                if (m_scanSupport == ScanSupport::Unknown)
                    m_scanSupport = ScanSupport::Unsupported; // defensive: got a prompt without seeing either expected reply
                if (m_scanProbeTimeoutTimer != nullptr)
                    m_scanProbeTimeoutTimer->stop();
                setParseState(WAIT_NANO_NO);
            }
        } else if (getParseState() == WAIT_NANO_SWEEP) {
            if (data.contains("ch>")) {
                setParseState(WAIT_NANO_FQ);
                sendData("frequencies\r\n");
                return arr.size();
            }
        } else if (getParseState() == WAIT_NANO_FQ) {
            if (data.contains("ch>")) {
                setParseState(WAIT_NANO_DATA);
                m_fqCursor = 0;
                m_s11Buffer.clear();
                sendData("data 0\r\n");
                return arr.size();
            } else {
              m_listFQ << data;
            }
        } else if (getParseState() == WAIT_NANO_DATA) {
            if (data.contains("ch>")) {
                // S11 pass done. Always follow up with a real "data 1"
                // pass so every fallback-sequence scan captures true
                // 2-port S21 too, not just S11 -- the hardware measures
                // both channels in the same sweep regardless, "data 1" is
                // just reading back the other half of it.
                m_fqCursor = 0;
                setParseState(WAIT_NANO_DATA_S21);
                sendData("data 1\r\n");
                return arr.size();
            } else {
                if (m_isMeasuring) {
                    if (m_fqCursor >= m_listFQ.size()) {
                        // more data lines than frequencies -- ignore the stray line rather than end the scan early
                    } else {
                      QString s1p = m_listFQ.at(m_fqCursor) + " " + data;
                      RawData raw = toRawData(s1p);
                      m_s11Buffer.append(parseReIm(data));
                      m_fqCursor++;
                      if (raw.fq > 0) {
                          emit newData(raw);
                      } else {
                        //qDebug() << "NanovnaAnalyzer::parse SKIP dot " << raw.fq;
                      }
                    }
                }
            }
        } else if (getParseState() == WAIT_NANO_DATA_S21) {
            if (data.contains("ch>")) {
                finishMeasurementSegment();
                return arr.size();
            } else {
                if (m_isMeasuring) {
                    if (m_fqCursor >= m_listFQ.size() || m_fqCursor >= m_s11Buffer.size()) {
                        // stray extra line -- ignore, same as the S11 pass above
                    } else {
                        double fq = m_listFQ.at(m_fqCursor).toDouble() * 0.000001;
                        std::complex<double> s11 = m_s11Buffer.at(m_fqCursor);
                        std::complex<double> s21 = parseReIm(data);
                        m_fqCursor++;
                        // Same guard as the S11 pass above (WAIT_NANO_DATA's
                        // "if (raw.fq > 0)") and for the same reason: the
                        // firmware echoes "data 1" back as its own line
                        // before the real output, and that echo lands right
                        // here as cursor 0's "data" -- paired against
                        // m_listFQ[0], which is the *other* echo ("frequencies"
                        // itself, non-numeric) left over from WAIT_NANO_FQ.
                        // toDouble() on that yields fq<=0, not a real point.
                        // m_fqCursor still advances unconditionally above --
                        // that's what keeps this pass in sync with the S11
                        // pass's own identical one-bogus-entry absorption.
                        if (fq > 0) {
                            emit newSParamPoint(makeSParamPoint(fq, s11, s21));
                        }
                    }
                }
            }
        } else if (getParseState() == WAIT_NANO_SCAN_ASCII) {
            if (data.contains("ch>")) {
                finishMeasurementSegment();
                return arr.size();
            } else {
                parseAsciiScanLine(data);
            }
        } else {
            emit dataReceived(data);
        }
    }
    return ret;
}

void NanovnaAnalyzer::searchAnalyzer()
{
    QList<QSerialPortInfo> listPorts = QSerialPortInfo::availablePorts();
    for (int idx=0; idx<listPorts.size(); idx++) {
        QSerialPortInfo info = listPorts.at(idx);

        QString portName = info.portName();
        QString systemLocation =info.systemLocation() ;
        QString description = info.description() ;
        QString manufacturer = info.manufacturer() ;
        QString serialNumber = info.serialNumber() ;

        quint16 vendorIdentifier = info.vendorIdentifier() ;
        quint16 productIdentifier = info.productIdentifier() ;

        bool hasVendorIdentifier = info.hasVendorIdentifier() ;
        bool hasProductIdentifier = info.hasProductIdentifier() ;
        QString _vendorIdentifier; _vendorIdentifier.setNum(
                    hasVendorIdentifier?vendorIdentifier:-1, 16);
        QString _productIdentifier; _productIdentifier.setNum(
                    hasProductIdentifier?productIdentifier:-1, 16);
//        qDebug() << " portName: " << portName << "\n"
//                 << "systemLocation: " << systemLocation << "\n"
//                 << "description: " << description << "\n"
//                 << "manufacturer: " << manufacturer << "\n"
//                 << "serialNumber: " << serialNumber << "\n"
//                 << "vendorIdentifier: " << _vendorIdentifier << "\n"
//                 << "productIdentifier: " << _productIdentifier << "\n"
//                 << "------------------------------------"
//                    ;

        info.description();
        QString name = info.description();
        //ui->comboBox->addItem(name);
        if (vendorIdentifier == NANOVNA_VID && productIdentifier == NANOVNA_PID) {
           openComPort(portName, 115200);
           if (!m_comPort->isOpen()) {
               QTimer::singleShot(2000, this, [this]() {
                   this->searchAnalyzer();
               });
           } else {
               emit analyzerFound(0);
           }
           return;
        }
    }
}

void NanovnaAnalyzer::checkAnalyzer()
{
    QTimer::singleShot(2000, this, [this](){ versionRequest(); });
}


qint64 NanovnaAnalyzer::sendData(QString data)
{
    //qDebug() << "NanovnaAnalyzer::sendData> " << data;

    QByteArray bytes = data.toLocal8Bit();
    DebugLog::nanovnaTx(bytes);
    qint64 res = m_comPort->write(bytes);
    return res;
}

void NanovnaAnalyzer::startMeasure(qint64 fqFrom, qint64 fqTo, int dotsNumber, bool frx)
{
    //qDebug() << "NanovnaAnalyzer::startMeasure" << fqFrom << fqTo << dotsNumber;
    if (getParseState() != WAIT_NANO_NO) {
        // First-use race, confirmed via gdb 2026-09-02: right after connect,
        // the capability-probe handshake (info -> "scan"/binary support
        // checks, WAIT_NANO_VER..WAIT_NANO_SCAN_BINARY) can still be in
        // flight -- even, in the tightest case, before it's started at all
        // (parseState still BaseAnalyzer's raw constructed default). A scan
        // request landing in that window used to be silently dropped right
        // here, while AnalyzerPro::on_measure() had *already* committed to
        // "measuring" (busy indicator shown, watchdog armed, a new
        // Measurements row created) -- left stuck until the user noticed
        // and pressed Single again, which its own AnalyzerPro::m_isMeasuring
        // bookkeeping then misread as Stop rather than Start, costing a
        // *second* extra click to actually recover.
        // Retry shortly instead of dropping it -- the handshake is a
        // handful of fast serial round-trips (sub-second on every device
        // tested), so this settles on its own almost immediately. Unbounded
        // by design: AnalyzerPro's own scan-timeout watchdog (already
        // running from the moment this measurement was requested) is the
        // backstop if the device is genuinely wedged, same as it is for any
        // other stuck scan -- no need for a second, duplicate timeout here.
        //
        // A real (reusable) QTimer, not QTimer::singleShot() -- the latter
        // gave stopMeasure() no way to cancel a pending retry. Without that,
        // clicking Single again while this first attempt was still waiting
        // out the handshake got read as "Stop" (AnalyzerPro::m_isMeasuring
        // was already true from the initial click), which reset the whole
        // UI back to idle -- but the retry fired anyway once the handshake
        // settled a moment later, silently starting a scan the app no
        // longer thought was running. Its points were real but had nowhere
        // to land (chart/UI already back to "idle"), matching the exact
        // reported symptom for issue #27: first click shows nothing, second
        // click (now with no handshake left to race) works normally.
        if (m_measureRetryTimer == nullptr) {
            m_measureRetryTimer = new QTimer(this);
            m_measureRetryTimer->setSingleShot(true);
        }
        m_measureRetryTimer->disconnect(); // drop any previous retry's lambda before arming a new one
        connect(m_measureRetryTimer, &QTimer::timeout, this, [this, fqFrom, fqTo, dotsNumber, frx]() {
            startMeasure(fqFrom, fqTo, dotsNumber, frx);
        });
        m_measureRetryTimer->start(50);
        return;
    }
    if (m_measureRetryTimer != nullptr)
        m_measureRetryTimer->stop(); // this attempt is proceeding for real now -- nothing left to retry
    Q_UNUSED (frx)
    m_fqFrom = fqFrom;
    m_fqTo = fqTo;
    // Issue #37: the device itself enforces a hard point-count range and
    // errors out on anything outside it -- confirmed live 2026-09-05, a
    // 500-point request from the S21 tab got back the device's own plain-
    // text "sweep points exceeds range 401" reply, which parseBinaryScan()
    // then misread as a corrupt binary header (see its comment). AnalyzerPro's
    // generic multi-segment stitching (buildStitchSegments()) can't fix this
    // properly on its own: it decides when to advance to the next segment by
    // counting points through on_newData(), but NanoVNA's real completion
    // signal (finishMeasurementSegment(), on the device's "ch>" prompt) fires
    // per scan command with no awareness of a multi-segment plan -- wiring
    // that up correctly is tracked separately (full stitching support for
    // NANO, see the GH issue). For now, just clamp to the device's own real
    // range rather than stitch: a request outside [100, 401] gets the
    // nearest in-range value, trading resolution for actually completing
    // instead of erroring out. 100 is the confirmed practical floor on real
    // hardware (2026-09-05); 401 is the device's own stated ceiling.
    constexpr int kNanoMinPoints = 100;
    constexpr int kNanoMaxPoints = 401;
    m_dotsNumber = qBound(kNanoMinPoints, dotsNumber, kNanoMaxPoints);
    m_isMeasuring = true;
    m_listFQ.clear();
    m_s11Buffer.clear();
    m_fqCursor = 0;

    switch (m_scanSupport) {
    case ScanSupport::AsciiAndBinary:
        startScanSweep(true);
        break;
    case ScanSupport::AsciiOnly:
        startScanSweep(false);
        break;
    case ScanSupport::Unsupported:
    case ScanSupport::Unknown:
    default:
        startFallbackSweep();
        break;
    }
}

void NanovnaAnalyzer::startFallbackSweep()
{
    setParseState(WAIT_NANO_SWEEP);
    QString cmd = QString("sweep %1 %2 %3\r\n").arg(m_fqFrom).arg(m_fqTo).arg(m_dotsNumber);
    sendData(cmd);
}

void NanovnaAnalyzer::startScanSweep(bool useBinary)
{
    // outmask bits from NanoVNA-D's cmd_scan(): OUT_FREQ=0x01,
    // OUT_DATA0(S11)=0x02, OUT_DATA1(S21)=0x04, BINARY=0x80. Always ask
    // for freq+S11+S21 together -- that's the whole point of this path.
    quint16 mask = 0x01 | 0x02 | 0x04;
    if (useBinary)
        mask |= 0x80;

    m_binarySentMask = mask;
    m_binarySentPoints = (quint16)m_dotsNumber;
    m_binaryExpectedBytes = -1;

    setParseState(useBinary ? WAIT_NANO_SCAN_BINARY : WAIT_NANO_SCAN_ASCII);
    QString cmd = QString("scan %1 %2 %3 %4\r\n").arg(m_fqFrom).arg(m_fqTo).arg(m_dotsNumber).arg(mask);
    if (useBinary)
        m_lastScanCommand = cmd.toLatin1(); // see parseBinaryScan()'s comment -- not needed for the ASCII tier, which already tolerates its own echo (emitPoint()'s fqMHz<=0 guard)
    sendData(cmd);
}

void NanovnaAnalyzer::parseAsciiScanLine(const QString& line)
{
    // "<freq> <s11re> <s11im> <s21re> <s21im>" -- one point per line
    // (mask = OUT_FREQ|OUT_DATA0|OUT_DATA1, see startScanSweep()).
    QStringList tok = line.split(' ', Qt::SkipEmptyParts);
    if (tok.size() < 5)
        return; // malformed/short line -- skip rather than index out of bounds

    bool ok;
    double freqHz = tok.at(0).toDouble(&ok);
    double s11re  = tok.at(1).toDouble(&ok);
    double s11im  = tok.at(2).toDouble(&ok);
    double s21re  = tok.at(3).toDouble(&ok);
    double s21im  = tok.at(4).toDouble(&ok);
    Q_UNUSED(ok) // permissive, matches toRawData()'s own style -- a bad token just yields 0 rather than dropping the whole point

    emitPoint(freqHz * 0.000001, std::complex<double>(s11re, s11im), std::complex<double>(s21re, s21im));
}

qint32 NanovnaAnalyzer::parseBinaryScan()
{
    // The device echoes the exact command it was just sent, as its own
    // "\r\n"-terminated text line, before its real reply -- same behavior
    // the ASCII scan-line path already tolerates (parseAsciiScanLine()'s
    // 5-token parse turns the echoed "scan ..." line into a point with
    // freq==0, which emitPoint()'s "fqMHz <= 0" guard then drops -- see its
    // own comment). This byte-buffer binary parser had no equivalent: it
    // read the echo's own first 4 bytes as the mask/points header, which
    // essentially never happens to match what was actually sent, so the
    // "malformed reply" bail-out below fired on *every* binary probe/scan.
    // For the capability probe specifically, that bail-out demoted
    // m_scanSupport to AsciiOnly and reset parse state mid-handshake --
    // which then corrupted the *next* real scan's own completion: its
    // "ch>" prompt arrived merged with this leftover garbage in one chunk
    // and matched the WAIT_NANO_SCAN_ASCII/WAIT_NANO_SCAN_BINARY "ch>"
    // check immediately, calling finishMeasurementSegment() before a
    // single real data point had been parsed -- the scan then continued to
    // stream real data on the wire (confirmed via a live debug log,
    // 2026-09-05) that the app was no longer listening for. Root cause of
    // issues #27 (first scan after connect shows nothing) and #41 (binary
    // mode negotiates successfully but is never actually used).
    if (!m_lastScanCommand.isEmpty()) {
        if (m_incomingBuffer.size() < m_lastScanCommand.size())
            return 0; // echo hasn't fully arrived yet -- wait for more bytes
        if (m_incomingBuffer.startsWith(m_lastScanCommand))
            m_incomingBuffer.remove(0, m_lastScanCommand.size());
        m_lastScanCommand.clear(); // only ever check once, right after this request
    }

    const int HEADER_SIZE = 4; // uint16 mask + uint16 points, see NanoVNA-D main.c cmd_scan()'s SCAN_MASK_BINARY branch

    if (m_incomingBuffer.size() < HEADER_SIZE)
        return 0; // wait for the rest of the header

    if (m_binaryExpectedBytes < 0) {
        std::memcpy(&m_binaryMask, m_incomingBuffer.constData(), sizeof(quint16));
        std::memcpy(&m_binaryPoints, m_incomingBuffer.constData()+2, sizeof(quint16));

        if (m_binaryMask != m_binarySentMask || m_binaryPoints != m_binarySentPoints) {
            // Doesn't look like the binary reply we asked for (stale text,
            // or this firmware doesn't really support it) -- bail out
            // rather than trust framing we can't verify.
            m_binaryExpectedBytes = -1;
            if (m_scanBinaryProbeInProgress) {
                m_scanBinaryProbeInProgress = false;
                m_scanSupport = ScanSupport::AsciiOnly;
                if (m_scanProbeTimeoutTimer != nullptr)
                    m_scanProbeTimeoutTimer->stop();
            } else {
                // A real measurement's binary reply came back malformed --
                // demote for next time and end this scan cleanly instead
                // of silently returning wrong data. The mismatched mask/
                // points values are the diagnostic data at hand -- cheap to
                // include, previously just a silent beep+cancel with no
                // message at all.
                m_scanSupport = ScanSupport::AsciiOnly;
                emit signalAnalyzerError(tr("NanoVNA binary scan reply didn't match the "
                                             "request (mask %1 vs %2, points %3 vs %4) -- "
                                             "falling back to ASCII scanning.")
                                              .arg(m_binaryMask).arg(m_binarySentMask)
                                              .arg(m_binaryPoints).arg(m_binarySentPoints));
                emit signalMeasurementError();
                // Route through the same completion path every other
                // end-of-segment case uses instead of just clearing
                // isMeasuring directly -- otherwise completeMeasurement()
                // never fires, leaving the just-created (empty) measurement
                // row orphaned and, in Continuous mode, the continuous flag
                // stuck set. Also handles setParseState(WAIT_NANO_NO) itself.
                finishMeasurementSegment();
                return m_incomingBuffer.size(); // discard -- can't trust anything else in here
            }
            setParseState(WAIT_NANO_NO);
            return m_incomingBuffer.size(); // discard -- can't trust anything else in here
        }

        int recordSize = 0;
        if (m_binaryMask & 0x01) recordSize += sizeof(quint32); // freq_t
        if (m_binaryMask & 0x02) recordSize += sizeof(float)*2; // S11 re/im
        if (m_binaryMask & 0x04) recordSize += sizeof(float)*2; // S21 re/im
        m_binaryExpectedBytes = HEADER_SIZE + recordSize * m_binaryPoints;
    }

    if (m_incomingBuffer.size() < m_binaryExpectedBytes)
        return 0; // frame not fully arrived yet

    const char* p = m_incomingBuffer.constData() + HEADER_SIZE;
    for (int i = 0; i < m_binaryPoints; i++) {
        quint32 freqHz = 0;
        float s11re=0, s11im=0, s21re=0, s21im=0;
        if (m_binaryMask & 0x01) { std::memcpy(&freqHz, p, sizeof(quint32)); p += sizeof(quint32); }
        if (m_binaryMask & 0x02) { std::memcpy(&s11re, p, sizeof(float)); p += sizeof(float); std::memcpy(&s11im, p, sizeof(float)); p += sizeof(float); }
        if (m_binaryMask & 0x04) { std::memcpy(&s21re, p, sizeof(float)); p += sizeof(float); std::memcpy(&s21im, p, sizeof(float)); p += sizeof(float); }

        if (!m_scanBinaryProbeInProgress) {
            emitPoint((double)freqHz * 0.000001, std::complex<double>(s11re, s11im), std::complex<double>(s21re, s21im));
        }
    }

    qint32 consumed = m_binaryExpectedBytes;
    m_binaryExpectedBytes = -1;

    if (m_scanBinaryProbeInProgress) {
        m_scanBinaryProbeInProgress = false;
        m_scanSupport = ScanSupport::AsciiAndBinary;
        if (m_scanProbeTimeoutTimer != nullptr)
            m_scanProbeTimeoutTimer->stop();
        setParseState(WAIT_NANO_NO);
    } else {
        finishMeasurementSegment();
    }

    return consumed;
}

void NanovnaAnalyzer::emitPoint(double fqMHz, std::complex<double> s11, std::complex<double> s21)
{
    // Same reasoning as WAIT_NANO_DATA_S21's guard above: the firmware
    // echoes the sent "scan ..." command back as its own line before the
    // real per-point output, and that echo line still parses as 5+ tokens
    // (parseAsciiScanLine's own malformed/short-line check doesn't catch
    // it), just with a non-numeric first token -- toDouble() on "scan"
    // silently yields 0 (parseBinaryScan's freqHz starts zero-initialized
    // for the same underlying non-data-line case). A real sweep point
    // never legitimately has fq<=0, so drop it here rather than in every
    // caller.
    if (fqMHz <= 0)
        return;

    RawData raw;
    raw.fq = fqMHz;
    std::complex<double> z = impedanceFromReflection(s11);
    raw.r = z.real();
    raw.x = z.imag();
    emit newData(raw);

    // newData just above already covers this point's redraw -- see
    // SParamPoint::skipRedraw's own comment. Issue #17.
    SParamPoint sp = makeSParamPoint(fqMHz, s11, s21);
    sp.skipRedraw = true;
    emit newSParamPoint(sp);
}

std::complex<double> NanovnaAnalyzer::parseReIm(const QString& line)
{
    QStringList tok = line.split(' ', Qt::SkipEmptyParts);
    if (tok.size() < 2)
        return std::complex<double>(0,0);
    return std::complex<double>(tok.at(0).toDouble(), tok.at(1).toDouble());
}

std::complex<double> NanovnaAnalyzer::impedanceFromReflection(std::complex<double> gamma)
{
    double re = gamma.real(), im = gamma.imag();
    double denom = (1-re)*(1-re) + im*im;
    double r = (1 - re*re - im*im) / denom * 50;
    double x = (2*im) / denom * 50;
    return std::complex<double>(r, x);
}

SParamPoint NanovnaAnalyzer::makeSParamPoint(double fqMHz, std::complex<double> s11, std::complex<double> s21)
{
    SParamPoint sp;
    sp.fq = fqMHz;
    sp.s11 = s11;
    sp.s12 = std::complex<double>(0,0); // NanoVNA-family hardware only measures forward S11+S21 in one sweep
    sp.s21 = s21;
    sp.s22 = std::complex<double>(0,0);
    return sp;
}

void NanovnaAnalyzer::finishMeasurementSegment()
{
    setParseState(WAIT_NANO_NO);
    //qDebug() << "NanovnaAnalyzer::parse STOP";
    emit completeMeasurement();
    if (getContinuos()) {
        startMeasure(m_fqFrom, m_fqTo, m_dotsNumber);
    } else {
        setIsMeasuring(false);
        setContinuos(false);
    }
}

void NanovnaAnalyzer::probeScanCapability()
{
    // Unlike every other stage of this handshake, a bare "scan" with no
    // reply at all previously had nothing to time it out: AnalyzerPro's own
    // watchdog only arms once a *measurement* starts (kickWatchdog(), called
    // from on_measure*()), and this probe runs earlier, right after connect.
    // Firmware that swallows this one command (seen on some clones) left
    // the state machine stuck in WAIT_NANO_SCAN_PROBE forever -- and
    // startMeasure()'s own "handshake still in flight, retry in 50ms" loop
    // (see its comment) retries unboundedly on exactly that state, so the
    // symptom was a Single/Continuous click that silently never scans, with
    // no error ever surfaced. Same 3s backstop as probeBinaryScanSupport()
    // below, and the same shared timer (only one of these two stages is
    // ever in flight at once).
    m_scanCapabilityProbeInProgress = true;
    setParseState(WAIT_NANO_SCAN_PROBE);
    sendData("scan\r\n");

    if (m_scanProbeTimeoutTimer == nullptr) {
        m_scanProbeTimeoutTimer = new QTimer(this);
        m_scanProbeTimeoutTimer->setSingleShot(true);
        connect(m_scanProbeTimeoutTimer, &QTimer::timeout, this, &NanovnaAnalyzer::onScanProbeTimeout);
    }
    m_scanProbeTimeoutTimer->start(3000);
}

void NanovnaAnalyzer::probeBinaryScanSupport()
{
    // Small throwaway 2-point sweep purely to see whether a binary-mode
    // "scan" reply round-trips on this firmware -- the frequencies and
    // point count don't matter, the result is discarded either way once
    // the capability is known (see m_scanBinaryProbeInProgress below).
    m_scanBinaryProbeInProgress = true;
    quint16 mask = 0x01 | 0x02 | 0x04 | 0x80;
    m_binarySentMask = mask;
    m_binarySentPoints = 2;
    m_binaryExpectedBytes = -1;

    setParseState(WAIT_NANO_SCAN_BINARY);
    QString cmd = QString("scan 1000000 2000000 2 %1\r\n").arg(mask);
    m_lastScanCommand = cmd.toLatin1(); // see parseBinaryScan()'s comment
    sendData(cmd);

    if (m_scanProbeTimeoutTimer == nullptr) {
        m_scanProbeTimeoutTimer = new QTimer(this);
        m_scanProbeTimeoutTimer->setSingleShot(true);
        connect(m_scanProbeTimeoutTimer, &QTimer::timeout, this, &NanovnaAnalyzer::onScanProbeTimeout);
    }
    m_scanProbeTimeoutTimer->start(3000);
}

void NanovnaAnalyzer::onScanProbeTimeout()
{
    if (m_scanCapabilityProbeInProgress) {
        // No reply at all to the bare "scan\r\n" probe -- assume this
        // firmware doesn't have the command rather than leaving the state
        // machine (and startMeasure()'s retry loop) stuck forever. Falls
        // back to the classic sweep/frequencies/data-0/data-1 sequence,
        // same as an explicit "scan?" reply would.
        m_scanCapabilityProbeInProgress = false;
        m_scanSupport = ScanSupport::Unsupported;
        setParseState(WAIT_NANO_NO);
        return;
    }

    if (!m_scanBinaryProbeInProgress)
        return; // already resolved (parseBinaryScan() got there before the timer fired)

    // No valid binary frame showed up in time -- "scan" itself is
    // confirmed present (we already saw the ascii "usage:" reply), but
    // its binary mode either isn't there or didn't answer as expected.
    // Stay at AsciiOnly rather than hang waiting for bytes that may never
    // come.
    m_scanBinaryProbeInProgress = false;
    m_scanSupport = ScanSupport::AsciiOnly;
    m_binaryExpectedBytes = -1;
    m_incomingBuffer.clear();
    setParseState(WAIT_NANO_NO);
}

void NanovnaAnalyzer::stopMeasure()
{
    m_isMeasuring = false;
    // Cancel a pending handshake-retry scan request, if one's in flight --
    // see startMeasure()'s comment. Issue #27.
    if (m_measureRetryTimer != nullptr)
        m_measureRetryTimer->stop();
}

void NanovnaAnalyzer::makeScreenshot()
{
    setIsMeasuring(true);
    m_parseState = WAIT_SCREENSHOT_DATA;
    m_incomingBuffer.clear();
    // TODO
}

void NanovnaAnalyzer::on_screenshotComplete()
{
    m_parseState = WAIT_NO;
    setIsMeasuring(false);
}

void NanovnaAnalyzer::on_measurementComplete()
{
    setIsMeasuring(false);
}

void NanovnaAnalyzer::on_changedSerialPort(QString portName, int analyzerIndex)
{
    Q_UNUSED(analyzerIndex)
    m_serialPortName = portName;
    closeComPort();
    //m_analyzerModel = 0;
    emit analyzerDisconnected();
}

void NanovnaAnalyzer::versionRequest()
{
    setParseState(WAIT_NANO_VER);
    sendData("info\r\n");
//    sendData("help\r\n");
//    sendData("version\r\n");
//    sendData("info\r\n");
}

void NanovnaAnalyzer::portClosed()
{
    //qDebug() << "Port closed";
}

void NanovnaAnalyzer::detectPorts()
{
    NanovnaAnalyzer::m_listNanovnaPorts.clear();
    QList<QSerialPortInfo> listPorts = QSerialPortInfo::availablePorts();
    for (int idx=0; idx<listPorts.size(); idx++) {
        QSerialPortInfo info = listPorts.at(idx);
        quint16 vendorIdentifier = info.vendorIdentifier() ;
        quint16 productIdentifier = info.productIdentifier() ;

        if (vendorIdentifier == NANOVNA_VID && productIdentifier == NANOVNA_PID) {
            NanovnaAnalyzer::m_listNanovnaPorts << info;
        }
    }
}

RawData NanovnaAnalyzer::toRawData(QString& s1p)
{
    RawData data;
    data.fq = -1;
    data.r = 0;
    data.x = 0;

    QString str;
    QStringList stringList = s1p.split(' ');
    if (stringList.size() < 3)
        return data;

    bool ok;
    str = stringList.takeFirst();
    data.fq = str.toDouble(&ok) * 0.000001;
    if (!ok)
        qDebug() << "***** ERROR: " << str;

    str = stringList.takeFirst();
    double param1 = str.toDouble(&ok);
    if (!ok)
        qDebug() << "***** ERROR: " << str;
    str = stringList.takeFirst();
    double param2 = str.toDouble(&ok);
    if (!ok)
        qDebug() << "***** ERROR: " << str;
    std::complex<double> z = impedanceFromReflection(std::complex<double>(param1, param2));
    data.r = z.real();
    data.x = z.imag();
    return data;
}

bool NanovnaAnalyzer::connectAnalyzer()
{
    AnalyzerParameters* analyzer = AnalyzerParameters::byIndex(SelectionParameters::selected.modelIndex);
    if (analyzer == nullptr)
        return false;

    // A fresh connection might be to a different device/firmware than
    // last time -- don't carry over a stale capability verdict.
    m_scanSupport = ScanSupport::Unknown;
    m_scanBinaryProbeInProgress = false;

    QString _serialPortName = SelectionParameters::selected.id;
    bool connected = openComPort(_serialPortName);
//    connect(this, &NanovnaAnalyzer::completeMeasurement, this, [=](){
//       emit measurementCompleteNano();
//    });

    checkAnalyzer();
    return connected;
}

void NanovnaAnalyzer::disconnectAnalyzer()
{

}

bool NanovnaAnalyzer::refreshConnection()
{
    return connectAnalyzer();
}

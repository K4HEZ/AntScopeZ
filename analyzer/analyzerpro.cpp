#include "analyzerpro.h"
#include "popupindicator.h"
#include "customanalyzer.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <memory>
#include "Notification.h"
#include "hid_analyzer.h"
#include "com_analyzer.h"
#include "nanovna_analyzer.h"
#include "nanovna_v2_analyzer.h"
#include "ble_analyzer.h"
#include "settings.h"

// static member
QList<AnalyzerParameters*> AnalyzerParameters::m_analyzers;
AnalyzerParameters* AnalyzerParameters::m_current=nullptr;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
extern int g_analyzerMaxPoints; // see mainwindow.cpp
extern int g_analyzerTimeoutSec; // see mainwindow.cpp

AnalyzerPro::AnalyzerPro(QObject *parent) : QObject(parent),
    m_baseAnalyzer(nullptr),
    m_analyzerModel(0),
    m_chartCounter(0),
    m_isMeasuring(false),
    m_isContinuos(false),
    m_dotsNumber(100),
    m_downloader(nullptr),
    m_updateDialog(nullptr),
    m_pfw(nullptr),
    m_INFOSIZE(512),
    m_calibrationMode(false)
{
    m_pfw = new QByteArray;

    AnalyzerParameters::fill();
    // TODO
    //on_comAnalyzerDisconnected(); // create hidAnalyzer

    m_watchdogTimer = new QTimer(this);
    m_watchdogTimer->setSingleShot(true);
    connect(m_watchdogTimer, &QTimer::timeout, this, &AnalyzerPro::on_watchdogTimeout);
}

AnalyzerPro::~AnalyzerPro()
{
    if(m_downloader)
    {
        delete m_downloader;
        m_downloader = nullptr;
    }
    delete m_pfw;
    if (m_baseAnalyzer != nullptr) {
        m_baseAnalyzer->deleteLater();
        m_baseAnalyzer = nullptr;
    }
}

ReDeviceInfo::InterfaceType AnalyzerPro::connectionType()
{
    if (m_baseAnalyzer != nullptr)
        return m_baseAnalyzer->connectionType();
    return ReDeviceInfo::WRONG;
}

QString AnalyzerPro::scanCapabilityDescription() const
{
    if (m_baseAnalyzer != nullptr && m_baseAnalyzer->connectionType() == ReDeviceInfo::NANO) {
        NanovnaAnalyzer* nano = qobject_cast<NanovnaAnalyzer*>(m_baseAnalyzer);
        if (nano != nullptr)
            return nano->scanCapabilityDescription();
    }
    return QString();
}

double AnalyzerPro::getVersion() const
{
    if(m_baseAnalyzer != nullptr)
    {
        return m_baseAnalyzer->getVersion().toDouble();
    }
    return 0;
}

QString AnalyzerPro::getVersionString() const
{
    if(m_baseAnalyzer != nullptr)
    {
        return m_baseAnalyzer->getVersion();
    }
    return QString();
}

QString AnalyzerPro::getRevision() const
{
    if(m_baseAnalyzer != nullptr)
    {
        return m_baseAnalyzer->getRevision();
    }
    return QString();
}

// Only reachable from the "Check for firmware updates" button in Settings
// (on_checkUpdatesBtn_clicked()) -- there's no automatic/timed path into
// this any more (see checkFirmwareUpdate()'s removal), so this used to also
// have a non-manual branch here that rate-limited itself to once/day and
// raised a passive notification instead of the dialog; that's gone too.
//
// WARNING: Disabled due to firmware-update concerns, same as
// on_checkUpdatesBtn_clicked() above -- this is only ever reached via that
// function's now-disabled m_downloader->startDownloadInfo() call, but
// disabled here too rather than relying on that alone. See the comment
// there before re-enabling anything in this chain.
void AnalyzerPro::on_downloadInfoComplete()
{
#if 0
    QString ver = m_downloader->version();
    if(ver.isEmpty())
    {
        g_showMessageBox(nullptr, QMessageBox::Information, tr("Latest version"),
                             tr("Can not get the latest version.\nPlease try later."));
    }else
    {
        double internetVersion = ver.toDouble();//ver.remove(".").toInt();
        m_updateDialog = new UpdateDialog();
        m_updateDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_updateDialog->setWindowTitle(tr("Firmware update"));
        connect(m_updateDialog,SIGNAL(update()),this,SLOT(on_internetUpdate()));
        connect(this, SIGNAL(updatePercentChanged(int)),m_updateDialog,SLOT(on_percentChanged(qint32)));
        if(internetVersion > getVersion())
        {
            m_updateDialog->setMainText(tr("New version of firmware is available! Click Download to save it."));
        }else
        {
            m_updateDialog->setMainText(tr("You have the latest version of firmware."));
        }
        m_updateDialog->exec();
    }
#endif
}

// Downloads and saves the firmware file, but stops short of flashing it --
// AnalyzerPro::updateFirmware()/BaseAnalyzer::update() (the actual apply
// step) is deliberately not called here. Applying vendor firmware isn't
// something this (non-vendor-distributed) build should attempt on its own;
// the user can take the saved file to the vendor's own tool if they want to
// apply it.
//
// WARNING: Disabled due to firmware-update concerns, same as
// on_checkUpdatesBtn_clicked() above -- only reachable via that now-disabled
// chain, but disabled here too rather than relying on that alone.
void AnalyzerPro::on_downloadFileComplete()
{
#if 0
    *m_pfw = m_downloader->file();

    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty())
        dir = Settings::localDataFolder();
    QDir().mkpath(dir);

    QString model = AnalyzerParameters::getName().toLower().remove(" ").remove("-");
    QString fileName = QDir(dir).absoluteFilePath(
                QString("AntScopeZ_firmware_%1_%2.bin").arg(model, m_downloader->version()));

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly) && file.write(*m_pfw) == m_pfw->size()) {
        file.close();
        m_updateDialog->setFinished(tr("Firmware saved to:\n%1").arg(QDir::toNativeSeparators(fileName)));
    } else {
        m_updateDialog->setFinished(tr("Could not save firmware file."));
    }
#endif
}

// WARNING: Disabled due to firmware-update concerns, same as
// on_checkUpdatesBtn_clicked() above -- only reachable via that now-disabled
// chain, but disabled here too rather than relying on that alone.
void AnalyzerPro::on_internetUpdate()
{
#if 0
    m_downloader->startDownloadFw();
    m_updateDialog->setStatusText(tr("Downloading firmware..."));
#endif
}

void AnalyzerPro::readFile(QString pathToFw)
{
    QFile file(pathToFw);
    bool state = true;

    if(!file.open(QIODevice::ReadOnly))
    {
        g_showMessageBox(nullptr, QMessageBox::Warning, tr("Warning"), tr("Can not open firmware file."));
        return;
    }

    *m_pfw = file.readAll();

    if (m_pfw->isEmpty())
    {
        g_showMessageBox(nullptr, QMessageBox::Warning, tr("Warning"), tr("Can not read firmware file."));
        state = false;
    }

    file.close();

    if(state)
    {        
        //m_updateDialog->setStatusText(tr("Updating, please wait..."));
        QBuffer fwdata(m_pfw);
        fwdata.open(QIODevice::ReadOnly);
        fwdata.seek(m_INFOSIZE);
        updateFirmware(&fwdata);
    }
}

QString AnalyzerPro::getModelString( void )
{    
    return CustomAnalyzer::customized() ? CustomAnalyzer::currentPrototype() : AnalyzerParameters::getName();
}

quint32 AnalyzerPro::getModel( void )
{
    return m_analyzerModel;
}

QString AnalyzerPro::getSerialNumber(void) const
{
    if(m_baseAnalyzer != nullptr)
    {
        return m_baseAnalyzer->getSerial();
    }
    return QString();
}

QString AnalyzerPro::getMinFq()
{
    return CustomAnalyzer::customized() ? CustomAnalyzer::currentPrototype() : AnalyzerParameters::getMinFq();
}

QString AnalyzerPro::getMaxFq()
{
    return CustomAnalyzer::customized() ? CustomAnalyzer::currentPrototype() : AnalyzerParameters::getMaxFq();
}

// Splits [fqFrom, fqTo] into however many g_analyzerMaxPoints-sized slices
// totalDots needs (leaves m_stitchSegments empty, i.e. not stitching, if
// it already fits in one request). Each slice's own frequency width is
// proportional to its share of totalDots, not equal-width -- that keeps
// the Hz/point step uniform across the whole stitched result instead of
// giving the last (remainder) segment a different resolution than the
// rest. Segments are contiguous and non-overlapping.
void AnalyzerPro::clearStitchState()
{
    m_stitchSegments.clear();
    m_stitchIndex = 0;
    m_stitchSegCounter = 0;
    m_stitchSweepComplete = true;
}

void AnalyzerPro::buildStitchSegments(qint64 fqFrom, qint64 fqTo, qint32 totalDots)
{
    clearStitchState();

    if (totalDots <= g_analyzerMaxPoints || g_analyzerMaxPoints <= 0)
        return;

    qint64 span = fqTo - fqFrom;
    int segCount = (totalDots + g_analyzerMaxPoints - 1) / g_analyzerMaxPoints; // ceil
    qint32 assigned = 0;
    qint64 from = fqFrom;
    for (int i = 0; i < segCount; i++) {
        qint32 segDots = (i == segCount - 1) ? (totalDots - assigned) : g_analyzerMaxPoints;
        assigned += segDots;
        qint64 to = (i == segCount - 1) ? fqTo : (fqFrom + (span * assigned) / totalDots);
        m_stitchSegments.append({from, to, segDots});
        from = to;
    }
}

// Common tail of on_measure()/on_measureContinuous()/on_measureUser() --
// builds the segment plan (a no-op if it fits in one request), kicks off
// the first (or only) sweep, and sets m_dotsNumber to whatever raw point
// total on_newData()'s existing finNum-based completion check needs to
// count to. A real device returns dotsNumber+1 points per request (both
// endpoints inclusive, confirmed against a RigExpert Match's own debug
// log), so stitched across segCount segments the true raw total is
// Sum(segDots+1), not totalDots+1 -- accepted as-is (each internal
// segment boundary duplicates one frequency) rather than deduped, same
// as a single non-stitched scan already isn't guaranteed to return
// exactly what was asked for.
void AnalyzerPro::startStitchedMeasure(qint64 fqFrom, qint64 fqTo, qint32 dotsNumber)
{
    buildStitchSegments(fqFrom, fqTo, dotsNumber);
    if (m_stitchSegments.isEmpty()) {
        m_dotsNumber = dotsNumber;
        m_baseAnalyzer->startMeasure(fqFrom, fqTo, m_dotsNumber);
        return;
    }
    qint32 rawTotal = 0;
    for (const StitchSegment& seg : m_stitchSegments)
        rawTotal += seg.dots + 1;
    m_dotsNumber = rawTotal - 1; // finNum = m_dotsNumber-1 fires once rawTotal points arrive
    const StitchSegment& first = m_stitchSegments.at(0);
    m_baseAnalyzer->startMeasure(first.fqFrom, first.fqTo, first.dots);
}

void AnalyzerPro::advanceStitchSegmentIfNeeded()
{
    if (m_stitchSegments.isEmpty() || !m_isMeasuring)
        return;
    m_stitchSegCounter++;
    const StitchSegment& seg = m_stitchSegments.at(m_stitchIndex);
    if (m_stitchSegCounter > (quint32)seg.dots) {
        if (m_stitchIndex + 1 < m_stitchSegments.size()) {
            // More segments queued -- the current segment's own analyzer
            // backend is about to fire its own completeMeasurement()/
            // measurementCompleteNano() once its request-level framing
            // settles (e.g. NanovnaAnalyzer's "ch> " prompt,
            // NanovnaV2Analyzer's FIFO byte count reaching zero), same as
            // it would for a real final segment -- that signal is an
            // internal segment boundary, not the real end of the stitched
            // sweep. isStitchedSweepComplete() reflects that until the
            // *next* segment's own completion arrives and this function
            // sets it back to true below.
            m_stitchSweepComplete = false;
            m_stitchIndex++;
            m_stitchSegCounter = 0;
            const StitchSegment& next = m_stitchSegments.at(m_stitchIndex);
            m_baseAnalyzer->startMeasure(next.fqFrom, next.fqTo, next.dots);
        } else {
            m_stitchSweepComplete = true; // genuinely the last segment
        }
    }
}

void AnalyzerPro::kickWatchdog()
{
    m_watchdogTimer->start(qMax(1, g_analyzerTimeoutSec) * 1000);
}

void AnalyzerPro::stopWatchdog()
{
    m_watchdogTimer->stop();
}

quint32 AnalyzerPro::remainingPointsInCurrentRequest() const
{
    if (!m_stitchSegments.isEmpty()) {
        const StitchSegment& seg = m_stitchSegments.at(m_stitchIndex);
        const quint32 segTotal = quint32(seg.dots) + 1; // both endpoints inclusive, same convention as everywhere else
        return (m_stitchSegCounter < segTotal) ? (segTotal - m_stitchSegCounter) : 0;
    }
    // Mirrors on_newData()'s own finNum math so "how many more until it
    // would have declared completion" stays consistent with what actually
    // decides completion.
    const quint32 finNum = m_calibrationMode ? m_dotsNumber : (m_dotsNumber > 0 ? m_dotsNumber - 1 : 0);
    const quint32 total = finNum + 1;
    return (m_chartCounter < total) ? (total - m_chartCounter) : 0;
}

void AnalyzerPro::beginDraining(quint32 total)
{
    m_drainTotal = total;
    m_drainReceived = 0;
    if (m_drainTotal == 0) {
        // Nothing was actually outstanding (e.g. stopped right on a point
        // boundary) -- no need to enter the draining state at all.
        stopWatchdog();
        emit statusMessageChanged(tr("Ready"));
        return;
    }
    m_isDraining = true;
    kickWatchdog(); // fresh timeout window for the drain itself
    emit drainingChanged(true);
    emit statusMessageChanged(tr("Stopping — draining remaining data (%1/%2 points)...")
                                   .arg(m_drainReceived)
                                   .arg(m_drainTotal));
}

void AnalyzerPro::beginReconnectDrain()
{
    m_isDraining = true;
    kickWatchdog();
    emit drainingChanged(true);
    emit statusMessageChanged(tr("Stopping — reconnecting to abandon remaining data..."));

    m_baseAnalyzer->closeComPort();

    // One-shot: the next successful (re)connect, from any cause, counts as
    // "reconnect drain done". finishDraining()'s own m_isDraining guard
    // makes this harmless if it somehow fires after the drain already
    // ended some other way (e.g. the watchdog gave up first).
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(this, &AnalyzerPro::analyzerFound, this, [this, conn](int) {
        disconnect(*conn);
        finishDraining(tr("Ready"));
    });

    QTimer::singleShot(200, this, [this]() {
        if (m_baseAnalyzer != nullptr)
            m_baseAnalyzer->connectAnalyzer();
    });
}

void AnalyzerPro::announceScanProgress()
{
    const quint32 finNum = m_calibrationMode ? m_dotsNumber : (m_dotsNumber > 0 ? m_dotsNumber - 1 : 0);
    const quint32 total = finNum + 1;
    emit statusMessageChanged(tr("Scanning (%1/%2 points)...").arg(m_chartCounter + 1).arg(total));
}

void AnalyzerPro::advanceDraining()
{
    m_drainReceived++;
    if (m_drainReceived >= m_drainTotal) {
        finishDraining(tr("Ready"));
        return;
    }
    emit statusMessageChanged(tr("Stopping — draining remaining data (%1/%2 points)...")
                                   .arg(m_drainReceived)
                                   .arg(m_drainTotal));
}

void AnalyzerPro::finishDraining(const QString& reason)
{
    if (!m_isDraining)
        return; // already finished (or never started) -- see beginReconnectDrain()'s comment
    stopWatchdog();
    m_isDraining = false;
    m_drainTotal = 0;
    m_drainReceived = 0;
    emit drainingChanged(false);
    emit statusMessageChanged(reason);
}

void AnalyzerPro::on_watchdogTimeout()
{
    // The bounded worst case for a drain that never completes -- device
    // disconnected, powered off, user action on the device, whatever.
    // Distinct from a fresh communications error below: we were already
    // trying to stop, so give up quietly instead of alarming the user with
    // the same message a brand-new failure would get.
    if (m_isDraining) {
        finishDraining(tr("Stopped by timeout (device stopped responding)."));
        return;
    }

    // Can race a scan that finished/was cancelled in the same tick the
    // timer was already queued to fire -- stopWatchdog() should have caught
    // it first, but this is the backstop.
    if (!m_isMeasuring)
        return;

    emit signalAnalyzerError(tr("Analyzer communications error. Please check device, "
                                 "cables, configuration, and ensure no other process "
                                 "is using it."));
    // Reuses the exact cleanup every device backend's own protocol-level
    // errors already go through (MainWindow::onMeasurementError() -> Esc ->
    // AnalyzerPro::on_stopMeasure()) -- see signalMeasurementError()'s other
    // emit sites (hid_analyzer.cpp/com_analyzer.cpp/nanovna_analyzer.cpp).
    emit signalMeasurementError();
}

void AnalyzerPro::on_measure (qint64 fqFrom, qint64 fqTo, qint32 dotsNumber)
{
    //qDebug() << "AnalyzerPro::on_measure()";
    m_getAnalyzerData = false;
    if(!m_isMeasuring)
    {
        setIsMeasuring(true);
        QDateTime datetime = QDateTime::currentDateTime();
        // yyyyMMdd-hhmmss: sorts chronologically regardless of locale, and
        // (unlike the old "hh:mm:ss dd.MM.yyyy") has no ':' or '.' for a
        // later Save to have to sanitize/strip -- those used to survive
        // into the saved filename as "_"-for-":" and then confuse the
        // extension-stripping on the next save (dots from the date read as
        // part of the "extension" to remove). See CHANGELOG.md.
        QString name = datetime.toString("##yyyyMMdd-hhmmss");
        emit newMeasurement(name, fqFrom, fqTo, dotsNumber);
        m_chartCounter = 0;
        if (m_baseAnalyzer != nullptr)
        {
            m_baseAnalyzer->setIsFRXMode(true);
            startStitchedMeasure(fqFrom, fqTo, dotsNumber);
            PopUpIndicator::setIndicatorVisible(true);
            kickWatchdog();
            emit statusMessageChanged(tr("Scanning (%1 points)...").arg(dotsNumber));
            return;
        }
    }
    on_stopMeasure();
}

void AnalyzerPro::on_measureS21 (qint64 fqFrom, qint64 fqTo, qint32 dotsNumber)
{
    //qDebug() << "AnalyzerPro::on_measureS21()";
    m_getAnalyzerData = false;
    if(!m_isMeasuring)
    {
        setIsMeasuring(true);
        QDateTime datetime = QDateTime::currentDateTime();
        QString name = datetime.toString("##yyyyMMdd-hhmmss"); // see on_measure()'s comment
        emit newMeasurement(name, fqFrom, fqTo, dotsNumber);
        m_dotsNumber = dotsNumber;
        m_chartCounter = 0;
        if (m_baseAnalyzer != nullptr)
        {
            m_baseAnalyzer->setIsS21Mode(true);
            m_baseAnalyzer->startMeasure(fqFrom, fqTo, m_dotsNumber);
            PopUpIndicator::setIndicatorVisible(true);
            kickWatchdog();
            emit statusMessageChanged(tr("Scanning S21 (%1 points)...").arg(m_dotsNumber));
            return;
        }
    }
    on_stopMeasure();
}

void AnalyzerPro::on_measureContinuous(qint64 fqFrom, qint64 fqTo, qint32 dotsNumber)
{
    if(!m_isMeasuring)
    {
        setIsMeasuring(true);
        emit continueMeasurement(fqFrom, fqTo, dotsNumber);
        m_chartCounter = 0;
        if (m_baseAnalyzer != nullptr && m_baseAnalyzer->connectionType() != ReDeviceInfo::NANO)
        {
            startStitchedMeasure(fqFrom, fqTo, dotsNumber);
            PopUpIndicator::setIndicatorVisible(true);
            kickWatchdog();
            emit statusMessageChanged(tr("Scanning continuously (%1 points)...").arg(dotsNumber));
            return;
        }
    }
    on_stopMeasure();
}

void AnalyzerPro::on_measureUser (qint64 fqFrom, qint64 fqTo, qint32 dotsNumber)
{
    if(!m_isMeasuring)
    {
        setIsMeasuring(true);
        QDateTime datetime = QDateTime::currentDateTime();
        QString name = datetime.toString("##yyyyMMdd-hhmmss"); // see on_measure()'s comment
        emit newMeasurement(name, fqFrom, fqTo, dotsNumber);
        m_chartCounter = 0;
        if (m_baseAnalyzer != nullptr && m_baseAnalyzer->connectionType() != ReDeviceInfo::NANO)
        {
            m_baseAnalyzer->setIsFRXMode(false);
            startStitchedMeasure(fqFrom, fqTo, dotsNumber);
            PopUpIndicator::setIndicatorVisible(true);
            kickWatchdog();
            emit statusMessageChanged(tr("Scanning (%1 points)...").arg(dotsNumber));
            return;
        }
    }
    on_stopMeasure();
}

void AnalyzerPro::on_measureOneFq(QWidget* /*parent*/, qint64 fqFrom, qint32 dotsNumber)
{
    setIsMeasuring(true);
    // Was hardcoded to 100000 regardless of the caller's actual dotsNumber
    // (the parameter itself was even commented out as unused) -- sent
    // straight through to BaseAnalyzer::startMeasureOneFq() as the FRX
    // command's point-count argument ("FRX100000\r"), an order of
    // magnitude past this app's own POINTS_MAX (10000) ceiling anywhere
    // else. Root cause, not just a guess: a RigExpert Match RFE (FT810)
    // returned "Error.Not recognized" for this exact command in this
    // session's testing (2026-08-20) -- see BUILDINFO.md. Use the caller's
    // real value (already the normal Points/Speed-Accuracy slider,
    // 10-1000) instead.
    m_dotsNumber = dotsNumber;
    m_chartCounter = 0;
    if (m_baseAnalyzer != nullptr && m_baseAnalyzer->connectionType() != ReDeviceInfo::NANO)
    {
        m_baseAnalyzer->setIsFRXMode(true);
        m_baseAnalyzer->startMeasureOneFq(fqFrom,m_dotsNumber);
        kickWatchdog();
        emit statusMessageChanged(tr("Scanning single frequency..."));
    }
}

void AnalyzerPro::on_stopMeasure()
{
    // Several callers (Settings opening, Esc) send this unconditionally as
    // a "just in case something's running" precaution, not because they
    // know a measurement is actually in progress. measurementComplete() is
    // wired straight to MainWindow::on_measurementComplete() -- the same
    // handler a real scan's completion uses, including placing an auto-
    // marker at the lowest SWR -- so emitting it here when nothing was
    // measuring replayed "a scan just finished" and dropped a spurious
    // second marker every time Settings was opened. Only emit it if a
    // measurement was genuinely in progress.
    bool wasMeasuring = m_isMeasuring;
    PopUpIndicator::setIndicatorVisible(false);
    // See m_measurementStopped's own comment (analyzerpro.h) -- marks any
    // completeMeasurement() that arrives after this as stale, without
    // relying on the coarser m_isMeasuring (which on_newData()'s own
    // completion check also legitimately clears for a real, non-stopped
    // finish).
    m_measurementStopped = true;

    // Snapshot before m_chartCounter/clearStitchState() reset below --
    // remainingPointsInCurrentRequest() (and so beginDraining()) needs to
    // know how many points are still outstanding in the currently in-flight
    // request alone (not the whole original scan, if stitched), which it
    // can only read from m_chartCounter/m_stitchSegCounter *before* they're
    // zeroed. Getting this ordering backwards (chartCounter already 0 ->
    // "remaining" always reads as the full original total, however much of
    // that had already arrived before Stop was clicked) is exactly what
    // made draining always undercount what was really left and time out
    // waiting for points that were never actually coming -- confirmed live
    // 2026-09-04.
    quint32 remainingPoints = wasMeasuring ? remainingPointsInCurrentRequest() : 0;

    setIsMeasuring(false);
    m_chartCounter = 0;
    clearStitchState();
    if (m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->stopMeasure();
    }
    if (wasMeasuring)
        emit measurementComplete();

    // Deliberately *after* measurementComplete() -- that signal's own
    // handlers (MainWindow::on_measurementComplete()/on_measurementCompleteNano())
    // re-enable the scan buttons as part of normal completion; entering the
    // draining state afterward correctly re-disables them for its duration
    // instead of racing with (and losing to) that re-enable.
    // stopCommandAbortsDevice() -- HID/Serial (the base implementation
    // sends a real "off\r") and any other backend that genuinely tells the
    // device to stop have nothing left to drain: the device was just told
    // to stop and will comply, so waiting for "whatever's still
    // outstanding" would wait for data that's now never coming, guaranteed
    // to time out. Only backends confirmed to have no real wire-level
    // abort (NanovnaAnalyzer, NanovnaV2Analyzer, BleAnalyzer) need this at
    // all. Confirmed live 2026-09-04: a real Match device (HID) timed out
    // every time here before this check existed.
    if (wasMeasuring && m_baseAnalyzer != nullptr && !m_baseAnalyzer->stopCommandAbortsDevice()) {
        extern bool g_reconnectToDrain; // Settings > General, see mainwindow.cpp
        if (g_reconnectToDrain) {
            beginReconnectDrain();
        } else {
            beginDraining(remainingPoints);
        }
    } else {
        stopWatchdog();
        // No draining needed (device already genuinely stopped, or nothing
        // was measuring in the first place) -- without this, the status
        // bar just sat on whatever "Scanning (N/Total points)..." text was
        // last shown, forever, since nothing else here ever resets it.
        // Confirmed live 2026-09-04.
        if (wasMeasuring)
            emit statusMessageChanged(tr("Ready"));
    }
}

void AnalyzerPro::updateFirmware (QIODevice *fw)
{
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->update(fw);
    }
}

void AnalyzerPro::makeScreenshot()
{
    if(!m_isMeasuring)
    {

        if(m_baseAnalyzer != nullptr)
        {
            QTimer::singleShot(100, m_baseAnalyzer, &BaseAnalyzer::makeScreenshot);
        }
    }
}


void AnalyzerPro::on_newData(RawData _rawData)
{
    // Leftover/stale data from a scan that's already been stopped (Esc,
    // re-clicking Single/Continuous, the watchdog) -- on_stopMeasure()
    // already ran the full one-time completion sequence (including its own
    // emit measurementComplete()) synchronously when that happened. Several
    // devices (confirmed: BLE, see BleAnalyzer::stopMeasure()) have no real
    // wire command to abort a sweep already in flight, so bytes for it keep
    // arriving here regardless. Without this guard, the block below re-runs
    // -- and re-emits measurementComplete() -- on *every single one* of
    // those leftover points, since nothing else ever makes m_isMeasuring
    // true again until a fresh scan starts. Each spurious emit reaches
    // MainWindow::on_measurementComplete() same as a real completion does,
    // which calls Markers::autoPlaceAtLowestSwr() every time -- confirmed
    // 2026-09-01 live (BLE, Single scan interrupted mid-sweep -> 5 markers,
    // one per leftover point until every slot filled, instead of the one
    // real completion). Ignore it outright instead of half-processing it --
    // except while actually draining (m_isDraining), where this is exactly
    // the expected/wanted data: advanceDraining() counts it and updates the
    // status bar, then discards it same as before.
    if (!m_isMeasuring) {
        if (m_isDraining)
            advanceDraining();
        return;
    }

    //qDebug() << "AnalyzerPro::on_newData" << _rawData.fq << _rawData.r << _rawData.x << (m_chartCounter) << (m_dotsNumber);
    // A point actually arrived -- the device (and whatever's holding it) is
    // alive and responding, so push the "no progress" deadline back out.
    kickWatchdog();
    if (m_getAnalyzerData) {
        emit newAnalyzerData (_rawData);
    } else {
        emit newData (_rawData);
    }

    advanceStitchSegmentIfNeeded();

    // ???? if(m_chartCounter >= m_dotsNumber || !m_isMeasuring)
    quint32 finNum = m_calibrationMode ? m_dotsNumber : (m_dotsNumber-1);
    if(m_chartCounter > finNum || !m_isMeasuring)
    {
        //qDebug() << "AnalyzerPro::on_newData COMPLETE";
        m_chartCounter = 0;
        setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
        clearStitchState();
        emit statusMessageChanged(tr("Ready"));
        if(!m_calibrationMode)
        {
            emit measurementComplete();
        }
        return;
    }
    announceScanProgress();
    m_chartCounter++;
}

void AnalyzerPro::on_newS21Data(S21Data _s21Data)
{
    // See on_newData()'s own comment -- same leftover-data-after-stop guard
    // (and same m_isDraining exception).
    if (!m_isMeasuring) {
        if (m_isDraining)
            advanceDraining();
        return;
    }

    kickWatchdog(); // see on_newData()'s comment
    emit newS21Data (_s21Data);

    // ???? if(m_chartCounter >= m_dotsNumber || !m_isMeasuring)
    quint32 finNum = m_calibrationMode ? m_dotsNumber : (m_dotsNumber-1);
    if(m_chartCounter > finNum || !m_isMeasuring)
    {
        qDebug() << "AnalyzerPro::on_newS21Data COMPLETE";
        m_chartCounter = 0;
        setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
        emit statusMessageChanged(tr("Ready"));
        if(!m_calibrationMode)
        {
            emit measurementComplete();
        }
        return;
    }
    announceScanProgress();
    m_chartCounter++;
}

void AnalyzerPro::on_newSParamPoint(SParamPoint sp)
{
    // Same leftover-data-after-stop guard as on_newData()/on_newS21Data(),
    // but deliberately not touching m_chartCounter/completion -- every
    // point that carries S-param data also arrives via newData() on the
    // "scan"-command fast path (see NanovnaAnalyzer::emitPoint()), so
    // on_newData() already owns end-of-scan tracking for it there.
    //
    // kickWatchdog() IS still needed here, though: on older firmware
    // without "scan", NanovnaAnalyzer::parse()'s fallback WAIT_NANO_DATA_S21
    // pass (the second half of the sweep/data-0/data-1 sequence) emits
    // *only* newSParamPoint per point, no newData -- so without this call
    // nothing kicks the watchdog for the whole back half of a fallback-tier
    // scan, and a slow device can trip AnalyzerPro's own timeout mid-scan
    // even while it's still sending valid data. Redundant (harmless -- just
    // restarts the same timer) on the fast path, where on_newData() already
    // kicked it for this same point moments earlier.
    if (!m_isMeasuring)
        return;
    kickWatchdog();
    emit newSParamPoint(sp);
}

void AnalyzerPro::on_newUserData(RawData _rawData, UserData _userData)
{
    // See on_newData()'s own comment -- same leftover-data-after-stop guard
    // (and same m_isDraining exception).
    if (!m_isMeasuring) {
        if (m_isDraining)
            advanceDraining();
        return;
    }

    kickWatchdog(); // see on_newData()'s comment
    advanceStitchSegmentIfNeeded();
    if(++m_chartCounter == m_dotsNumber+1 || !m_isMeasuring)
    {
        emit newUserData (_rawData, _userData);
        setIsMeasuring(false);
        m_chartCounter = 0;
        PopUpIndicator::setIndicatorVisible(false);
        clearStitchState();
        emit statusMessageChanged(tr("Ready"));
        if(!m_calibrationMode)
        {
            emit measurementComplete();
        }
    }else
    {
        emit statusMessageChanged(tr("Scanning (%1/%2 points)...").arg(m_chartCounter).arg(m_dotsNumber+1));
        emit newUserData (_rawData, _userData);
    }
}

void AnalyzerPro::on_newUserDataHeader(QStringList fields)
{
    emit newUserDataHeader (fields);
}

void AnalyzerPro::on_analyzerDataStringArrived(QString str)
{
    emit analyzerDataStringArrived(str);
}

void AnalyzerPro::getAnalyzerData()
{
    if(!m_isMeasuring)
    {
        if(!isMeasuring() && m_baseAnalyzer != nullptr)
        {
            QTimer::singleShot(100, m_baseAnalyzer, SLOT(getAnalyzerData()));
        }
    }
}

void AnalyzerPro::closeAnalyzerData()
{
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->setTakeData(false);
    }
}

void AnalyzerPro::on_itemDoubleClick(QString info)
{   // idx,from,to,dots:name
    setIsMeasuring(true);

    QStringList list = info.split(",");
    QStringList list2 = list.at(3).split(":");

    int div = 1;

    AnalyzerParameters* param = AnalyzerParameters::current();
    QString model = param == nullptr ? "" : param->name();

    if (model == "AA-230 ZOOM" || model == "AA-55 ZOOM" || model == "AA-650 ZOOM")
        div = 1000;

    QString name = list2.at(1);
    QString idx = list.at(0);
    if (name.trimmed().isEmpty()) {
        name = idx;
    }
    m_getAnalyzerData = true;
    if(m_baseAnalyzer != nullptr)
    {
        m_chartCounter = 0;
        m_dotsNumber = list2.at(0).toInt();
        //emit newMeasurement(name);
        emit newMeasurement(name, list.at(1).toLongLong()/div, list.at(2).toLongLong()/div, list2.at(0).toInt());
        m_baseAnalyzer->getAnalyzerData(idx);
    }
}

void AnalyzerPro::on_analyzerScreenshotDataArrived(QByteArray arr)
{
    emit analyzerScreenshotDataArrived(arr);
}

void AnalyzerPro::on_analyzerScreenPaletteArrived(QByteArray arr, quint8 cmd)
{
    emit analyzerScreenPaletteArrived(arr, cmd);
}

void AnalyzerPro::on_screenshotComplete(void)
{
    emit screenshotComplete();
}

void AnalyzerPro::on_updatePercentChanged(int number)
{
    if (m_updateDialog != nullptr)
        m_updateDialog->on_percentChanged(number);
    emit updatePercentChanged(number);
}

// Only caller is the "Check for firmware updates" button in Settings --
// see the removed checkFirmwareUpdate()/needCheckForUpdate() for the
// automatic daily-check path this used to also have.
//
// WARNING: Disabled due to firmware-update concerns -- this builds a URL
// that phones home to RigExpert with the device's serial number, firmware
// revision, the user's OS/CPU/language, and our own app version, over a
// connection with TLS certificate verification disabled (see Downloader).
// checkUpdatesBtn is permanently disabled in Settings' constructor so this
// can't be reached from the UI, but the function itself is kept intact
// (not deleted) so the implementation isn't lost to a future cleanup pass.
// The #if 0 below -- not just the disabled button -- is what stops this
// from doing anything if something still calls it directly in code. Do not
// remove the #if 0 without a deliberate decision to re-enable phoning home
// to RigExpert.
void AnalyzerPro::on_checkUpdatesBtn_clicked()
{
#if 0
    if(m_downloader == nullptr)
    {
        m_downloader = new Downloader();
        connect(m_downloader, SIGNAL(downloadInfoComplete()),
                this, SLOT(on_downloadInfoComplete()));
        connect(m_downloader, SIGNAL(downloadFileComplete()),
                this, SLOT(on_downloadFileComplete()));
        connect(m_downloader, SIGNAL(progress(qint64,qint64)),
                this, SLOT(on_progress(qint64,qint64)));
    }

    QString url = "https://www.rigexpert.com/getfirmware?app=antscope2&model=";
    QString name = AnalyzerParameters::getName();
    if (name == "AA-1500 SE")
        name = "AA-1500 ZOOM SE"; // HUCK short names supprt
    url += name.toLower().remove(" ").remove("-");
    // reenable sn= reporting for vendor url
    url += "&sn=" + getSerialNumber();
    url += "&revision=" + getRevision();
    url += "&os=" + QSysInfo::prettyProductName().replace(" ", "-").toLower();
    url += "&cpu=" + QSysInfo::currentCpuArchitecture();
    url += "&lang=" + QLocale::languageToString(QLocale::system().language());
    url += "&sw=" + QString(ANTSCOPEZ_VER);
    url += "&fw=" + getVersionString();

    m_downloader->startDownloadInfo(QUrl(url));
#endif
}

void AnalyzerPro::on_progress(qint64 downloaded,qint64 total)
{
    int percent = downloaded*100/total;
    if (percent == 100)
    {
        emit updatePercentChanged(0);
        m_updateDialog->setStatusText(tr("Saving firmware file..."));
    }else
    {
        emit updatePercentChanged(percent);
    }
}

void AnalyzerPro::on_measureCalib(int dotsNumber)
{
    setIsMeasuring(true);
    m_dotsNumber = dotsNumber;
    m_chartCounter = 0;
    qint64 minFq_ = AnalyzerParameters::getMinFq().toULongLong()*1000;
    qint64 maxFq_ = AnalyzerParameters::getMaxFq().toULongLong()*1000;
    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            minFq_ = ca->minFq().toULongLong()*1000;
            maxFq_ = ca->maxFq().toULongLong()*1000;
        }
    }
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->startMeasure(minFq_, maxFq_, dotsNumber);
    }
}

void AnalyzerPro::setCalibrationMode(bool enabled)
{
    m_calibrationMode = enabled;
}

void AnalyzerPro::setIsMeasuring (bool _isMeasuring)
{
    // TEMPORARY (2026-09-05, see popupindicator.cpp's own comment) --
    // correlates against the busy-indicator instrumentation there.
    qDebug().noquote() << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
        << "[BUSY] AnalyzerPro::setIsMeasuring(" << _isMeasuring << ")";
    m_isMeasuring = _isMeasuring;
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->setIsMeasuring(_isMeasuring);
    }
    PopUpIndicator::setIndicatorVisible(_isMeasuring);
    // ISSUE #19: centralized here instead of at every individual
    // measurement-start/completion call site -- see the comment on
    // m_watchdogTimer (analyzerpro.h) for the full reasoning.
    if (_isMeasuring) {
        kickWatchdog();
        // A real new measurement is starting -- see m_measurementStopped's
        // own comment (analyzerpro.h): whatever stopped state a previous
        // scan left behind no longer applies to this one. Centralized here
        // rather than at every on_measure*()/startOneFq() call site, same
        // reasoning as kickWatchdog() above.
        m_measurementStopped = false;
    } else {
        stopWatchdog();
    }
}

void AnalyzerPro::setContinuos(bool _isContinuos)
{
    m_isContinuos = _isContinuos;
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->setContinuos(_isContinuos);
    }
}

void AnalyzerPro::searchAnalyzer()
{
    // TODO
    if (!isMeasuring())
    {
        if (m_baseAnalyzer != nullptr)
            m_baseAnalyzer->searchAnalyzer();
    }
    //
}

bool AnalyzerPro::refreshConnection()
{
    bool ret = createDevice(SelectionParameters::selected);
    if (ret) {
        connectSignals();
        ret = m_baseAnalyzer->refreshConnection();
    }
    return ret;
}

bool AnalyzerPro::sendData(const QByteArray& data)
{
    bool ret = true;
    if (m_baseAnalyzer != nullptr) {
        m_baseAnalyzer->sendData(data);
    } else {
        ret = false;
    }
    return ret;
}

bool AnalyzerPro::sendCommand(const QString& _command)
{
    bool ret = true;
    if (m_baseAnalyzer != nullptr) {
        m_baseAnalyzer->sendCommand(_command);
    } else {
        ret = false;
    }
    return ret;
}

void AnalyzerPro::setParseState(int _state)
{
    if (m_baseAnalyzer != nullptr) {
        m_baseAnalyzer->setParseState(_state);
    }
}

int AnalyzerPro::getParseState()
{
    if (m_baseAnalyzer != nullptr) {
        return m_baseAnalyzer->getParseState();
    }
    return WAIT_NO;
}


void AnalyzerPro::on_connectDevice(BaseAnalyzer* analyzer)
{
    if (! createDevice(SelectionParameters::selected, analyzer)) {
        return;
    }
    connectSignals();
    m_baseAnalyzer->connectAnalyzer();
}

void AnalyzerPro::connectSignals()
{
    connect(m_baseAnalyzer, &BaseAnalyzer::analyzerFound, this, &AnalyzerPro::on_analyzerFound);
    connect(m_baseAnalyzer, &BaseAnalyzer::analyzerDisconnected, this, &AnalyzerPro::on_disconnectDevice);
    connect(this, &AnalyzerPro::measurementComplete, m_baseAnalyzer, &BaseAnalyzer::on_measurementComplete);//, Qt::QueuedConnection);
    connect(m_baseAnalyzer, &BaseAnalyzer::signalFullInfo, this, &AnalyzerPro::slotFullInfo);
    connect(m_baseAnalyzer, &BaseAnalyzer::signalMeasurementError, this, &AnalyzerPro::signalMeasurementError);
    connect(m_baseAnalyzer, &BaseAnalyzer::newData,this,&AnalyzerPro::on_newData);
    connect(m_baseAnalyzer, &BaseAnalyzer::newS21Data,this, &AnalyzerPro::on_newS21Data);
    connect(m_baseAnalyzer, &BaseAnalyzer::newSParamPoint, this, &AnalyzerPro::on_newSParamPoint);
    connect(m_baseAnalyzer, &BaseAnalyzer::newUserData,this, &AnalyzerPro::on_newUserData);
    connect(m_baseAnalyzer,&BaseAnalyzer::newUserDataHeader,this, &AnalyzerPro::on_newUserDataHeader);
    connect(m_baseAnalyzer, &BaseAnalyzer::analyzerDataStringArrived,this, &AnalyzerPro::on_analyzerDataStringArrived);
    connect(m_baseAnalyzer,&BaseAnalyzer::analyzerScreenshotDataArrived,this, &AnalyzerPro::on_analyzerScreenshotDataArrived);
    connect(m_baseAnalyzer,&BaseAnalyzer::analyzerScreenPaletteArrived,this, &AnalyzerPro::on_analyzerScreenPaletteArrived);
    connect(this, &AnalyzerPro::screenshotComplete, m_baseAnalyzer, &BaseAnalyzer::on_screenshotComplete);
    connect(m_baseAnalyzer, &BaseAnalyzer::signalAnalyzerError, this, &AnalyzerPro::signalAnalyzerError);
    connect(m_baseAnalyzer, &BaseAnalyzer::completeMeasurement, this, [=](){
        // Same leftover-data-after-stop guard as on_newData()/on_newS21Data()/
        // on_newSParamPoint() (see on_newData()'s comment for the full
        // reasoning) -- NanovnaAnalyzer::stopMeasure() only clears
        // m_isMeasuring, not the parser state, so a scan already in flight
        // when Esc/Stop is pressed keeps parsing to its own natural end and
        // still emits completeMeasurement() once the trailing "ch>" arrives.
        // Without this guard that stale signal reaches
        // MainWindow::on_measurementCompleteNano() same as a real
        // completion does -- if the user has since started a *new* scan,
        // it wrongly runs that handler's completion bookkeeping (including
        // possibly deleting the new scan's just-created row and forcing
        // m_analyzer->setIsMeasuring(false) mid-flight) against it instead.
        //
        // Checks m_measurementStopped, not m_isMeasuring -- see that
        // flag's own comment (analyzerpro.h). Using m_isMeasuring here
        // used to also reject this same scan's own genuine, non-stopped
        // completion: on_newData()'s chartCounter-vs-dotsNumber check
        // already clears m_isMeasuring for the NanoVNA fast "scan" path's
        // real final point, just before this signal arrives for the exact
        // same scan -- silently dropping measurementCompleteNano() (the
        // Remote API's only "sweep_done" signal for NanoVNA) even though
        // nothing was ever stopped. Issue #42.
        if (m_measurementStopped)
            return;
        if (m_baseAnalyzer != nullptr) {
            m_baseAnalyzer->on_measurementComplete();
        }
       emit measurementCompleteNano();
    });
    connect(m_baseAnalyzer, &BaseAnalyzer::receivedMatch_12, this, [=](QByteArray data){
        emit signalMatch_12Received(data);
    });
    connect(m_baseAnalyzer, &BaseAnalyzer::receivedMatch_ProfileB16, this, [=](QByteArray data){
        emit signalMatch_Profile_B16Received(data);
    });
    connect(m_baseAnalyzer, &BaseAnalyzer::crcError, this, &AnalyzerPro::crcError);

    // aa30updateComplete() is ComAnalyzer-specific (serial-connected AA-30
    // firmware update flow), not part of the generic BaseAnalyzer interface.
    ComAnalyzer* comAnalyzer = qobject_cast<ComAnalyzer*>(m_baseAnalyzer);
    if (comAnalyzer != nullptr) {
        connect(comAnalyzer, &ComAnalyzer::aa30updateComplete, this, &AnalyzerPro::aa30updateComplete);
    }
}

bool AnalyzerPro::createDevice(const SelectionParameters& param, BaseAnalyzer* analyzer)
{
    BaseAnalyzer* tmp = m_baseAnalyzer;
    if (tmp != nullptr) {
        emit tmp->analyzerDisconnected();
        tmp->disconnect();
        // Synchronous, not left to tmp's destructor -- deleteLater() defers
        // destruction to the next event-loop pass, so without this the old
        // analyzer's QSerialPort (if it has one) can still be holding its
        // port open at the exact moment the *new* analyzer's own
        // connectAnalyzer() tries to open a port of its own, right below in
        // this same function, synchronously. Invisible with real hardware
        // (a fresh connection is essentially never to the identical port
        // path a moment after disconnecting from it under a different
        // analyzer type) but trivially reproducible with a single
        // multi-protocol dev target sitting at a fixed path -- confirmed
        // live 2026-09-03 reconnecting the NanoVNA emulator from classic to
        // V2: every write from NanovnaV2Analyzer's very first byte failed
        // with "device not open" because the prior NanovnaAnalyzer's
        // QSerialPort hadn't actually closed yet. closeComPort() is a
        // virtual BaseAnalyzer no-op by default, safe to call unconditionally.
        tmp->closeComPort();
        tmp->deleteLater();
    }
    m_baseAnalyzer = nullptr;
    if (analyzer != nullptr) {
        m_baseAnalyzer = analyzer;
        return true;
    }

    ReDeviceInfo::InterfaceType interfaceType = param.type;
    extern bool g_usbOnly;
    if (g_usbOnly) {
        interfaceType = ReDeviceInfo::HID;
    }

    switch(interfaceType) {
    case ReDeviceInfo::HID:
    {
        m_baseAnalyzer = new HidAnalyzer(this);
    }
        break;
    case ReDeviceInfo::Serial:
    {
        m_baseAnalyzer = new ComAnalyzer(this);
    }
        break;
    case ReDeviceInfo::NANO:
    {
        m_baseAnalyzer = new NanovnaAnalyzer(this);
    }
        break;
    case ReDeviceInfo::NANOV2:
    {
        m_baseAnalyzer = new NanovnaV2Analyzer(this);
    }
        break;
    case ReDeviceInfo::BLE:
    {
        m_baseAnalyzer = new BleAnalyzer(this);
    }
        break;
    default:
        return false;
    }

    return m_baseAnalyzer != nullptr;
}

void AnalyzerPro::on_disconnectDevice()
{
    if (m_baseAnalyzer != nullptr) {
        BaseAnalyzer* tmp = m_baseAnalyzer;
        m_baseAnalyzer = nullptr;
        tmp->deleteLater();
    }
    emit deviceDisconnected();
}

void AnalyzerPro::slotFullInfo(const QString& _info)
{
    int index = _info.indexOf("LIC");
    if (index != -1) {
        // The license level ("LIC1"/"LIC2"/"LIC3") describes the physically
        // connected device's own capability, unrelated to CustomAnalyzer's
        // user-picked "prototype" override -- which is what getModelString()
        // returns while "Use customized analyzer" is checked (see
        // AnalyzerPro::getModelString()). That prototype string is never a
        // real model name (defaults to placeholders like "Custom"), so
        // byName() reliably returned nullptr here whenever a custom analyzer
        // was active, crashing on the very next line. The device was already
        // identified by its serial-number prefix at connection time
        // (SelectDeviceDialog::onApply() -> AnalyzerParameters::setCurrent()),
        // so use that directly instead.
        AnalyzerParameters* par = AnalyzerParameters::current();
        if (par == nullptr)
            return;
        QString _name = _info.mid(index, 4);
        qInfo() << "AnalyzerPro::slotFullInfo" << _name;
        if (_name == "LIC1") {
            m_license = "ADVANCED";
            par->setMaxFq("230000");
        } else if (_name == "LIC2") {
            m_license = "RFE";
            par->setMaxFq("500000");
        } else if (_name == "LIC3") {
            m_license = "PRO";
            par->setMaxFq("690000");
        } else {
            m_license = "BASE";
            par->setMaxFq("70000");
        }
    }
}

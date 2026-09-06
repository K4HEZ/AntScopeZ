#ifndef ANALYZERPRO_H
#define ANALYZERPRO_H

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include "baseanalyzer.h"
#include <updatedialog.h>
#include <crc32.h>
#include "analyzerparameters.h"
#include <analyzer/updater/downloader.h>


class AnalyzerPro : public QObject
{
    Q_OBJECT

    BaseAnalyzer* m_baseAnalyzer=nullptr;
    quint32 m_analyzerModel;
    quint32 m_chartCounter;
    bool m_isMeasuring=false;
    // Set by on_stopMeasure() (Esc, re-clicking Single, Settings' "just in
    // case" call, the watchdog) and cleared the moment a new measurement
    // actually starts (setIsMeasuring(true), centralized here same as
    // kickWatchdog()/stopWatchdog() below) -- the completeMeasurement
    // lambda in connectSignals() checks THIS, not plain m_isMeasuring, to
    // decide whether a NanovnaAnalyzer::completeMeasurement() signal is
    // stale. It has to be a separate flag: m_isMeasuring is *also* cleared
    // by on_newData()'s own chartCounter-vs-dotsNumber completion check,
    // which -- confirmed live, issue #42 -- fires for the NanoVNA fast
    // "scan" path's own final point (the device returns dotsNumber+1
    // points, matching StitchSegment's own "device returns dots+1"
    // comment above), moments *before* the device's trailing "ch>" prompt
    // reaches finishMeasurementSegment(). Using m_isMeasuring there meant
    // that ordinary, non-stopped completion looked identical to a stale
    // post-Stop signal, so measurementCompleteNano() -- the Remote API's
    // only signal for a NanoVNA sweep_done event -- silently never fired
    // even though the sweep had genuinely finished.
    bool m_measurementStopped=false;
    bool m_isContinuos=false;
    quint32 m_dotsNumber;
    bool m_getAnalyzerData=false;

    Downloader *m_downloader;
    UpdateDialog *m_updateDialog;

    QByteArray  *m_pfw;
    qint32 m_INFOSIZE;
    volatile bool m_calibrationMode=false;
    QString m_license;

    // "Stitching" -- splitting a scan that asks for more points than
    // g_analyzerMaxPoints (Settings > General, mainwindow.cpp) into
    // several sequential sweeps, each within that ceiling, concatenated
    // into one continuous dataset. See buildStitchSegments()'s definition
    // (analyzerpro.cpp) for why each segment's own frequency slice is
    // sized proportional to its point count rather than given equal width.
    struct StitchSegment {
        qint64 fqFrom;
        qint64 fqTo;
        qint32 dots; // requested points for this segment (device returns dots+1)
    };
    QVector<StitchSegment> m_stitchSegments; // empty == not stitching
    int m_stitchIndex = 0;
    quint32 m_stitchSegCounter = 0;
    // True except in the exact window between "the current segment's own
    // request just finished" and "the next segment's request has been
    // kicked off" -- see advanceStitchSegmentIfNeeded()'s comment and
    // isStitchedSweepComplete() below for why MainWindow's completion
    // handlers need this distinction.
    bool m_stitchSweepComplete = true;
    void buildStitchSegments(qint64 fqFrom, qint64 fqTo, qint32 totalDots);

    // Scan-silence watchdog -- single-shot, (re)started every time a scan
    // begins or a data point actually arrives, stopped on real
    // completion/cancel. ISSUE #19 (2026-09-04): the start/stop half of
    // this used to be manually threaded into 6+ separate on_measure*()
    // entry points and every completion branch instead of living here in
    // setIsMeasuring() -- the one place m_isMeasuring actually transitions
    // -- so a future new measurement-start path that called
    // setIsMeasuring(true) directly without separately remembering
    // kickWatchdog() got zero hang protection (this had already happened
    // at least once: on_itemDoubleClick()'s "fetch a stored measurement by
    // index" path set m_isMeasuring without ever kicking the watchdog).
    // setIsMeasuring() itself now calls kickWatchdog()/stopWatchdog()
    // directly; the per-point re-kicks in on_newData()/on_newS21Data()/
    // on_newSParamPoint()/on_newUserData() are NOT transitions (m_isMeasuring
    // stays true across an entire scan) so those still kick explicitly,
    // once per arriving point. Kicking on every setIsMeasuring(true) call
    // is safe even for paths that previously didn't (e.g. NANO-connection
    // branches of on_measure*() that skipped the old explicit call) --
    // this codebase already treats an extra/redundant kick as harmless
    // (see PopUpIndicator::setIndicatorVisible()'s identical
    // called-from-both-setIsMeasuring()-and-its-callers pattern just
    // below) -- it only ever adds coverage, never removes it, and
    // on_watchdogTimeout() itself no-ops once whatever started it has
    // already finished (checks m_isMeasuring first).
    //
    // If it ever fires, nothing has answered for
    // g_analyzerTimeoutSec (Settings > General) -- device gone, or busy
    // (held open by another program or another AntScopeZ window). This is
    // the only thing that ever used to time out a scan that never got a
    // single reply; see git history / project notes for the pre-watchdog
    // "spins forever, red indicator stuck" bug this replaces.
    QTimer* m_watchdogTimer = nullptr;
    void kickWatchdog();
    void stopWatchdog();

    // "Draining" -- once a scan is stopped mid-flight, the device (real or
    // emulated) has already committed to sending whatever points it was
    // asked for; neither the classic ASCII nor the V2/LiteVNA64 binary
    // protocol has a wire-level "abort" command (confirmed against
    // AntScopeZ's own client and two independent real V2 clients while
    // building the companion NanoVNA emulator, 2026-09). By default,
    // on_stopMeasure() waits out whatever's still outstanding in the
    // *current* in-flight request (not the whole original scan, if
    // stitched -- no further segments get requested once stopped) and
    // quietly discards it, bounded by the same watchdog timeout used
    // everywhere else. Settings > General's "Use reconnect to drain
    // unwanted data" checkbox (g_reconnectToDrain, mainwindow.cpp) switches to
    // closing and reopening the connection instead -- often faster for a
    // large scan, but not guaranteed to make every device actually discard
    // what it already queued internally, only that we stop listening and
    // resync cleanly on the next connect.
    bool m_isDraining = false;
    quint32 m_drainTotal = 0;
    quint32 m_drainReceived = 0;
    quint32 remainingPointsInCurrentRequest() const;
    void beginDraining(quint32 total);
    void beginReconnectDrain();
    void advanceDraining(); // called from on_newData()/on_newS21Data()/on_newUserData()'s existing "!m_isMeasuring -> stale data" guard
    // Live "Scanning (N/Total points)..." status-bar text -- called from
    // the same three handlers, on every point, while a scan is actually in
    // progress (not draining -- that's advanceDraining()'s own message).
    void announceScanProgress();
    void finishDraining(const QString& reason); // "Stopped by user."/"Stopped by timeout..." -- also the idempotency guard for beginReconnectDrain()'s one-shot analyzerFound() connection
    // Kicks off segment 0 (or the single, non-stitched request if
    // buildStitchSegments() left m_stitchSegments empty) and sets
    // m_dotsNumber to whatever total on_newData()'s completion check
    // actually needs to count to -- see analyzerpro.cpp for the raw-
    // point-count math (each segment returns dots+1, not dots).
    void startStitchedMeasure(qint64 fqFrom, qint64 fqTo, qint32 dotsNumber);
    // Called once per incoming point (on_newData()/on_newUserData()) while
    // m_stitchSegments is non-empty -- a no-op once per point until the
    // current segment's own expected raw points (segDots+1) have all
    // arrived, at which point it kicks off the next segment's sweep. The
    // grand-total completion check in the caller is untouched by this;
    // see startStitchedMeasure()'s comment for why m_dotsNumber already
    // accounts for it.
    void advanceStitchSegmentIfNeeded();
    // Clears stitch state -- called on every completion/cancel (not just
    // when a new stitched scan begins) so a leftover segment plan from a
    // prior stitched scan can't misapply to a later scan that doesn't go
    // through startStitchedMeasure() at all (TDR, Calibration, S21).
    void clearStitchState();

public:
    explicit AnalyzerPro(QObject *parent = nullptr);
    virtual ~AnalyzerPro();

    QString getModelString(void);
    quint32 getModel(void);
    double getVersion() const;
    QString getVersionString() const;
    QString getRevision() const;
    QString getSerialNumber(void) const;
    QString getLicense() const { return m_license; }
    QString getMinFq(void);
    QString getMaxFq(void);

    void updateFirmware (QIODevice *fw);
    void setContinuos(bool isContinuos);
    bool isMeasuring() { return m_isMeasuring; }
    // False only in the narrow window between one stitched segment's own
    // completion and the next segment's request actually being sent --
    // MainWindow's completeMeasurement()/measurementCompleteNano() handlers
    // check this before doing any real finalization (UI resets,
    // Measurements::on_measurementComplete(), setIsMeasuring(false), ...),
    // since each individual segment fires that same completion signal on
    // its own way to being stitched into one continuous result, not just
    // the truly last one.
    bool isStitchedSweepComplete() const { return m_stitchSweepComplete; }
    bool isDraining() const { return m_isDraining; }
    void setIsMeasuring (bool _isMeasuring);
    bool sendData(const QByteArray& _data);
    bool sendCommand(const QString& _cmd);
    void setParseState(int _state);
    int  getParseState();
    ReDeviceInfo::InterfaceType connectionType();
    // Empty for anything but a classic NanoVNA/H/H4 connection -- see
    // NanovnaAnalyzer::scanCapabilityDescription()'s own comment. For
    // MainWindow's connection-info status label.
    QString scanCapabilityDescription() const;

protected:
    bool createDevice(const SelectionParameters& param, BaseAnalyzer* analyzer=nullptr);
    void connectSignals();

signals:
    void newMeasurement(QString, qint64 fqFrom, qint64 fqTo, qint32 dotsNumber);
    void continueMeasurement(qint64 fqFrom, qint64 fqTo, qint32 dotsNumber);
    void measurementComplete();
    void measurementCompleteNano();
    void newData (RawData);
    void newS21Data (S21Data);
    void newSParamPoint (SParamPoint); // real 2-port data, NanoVNA-only today -- guarded passthrough, see on_newSParamPoint()
    void newUserData (RawData, UserData);
    void newUserDataHeader (QStringList);
    void newAnalyzerData (RawData);
    void newMeasurement(QString);
    void analyzerDataStringArrived(QString);
    void analyzerScreenshotDataArrived(QByteArray);
    void analyzerScreenPaletteArrived(QByteArray, quint8 cmd);
    void screenshotComplete(void);
    void updatePercentChanged(int number);
    void signalMeasurementError();
    void deviceDisconnected();
    void updateAutocalibrate5(int _dots, QString _msg);
    void stopAutocalibrate5();
    void analyzerFound (int analyzerIndex);
    void signalAnalyzerError(const QString& msg);
    void signalMatch_12Received(QByteArray data);
    void signalMatch_Profile_B16Received(QByteArray data);
    void crcError();
    void aa30updateComplete();
    // Draining state -- see m_isDraining's own comment. MainWindow gates
    // scan-triggering buttons on drainingChanged() and shows
    // statusMessageChanged()'s text in the status bar; both fire regardless
    // of which stop trigger caused this (Esc, re-clicking Single, closing a
    // dialog mid-scan, ...) since they all already funnel through
    // on_stopMeasure().
    void drainingChanged(bool draining);
    void statusMessageChanged(const QString& text);

public slots:
    bool refreshConnection(); // use SelectionParameters::selected
    void searchAnalyzer();
    void on_analyzerFound (int analyzerIndex) { emit analyzerFound(analyzerIndex); }
    void on_connectDevice(BaseAnalyzer* analyzer=nullptr);
    void on_disconnectDevice();
    void on_measure (qint64 fqFrom_hz, qint64 fqTo_hz, qint32 dotsNumber);
    void on_measureS21 (qint64 fqFrom_hz, qint64 fqTo_hz, qint32 dotsNumber);
    void on_measureUser (qint64 fqFrom_hz, qint64 fqTo_hz, qint32 dotsNumber);
    void on_measureContinuous(qint64 fqFrom_hz, qint64 fqTo_hz, qint32 dotsNumber);
    void on_measureOneFq(QWidget* parent, qint64 fqFrom_hz, qint32 dotsNumber);
    void on_stopMeasure();
    void on_newData(RawData _rawData);
    void on_newS21Data(S21Data _s21Data);
    void on_newSParamPoint(SParamPoint sp);
    void on_newUserData(RawData,UserData);
    void on_newUserDataHeader(QStringList);
    void on_analyzerDataStringArrived(QString str);
    void getAnalyzerData();
    void closeAnalyzerData();

    //void on_itemDoubleClick(QString number, QString dotsNumber, QString name);
    void on_itemDoubleClick(QString info); // idx,from,to,dots:name

    void on_analyzerScreenshotDataArrived(QByteArray arr);
    void on_analyzerScreenPaletteArrived(QByteArray arr, quint8 cmd);
    void on_screenshotComplete(void);
    void makeScreenshot();

    void on_updatePercentChanged(int number);
    void on_downloadInfoComplete();
    void on_downloadFileComplete();
    // Only caller is the "Check for firmware updates" button in Settings --
    // there's no automatic trigger any more (see CHANGELOG.md).
    void on_checkUpdatesBtn_clicked();
    void readFile(QString pathToFw);
    void on_internetUpdate();
    void on_progress(qint64 downloaded,qint64 total);

    void on_measureCalib(int dotsNumber);
    void setCalibrationMode(bool enabled);

    void slotFullInfo(const QString& _info);

private slots:
    void on_watchdogTimeout();

};

#endif // ANALYZERPRO_H

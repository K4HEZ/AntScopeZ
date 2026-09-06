#ifndef BASEANALYZER_H
#define BASEANALYZER_H

#include <QObject>
#include <analyzer/analyzerparameters.h>

class BaseAnalyzer : public QObject
{
    Q_OBJECT
public:
    explicit BaseAnalyzer(QObject *parent = 0);

    virtual QString getVersion() const { return m_version; }
    virtual QString getRevision() const { return m_revision; }
    virtual void setRevision(QString rev){ m_revision = rev; }
    virtual QString getSerial() const { return m_serialNumber; }
    virtual bool update(QIODevice *) { return false; }
    virtual bool openComPort(const QString& portName, quint32 portSpeed) { Q_UNUSED(portSpeed); Q_UNUSED(portName); return false; }
    virtual void closeComPort() {}

    virtual void setIsMeasuring (bool isMeasuring) {m_isMeasuring = isMeasuring;}
    virtual bool isMeasuring (void) const { return m_isMeasuring; }
    virtual void setContinuos(bool continuos){m_isContinuos = continuos;}
    virtual bool getContinuos(void){ return m_isContinuos;}
    virtual void setAnalyzerModel (int model) {m_analyzerModel = model;}
    virtual int getAnalyzerModel (void) const { return m_analyzerModel;}
    virtual void setIsFRXMode(bool _mode=true) { m_isFRX = _mode;}
    virtual bool getIsFRXMode() { return m_isFRX; }
    virtual void setIsS21Mode(bool _mode=true) { m_isS21 = _mode;}
    virtual bool getIsS21Mode() { return m_isS21; }
    virtual qint64 sendData(const QByteArray& ) { return 0; }
    virtual qint64 sendCommand(const QString& ) { return 0; }
    virtual void setParseState(int _state) { m_parseState=_state; } // analyzerparameters.h: enum parse{}
    virtual int getParseState() { return m_parseState; }
    virtual void applyLicense(QString _license) {}
    virtual void setTakeData(bool _state) { m_isTakeData = _state; }
    virtual bool refreshConnection() { return false; }
    virtual bool connectAnalyzer() = 0;
    // True if this backend's stopMeasure() actually tells the device (over
    // the wire) to stop producing more data -- the base implementation
    // below does (sends "off\r"), so this defaults true. NanovnaAnalyzer,
    // NanovnaV2Analyzer, and BleAnalyzer override it false: none of those
    // protocols has a real wire-level abort, so their own stopMeasure()
    // overrides are just a local flag with no wire command (confirmed for
    // all three; see each one's own stopMeasure() comment).
    // AnalyzerPro::on_stopMeasure() uses this to decide whether there's
    // anything left to actually drain after stopping -- assuming every
    // backend behaves like the no-abort ones (as this project's draining
    // feature originally did) means waiting for data that a device just
    // told to stop will never send, guaranteeing a timeout instead of a
    // clean "Ready". Confirmed live 2026-09-04 against a real Match
    // device.
    virtual bool stopCommandAbortsDevice() const { return true; }
    virtual void disconnectAnalyzer() = 0;
    ReDeviceInfo::InterfaceType connectionType() { return m_type; }

    // ISSUE #20 (2026-09-04): "device busy" detection used to be
    // hand-implemented independently per subclass -- HidAnalyzer had a
    // debounced version (m_reportedBusy, since its connect attempt lives
    // inside a repeating searchAnalyzer() poll and would otherwise spam
    // the same popup every tick), ComAnalyzer had a one-shot version with
    // different wording and no dedup member at all (its connect attempt
    // only ever runs once per explicit user click, on a freshly-constructed
    // instance -- see AnalyzerPro::createDevice(), which deletes and
    // recreates m_baseAnalyzer on every connect attempt -- so there was
    // nothing to debounce). Centralized here so both patterns share one
    // mechanism and one base message instead of a third subclass having to
    // hand-copy this again: reportBusy() is safe to call from either a
    // one-shot context (each fresh instance starts with m_reportedBusy
    // false, so it always reports) or a repeating-poll context (call
    // clearBusyDebounce() once the device is no longer seen as busy/
    // matched at all, so a *future* busy episode reports fresh -- see
    // HidAnalyzer::tryConnectMatchingDevice() for that reset).
    //
    // Deliberately NOT wired into BleAnalyzer as part of this pass: Qt's
    // QLowEnergyController::Error enum has no equivalent to
    // QSerialPort::PermissionError/a failed hid_open() -- there's no
    // Qt-visible signal that specifically means "another process already
    // holds this device", as opposed to any other connection failure
    // (device out of range, GATT negotiation failure, etc.). Wiring BLE
    // into this facility with a guessed error-code mapping would risk
    // mislabeling a genuine unrelated connection failure as "busy" --
    // left as a documented gap (see the issue) rather than faked.
    void reportBusy(const QString& detail = QString());
    void clearBusyDebounce() { m_reportedBusy = false; }

signals:
    void analyzerFound (int analyzerIndex);
    void analyzerDisconnected();
    void newData(RawData);
    void newS21Data(S21Data);
    // Real 2-port S-parameter data, per point -- only NanovnaAnalyzer
    // emits this today (see its own comment on SParamPoint vs. the legacy
    // scalar S21Data above). Deliberately not tied into any completion
    // counter here or in AnalyzerPro -- it's a pure side channel to the
    // S21 tab's dataSParam pipeline.
    void newSParamPoint(SParamPoint);
    void newUserDataHeader(QStringList);
    void newUserData(RawData, UserData);
    void analyzerDataStringArrived(QString);
    void analyzerScreenshotDataArrived(QByteArray);
    void analyzerScreenPaletteArrived(QByteArray, quint8 cmd);
    void updatePercentChanged(int);
    void signalFullInfo(const QString& _info);
    void signalMeasurementError();
    void signalAnalyzerError(const QString& msg);
    void signalOk();
    void completeMeasurement(); // emited by NanoVNA
    void receivedMatch_12(QByteArray arr);
    void receivedMatch_ProfileB16(QByteArray arr);
    void crcError();

public slots:
    virtual void searchAnalyzer() {}
    virtual void startMeasure(qint64 fqFrom, qint64 fqTo, int dotsNumber, bool frx=true);
    virtual void startMeasureOneFq(qint64 fqFrom, int dotsNumber, bool frx=true);
    virtual void stopMeasure();
    virtual void getAnalyzerData();
    virtual void getAnalyzerData(QString number);
    virtual void makeScreenshot() {}
    virtual void on_screenshotComplete();
    virtual void on_measurementComplete();
    virtual void continueMeasurement();

protected:
    QString m_version;
    QString m_revision;
    QString m_serialNumber;
    quint32 m_parseState;
    quint32 m_analyzerModel;
    volatile bool m_isMeasuring;
    volatile bool m_isContinuos;
    volatile bool m_isFRX = true;
    volatile bool m_isS21 = false;
    bool m_ok = false;
    QTimer * m_sendTimer;
    bool m_isTakeData = false;
    QByteArray m_incomingBuffer;
    QList <QString> m_stringList;
    ReDeviceInfo::InterfaceType m_type = ReDeviceInfo::WRONG;
    bool m_reportedBusy = false; // see reportBusy()/clearBusyDebounce() above

};

#endif // BASEANALYZER_H

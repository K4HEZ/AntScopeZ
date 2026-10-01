#ifndef ANDROID_SERIALPORT_H
#define ANDROID_SERIALPORT_H

// Android replacement for QSerialPort/QSerialPortInfo: Qt's own backend
// can't open USB serial devices there (no /dev/tty* access for ordinary
// apps), so this talks to usb-serial-for-android through SerialBridge.java.
// Only the API the analyzer classes use is provided.

#include <QByteArray>
#include <QIODevice>
#include <QList>
#include <QMutex>
#include <QString>
#include <QWaitCondition>

class AndroidSerialPortInfo
{
public:
    static QList<AndroidSerialPortInfo> availablePorts();
    quint16 vendorIdentifier() const { return m_vid; }
    quint16 productIdentifier() const { return m_pid; }
    QString portName() const { return m_name; }
    QString description() const { return m_product; }
    QString productName() const { return m_product; }
    QString systemLocation() const { return m_name; }
    QString manufacturer() const { return QString(); }
    QString serialNumber() const { return QString(); }
    bool hasVendorIdentifier() const { return m_vid != 0; }
    bool hasProductIdentifier() const { return m_pid != 0; }

private:
    QString m_name;
    quint16 m_vid = 0;
    quint16 m_pid = 0;
    QString m_product;
};

class AndroidSerialPort : public QIODevice
{
    Q_OBJECT
public:
    enum BaudRate { Baud9600 = 9600, Baud19200 = 19200, Baud38400 = 38400, Baud57600 = 57600, Baud115200 = 115200 };
    enum DataBits { Data5 = 5, Data6, Data7, Data8 };
    enum Parity { NoParity = 0 };
    enum StopBits { OneStop = 1 };
    enum FlowControl { NoFlowControl = 0 };
    enum SerialPortError { NoError, DeviceNotFoundError, PermissionError, OpenError, WriteError, ResourceError };

    explicit AndroidSerialPort(QObject* parent = nullptr);
    ~AndroidSerialPort() override;

    void setPortName(const QString& name) { m_portName = name; }
    QString portName() const { return m_portName; }
    bool setBaudRate(qint32 baud) { m_baud = baud; return true; }
    qint32 baudRate() const { return m_baud; }
    // Fixed 8N1, no flow control -- all this app uses.
    bool setDataBits(DataBits) { return true; }
    bool setParity(Parity) { return true; }
    bool setStopBits(StopBits) { return true; }
    bool setFlowControl(FlowControl) { return true; }
    SerialPortError error() const { return m_error; }

    bool open(OpenMode mode) override;
    void close() override;
    qint64 bytesAvailable() const override;
    bool waitForReadyRead(int msecs) override;
    bool isSequential() const override { return true; }

    // Called from the JNI callbacks (any thread).
    void deliver(const QByteArray& data);
    void deliverError(const QString& message);

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    QString m_portName;
    qint32 m_baud = 115200;
    int m_handle = 0;
    SerialPortError m_error = NoError;
    mutable QMutex m_mutex;
    QWaitCondition m_dataArrived;
    QByteArray m_rx;
};

#endif // ANDROID_SERIALPORT_H

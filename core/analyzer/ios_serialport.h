#ifndef IOS_SERIALPORT_H
#define IOS_SERIALPORT_H

// iOS stand-in for QSerialPort/QSerialPortInfo: Qt doesn't ship a
// SerialPort module for iOS at all (no general-purpose USB/serial device
// access for an ordinary, non-MFi-certified app there -- see BUILDINFO.md's
// iOS section), so this is a pure no-op -- always reports zero available
// ports, open() always fails. Only the API the analyzer classes use is
// provided (see android_serialport.h, the equivalent Android stand-in).

#include <QIODevice>
#include <QList>
#include <QString>

class IosSerialPortInfo
{
public:
    static QList<IosSerialPortInfo> availablePorts() { return {}; }
    quint16 vendorIdentifier() const { return 0; }
    quint16 productIdentifier() const { return 0; }
    QString portName() const { return QString(); }
    QString description() const { return QString(); }
    QString productName() const { return QString(); }
    QString systemLocation() const { return QString(); }
    QString manufacturer() const { return QString(); }
    QString serialNumber() const { return QString(); }
    bool hasVendorIdentifier() const { return false; }
    bool hasProductIdentifier() const { return false; }
};

class IosSerialPort : public QIODevice
{
    Q_OBJECT
public:
    enum BaudRate { Baud9600 = 9600, Baud19200 = 19200, Baud38400 = 38400, Baud57600 = 57600, Baud115200 = 115200 };
    enum DataBits { Data5 = 5, Data6, Data7, Data8 };
    enum Parity { NoParity = 0 };
    enum StopBits { OneStop = 1 };
    enum FlowControl { NoFlowControl = 0 };
    enum SerialPortError { NoError, DeviceNotFoundError, PermissionError, OpenError, WriteError, ResourceError };

    explicit IosSerialPort(QObject* parent = nullptr) : QIODevice(parent) {}

    void setPortName(const QString&) {}
    QString portName() const { return QString(); }
    bool setBaudRate(qint32) { return true; }
    qint32 baudRate() const { return 0; }
    // Fixed 8N1, no flow control -- matches android_serialport.h; never
    // actually reached since open() always fails.
    bool setDataBits(DataBits) { return true; }
    bool setParity(Parity) { return true; }
    bool setStopBits(StopBits) { return true; }
    bool setFlowControl(FlowControl) { return true; }
    SerialPortError error() const { return DeviceNotFoundError; }

    bool open(OpenMode) override { return false; }
    void close() override {}
    qint64 bytesAvailable() const override { return 0; }
    bool waitForReadyRead(int) override { return false; }
    bool isSequential() const override { return true; }

protected:
    qint64 readData(char*, qint64) override { return -1; }
    qint64 writeData(const char*, qint64) override { return -1; }
};

#endif // IOS_SERIALPORT_H

#include "android_serialport.h"

#include <QCoreApplication>
#include <QHash>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QPointer>
#include <QElapsedTimer>

namespace {
const char* const kBridge = "io/github/k4hez/antscopez/SerialBridge";

// Handle -> port, for the JNI callbacks.
QMutex g_portsMutex;
QHash<int, QPointer<AndroidSerialPort>> g_ports;

QJniObject androidContext()
{
    return QJniObject(QNativeInterface::QAndroidApplication::context());
}

void nativeData(JNIEnv* env, jclass, jint handle, jbyteArray data)
{
    const jsize len = env->GetArrayLength(data);
    QByteArray bytes(len, Qt::Uninitialized);
    env->GetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(bytes.data()));
    QMutexLocker lock(&g_portsMutex);
    if (AndroidSerialPort* port = g_ports.value(handle).data())
        port->deliver(bytes);
}

void nativeError(JNIEnv* env, jclass, jint handle, jstring message)
{
    const char* chars = env->GetStringUTFChars(message, nullptr);
    const QString text = QString::fromUtf8(chars);
    env->ReleaseStringUTFChars(message, chars);
    QMutexLocker lock(&g_portsMutex);
    if (AndroidSerialPort* port = g_ports.value(handle).data())
        port->deliverError(text);
}

void registerNatives()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    const JNINativeMethod methods[] = {
        {"nativeData", "(I[B)V", reinterpret_cast<void*>(nativeData)},
        {"nativeError", "(ILjava/lang/String;)V", reinterpret_cast<void*>(nativeError)},
    };
    QJniEnvironment env;
    env.registerNativeMethods(kBridge, methods, 2);
}
} // namespace

QList<AndroidSerialPortInfo> AndroidSerialPortInfo::availablePorts()
{
    registerNatives();
    QList<AndroidSerialPortInfo> result;
    QJniObject context = androidContext();
    QJniObject array = QJniObject::callStaticObjectMethod(
        kBridge, "list", "(Landroid/content/Context;)[Ljava/lang/String;", context.object<jobject>());
    if (!array.isValid())
        return result;

    QJniEnvironment env;
    auto jarray = array.object<jobjectArray>();
    const jsize count = env->GetArrayLength(jarray);
    for (jsize i = 0; i < count; ++i) {
        QJniObject item(env->GetObjectArrayElement(jarray, i));
        // "deviceName|vid|pid|product"
        const QStringList parts = item.toString().split('|');
        if (parts.size() < 4)
            continue;
        AndroidSerialPortInfo info;
        info.m_name = parts.at(0);
        info.m_vid = parts.at(1).toUShort();
        info.m_pid = parts.at(2).toUShort();
        info.m_product = parts.at(3);
        result.append(info);
    }
    return result;
}

AndroidSerialPort::AndroidSerialPort(QObject* parent)
    : QIODevice(parent)
{
}

AndroidSerialPort::~AndroidSerialPort()
{
    close();
}

// Never blocks on the USB permission dialog (it runs on this same thread):
// the first call requests it and fails with PermissionError; callers retry.
bool AndroidSerialPort::open(OpenMode mode)
{
    registerNatives();
    m_error = NoError;
    if (m_handle != 0)
        return true;

    const jint result = QJniObject::callStaticMethod<jint>(
        kBridge, "open", "(Landroid/content/Context;Ljava/lang/String;I)I",
        androidContext().object<jobject>(),
        QJniObject::fromString(m_portName).object<jstring>(), jint(m_baud));

    if (result <= 0) {
        switch (result) {
        case -1:
            m_error = DeviceNotFoundError;
            setErrorString(tr("Device %1 not found").arg(m_portName));
            break;
        case -2:
            m_error = PermissionError;
            setErrorString(tr("Waiting for USB permission"));
            break;
        default:
            m_error = OpenError;
            setErrorString(tr("Could not open %1").arg(m_portName));
            break;
        }
        return false;
    }

    m_handle = result;
    {
        QMutexLocker lock(&g_portsMutex);
        g_ports.insert(m_handle, this);
    }
    return QIODevice::open(mode);
}

void AndroidSerialPort::close()
{
    if (m_handle != 0) {
        {
            QMutexLocker lock(&g_portsMutex);
            g_ports.remove(m_handle);
        }
        QJniObject::callStaticMethod<void>(kBridge, "close", "(I)V", jint(m_handle));
        m_handle = 0;
    }
    {
        QMutexLocker lock(&m_mutex);
        m_rx.clear();
    }
    if (isOpen())
        QIODevice::close();
}

qint64 AndroidSerialPort::bytesAvailable() const
{
    QMutexLocker lock(&m_mutex);
    return m_rx.size() + QIODevice::bytesAvailable();
}

bool AndroidSerialPort::waitForReadyRead(int msecs)
{
    QMutexLocker lock(&m_mutex);
    if (!m_rx.isEmpty())
        return true;
    return m_dataArrived.wait(&m_mutex, msecs) && !m_rx.isEmpty();
}

void AndroidSerialPort::deliver(const QByteArray& data)
{
    {
        QMutexLocker lock(&m_mutex);
        m_rx.append(data);
    }
    m_dataArrived.wakeAll();
    QMetaObject::invokeMethod(this, [this]() { emit readyRead(); }, Qt::QueuedConnection);
}

void AndroidSerialPort::deliverError(const QString& message)
{
    QMetaObject::invokeMethod(this, [this, message]() {
        m_error = ResourceError;
        setErrorString(message);
    }, Qt::QueuedConnection);
}

qint64 AndroidSerialPort::readData(char* data, qint64 maxSize)
{
    QMutexLocker lock(&m_mutex);
    const qint64 n = qMin<qint64>(maxSize, m_rx.size());
    memcpy(data, m_rx.constData(), n);
    m_rx.remove(0, n);
    return n;
}

qint64 AndroidSerialPort::writeData(const char* data, qint64 maxSize)
{
    if (m_handle == 0)
        return -1;
    QJniEnvironment env;
    jbyteArray array = env->NewByteArray(jsize(maxSize));
    env->SetByteArrayRegion(array, 0, jsize(maxSize), reinterpret_cast<const jbyte*>(data));
    const jint written = QJniObject::callStaticMethod<jint>(
        kBridge, "write", "(I[B)I", jint(m_handle), array);
    env->DeleteLocalRef(array);
    if (written < 0) {
        m_error = WriteError;
        setErrorString(tr("Write failed"));
        return -1;
    }
    return written;
}

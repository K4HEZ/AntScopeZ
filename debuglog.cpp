#include "debuglog.h"
#include "filedialog.h"
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QDateTime>

namespace {

bool g_serialEnabled = false;
bool g_usbHidEnabled = false;
bool g_bleEnabled = false;
bool g_nanovnaEnabled = false;
bool g_bleShowPings = true;

// Kept open across calls rather than reopened per packet -- BLE especially
// can be chatty during a sweep. Reopened when the calendar date changes
// (g_logFileDate tracks which day's file is currently open).
QFile g_logFile;
QString g_logFileDate;

// Traditional hexdump -C style: offset, 16 bytes/line (8-byte gap in the
// middle), uppercase hex (matches the .toHex(' ').toUpper() convention the
// commented-out BLE debug lines already used), ASCII column with '.' for
// anything non-printable. Every line (not just the header above it) ends
// with a direction marker -- ">>" for TX, "<<" for RX -- so a line is
// self-identifying even skimmed out of context, e.g. mid-scroll through a
// long interleaved log.
QString hexDump(const QByteArray &data, const QString &directionMarker)
{
    QString out;
    for (int offset = 0; offset < data.size(); offset += 16) {
        QByteArray chunk = data.mid(offset, 16);
        QString line = QString("%1  ").arg(offset, 8, 16, QChar('0'));
        QString ascii;
        for (int i = 0; i < 16; i++) {
            if (i < chunk.size()) {
                unsigned char c = static_cast<unsigned char>(chunk.at(i));
                line += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
                ascii += (c >= 0x20 && c < 0x7F) ? QChar(c) : QChar('.');
            } else {
                line += "   ";
            }
            if (i == 7)
                line += " ";
        }
        line += " |" + ascii + "| " + directionMarker + "\n";
        out += line;
    }
    return out;
}

void ensureLogFileOpen()
{
    QString today = QDate::currentDate().toString("yyyyMMdd");
    if (g_logFile.isOpen() && g_logFileDate == today)
        return;

    if (g_logFile.isOpen())
        g_logFile.close();

    g_logFileDate = today;
    g_logFile.setFileName(FileDialog::userDataDir() + "/Debug-" + today + ".log");
    // Open failure is handled by the isOpen() check writeEntry() already
    // does right after calling this -- nothing more to do with the result
    // here, just silencing the [[nodiscard]] warning explicitly.
    (void)g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void writeEntry(const QString &tag, const QString &direction, const QByteArray &data)
{
    ensureLogFileOpen();
    if (!g_logFile.isOpen())
        return;

    QString marker = (direction == "TX") ? ">>" : "<<";

    QTextStream ts(&g_logFile);
    ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
       << " " << tag << " " << direction << " (" << data.size() << " bytes)\n"
       << hexDump(data, marker) << "\n";
    ts.flush();
    g_logFile.flush();
}

} // namespace

void DebugLog::setSerialEnabled(bool enabled) { g_serialEnabled = enabled; }
void DebugLog::setUsbHidEnabled(bool enabled) { g_usbHidEnabled = enabled; }
void DebugLog::setBleEnabled(bool enabled) { g_bleEnabled = enabled; }
void DebugLog::setNanovnaEnabled(bool enabled) { g_nanovnaEnabled = enabled; }
void DebugLog::setBleShowPings(bool show) { g_bleShowPings = show; }

void DebugLog::serialTx(const QByteArray &data)
{
    if (g_serialEnabled) writeEntry("SERIAL", "TX", data);
}

void DebugLog::serialRx(const QByteArray &data)
{
    if (g_serialEnabled) writeEntry("SERIAL", "RX", data);
}

void DebugLog::usbHidTx(const QByteArray &data)
{
    if (g_usbHidEnabled) writeEntry("USB/HID", "TX", data);
}

void DebugLog::usbHidRx(const QByteArray &data)
{
    if (g_usbHidEnabled) writeEntry("USB/HID", "RX", data);
}

void DebugLog::bleTx(const QByteArray &data, bool isPing)
{
    if (!g_bleEnabled) return;
    if (isPing && !g_bleShowPings) return;
    writeEntry("BLE", "TX", data);
}

void DebugLog::bleRx(const QByteArray &data, bool isPing)
{
    if (!g_bleEnabled) return;
    if (isPing && !g_bleShowPings) return;
    writeEntry("BLE", "RX", data);
}

void DebugLog::nanovnaTx(const QByteArray &data)
{
    if (g_nanovnaEnabled) writeEntry("NANOVNA", "TX", data);
}

void DebugLog::nanovnaRx(const QByteArray &data)
{
    if (g_nanovnaEnabled) writeEntry("NANOVNA", "RX", data);
}

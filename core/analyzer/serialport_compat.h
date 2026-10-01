#ifndef SERIALPORT_COMPAT_H
#define SERIALPORT_COMPAT_H

// SerialPort/SerialPortInfo: QSerialPort/QSerialPortInfo everywhere except
// Android, where android_serialport.h stands in (see there).

#include <QtGlobal>

#ifdef Q_OS_ANDROID
#include "android_serialport.h"
using SerialPort = AndroidSerialPort;
using SerialPortInfo = AndroidSerialPortInfo;
#else
#include <QtSerialPort/QSerialPort>
#include <QSerialPortInfo>
using SerialPort = QSerialPort;
using SerialPortInfo = QSerialPortInfo;
#endif

#endif // SERIALPORT_COMPAT_H

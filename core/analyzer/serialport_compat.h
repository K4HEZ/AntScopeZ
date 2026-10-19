#ifndef SERIALPORT_COMPAT_H
#define SERIALPORT_COMPAT_H

// SerialPort/SerialPortInfo: QSerialPort/QSerialPortInfo everywhere except
// Android and iOS, where android_serialport.h/ios_serialport.h stand in
// (see there).

#include <QtGlobal>

#ifdef Q_OS_ANDROID
#include "android_serialport.h"
using SerialPort = AndroidSerialPort;
using SerialPortInfo = AndroidSerialPortInfo;
#elif defined(Q_OS_IOS)
#include "ios_serialport.h"
using SerialPort = IosSerialPort;
using SerialPortInfo = IosSerialPortInfo;
#else
#include <QtSerialPort/QSerialPort>
#include <QSerialPortInfo>
using SerialPort = QSerialPort;
using SerialPortInfo = QSerialPortInfo;
#endif

#endif // SERIALPORT_COMPAT_H

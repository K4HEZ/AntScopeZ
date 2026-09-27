#include "measurementdata.h"
#include "calibration.h"
#include "rfmath.h"
#include <QCoreApplication>

static void storeAt(QVector<RawData>& v, const RawData& p, int index, bool replace)
{
    if (replace && index < v.size())
        v[index] = p;
    else
        v.append(p);
}

bool MeasurementData::addPoint(const RawData& raw, int index, bool replace, double Z0,
                               Calibration* calibration, RawData* calibrated)
{
    storeAt(dataRX, raw, index, replace);

    if (applied.osl) {
        storeAt(dataRXCalib, raw, index, replace);
        if (calibrated)
            *calibrated = raw;
        return true;
    }

    if (calibration == nullptr || !calibration->getCalibrationPerformed())
        return false;

    RawData c = raw;
    if (qIsNaN(c.r) || (c.r < 0.001))
        c.r = 0.01;
    if (qIsNaN(c.x))
        c.x = 0;
    Complex z = RfMath::calibratedZ(c.fq, c.r, c.x, Z0, calibration);
    c.r = z.real();
    c.x = z.imag();
    storeAt(dataRXCalib, c, index, replace);
    if (calibrated)
        *calibrated = c;
    return true;
}

void MeasurementData::recalibrate(double Z0, Calibration* calibration)
{
    dataRXCalib.clear();
    if (calibration == nullptr || !calibration->getCalibrationPerformed())
        return;
    for (const RawData& p : dataRX) {
        RawData c = p;
        if (qIsNaN(c.r) || (c.r < 0.001))
            c.r = 0.01;
        if (qIsNaN(c.x))
            c.x = 0;
        Complex z = RfMath::calibratedZ(c.fq, c.r, c.x, Z0, calibration);
        c.r = z.real();
        c.x = z.imag();
        dataRXCalib.append(c);
    }
}

Corrections MeasurementData::shownCorrections() const
{
    Corrections c = corrections;
    c.osl = applied.osl || (corrections.osl && hasCalibrated());
    if (applied.cableMode != 0) {
        c.cableMode = applied.cableMode;
        c.cable = applied.cable;
        c.cableLossFqMHz = applied.cableLossFqMHz;
    }
    return c;
}

QVector<RawData> MeasurementData::exportPoints(bool osl, int cableMode, const RfMath::CableParams& cable) const
{
    QVector<RawData> out = (osl && !applied.osl && hasCalibrated()) ? dataRXCalib : dataRX;
    if (cableMode != 0 && applied.cableMode == 0) {
        for (RawData& p : out) {
            // Same transform and clamps as the cable-corrected charts.
            Complex z = RfMath::cableTransform(p.fq, p.r, p.x, cable, cableMode == 1);
            p.r = z.real();
            p.x = z.imag();
            if (qIsNaN(p.r) || (p.r < 0.001))
                p.r = 0.01;
            if (qIsNaN(p.x))
                p.x = 0;
        }
    }
    return out;
}

static QString cableLength(const Corrections& c, bool metric)
{
    double len = metric ? c.cable.lengthFeet / FEETINMETER : c.cable.lengthFeet;
    return QString("%1 %2").arg(len, 0, 'f', 1).arg(metric ? "m" : "ft");
}

QString Corrections::summary(bool metric) const
{
    QStringList parts;
    if (osl)
        parts << QCoreApplication::translate("Corrections", "OSL");
    if (cableMode != 0)
        parts << QCoreApplication::translate("Corrections", "cable %1%2")
                     .arg(cableMode == 1 ? QString::fromUtf8("\u2212") : "+", cableLength(*this, metric));
    return parts.isEmpty() ? QCoreApplication::translate("Corrections", "none")
                           : parts.join(QString::fromUtf8(" \u00b7 "));
}

QString Corrections::tag() const
{
    QStringList parts;
    if (osl)
        parts << QCoreApplication::translate("Corrections", "OSL");
    if (cableMode != 0)
        parts << (cableMode == 1 ? QString::fromUtf8("\u2212C") : QString("+C"));
    return parts.join(' ');
}

QString Corrections::details(bool metric) const
{
    QStringList lines;
    lines << (osl ? QCoreApplication::translate("Corrections", "OSL calibration applied")
                  : QCoreApplication::translate("Corrections", "No OSL calibration"));
    if (cableMode == 0) {
        lines << QCoreApplication::translate("Corrections", "No cable correction");
    } else {
        lines << QCoreApplication::translate("Corrections", "Cable %1: %2, velocity factor %3, R0 %4 ohm")
                     .arg(cableMode == 1 ? QCoreApplication::translate("Corrections", "subtracted")
                                         : QCoreApplication::translate("Corrections", "added"))
                     .arg(cableLength(*this, metric))
                     .arg(cable.velFactor, 0, 'f', 3)
                     .arg(cable.resistance, 0, 'f', 1);
    }
    return lines.join('\n');
}

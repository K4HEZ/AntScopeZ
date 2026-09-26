#include "measurementdata.h"
#include "calibration.h"
#include "rfmath.h"

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

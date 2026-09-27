#ifndef MARKERMATH_H
#define MARKERMATH_H

#include <QVector>
#include <float.h>
#include "measurementdata.h"

// UI-free chart/marker values. The charts and the markers both use
// chartPoint(), so a marker always reads what the chart draws.
namespace MarkerMath {

// Which chart series a point belongs to. Each has its own (historical)
// formulas and clamps -- see chartPoint().
enum class Series {
    Raw,        // measured points
    Calibrated, // OSL-corrected points (dataRXCalib)
    FarEnd,     // cable added/subtracted points
};

struct ChartPoint {
    double swr = 1, rl = 0, phase = 0, rho = 0;
    double r = 0, x = 0, z = 0;
    double rpar = 0, xpar = 0, zpar = 0;
    // Raw only: unclamped VSWR/RL, the Calibrated series' fallback.
    double rawSwr = 1, rawRl = 0;
};

// Values the charts plot for one point. Raw: when SWR can't be computed
// (pure reactance, or an exact Z0 match) SWR/RL repeat prevSwr/prevRl.
// Calibrated: prevSwr/prevRl are the Raw point's rawSwr/rawRl.
ChartPoint chartPoint(const RawData& p, double Z0, Series series,
                      double prevSwr, double prevRl);

// Values at a marker frequency, interpolated between the two bracketing
// points. DBL_MAX == no data (outside the sweep, or no 2-port data).
struct Values {
    bool found = false;
    double swr = DBL_MAX, rl = DBL_MAX, phase = DBL_MAX, rho = DBL_MAX;
    double r = DBL_MAX, x = DBL_MAX, zmod = DBL_MAX;
    double rpar = DBL_MAX, xpar = DBL_MAX, zpar = DBL_MAX;
    double l = DBL_MAX, c = DBL_MAX, lpar = DBL_MAX, cpar = DBL_MAX;
    double s21 = DBL_MAX, s21Phase = DBL_MAX, s12 = DBL_MAX, s12Phase = DBL_MAX;
};

// fqKHz as the charts' x axis. farEnd: the cable-corrected points to read
// from instead of `m`'s own (nullptr when cable add/subtract is off).
Values valuesAt(const MeasurementData& m, const QVector<RawData>* farEnd,
                bool calibrated, double fqKHz, double Z0);

// SWR series (kHz, swr) of whatever the user is looking at: farEnd if
// given, else calibrated or raw points.
QVector<QPair<double, double>> swrSeries(const MeasurementData& m, const QVector<RawData>* farEnd,
                                         bool calibrated, double Z0);

// Frequency (kHz) of the lowest SWR; false if there are no points.
bool lowestSwr(const QVector<QPair<double, double>>& swr, double* fqKHz);

// centerKHz / 2:1-SWR bandwidth around the point nearest centerKHz;
// 0 if either 2:1 crossing is outside the sweep.
double qFactor(const QVector<QPair<double, double>>& swr, double centerKHz);

double regulate(double val, double limit);

}

#endif // MARKERMATH_H

#include "tdranalysis.h"

#include <QtMath>

TdrAnalysis TdrAnalysis::compute(const QVector<RawData>& data, double velocityFactor, bool metric,
                                 TdrWindow window, double kaiserBeta, double z0)
{
    TdrAnalysis a;
    TdrMath::Result r = TdrMath::compute(data, velocityFactor, metric, window, kaiserBeta, z0);
    if (r.fftSize == 0 || r.impulse.isEmpty())
        return a;

    const int len = r.fftSize;
    a.valid = true;
    a.range = r.range;
    a.xStep = r.range / len;
    a.impulse = QList<double>(r.impulse.begin(), r.impulse.end());
    a.step = QList<double>(r.step.begin(), r.step.end());
    a.impedance = QList<double>(r.impedance.begin(), r.impedance.end());

    const int n = a.impulse.size();
    int best = 0;
    for (int i = 1; i < n; ++i) {
        if (qAbs(a.impulse[i]) > qAbs(a.impulse[best]))
            best = i;
    }

    // Impedance is read from where the step response has settled after the
    // peak, not at the impulse peak itself (see Measurements::findTdrPeak()).
    int zIndex = best;
    double bestStep = a.step.value(best);
    for (int i = best + 1; i < n && i < a.step.size(); ++i) {
        if (qAbs(a.step[i]) > qAbs(bestStep)) {
            bestStep = a.step[i];
            zIndex = i;
        }
    }

    a.peakFound = true;
    a.peakDistance = best * a.xStep;
    a.peakAmplitude = a.impulse[best];
    a.peakImpedance = a.impedance.value(zIndex);
    const double lastDistance = (n - 1) * a.xStep;
    a.peakNearRangeEdge = lastDistance > 0 && a.peakDistance >= 0.95 * lastDistance;
    return a;
}

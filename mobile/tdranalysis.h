#ifndef TDRANALYSIS_H
#define TDRANALYSIS_H

#include <QList>
#include <QVector>

#include <rfmath.h>
#include <tdrmath.h>

// Traces and strongest-reflection analysis for one TDR sweep. The
// reflection logic mirrors the desktop's Measurements::findTdrPeak() and
// TdrScanPanel::refreshResult().
struct TdrAnalysis
{
    // Below this impulse amplitude a peak is treated as noise.
    static constexpr double kNoiseFloor = 0.015;

    bool valid = false;
    double range = 0;    // unambiguous range, in the chosen units
    double xStep = 0;    // distance per trace index
    QList<double> impulse;
    QList<double> step;
    QList<double> impedance;

    bool peakFound = false;
    double peakDistance = 0;
    double peakAmplitude = 0;
    double peakImpedance = 0;
    bool peakNearRangeEdge = false;

    static TdrAnalysis compute(const QVector<RawData>& data, double velocityFactor, bool metric,
                               TdrWindow window, double kaiserBeta, double z0);
};

#endif // TDRANALYSIS_H

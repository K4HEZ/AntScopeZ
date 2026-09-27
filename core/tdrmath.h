#ifndef TDRMATH_H
#define TDRMATH_H

#include <QVector>
#include <analyzer/analyzerparameters.h>
#include "rfmath.h" // FEETINMETER

#define TDR_MAXARRAY 20000
//#define TDR_MAXARRAY 32768
//#define TDR_MAXARRAY 65536

// One-sided taper applied to a TDR scan's frequency-domain data before the
// inverse FFT -- full gain at DC, tapering toward the window's minimum at
// the top of the measured band (see TdrMath::windowCoeff()). Default stays
// Hamming, the original hardcoded window.
enum class TdrWindow { Rectangular, Hamming, Hann, Blackman, Kaiser };

// UI-free TDR math: sweep -> impulse/step/impedance vs distance.
namespace TdrMath {

// Pure function of scan parameters. Used for real captured data and for the
// TDR scan panel's pre-scan preview, so both agree.
struct Estimate {
    // Max unambiguous range, c x VF x (N-1) / (2 x BW) (hand-verified
    // 2026-08-20). Depends on dot count via FFT-size rounding. Units per
    // `metric` (m or ft), same as the TDR chart's x-axis.
    double unambiguousRange = 0;
    // True resolving power, c x VF / (2 x BW); independent of dot count.
    // Not the chart's per-point spacing (~8x finer from zero-padding).
    double resolution = 0;
    // Raw (pre unit-conversion) chart step the range is derived from.
    double chartStep = 0;
    int fftSize = 0;
};

Estimate estimateRaw(int asize, double minfqMHz, double maxfqMHz, double velFactor, bool metric);
// Preview before any scan: asize = dots+1 (a device returns dots+1 points),
// minfq = 0 (TDR needs data starting within 0.1 MHz of DC).
Estimate estimate(int dots, double topFreqMHz, double velFactor, bool metric);

// Window-shape-and-normalization coefficient for bin i of n.
double windowCoeff(TdrWindow type, double beta, int i, int n);

// In-place radix-2 FFT (Inverse != 0 for IFFT, normalized).
void fft(float real[], float imag[], int length, int inverse = 0);

struct Result {
    int fftSize = 0; // 0 == no valid TDR for this data
    double range = 0; // == Estimate::unambiguousRange
    QVector<double> impulse;
    QVector<double> step;
    QVector<double> impedance;
};

// Full TDR of one sweep. `data` must start near DC; returns fftSize 0
// otherwise, or if there aren't enough points.
Result compute(const QVector<RawData>& data, double velFactor, bool metric,
               TdrWindow window, double kaiserBeta, double z0);

}

#endif // TDRMATH_H

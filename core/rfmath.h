#ifndef RFMATH_H
#define RFMATH_H

#include <complex>
#include <QtGlobal>
#include <analyzer/analyzerparameters.h>

#ifndef SPEEDOFLIGHT
#define SPEEDOFLIGHT 299792458.0
#endif

#ifndef FEETINMETER
#define FEETINMETER 3.2808399
#endif

typedef std::complex <double> Complex;

class Calibration;

// UI-free RF math on single measurement points.
namespace RfMath {

// VSWR/return loss of R+jX against Z0. Returns 0 (outputs untouched) for a
// degenerate point. SWR is clamped to [1, 200].
quint32 computeSWR(double Z0, double R, double X, double *VSWR, double *RL);
double computeZ(double R, double X);
// Series R+jX -> parallel Rp || jXp (R clamped to >= 0.01 ohm, NaN X -> 0).
void parallel(double R, double X, double& Rpar, double& Xpar);
// Normalized R/X -> Smith chart coordinates (radius 6).
void smithPoint(double Rnorm, double Xnorm, double &x, double &y);

// OSL-corrected impedance of one point; `calibration` must have a
// calibration performed.
Complex calibratedZ(double fq, double R, double X, double Z0, Calibration* calibration);

// Every derived value for one raw point; calibData too if `calibration`
// has an OSL calibration performed.
void prepareGraphs(const RawData& raw, double Z0, Calibration* calibration,
                   GraphData& data, GraphData& calibData);

// Transmission-line parameters for adding/subtracting a cable.
struct CableParams {
    double velFactor = 0.66;
    double resistance = 50;      // characteristic impedance, ohms
    double lossConductive = 0;   // K1
    double lossDielectric = 0;   // K2
    int lossUnits = 0;           // index into Settings' loss-units combo
    bool lossAtAnyFq = false;    // loss given at one fq, not K1/K2 per MHz
    double lengthFeet = 0;
};
// R+jX seen through the cable (subtract: de-embed it instead). R >= 0.0001.
Complex cableTransform(double fqMHz, double R, double X, const CableParams& cable, bool subtract);

// Touchstone value pair -> complex, iFormat 1 MA, 2 RI, 3 DB.
std::complex<double> sparamFromFormat(int iFormat, double v1, double v2);
// 2-port Z parameters -> S parameters at reference z0.
SParamPoint zToSParam(double fq, std::complex<double> z11, std::complex<double> z21,
                      std::complex<double> z12, std::complex<double> z22, double z0);
// Running phase unwrap; state lives in the three reference args.
double unwrapPhaseDeg(double rawDeg, bool& havePrev, double& prevRaw, double& prevUnwrapped);

}

#endif // RFMATH_H

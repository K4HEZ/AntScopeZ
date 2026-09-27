#include "markermath.h"
#include "rfmath.h"
#include <math.h>

double MarkerMath::regulate(double val, double limit)
{
    if (val < -limit)
        return -limit;
    if (val > limit)
        return limit;
    return val;
}

static void reflection(double R, double X, double Z0, double& phase, double& rho)
{
    double Rnorm = R/Z0;
    double Xnorm = X/Z0;
    double Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
    double RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
    double RhoImag = 2*Xnorm/Denom;
    phase = atan2(RhoImag, RhoReal) / M_PI * 180.0;
    rho = sqrt(RhoReal*RhoReal+RhoImag*RhoImag);
}

static void clampRX(double& R, double& X)
{
    if (qIsNaN(R) || (R<0.001))
        R = 0.01;
    if (qIsNaN(X))
        X = 0;
}

MarkerMath::ChartPoint MarkerMath::chartPoint(const RawData& p, double Z0, Series series,
                                              double prevSwr, double prevRl)
{
    ChartPoint c;
    if (series == Series::Raw)
    {
        double VSWR, RL;
        if (RfMath::computeSWR(Z0, p.r, p.x, &VSWR, &RL) != 1) {
            VSWR = prevSwr;
            RL = prevRl;
        }
        c.rawSwr = VSWR;
        c.rawRl = RL;
        c.swr = regulate(VSWR, MAX_SWR);
        c.rl = RL;
        c.r = regulate(p.r, VALUE_LIMIT);
        c.x = regulate(p.x, VALUE_LIMIT);
        c.z = regulate(RfMath::computeZ(p.r, p.x), VALUE_LIMIT);
        double R = p.r, X = p.x;
        clampRX(R, X);
        double Rpar, Xpar;
        RfMath::parallel(R, X, Rpar, Xpar);
        c.rpar = regulate(Rpar, VALUE_LIMIT);
        c.xpar = regulate(Xpar, VALUE_LIMIT);
        c.zpar = regulate(RfMath::computeZ(Rpar, Xpar), VALUE_LIMIT);
        reflection(R, X, Z0, c.phase, c.rho);
    }
    else if (series == Series::Calibrated)
    {
        double calR = p.r, calX = p.x;
        double VSWR = prevSwr, RL = prevRl;
        RfMath::computeSWR(Z0, calR, calX, &VSWR, &RL);
        c.swr = (VSWR > MAX_SWR) ? MAX_SWR : VSWR;
        c.rl = RL;
        c.r = regulate(calR, VALUE_LIMIT);
        c.x = regulate(calX, VALUE_LIMIT);
        c.z = regulate(RfMath::computeZ(calR, calX), VALUE_LIMIT);
        double calRpar, calXpar;
        RfMath::parallel(calR, calX, calRpar, calXpar);
        c.rpar = regulate(calRpar, VALUE_LIMIT);
        c.xpar = regulate(calXpar, VALUE_LIMIT);
        c.zpar = regulate(RfMath::computeZ(calRpar, calXpar), VALUE_LIMIT);
        clampRX(calR, calX);
        reflection(calR, calX, Z0, c.phase, c.rho);
    }
    else // FarEnd -- no display clamps
    {
        double R = p.r, X = p.x;
        RfMath::parallel(R, X, c.rpar, c.xpar);
        clampRX(R, X);
        reflection(R, X, Z0, c.phase, c.rho);
        c.swr = 1;
        c.rl = 0;
        RfMath::computeSWR(Z0, R, X, &c.swr, &c.rl);
        c.r = R;
        c.x = X;
        c.z = RfMath::computeZ(R, X);
        c.zpar = RfMath::computeZ(c.rpar, c.xpar);
    }
    return c;
}

// Raw-series point k, with the chart's "repeat the previous SWR/RL"
// fallback resolved by walking back.
static MarkerMath::ChartPoint rawPoint(const QVector<RawData>& d, int k, double Z0)
{
    double prevSwr = MAX_SWR, prevRl = 0;
    int start = k;
    while (start > 0) {
        double s, r;
        if (RfMath::computeSWR(Z0, d[start-1].r, d[start-1].x, &s, &r) == 1)
            break;
        --start;
    }
    // Begin at the last point that computed normally (its own value doesn't
    // depend on the fallback), or at 0 if there is none.
    if (start > 0)
        --start;
    for (int i = start; i < k; ++i) {
        MarkerMath::ChartPoint p = MarkerMath::chartPoint(d[i], Z0, MarkerMath::Series::Raw, prevSwr, prevRl);
        prevSwr = p.swr;
        prevRl = p.rl;
    }
    return MarkerMath::chartPoint(d[k], Z0, MarkerMath::Series::Raw, prevSwr, prevRl);
}

static MarkerMath::ChartPoint seriesPoint(const MeasurementData& m, const QVector<RawData>* farEnd,
                                          bool calibrated, int k, double Z0)
{
    using namespace MarkerMath;
    if (farEnd != nullptr)
        return chartPoint(farEnd->at(k), Z0, Series::FarEnd, 1, 0);
    if (calibrated) {
        ChartPoint raw = rawPoint(m.dataRX, k, Z0);
        return chartPoint(m.dataRXCalib.at(k), Z0, Series::Calibrated, raw.rawSwr, raw.rawRl);
    }
    return rawPoint(m.dataRX, k, Z0);
}

static double interpolate(double fq1, double fq2, double fq3, double param1, double param2)
{
    return param1 + (fq2-fq1)/(fq3-fq1) *(param2-param1);
}

MarkerMath::Values MarkerMath::valuesAt(const MeasurementData& m, const QVector<RawData>* farEnd,
                                        bool calibrated, double fq0, double Z0)
{
    Values v;
    // Frequencies come from the measurement's own (calibrated) points;
    // far-end points are one per point of those, in order.
    const QVector<RawData>& keys = calibrated ? m.dataRXCalib : m.dataRX;
    int n = keys.size();
    if (farEnd != nullptr)
        n = qMin(n, int(farEnd->size()));
    if (calibrated && farEnd == nullptr)
        n = qMin(n, int(m.dataRX.size())); // Calibrated falls back on Raw

    for (int ii = 0; ii < n-1; ++ii)
    {
        double fq1 = keys.at(ii).fq*1000;
        double fq2 = keys.at(ii+1).fq*1000;
        if (!((fq1 <= fq0) && (fq2 >= fq0)))
            continue;

        ChartPoint p1 = seriesPoint(m, farEnd, calibrated, ii, Z0);
        ChartPoint p2 = seriesPoint(m, farEnd, calibrated, ii+1, Z0);

        v.found = true;
        v.phase = interpolate(fq1, fq0, fq2, p1.phase, p2.phase);
        v.r = interpolate(fq1, fq0, fq2, p1.r, p2.r);
        v.x = interpolate(fq1, fq0, fq2, p1.x, p2.x);
        v.rho = interpolate(fq1, fq0, fq2, p1.rho, p2.rho);
        v.rpar = interpolate(fq1, fq0, fq2, p1.rpar, p2.rpar);
        v.xpar = interpolate(fq1, fq0, fq2, p1.xpar, p2.xpar);
        v.swr = interpolate(fq1, fq0, fq2, p1.swr, p2.swr);
        v.rl = interpolate(fq1, fq0, fq2, p1.rl, p2.rl);

        if (qIsNaN(v.r) || (v.r<0.001))
            v.r = 0.01;
        if (qIsNaN(v.x))
            v.x = 0;

        const double maxRp = VALUE_LIMIT;
        v.rpar = regulate(v.rpar, maxRp);
        v.xpar = regulate(v.xpar, maxRp);
        v.x = regulate(v.x, maxRp); // far-end series isn't pre-clamped

        v.zmod = interpolate(fq1, fq0, fq2, p1.z, p2.z);
        v.zpar = interpolate(fq1, fq0, fq2, p1.zpar, p2.zpar);

        v.l = 1E9 * v.x / (2*M_PI * fq0 * 1E3);//nH
        v.c = 1E12 / (2*M_PI * fq0 * (v.x * (-1)) * 1E3);//pF
        v.lpar = 1E9 * v.xpar / (2*M_PI * fq0 * 1E3);
        v.cpar = 1E12 / (2*M_PI * fq0 * (v.xpar * (-1)) * 1E3);

        // 2-port data is the plain measurement's own (never far-end's),
        // matched to the bracketing frequencies exactly; 0 if absent, as
        // the chart lookup did.
        if (!m.dataSParam.isEmpty()) {
            QVector<double> s21Mag, s21Ph, s12Mag, s12Ph, key;
            bool h21 = false, h12 = false;
            double r21 = 0, u21 = 0, r12 = 0, u12 = 0;
            for (const SParamPoint& sp : m.dataSParam) {
                key.append(sp.fq*1000);
                s21Mag.append(20*log10(std::abs(sp.s21)));
                s21Ph.append(RfMath::unwrapPhaseDeg(std::arg(sp.s21)*180.0/M_PI, h21, r21, u21));
                s12Mag.append(20*log10(std::abs(sp.s12)));
                s12Ph.append(RfMath::unwrapPhaseDeg(std::arg(sp.s12)*180.0/M_PI, h12, r12, u12));
            }
            auto at = [&](const QVector<double>& vals, double k) {
                for (int i = 0; i < key.size(); ++i)
                    if (key[i] == k)
                        return vals[i];
                return 0.0;
            };
            v.s21 = interpolate(fq1, fq0, fq2, at(s21Mag, fq1), at(s21Mag, fq2));
            v.s21Phase = interpolate(fq1, fq0, fq2, at(s21Ph, fq1), at(s21Ph, fq2));
            // Live NanoVNA capture never fills S12; those read 0, as before.
            bool haveS12 = false;
            for (const SParamPoint& sp : m.dataSParam)
                haveS12 = haveS12 || (std::abs(sp.s12) != 0.0);
            v.s12 = haveS12 ? interpolate(fq1, fq0, fq2, at(s12Mag, fq1), at(s12Mag, fq2)) : 0;
            v.s12Phase = haveS12 ? interpolate(fq1, fq0, fq2, at(s12Ph, fq1), at(s12Ph, fq2)) : 0;
        }
        // First bracketing interval only (an exact hit matches two).
        break;
    }
    return v;
}

QVector<QPair<double, double>> MarkerMath::swrSeries(const MeasurementData& m, const QVector<RawData>* farEnd,
                                                     bool calibrated, double Z0)
{
    QVector<QPair<double, double>> out;
    if (farEnd != nullptr) {
        for (const RawData& p : *farEnd)
            out.append({p.fq*1000, chartPoint(p, Z0, Series::FarEnd, 1, 0).swr});
        return out;
    }
    double prevSwr = MAX_SWR, prevRl = 0;
    int n = calibrated ? qMin(m.dataRX.size(), m.dataRXCalib.size()) : m.dataRX.size();
    for (int i = 0; i < n; ++i) {
        ChartPoint raw = chartPoint(m.dataRX.at(i), Z0, Series::Raw, prevSwr, prevRl);
        prevSwr = raw.swr;
        prevRl = raw.rl;
        if (calibrated) {
            ChartPoint cal = chartPoint(m.dataRXCalib.at(i), Z0, Series::Calibrated, raw.rawSwr, raw.rawRl);
            out.append({m.dataRXCalib.at(i).fq*1000, cal.swr});
        } else {
            out.append({m.dataRX.at(i).fq*1000, raw.swr});
        }
    }
    return out;
}

bool MarkerMath::lowestSwr(const QVector<QPair<double, double>>& swr, double* fqKHz)
{
    if (swr.isEmpty())
        return false;
    double bestFq = swr.at(0).first;
    double bestSwr = swr.at(0).second;
    for (int i = 1; i < swr.size(); ++i) {
        if (swr.at(i).second < bestSwr) {
            bestSwr = swr.at(i).second;
            bestFq = swr.at(i).first;
        }
    }
    *fqKHz = bestFq;
    return true;
}

double MarkerMath::qFactor(const QVector<QPair<double, double>>& swr, double centerFq)
{
    if (swr.isEmpty())
        return 0;

    int centerIdx = 0;
    double bestDist = qAbs(swr.at(0).first - centerFq);
    for (int i = 1; i < swr.size(); ++i) {
        double dist = qAbs(swr.at(i).first - centerFq);
        if (dist < bestDist) {
            bestDist = dist;
            centerIdx = i;
        }
    }

    const double threshold = 2.0;
    int lowIdx = centerIdx;
    while (lowIdx > 0 && swr.at(lowIdx).second <= threshold)
        lowIdx--;
    int highIdx = centerIdx;
    while (highIdx < swr.size() - 1 && swr.at(highIdx).second <= threshold)
        highIdx++;

    if (lowIdx == 0 || highIdx == swr.size() - 1)
        return 0;

    double bandwidth = swr.at(highIdx).first - swr.at(lowIdx).first;
    if (bandwidth <= 0)
        return 0;

    return centerFq / bandwidth;
}

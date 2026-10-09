#include "tdrmath.h"
#include <math.h>

TdrMath::Estimate TdrMath::estimate(int dots, double topFreqMHz, double velFactor, bool metric)
{
    return estimateRaw(dots + 1, 0.0, topFreqMHz, velFactor, metric);
}

// See the declaration in tdrmath.h for what each Estimate field means.
TdrMath::Estimate TdrMath::estimateRaw(int asize, double minfqMHz, double maxfqMHz,
                                       double velFactor, bool metric)
{
    Estimate est;

    if (asize < 2 || maxfqMHz <= minfqMHz)
        return est; // NaN guard -- see Measurements::CalcTdr()'s comment on why (0-width/0-length data)

    int fftSize = 0;
    for (int i=0; ; i++)
    {
        fftSize = (1<<i);
        if ( (fftSize/2) >= (asize-1) )
            break;

        if (i==14)
            return Estimate(); // bug
    }

    fftSize *= 8;

    if (fftSize > TDR_MAXARRAY)
        return Estimate(); // bug

    double bw = maxfqMHz - minfqMHz;

    double chartStep = 1.0/bw/4*299792458*velFactor / (fftSize/2) * (asize-1);
    double range = chartStep*fftSize/1000000;
    // c x VF / (2 x BW), BW in Hz (maxfqMHz/minfqMHz are MHz, hence *1e6) --
    // independent of asize/fftSize, unlike chartStep/range above.
    double resolution = (299792458.0 * velFactor) / (2.0 * bw * 1.0e6);

    if (!metric)
    {
        range *= FEETINMETER;
        resolution *= FEETINMETER;
    }

    est.unambiguousRange = range;
    est.resolution = resolution;
    est.chartStep = chartStep;
    est.fftSize = fftSize;
    return est;
}

// Modified Bessel function of the first kind, order 0 -- series expansion,
// only needed for the Kaiser window below. Accurate to well under the
// window's own precision needs across the beta range a Kaiser window
// picker would realistically expose (0-20ish); no external math library
// pulled in for one function.
static double besselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    double xx = x*x/4.0;
    for (int k = 1; k <= 25; k++)
    {
        term *= xx/((double)k*(double)k);
        sum += term;
        if (term < 1e-12*sum)
            break;
    }
    return sum;
}

// Combined window-shape-and-normalization coefficient CalcTdr() multiplies
// each frequency bin's reflection coefficient by before the inverse FFT.
// i=0..n-1 maps onto the *falling half* (x=0.5..1.0) of a standard
// full-length window -- full gain at DC (i=0), tapering toward that
// window's minimum at the top of the measured band (i=n-1). That's
// deliberate, not a simplification: a TDR sweep only has one truncation
// edge to taper (the top of the measured band) -- the low edge is the
// spectrum's real DC start, not an artifact -- unlike a typical centered
// FFT window, which tapers both edges of data that's truncated on both
// sides. This mapping reproduces the original hardcoded Hamming formula
// exactly (case TdrWindow::Hamming below is algebraically identical to the
// pre-2026-08-21 inline code).
//
// Normalization: each case (Kaiser aside) divides by that window's own
// additive/DC-offset coefficient ("a0"), matching the original Hamming
// code's own KP=1/0.53836 convention -- an approximation (not a rigorous
// coherent-gain correction), kept for the sake of not changing today's
// Hamming output. Kaiser's own formula already peaks at exactly 1.0 at
// x=0.5 (i=0), so it needs no extra normalization.
double TdrMath::windowCoeff(TdrWindow type, double beta, int i, int n)
{
    double x = (n > 1) ? (0.5 + 0.5*(double)i/(double)(n-1)) : 0.5;

    switch (type)
    {
    case TdrWindow::Rectangular:
        return 1.0;

    case TdrWindow::Hann:
        return (0.5 - 0.5*cos(2*M_PI*x)) / 0.5;

    case TdrWindow::Blackman:
        return (0.42 - 0.5*cos(2*M_PI*x) + 0.08*cos(4*M_PI*x)) / 0.42;

    case TdrWindow::Kaiser:
    {
        double arg = 1.0 - (2.0*x-1.0)*(2.0*x-1.0);
        if (arg < 0)
            arg = 0;
        return besselI0(beta*sqrt(arg)) / besselI0(beta);
    }

    case TdrWindow::Hamming:
    default:
        return (0.53836 - 0.46146*cos(2*M_PI*x)) / 0.53836;
    }
}

bool TdrMath::canCompute(const QVector<RawData>& data)
{
    return data.size() >= TDR_MIN_DATA_POINTS && data.first().fq <= TDR_MAX_START_MHZ;
}

TdrMath::Result TdrMath::compute(const QVector<RawData>& data, double velFactor, bool metric,
                                 TdrWindow window, double kaiserBeta, double z0)
{
    Result res;
    int asize = data.length();
    if (asize == 0)
        return res;

    double minfq = data.at(0).fq;
    if ( minfq > TDR_MAX_START_MHZ )
    {
        return res; // Wrong fq
    }

    double maxfq = data.at(asize-1).fq;

    // A single data point (the norm for the first tick of a live scan, e.g.
    // when TDR is one of the joined "Multi" views and gets redrawn after
    // every incoming point) makes maxfq==minfq, so 1.0/(maxfq-minfq) inside
    // estimateRaw() would be +-inf, multiplied by a trailing
    // *(asize-1) that's 0 in this same case, giving inf*0 == NaN -- which
    // would flow into the axis range and graph data and crash QCustomPlot's
    // internal qRound() the next time it renders. estimateRaw() bails
    // out (fftSize stays 0) in exactly this case; the caller (redrawTDR)
    // already needs to tolerate a 0 return since the checks above it can do
    // the same.
    Estimate est = estimateRaw(asize, minfq, maxfq, velFactor, metric);
    if (est.fftSize == 0)
        return res; // bug, or not enough/valid data yet -- see estimateRaw()

    int fftSize = est.fftSize;
    res.range = est.unambiguousRange;
    res.resolution = est.resolution;
    res.impulse.resize(fftSize);
    res.step.resize(fftSize);
    res.impedance.resize(fftSize);

    int i;

    float *TdrReal = new float[TDR_MAXARRAY];
    float *TdrImag = new float[TDR_MAXARRAY];

#define Rdevice 50.0

    for (i=0; i<=fftSize/2; i++)
    {
        double R=0;
        double X=0;
        double Gre=0;
        double Gim=0;
        double FQ=0;
        if (i < asize)
        {
            FQ = data.at(i).fq;
            R = data.at(i).r;
            X = data.at(i).x;

            Gre = (R*R-Rdevice*Rdevice+X*X)/((R+Rdevice)*(R+Rdevice)+X*X);
            Gim = (2*Rdevice*X)/((R+Rdevice)*(R+Rdevice)+X*X);

            if ( i==0)
            {
                double m_dFarEndImpedance = 50;
                Gre = (m_dFarEndImpedance-Rdevice)/(m_dFarEndImpedance+Rdevice);
                Gim = 0;
            }

            double k = windowCoeff(window, kaiserBeta, i, asize);

            TdrReal[i] = Gre*fftSize/asize/2.0*k;
            TdrImag[i] = Gim*fftSize/asize/2.0*k;
        }
        else
        {
            TdrReal[i] = 0;
            TdrImag[i] = 0;
        }
    }

// Interpolate zero frequency

#define BDR 1
    for (i=0; i<BDR; i++)
    {
        double newreal = sqrt(TdrReal[BDR]*TdrReal[BDR]+TdrImag[BDR]*TdrImag[BDR]);

        if (TdrReal[BDR] < 0)
            TdrReal[i] = -newreal;
        else
            TdrReal[i] = newreal;

        TdrImag[i] = 0;

    }

// Mirror
    for (i=1; i<fftSize/2; i++)
    {
        TdrReal[fftSize-i] = TdrReal[i];
        TdrImag[fftSize-i] = -TdrImag[i];
    }
    TdrReal[fftSize/2] = 0;
    TdrImag[fftSize/2] = 0;

    fft(TdrReal, TdrImag, fftSize, 1/*Inverse*/);	// Inverse FFT

    double ig = 0;
    for (i=0; i<fftSize; i++)
    {
        double Amp = TdrReal[i];
        if((Amp > 0.015) || (Amp < -0.015))
        {
            res.impulse[i] = Amp;
            ig += Amp/2/(((double)fftSize)/asize/2);
        }else
        {
            res.impulse[i] = 0;
        }

        res.step[i] = ig;

        double Z = z0*(1+ig)/(1-ig);
        Z = (Z < 0) ? 0 : Z;
        res.impedance[i] = (Z > VALUE_LIMIT) ? VALUE_LIMIT : Z;
    }

    delete[] TdrReal;
    delete[] TdrImag;
    res.fftSize = fftSize;
    return res;
}

void TdrMath::fft(float real[], float imag[], int length, int Inverse)
{
    double wreal, wpreal, wimag, wpimag, theta;
    double tempreal, tempimag, tempwreal, direction;

    int Addr, Position, Mask, BitRevAddr, PairAddr;
    int m, k;

    direction = -1.0;		// direction of rotating phasor for FFT

    if(Inverse)
        direction = 1.0;	// direction of rotating phasor for IFFT

    //  bit-reverse the addresses of both the real and imaginary arrays
    //  real[0..length-1] and imag[0..length-1] are the paired complex numbers

    for (Addr=0; Addr<length; Addr++)
    {
        // Derive Bit-Reversed Address
        BitRevAddr = 0;
        Position = length >> 1;
        Mask = Addr;
        while (Mask)
        {
            if(Mask & 1)
                BitRevAddr += Position;
            Mask >>= 1;
            Position >>= 1;
        }

        if (BitRevAddr > Addr)				// Swap
        {
            double s;
            s = real[BitRevAddr];			// real part
            real[BitRevAddr] = real[Addr];
            real[Addr] = s;
            s = imag[BitRevAddr];			// imaginary part
            imag[BitRevAddr] = imag[Addr];
            imag[Addr] = s;
        }
    }

    // FFT, IFFT Kernel

    for (k=1; k < length; k <<= 1)
    {
        theta = direction * M_PI / (double)k;
        wpimag = sin(theta);
        wpreal = cos(theta);
        wreal = 1.0;
        wimag = 0.0;

        for (m=0; m < k; m++)
        {
            for (Addr = m; Addr < length; Addr += (k*2))
            {
                PairAddr = Addr + k;

                tempreal = wreal * (double)real[PairAddr] - wimag * (double)imag[PairAddr];
                tempimag = wreal * (double)imag[PairAddr] + wimag * (double)real[PairAddr];


                real[PairAddr] = (double)real[Addr] - tempreal;
                imag[PairAddr] = (double)imag[Addr] - tempimag;
                real[Addr] += tempreal;
                imag[Addr] += tempimag;
            }
            tempwreal = wreal;
            wreal = wreal * wpreal - wimag * wpimag;
            wimag = wimag * wpreal + tempwreal * wpimag;
        }
    }

    if(Inverse)							// Normalize the IFFT coefficients
        for(int i=0; i<length; i++)
        {
            real[i] /= (double)length;
            imag[i] /= (double)length;
        }
}

double TdrMath::roundTripNs(double distance, double velFactor, bool metric)
{
    double meters = metric ? distance : distance / FEETINMETER;
    if (velFactor <= 0)
        return 0;
    return 2.0 * meters / (299792458.0 * velFactor) * 1.0e9;
}

QVector<TdrMath::Event> TdrMath::findEvents(const Result& result, const EventParams& params)
{
    QVector<Event> events;
    int n = result.fftSize;
    if (n <= 1 || result.impulse.size() < n || result.range <= 0)
        return events;

    double step = result.range / n;
    double maxAbs = 0;
    for (int i = 0; i < n; ++i)
        maxAbs = qMax(maxAbs, qAbs(result.impulse.at(i)));
    double floor = qMax(params.noiseFloor, params.relativeFloor * maxAbs);
    if (maxAbs < params.noiseFloor)
        return events;

    // Peaks: local maxima of |impulse|, strongest within half a resolution.
    int half = qMax(1, qRound(0.5 * result.resolution / step));
    QVector<int> peaks;
    for (int i = 0; i < n; ++i) {
        double a = qAbs(result.impulse.at(i));
        if (a < floor)
            continue;
        bool best = true;
        int lo = qMax(0, i - half), hi = qMin(n - 1, i + half);
        for (int j = lo; j <= hi && best; ++j) {
            double b = qAbs(result.impulse.at(j));
            // Ties go to the earlier bin so a flat top gives one peak.
            if (b > a || (b == a && j < i))
                best = false;
        }
        if (best)
            peaks << i;
    }

    double lastKey = (n - 1) * step;
    for (int k = 0; k < peaks.size(); ++k) {
        int idx = peaks.at(k);
        int stop = (k + 1 < peaks.size()) ? peaks.at(k + 1) : n;
        // Impedance at the quietest bin after this pulse (before the next
        // one starts), where the step response has settled.
        int zIdx = idx;
        double quiet = 1e300;
        for (int i = idx + 1; i < stop; ++i) {
            double a = qAbs(result.impulse.at(i));
            if (a < quiet) {
                quiet = a;
                zIdx = i;
            }
            if (a == 0)
                break;
        }
        Event e;
        e.distance = idx * step;
        e.amplitude = result.impulse.at(idx);
        e.impedance = result.impedance.at(zIdx);
        e.nearRangeEdge = lastKey > 0 && e.distance >= 0.95 * lastKey;
        events << e;
    }

    // Echoes: weaker events about twice an earlier event's distance.
    for (int i = 0; i < events.size(); ++i) {
        for (int j = 0; j < i; ++j) {
            if (events[j].echoOf >= 0 || qAbs(events[i].amplitude) >= qAbs(events[j].amplitude))
                continue;
            if (qAbs(events[i].distance - 2.0 * events[j].distance) <= result.resolution) {
                events[i].echoOf = j;
                break;
            }
        }
    }

    int last = -1;
    for (int i = 0; i < events.size(); ++i) {
        if (events[i].echoOf < 0)
            last = i;
    }
    for (int i = 0; i < events.size(); ++i) {
        Event& e = events[i];
        bool rising = e.amplitude > 0;
        if (e.echoOf >= 0)
            e.kind = EventKind::PossibleEcho;
        else if (e.distance <= result.resolution)
            e.kind = EventKind::NearEndMismatch;
        else if (i == last && qAbs(e.amplitude) >= params.endStrength)
            e.kind = rising ? EventKind::OpenEnd : EventKind::ShortEnd;
        else
            e.kind = rising ? EventKind::HighZ : EventKind::LowZ;
    }
    return events;
}

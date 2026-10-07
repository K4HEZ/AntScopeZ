// tdr_events_dump scan.asd [vf=0.66] [window=hamming] [ft|m]
// Prints a saved TDR scan's trace statistics, and the events found under a
// grid of detection thresholds, for tuning TdrMath::EventParams.
#include <measurementfiles.h>
#include <tdrmath.h>
#include <QCoreApplication>
#include <QStringList>
#include <cstdio>

static TdrWindow windowFromName(const QString& n)
{
    if (n == "rect") return TdrWindow::Rectangular;
    if (n == "hann") return TdrWindow::Hann;
    if (n == "blackman") return TdrWindow::Blackman;
    if (n == "kaiser") return TdrWindow::Kaiser;
    return TdrWindow::Hamming;
}

static const char* kindName(TdrMath::EventKind k)
{
    switch (k) {
    case TdrMath::EventKind::OpenEnd: return "Open end";
    case TdrMath::EventKind::ShortEnd: return "Short end";
    case TdrMath::EventKind::HighZ: return "High-Z";
    case TdrMath::EventKind::LowZ: return "Low-Z";
    case TdrMath::EventKind::NearEndMismatch: return "Near-end";
    case TdrMath::EventKind::PossibleEcho: return "Echo";
    case TdrMath::EventKind::User: return "User";
    }
    return "?";
}

static void print(const QVector<TdrMath::Event>& events, const char* indent)
{
    for (int i = 0; i < events.size(); ++i) {
        const TdrMath::Event& e = events.at(i);
        std::printf("%s#%d  d=%7.2f  rho=%+.3f  Z=%6.0f  %-9s%s%s\n", indent, i + 1, e.distance,
                    e.amplitude, e.impedance, kindName(e.kind), e.nearRangeEdge ? " [range edge]" : "",
                    e.echoOf >= 0 ? " [echo]" : "");
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 2) {
        std::printf("usage: tdr_events_dump scan.asd [vf=0.66] [rect|hamming|hann|blackman|kaiser] [ft|m]\n");
        return 2;
    }
    double vf = args.value(2, "0.66").toDouble();
    TdrWindow window = windowFromName(args.value(3, "hamming"));
    bool metric = args.value(4, "ft") == "m";

    MeasurementFiles::ReadResult r = MeasurementFiles::readAsd(args.at(1));
    if (!r.ok()) {
        std::printf("can't read %s\n", qPrintable(args.at(1)));
        return 1;
    }
    TdrMath::Result tdr = TdrMath::compute(r.raw, vf, metric, window, 6.0, 50.0);
    if (tdr.fftSize == 0) {
        std::printf("no valid TDR in this file (needs a sweep starting near DC)\n");
        return 1;
    }
    double maxAbs = 0;
    int nonZero = 0;
    for (double v : tdr.impulse) {
        maxAbs = qMax(maxAbs, qAbs(v));
        nonZero += (v != 0);
    }
    std::printf("%d points, %.3f..%.3f MHz, vf %.3f, %s\n", (int)r.raw.size(), r.raw.first().fq,
                r.raw.last().fq, vf, metric ? "m" : "ft");
    std::printf("range %.2f, resolution %.3f, fft %d, max|impulse| %.3f, %d nonzero bins\n\n",
                tdr.range, tdr.resolution, tdr.fftSize, maxAbs, nonZero);

    std::printf("default thresholds:\n");
    print(TdrMath::findEvents(tdr), "  ");

    std::printf("\nevent count by noiseFloor (rows) x relativeFloor (columns):\n          ");
    const double rel[] = {0.02, 0.05, 0.10, 0.15, 0.25, 0.40};
    const double noise[] = {0.015, 0.03, 0.05, 0.08};
    for (double rf : rel)
        std::printf(" rel%.2f", rf);
    std::printf("\n");
    for (double nf : noise) {
        std::printf("  noise%.3f", nf);
        for (double rf : rel) {
            TdrMath::EventParams p;
            p.noiseFloor = nf;
            p.relativeFloor = rf;
            std::printf(" %7d", (int)TdrMath::findEvents(tdr, p).size());
        }
        std::printf("\n");
    }
    return 0;
}

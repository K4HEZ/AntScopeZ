// TdrMath::findEvents on synthetic cable sweeps.
#include <tdrmath.h>
#include <complex>
#include <cstdio>
#include <cstdlib>

using Cd = std::complex<double>;

static const double kC = 299792458.0;
static const double kVf = 0.66;

// Sweep 0..topMHz of a lossless 50 ohm line. Each reflector is
// {distance m, reflection coefficient}; later ones see (1 - rho^2) less.
struct Reflector { double meters; double rho; };

static QVector<RawData> sweep(const QVector<Reflector>& refl, double topMHz, int dots)
{
    QVector<RawData> data;
    for (int i = 0; i <= dots; ++i) {
        double fMHz = topMHz * i / dots;
        Cd gamma = 0;
        double through = 1.0;
        for (const Reflector& r : refl) {
            double phase = -2.0 * M_PI * (fMHz * 1e6) * 2.0 * r.meters / (kC * kVf);
            gamma += through * r.rho * std::polar(1.0, phase);
            through *= (1.0 - r.rho * r.rho);
        }
        Cd z = 50.0 * (1.0 + gamma) / (1.0 - gamma + Cd(1e-9, 0));
        data.append({fMHz, z.real(), z.imag()});
    }
    return data;
}

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static QVector<TdrMath::Event> run(const QVector<Reflector>& refl, double* resolution)
{
    TdrMath::Result r = TdrMath::compute(sweep(refl, 100.0, 200), kVf, true,
                                         TdrWindow::Hamming, 6.0, 50.0);
    CHECK(r.fftSize != 0, "compute failed");
    *resolution = r.resolution;
    QVector<TdrMath::Event> events = TdrMath::findEvents(r);
    if (std::getenv("TDR_TEST_VERBOSE")) {
        std::printf("res %.2f m, %d events\n", r.resolution, (int)events.size());
        for (const TdrMath::Event& e : events)
            std::printf("  d=%.2f amp=%+.2f z=%.0f kind=%d echoOf=%d\n", e.distance, e.amplitude,
                        e.impedance, (int)e.kind, e.echoOf);
    }
    return events;
}

int main()
{
    double res = 0;

    QVector<TdrMath::Event> open = run({{10.0, 1.0}}, &res);
    CHECK(!open.isEmpty(), "open: no events");
    if (!open.isEmpty()) {
        const TdrMath::Event& e = open.last();
        CHECK(e.kind == TdrMath::EventKind::OpenEnd, "open: kind %d", (int)e.kind);
        CHECK(std::abs(e.distance - 10.0) <= res, "open: distance %.2f (res %.2f)", e.distance, res);
        CHECK(e.amplitude > 0, "open: amplitude %.2f", e.amplitude);
    }

    QVector<TdrMath::Event> shorted = run({{10.0, -1.0}}, &res);
    CHECK(!shorted.isEmpty(), "short: no events");
    if (!shorted.isEmpty()) {
        const TdrMath::Event& e = shorted.last();
        CHECK(e.kind == TdrMath::EventKind::ShortEnd, "short: kind %d", (int)e.kind);
        CHECK(std::abs(e.distance - 10.0) <= res, "short: distance %.2f", e.distance);
    }

    QVector<TdrMath::Event> matched = run({}, &res);
    CHECK(matched.isEmpty(), "matched: %d events", (int)matched.size());

    QVector<TdrMath::Event> two = run({{5.0, 0.3}, {15.0, 1.0}}, &res);
    bool midHigh = false, endOpen = false;
    for (const TdrMath::Event& e : two) {
        if (e.kind == TdrMath::EventKind::HighZ && std::abs(e.distance - 5.0) <= res)
            midHigh = true;
        if (e.kind == TdrMath::EventKind::OpenEnd && std::abs(e.distance - 15.0) <= res)
            endOpen = true;
    }
    for (const TdrMath::Event& e : two) {
        if (e.kind == TdrMath::EventKind::HighZ)
            CHECK(e.impedance > 70 && e.impedance < 110, "two: High-Z reads %.0f ohms (ideal 93)", e.impedance);
    }
    CHECK(midHigh, "two: no High-Z near 5 m");
    CHECK(endOpen, "two: no open end near 15 m");

    double ns = TdrMath::roundTripNs(10.0, kVf, true);
    CHECK(std::abs(ns - 2.0 * 10.0 / (kC * kVf) * 1e9) < 1e-6, "roundTripNs %.3f", ns);

    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}

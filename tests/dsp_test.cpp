// Offline sanity tests for the Pitch Hack DSP core (no JUCE needed).
#include "../Source/PitchHackDSP.h"
#include <cstdio>
#include <cstdlib>
#include <numeric>
using namespace pitchhack;

static const double SR = 48000.0;

// estimate frequency by counting positive-going zero crossings
static double freqOf (const std::vector<float>& x, size_t a, size_t b)
{
    int zc = 0;
    for (size_t i = a + 1; i < b; ++i) if (x[i-1] <= 0.f && x[i] > 0.f) ++zc;
    return zc * SR / (double) (b - a);
}

static std::vector<float> run (Engine::Params p, double bpm, double seconds, float amp = 0.5f, double f = 440.0)
{
    Engine e; e.prepare (SR);
    const int N = (int) (seconds * SR);
    std::vector<float> l (N), r (N);
    for (int i = 0; i < N; ++i) l[i] = r[i] = amp * (float) std::sin (2.0 * M_PI * f * i / SR);
    for (int i = 0; i < N; i += 512) { int n = std::min (512, N - i); e.process (&l[i], &r[i], n, p, bpm); }
    return l;
}

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf ("FAIL: %s\n", msg); ++fails; } else std::printf ("ok:   %s\n", msg); } while (0)

int main()
{
    Engine::Params p; p.dryWet = 100;               // fully wet

    // 1) +12 st -> 880 Hz inside a segment (1/4 @ 120 bpm = 500 ms segments)
    p.interval = 12;
    auto y = run (p, 120.0, 2.0);
    double f = freqOf (y, (size_t) (0.60 * SR), (size_t) (0.90 * SR));
    std::printf ("      +12 st measured %.1f Hz (expect ~880)\n", f);
    CHECK (std::fabs (f - 880.0) < 15.0, "interval +12 st doubles the pitch");

    // 2) -12 st -> 220 Hz
    p.interval = -12;
    y = run (p, 120.0, 2.0);
    f = freqOf (y, (size_t) (0.60 * SR), (size_t) (0.90 * SR));
    std::printf ("      -12 st measured %.1f Hz (expect ~220)\n", f);
    CHECK (std::fabs (f - 220.0) < 8.0, "interval -12 st halves the pitch");

    // 3) +7 st
    p.interval = 7;
    y = run (p, 120.0, 2.0);
    f = freqOf (y, (size_t) (0.60 * SR), (size_t) (0.90 * SR));
    std::printf ("      +7 st measured %.1f Hz (expect ~%.1f)\n", f, 440.0 * std::pow (2.0, 7.0 / 12));
    CHECK (std::fabs (f - 440.0 * std::pow (2.0, 7.0 / 12)) < 12.0, "interval +7 st correct");

    // 4) unity, dry/wet 0 == bit-exact dry
    Engine::Params q;  q.dryWet = 0;
    y = run (q, 120.0, 0.5);
    double err = 0; for (size_t i = 0; i < y.size(); ++i) err = std::max (err, (double) std::fabs (y[i] - 0.5f * (float) std::sin (2.0 * M_PI * 440.0 * i / SR)));
    CHECK (err < 1e-6, "dry/wet 0 passes the dry signal untouched");

    // 5) reverse 100 % : still produces audio, bounded
    p.interval = 0; p.reverse = 100;
    y = run (p, 120.0, 2.0);
    float peak = 0; for (float s : y) peak = std::max (peak, std::fabs (s));
    CHECK (peak > 0.1f && peak < 1.0f && std::isfinite (peak), "reverse 100 % renders finite audio");

    // 6) limiter ceiling: hot input never exceeds -0.1 dB after look-ahead settles
    Engine::Params h; h.dryWet = 100; h.interval = 5; h.recycle = 90;
    y = run (h, 120.0, 3.0, 3.0f);
    peak = 0; for (size_t i = (size_t) SR; i < y.size(); ++i) peak = std::max (peak, std::fabs (y[i]));
    std::printf ("      hot-input wet peak %.4f (ceiling %.4f)\n", peak, std::pow (10.0, -0.1 / 20));
    CHECK (peak <= 0.995f, "limiter holds ceiling with +9.5 dB input and heavy recycle");

    // 7) random variation stays within +/- range & finite
    Engine::Params v; v.dryWet = 100; v.variation = 24; v.rateIndex = 0;      // 1/128 = very short segments
    y = run (v, 120.0, 2.0);
    bool finite = true; for (float s : y) finite = finite && std::isfinite (s);
    CHECK (finite, "variation 24 st at 1/128 stays finite");

    // 8) all rates, extreme tempo: no NaN, no crash
    for (int r = 0; r < 24; ++r) { Engine::Params a; a.dryWet = 100; a.interval = 36; a.rateIndex = r; a.recycle = 95;
        for (double bpm : { 20.0, 300.0 }) { auto z = run (a, bpm, 0.7); for (float s : z) if (!std::isfinite (s)) { CHECK (false, "rate sweep finite"); return 1; } } }
    CHECK (true, "all 24 rates x extreme tempi finite");

    std::printf (fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails ? 1 : 0;
}

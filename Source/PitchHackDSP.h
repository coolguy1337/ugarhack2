// ============================================================================
//  Pitch Hack - DSP core (framework independent, header only)
//
//  This is a line-by-line port of the signal/control graph found inside
//  "Pitch Hack.amxd" (Max for Live):
//
//    plugin~ --> p delay1 --> vs.limiter~ --> pluggo.dryWet2~ --> plugout~
//                                                ^
//    plugin~ -----------------(dry)--------------'
//
//  p delay1 contains: coll(rate) -> translate notevalues ms, vs.randomizer,
//  round, vs.ratio, p reverser, vs.split, p movehead (2x line~), trapezoid~,
//  2x p delay_channel (tapin~/tapout~ 20000), vs.limiter~, p stereovolume.
//  Object names are quoted in the comments below.
// ============================================================================
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pitchhack
{

// ---------------------------------------------------------------- helpers ---
inline float dbtoa (float db)  { return std::pow (10.0f, db * 0.05f); }
inline float atodb (float a)   { return a > 0.0f ? std::max (20.0f * std::log10 (a), -999.0f) : -999.0f; }

// Max "rampsmooth~": every time the input value changes, a new linear ramp
// from the current output to the new input is started (length = up/down samples).
struct RampSmooth
{
    float out = 0.f, target = 0.f, step = 0.f;
    int   count = 0;

    void reset() { out = target = step = 0.f; count = 0; }

    float process (float in, float upSamples, float downSamples)
    {
        if (in != target)
        {
            target = in;
            const float n = (target > out) ? upSamples : downSamples;
            if (n < 1.0f) { out = target; step = 0.f; count = 0; }
            else          { step = (target - out) / n; count = (int) n; }
        }
        if (count > 0)
        {
            out += step;
            if (--count == 0) out = target;
        }
        return out;
    }
};

// Tiny deterministic PRNG (Max's random is seeded by "loadmess" in vs.random,
// i.e. reproducible after load - we do the same).
struct Rng
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t) (s >> 16); }
    float    uniform01() { return (float) (next() & 0x7FFFFFFF) / 2147483648.0f; } // random 2147483647 / 2147483648.
    int      randomInt (int n) { return (int) (next() % (uint32_t) n); }              // random n
};

// ------------------------------------------------------------ vs.limiter~ ---
// Look-ahead peak limiter, defaults from "patcherargs 2.5 500. -0.1".
class Limiter
{
public:
    void prepare (double sr, float lookaheadMs = 2.5f, float releaseMs = 500.f, float ceilingDb = -0.1f)
    {
        lookahead = std::min ((int) (lookaheadMs * sr / 1000.0), 1000); // mstosamps~ -> int, delay~ 1000 0
        release   = (float) (releaseMs * sr / 1000.0);
        ceiling   = ceilingDb;
        std::fill (bufL.begin(), bufL.end(), 0.f);
        std::fill (bufR.begin(), bufR.end(), 0.f);
        std::fill (bufE.begin(), bufE.end(), 0.f);
        w = 0;
        rsPeak.reset(); rsSel.reset();
    }

    void process (float& l, float& r)
    {
        // p maximum_peak : abs~ / maximum~ / atodb~
        const float peakDb = atodb (std::max (std::fabs (l), std::fabs (r)));
        // p peak_follow : rampsmooth~ 0 <release>, delay~ <lookahead>, p x_solve
        const float e = rsPeak.process (peakDb, 0.f, release);
        bufE[w & mask] = e;
        const float d   = bufE[(w - lookahead) & mask];                    // delayed envelope
        const float sel = (d < e) ? 1.f : 0.f;                              // <~
        const float s   = rsSel.process (sel, (float) lookahead, 0.f);      // rampsmooth~ 0 0 (up = lookahead)
        const float env = d * (1.f - s) + e * s;
        // p limiting : -~ ceiling, maximum~ 0, *~ -1, dbtoa~
        const float excess = std::max (env - ceiling, 0.f);
        const float gain   = dbtoa (-excess);
        // p delay : delay~ (lookahead samples) on both channels
        bufL[w & mask] = l;  bufR[w & mask] = r;
        l = bufL[(w - lookahead) & mask] * gain;
        r = bufR[(w - lookahead) & mask] * gain;
        ++w;
    }

private:
    static constexpr int size = 2048, mask = size - 1;
    std::array<float, size> bufL {}, bufR {}, bufE {};
    int   w = 0, lookahead = 110;
    float release = 22050.f, ceiling = -0.1f;
    RampSmooth rsPeak, rsSel;
};

// ----------------------------------------------------------------- Engine ---
class Engine
{
public:
    // tapout~ can't read closer than one signal vector to the write head.
    static constexpr int kMinTapDelaySamples = 64;

    struct Params
    {
        int   interval   = 0;     // st   (-36..36)   "Interval"
        int   cents      = 0;     // ct   (-99..99)   "Cents"
        int   variation  = 0;     // st   (0..24)     "Variation"
        int   rateIndex  = 13;    //      (0..23)     "Rate"  (13 = 1/4)
        int   reverse    = 0;     // %    (0..100)    "Reverse"
        int   recycle    = 0;     // %    (0..95)     "Recycle"
        float levelDb    = 0.f;   // dB   (-96..0)    "Level"
        int   dryWet     = 0;     // %    (0..100)    "Dry / Wet"
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        tapLen = (int) std::ceil (20.0 * sr) + 4;           // tapin~ 20000
        for (auto& t : tap) t.assign ((size_t) tapLen, 0.f);
        tapW = 0;
        lim1.prepare (sr);     // vs.limiter~ inside p delay1
        lim2.prepare (sr);     // vs.limiter~ before pluggo.dryWet2~
        volGain = 1.0; volTarget = 1.0; volInc = 0.0; volLeft = 0;
        lastLevelDb = 1000.f;
        segPos = 0.0; segLen = 1.0;
        shifted = true;
        restartPending = true;
    }

    // "---init_all": live.thisdevice (load), transport start, Restart button.
    void restart() { restartPending = true; }

    void process (float* l, float* r, int n, const Params& p, double bpm)
    {
        // ---- control inlets of p delay1 (cold unless noted) ----
        storedSemis = (float) p.interval + (float) p.cents * 0.01f;      // p pitch: pak/+ 0.
        varRange    = (float) p.variation;                                // vs.randomizer
        roundMult   = (p.cents == 0) ? 1.0f : 0.01f;                      // == 0. -> sel 1 0 -> round
        revProb     = p.reverse;                                          // p reverser
        rateIndex   = std::clamp (p.rateIndex, 0, 23);
        tempo       = bpm > 1.0 ? bpm : 120.0;
        const float recycle = ((float) p.recycle / 95.0f) * 0.95f;        // live.dial 2nd outlet -> scale 0. 1. 0. 0.95
        const float wetAmt  = (float) p.dryWet * 0.01f;                   // * 0.01
        // p stereovolume: dbtoa -> "$1 5" -> line~
        if (p.levelDb != lastLevelDb)
        {
            lastLevelDb = p.levelDb;
            volTarget = dbtoa (p.levelDb);
            volLeft = std::max (1, (int) (0.005 * sr));
            volInc = (volTarget - volGain) / volLeft;
        }

        for (int i = 0; i < n; ++i)
        {
            if (restartPending) { restartPending = false; startSegment(); }

            const float dryL = l[i], dryR = r[i];

            // ---- the two line~ objects of p movehead ----
            const double phase = std::clamp (segPos / segLen, 0.0, 1.0);
            const float delayMs = (float) (dStart + (dTarget - dStart) * phase);
            const float window  = trapezoid ((float) phase);              // trapezoid~ 0.05 0.95

            // ---- p delay_channel x2 : tapin~/tapout~ 20000 ----
            double dSamp = (double) delayMs * sr * 0.001;
            dSamp = std::clamp (dSamp, (double) kMinTapDelaySamples, (double) (tapLen - 2));
            const float gB = shifted ? recycle : 0.f;                     // pak 1. 0. -> * 1.  (flag * recycle)
            float wet[2];
            const float in[2] = { dryL, dryR };
            for (int ch = 0; ch < 2; ++ch)
            {
                const float t = tapRead (tap[(size_t) ch], dSamp);
                wet[ch] = t * window;                                      // *~ (gain A = trapezoid)
                tap[(size_t) ch][(size_t) tapW] = in[ch] + t * gB;         // tapin~ <- in + (tapout * gain B)
            }
            if (++tapW >= tapLen) tapW = 0;

            // ---- vs.limiter~ (inside p delay1) -> p stereovolume ----
            lim1.process (wet[0], wet[1]);
            if (volLeft > 0) { volGain += volInc; if (--volLeft == 0) volGain = volTarget; }
            wet[0] *= (float) volGain;  wet[1] *= (float) volGain;

            // ---- vs.limiter~ (main) -> pluggo.dryWet2~ ----
            lim2.process (wet[0], wet[1]);
            l[i] = dryL * (1.f - wetAmt) + wet[0] * wetAmt;
            r[i] = dryR * (1.f - wetAmt) + wet[1] * wetAmt;

            // ---- line~ right outlet "done" bang -> float -> next segment ----
            segPos += 1.0;
            if (segPos >= segLen) { segPos -= segLen; startSegment(); }
        }
    }

    // exposed for tests
    float currentRatio() const { return lastRatio; }

private:
    static float trapezoid (float p)
    {
        if (p < 0.05f) return p / 0.05f;
        if (p < 0.95f) return 1.0f;
        return std::max (0.f, (1.0f - p) / 0.05f);
    }

    float tapRead (const std::vector<float>& b, double d) const
    {
        double pos = (double) tapW - d;
        if (pos < 0.0) pos += tapLen;
        int i0 = (int) pos;  const double fr = pos - i0;
        int i1 = i0 + 1;     if (i1 >= tapLen) i1 -= tapLen;
        if (i0 >= tapLen) i0 -= tapLen;
        return (float) (b[(size_t) i0] * (1.0 - fr) + b[(size_t) i1] * fr);    // tapout~ interpolates
    }

    // coll + translate notevalues ms  (whole-note fractions; 2/3 = triplet, x1.5 = dotted)
    double rateMs() const
    {
        static const double whole[24] = {
            1.0/128, 1.0/64, (1.0/32)*(2.0/3), (1.0/64)*1.5, 1.0/32, (1.0/16)*(2.0/3), (1.0/32)*1.5, 1.0/16,
            (1.0/8)*(2.0/3), (1.0/16)*1.5, 1.0/8, (1.0/4)*(2.0/3), (1.0/8)*1.5, 1.0/4, (1.0/2)*(2.0/3), (1.0/4)*1.5,
            1.0/2, 2.0/3, (1.0/2)*1.5, 1.0, 1.5, 2.0, 3.0, 4.0 };
        return whole[rateIndex] * 4.0 * 60000.0 / tempo;
    }

    // one pass through: float -> vs.randomizer -> round -> vs.ratio -> p reverser
    //                   -> vs.split 1 -> p movehead
    void startSegment()
    {
        // vs.randomizer: stored + random(-V, +V)
        const float rnd = -varRange + rng.uniform01() * (2.f * varRange);
        float x = storedSemis + rnd;
        // round (to 1 st when Cents == 0, else 0.01 st)
        x = std::floor (x / roundMult + 0.5f) * roundMult;
        // vs.ratio : +69 -> mtof -> /440
        const float hz = 440.0f * std::pow (2.0f, ((x + 69.0f) - 69.0f) / 12.0f);
        float v = hz / 440.0f;
        lastRatio = v;
        // p reverser : random 100 < Reverse%  ->  -1
        if (rng.randomInt (100) < revProb) v = -v;
        shifted = (v != 1.0f);                                          // != 1. -> pak 1. 0.

        // p movehead (vs.split 1 picks the branch)
        const float L = (float) rateMs();
        float X;
        if (v < 1.0f) { X = std::min ((1.0f - v) * L, 20000.f); dStart = 0.0;  dTarget = X;   }  // "0, X L"
        else          { X = std::min ((v - 1.0f) * L, 20000.f); dStart = X;    dTarget = 0.0; }  // "X, 0 L"
        segLen = std::max (1.0, (double) L * sr * 0.001);               // line~ time
    }

    double sr = 44100.0, tempo = 120.0;
    // control state
    float storedSemis = 0.f, varRange = 0.f, roundMult = 1.f, lastRatio = 1.f;
    int   revProb = 0, rateIndex = 13;
    bool  shifted = true, restartPending = true;
    Rng   rng;
    // segment (the two lockstep line~: delay-time and window)
    double segPos = 0.0, segLen = 1.0, dStart = 0.0, dTarget = 0.0;
    // tapin~/tapout~
    std::array<std::vector<float>, 2> tap;
    int tapLen = 0, tapW = 0;
    // volume line~
    double volGain = 1.0, volTarget = 1.0, volInc = 0.0;
    int    volLeft = 0;
    float  lastLevelDb = 1000.f;
    Limiter lim1, lim2;
};

} // namespace pitchhack

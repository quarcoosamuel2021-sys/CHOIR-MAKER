// Stand-alone checks for the choir DSP core.
//   g++ -std=c++17 -O2 -ISource tests/engine_test.cpp -o engine_test && ./engine_test
#include "ChoirEngine.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace choir;

static int failures = 0;
#define CHECK(cond, ...)                                  \
    do { if (! (cond)) { ++failures; std::printf ("  FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static const float FS = 48000.0f;

// Goertzel magnitude (amplitude) of one frequency
static float goertzel (const std::vector<float>& x, size_t start, size_t n, float freq)
{
    const double w = 2.0 * 3.14159265358979 * freq / FS, c = 2.0 * std::cos (w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) i / (double) (n - 1));
        s0 = x[start + i] * win + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return (float) (2.0 * std::sqrt (re * re + im * im) / ((double) n * 0.5));
}

static float rms (const std::vector<float>& x, size_t start, size_t n)
{
    double a = 0;
    for (size_t i = 0; i < n; ++i) a += (double) x[start + i] * x[start + i];
    return (float) std::sqrt (a / (double) n);
}

// vowel-like test source: harmonic series with a gentle roll-off and a little vibrato
static std::vector<float> makeVoice (size_t n, float f0)
{
    std::vector<float> x (n);
    double ph = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / FS;
        const double f = f0 * (1.0 + 0.004 * std::sin (2.0 * 3.14159265 * 5.0 * t));
        ph += 2.0 * 3.14159265358979 * f / FS;
        double s = 0;
        for (int h = 1; h <= 20; ++h) s += std::sin (h * ph) / (double) h;
        x[i] = (float) (0.3 * s);
    }
    return x;
}

int main()
{
    const size_t N = (size_t) (3.0f * FS);

    // 1. pitch accuracy: octaves chord => 110 / 220 / 440 Hz present, 330 Hz absent
    {
        std::printf ("pitch accuracy ... ");
        Engine e; e.prepare (FS);
        Params p; p.numVoices = 3; p.chord = 1; p.detuneCents = 0; p.vibDepth = 0;
        p.formantAmt = 0; p.timingMs = 0; p.spread = 0;
        e.setParams (p);

        std::vector<float> in (N), L (N), R (N);
        for (size_t i = 0; i < N; ++i) in[i] = 0.5f * std::sin (2.0f * kPi * 220.0f * (float) i / FS);
        for (size_t i = 0; i < N; ++i) e.process (in[i], L[i], R[i]);

        const size_t st = (size_t) FS, len = (size_t) FS;
        const float a110 = goertzel (L, st, len, 110.0f), a220 = goertzel (L, st, len, 220.0f),
                    a440 = goertzel (L, st, len, 440.0f), a330 = goertzel (L, st, len, 330.0f),
                    a165 = goertzel (L, st, len, 165.0f);
        std::printf ("110:%.3f 220:%.3f 440:%.3f | 165:%.4f 330:%.4f\n", a110, a220, a440, a165, a330);
        CHECK (a110 > 0.08f && a220 > 0.08f && a440 > 0.08f, "expected strong 110/220/440 components");
        CHECK (a330 < 0.1f * a220 && a165 < 0.1f * a220, "unexpected energy at 165/330 Hz");
    }

    // 2. level sanity with all realism on, harmonic input
    for (int voices : { 1, 4, 8, 16 })
    {
        std::printf ("level sanity (%2d voices) ... ", voices);
        Engine e; e.prepare (FS);
        Params p; p.numVoices = voices;   // defaults: detune 15, timing 25, vibrato, formant 0.7
        e.setParams (p);
        const auto in = makeVoice (N, 220.0f);
        std::vector<float> L (N), R (N);
        for (size_t i = 0; i < N; ++i) e.process (in[i], L[i], R[i]);

        float peak = 0; bool finite = true;
        for (size_t i = 0; i < N; ++i)
        {
            peak = std::max ({ peak, std::fabs (L[i]), std::fabs (R[i]) });
            finite = finite && std::isfinite (L[i]) && std::isfinite (R[i]);
        }
        const float rin = rms (in, (size_t) FS, (size_t) FS);
        const float rl  = rms (L, (size_t) FS, (size_t) FS), rr = rms (R, (size_t) FS, (size_t) FS);
        std::printf ("in rms %.3f  out rms L %.3f R %.3f (ratio %.2f)  peak %.2f\n", rin, rl, rr, 0.5f * (rl + rr) / rin, peak);
        CHECK (finite, "non-finite output");
        CHECK (peak < 4.0f, "output peak too high");
        const float ratio = 0.5f * (rl + rr) / rin;
        CHECK (ratio > 0.25f && ratio < 1.5f, "output level out of range");
    }

    // 3. every chord / extreme vowel / extreme settings stay finite and bounded
    {
        std::printf ("extreme settings ... ");
        bool ok = true; float peak = 0;
        const auto in = makeVoice (N / 2, 180.0f);
        for (int c = 0; c < kNumChords; ++c)
            for (float vowel : { 0.0f, 1.5f, 4.0f })
            {
                Engine e; e.prepare (FS);
                Params p; p.numVoices = 16; p.chord = c; p.vowel = vowel; p.formantAmt = 1.0f;
                p.detuneCents = 50; p.timingMs = 60; p.vibDepth = 50; p.vibRate = 8; p.spread = 1;
                e.setParams (p);
                for (size_t i = 0; i < in.size(); ++i)
                {
                    float l, r; e.process (in[i], l, r);
                    if (! std::isfinite (l) || ! std::isfinite (r)) ok = false;
                    peak = std::max ({ peak, std::fabs (l), std::fabs (r) });
                }
            }
        std::printf ("peak %.2f\n", peak);
        CHECK (ok, "non-finite output at extreme settings");
        CHECK (peak < 6.0f, "extreme-settings peak too high");
    }

    // 4. changing voice count / chord live must not produce hard discontinuities
    {
        std::printf ("live parameter changes ... ");
        Engine e; e.prepare (FS);
        Params p; p.numVoices = 4; e.setParams (p);
        const auto in = makeVoice (N, 200.0f);
        float prevL = 0, maxStep = 0, maxAbs = 0;
        for (size_t i = 0; i < N; ++i)
        {
            if (i == (size_t) FS)        { p.numVoices = 12; e.setParams (p); }
            if (i == (size_t) (2 * FS))  { p.numVoices = 2;  p.chord = 3; e.setParams (p); }
            float l, r; e.process (in[i], l, r);
            if (i > (size_t) (0.5f * FS)) { maxStep = std::max (maxStep, std::fabs (l - prevL)); maxAbs = std::max (maxAbs, std::fabs (l)); }
            prevL = l;
        }
        std::printf ("max sample step %.3f (max |y| %.3f)\n", maxStep, maxAbs);
        CHECK (maxStep < 1.0f, "discontinuity when changing parameters");
    }

    std::printf (failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}

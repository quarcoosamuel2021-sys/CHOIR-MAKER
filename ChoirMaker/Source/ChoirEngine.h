// ChoirEngine.h
// Framework-free DSP core: turns one mono voice into a multi-voice choir.
// Pure C++17, no JUCE dependency, so it can be unit-tested on its own.
//
// Per voice:
//   - shared input delay line -> time-domain pitch shifter: a read head that moves at the
//     pitch ratio, and when it runs out of room jumps by one grain to the position that best
//     lines up with the waveform (cross-correlation search, i.e. SOLA), with a short crossfade.
//     Because the jump is phase-aligned there is no comb filtering / tremolo on sustained notes.
//   - static detune + slow random drift + vibrato (own rate/depth/phase)
//   - own timing offset (voices never start exactly together)
//   - 3-band formant filter bank (A E I O U morph, slightly different per singer)
//   - own stereo position, constant-power pan
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace choir
{
constexpr int   kMaxVoices   = 16;
constexpr int   kControlRate = 32;      // samples between control-rate updates
constexpr float kPi          = 3.14159265358979f;
constexpr float kTwoPi       = 6.28318530717959f;

// ---------------------------------------------------------------------------
// Chord / interval sets. Voice i sings semis[i % count] relative to the input.
// The input note is always voice 0 (interval 0).
struct Chord
{
    const char* name;
    int count;
    int semis[6];
};

inline constexpr Chord kChords[] = {
    { "Unison (ensemble)",  1, { 0 } },
    { "Octaves",            3, { 0, -12, 12 } },
    { "Open 5th",           4, { 0, 7, -5, 12 } },
    { "Major",              5, { 0, 4, 7, 12, -12 } },
    { "Minor",              5, { 0, 3, 7, 12, -12 } },
    { "Sus2",               4, { 0, 2, 7, 12 } },
    { "Sus4",               4, { 0, 5, 7, 12 } },
    { "Major 7",            5, { 0, 4, 7, 11, 12 } },
    { "Minor 7",            5, { 0, 3, 7, 10, 12 } },
    { "Major (voice on top)", 4, { 0, -3, -7, -12 } },
    { "Minor (voice on top)", 4, { 0, -4, -7, -12 } },
};
inline constexpr int kNumChords = (int) (sizeof (kChords) / sizeof (kChords[0]));

// Vowel formants (Hz) for A E I O U, plus per-band Q and gain.
inline constexpr float kVowelF[5][3] = {
    { 800.0f, 1150.0f, 2900.0f },   // A
    { 400.0f, 1600.0f, 2700.0f },   // E
    { 350.0f, 1700.0f, 2700.0f },   // I
    { 450.0f,  800.0f, 2830.0f },   // O
    { 325.0f,  700.0f, 2530.0f },   // U
};
inline constexpr float kFormantQ[3]    = { 7.0f, 9.0f, 11.0f };
inline constexpr float kFormantGain[3] = { 1.0f, 0.6f, 0.35f };

struct Params
{
    int   numVoices   = 8;
    int   chord       = 0;
    float vowel       = 0.0f;    // 0..4 morph  A E I O U
    float formantAmt  = 0.7f;    // 0..1
    float detuneCents = 15.0f;   // spread of per-voice static detune
    float timingMs    = 25.0f;   // max per-voice onset offset
    float vibDepth    = 12.0f;   // cents
    float vibRate     = 5.2f;    // Hz
    float spread      = 0.8f;    // 0..1 stereo width
};

// ---------------------------------------------------------------------------
// Zavalishin/Cytomic TPT state-variable filter, band-pass with unity peak gain.
struct Svf
{
    float s1 = 0, s2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;

    void set (float fc, float q, float fs)
    {
        fc = std::min (fc, 0.45f * fs);
        const float g = std::tan (kPi * fc / fs);
        k  = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float bandpass (float v0)
    {
        const float v3 = v0 - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        return k * v1;
    }

    void reset() { s1 = s2 = 0; }
};

// ---------------------------------------------------------------------------
class Engine
{
public:
    // makeup gain of the formant branch (calibrated in tests/engine_test.cpp)
    static constexpr float kFormantMakeup = 4.0f;

    void prepare (double sampleRate)
    {
        fs       = (float) sampleRate;
        minDelay = 3.0f;
        lenFade  = std::max (32, (int) (0.008f * fs));
        lenWin   = std::max (64, ((int) (0.010f * fs)) & ~3);
        range    = std::max (16, ((int) (0.0067f * fs)) & ~3);
        jump     = std::max (256.0f, 0.030f * fs);
        dLow     = minDelay + 8.0f + (float) range + 1.1f * (float) lenFade;
        latency  = (int) std::lround (dLow + 0.5f * jump);

        fadeTable.resize ((size_t) lenFade);
        for (int i = 0; i < lenFade; ++i)
            fadeTable[(size_t) i] = 0.5f - 0.5f * std::cos (kPi * ((float) i + 0.5f) / (float) lenFade);

        const int need = (int) (dLow + jump + (float) range + 0.07f * fs + (float) lenWin + 64.0f);
        size = 1;
        while (size < need) size <<= 1;
        mask = (uint32_t) (size - 1);
        buf.assign ((size_t) size, 0.0f);
        w = 0;

        driftAlpha = 1.0f - std::exp (-(float) kControlRate / (0.15f * fs));
        delayAlpha = 1.0f - std::exp (-(float) kControlRate / (0.05f * fs));
        gainAlpha  = 1.0f - std::exp (-(float) kControlRate / (0.015f * fs));

        initVoices();
        lastN = -1;
        lastVowel = -1.0f;
        ctl = 0;
        setParams (params);
    }

    int latencySamples() const { return latency; }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (auto& v : voices)
        {
            for (auto& f : v.f) f.reset();
            v.gainL = v.gainR = 0.0f;
        }
    }

    void setParams (const Params& p)
    {
        params = p;
        params.numVoices = std::clamp (p.numVoices, 1, kMaxVoices);
        params.chord     = std::clamp (p.chord, 0, kNumChords - 1);
        params.vowel     = std::clamp (p.vowel, 0.0f, 4.0f);

        if (params.numVoices != lastN)
        {
            layoutPans (params.numVoices);
            lastN = params.numVoices;
        }

        const Chord& c = kChords[params.chord];
        for (int i = 0; i < kMaxVoices; ++i)
            voices[(size_t) i].interval = c.semis[i % c.count];

        if (params.vowel != lastVowel)
        {
            lastVowel = params.vowel;
            updateFormants();
        }
    }

    // One mono sample in, stereo choir out.
    inline void process (float in, float& outL, float& outR)
    {
        ++w;
        buf[w & mask] = in;

        if (ctl <= 0) { controlUpdate(); ctl = kControlRate; }
        --ctl;

        const float fa = params.formantAmt;
        float l = 0.0f, r = 0.0f;

        for (auto& v : voices)
        {
            if (! v.running) continue;

            // pitch shifter: read head moves at the pitch ratio (delay changes by 1 - ratio)
            float s = read (v.baseDelay + v.dA);
            v.dA += 1.0f - v.ratio;

            if (v.fading)
            {
                const float sB = read (v.baseDelay + v.dB);
                v.dB += 1.0f - v.ratio;
                const float g = fadeTable[(size_t) v.fadePos];
                s = (1.0f - g) * s + g * sB;
                if (++v.fadePos >= lenFade) { v.dA = v.dB; v.fading = false; }
            }
            else if (v.ratio > 1.0f && v.dA <= dLow)
                startJump (v, +jump);
            else if (v.ratio < 1.0f && v.dA >= dLow + jump)
                startJump (v, -jump);

            if (fa > 0.0f)
            {
                const float f = v.f[0].bandpass (s) * kFormantGain[0]
                              + v.f[1].bandpass (s) * kFormantGain[1]
                              + v.f[2].bandpass (s) * kFormantGain[2];
                s = (1.0f - fa) * s + fa * kFormantMakeup * f;
            }

            l += s * v.gainL;
            r += s * v.gainR;
        }

        outL = l;
        outR = r;
    }

private:
    struct Voice
    {
        int   interval = 0;
        float detuneStatic = 0, timingFrac = 0, basePan = 0, level = 1;
        float dA = 0, dB = 0, ratio = 1, baseDelay = 0;
        int   fadePos = 0;
        bool  fading = false;
        float vibPhase = 0, vibRateMul = 1, vibDepthMul = 1;
        float drift = 0, driftTarget = 0;
        int   driftCountdown = 0;
        float vowelOffset[3] = { 1, 1, 1 };
        float gainL = 0, gainR = 0;
        bool  running = false;
        uint32_t rng = 1;
        Svf   f[3];
    };

    static float rand01 (uint32_t& s)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (s >> 8) * (1.0f / 16777216.0f);
    }
    static float frac (float x) { return x - std::floor (x); }

    void initVoices()
    {
        for (int i = 0; i < kMaxVoices; ++i)
        {
            Voice& v = voices[(size_t) i];
            v = Voice();
            v.rng = 0x9E3779B9u * (uint32_t) (i + 1) + 12345u;
            for (int k = 0; k < 4; ++k) rand01 (v.rng);

            v.detuneStatic = (i == 0) ? 0.0f : frac ((float) i * 0.6180339887f) * 2.0f - 1.0f;
            v.timingFrac   = (i == 0) ? 0.0f : frac ((float) i * 0.7548776662f);
            v.dA           = dLow + rand01 (v.rng) * jump;
            v.vibPhase     = rand01 (v.rng);
            v.vibRateMul   = 0.8f + 0.4f * rand01 (v.rng);
            v.vibDepthMul  = 0.6f + 0.6f * rand01 (v.rng);
            v.level        = 0.85f + 0.15f * rand01 (v.rng);
            for (float& o : v.vowelOffset) o = 0.94f + 0.12f * rand01 (v.rng);
            v.driftCountdown = (int) (rand01 (v.rng) * 0.5f * fs);
        }
        lastVowel = -1.0f;
    }

    // evenly spaced pan positions, voice 0 nearest the centre, then alternating outward
    void layoutPans (int n)
    {
        std::vector<float> pos ((size_t) n);
        for (int k = 0; k < n; ++k)
            pos[(size_t) k] = (n == 1) ? 0.0f : ((float) k / (float) (n - 1)) * 2.0f - 1.0f;

        std::stable_sort (pos.begin(), pos.end(), [] (float a, float b)
        {
            const float aa = std::fabs (a), bb = std::fabs (b);
            if (std::fabs (aa - bb) > 1.0e-6f) return aa < bb;
            return a < b;
        });

        for (int i = 0; i < kMaxVoices; ++i)
            voices[(size_t) i].basePan = (i < n) ? pos[(size_t) i] : 0.0f;
    }

    void updateFormants()
    {
        const int   i0 = std::min ((int) params.vowel, 3);
        const float t  = params.vowel - (float) i0;
        float freq[3];
        for (int j = 0; j < 3; ++j)
            freq[j] = kVowelF[i0][j] * (1.0f - t) + kVowelF[i0 + 1][j] * t;

        for (auto& v : voices)
            for (int j = 0; j < 3; ++j)
                v.f[j].set (freq[j] * v.vowelOffset[j], kFormantQ[j], fs);
    }

    void controlUpdate()
    {
        const float norm = 1.0f / std::sqrt ((float) params.numVoices);

        for (int i = 0; i < kMaxVoices; ++i)
        {
            Voice& v = voices[(size_t) i];
            const bool wanted = i < params.numVoices;

            // fade voices in / out
            const float targetLevel = wanted ? v.level * norm : 0.0f;
            const float angle = (v.basePan * params.spread + 1.0f) * (kPi * 0.25f);
            const float tgtL = targetLevel * std::cos (angle);
            const float tgtR = targetLevel * std::sin (angle);
            v.gainL += (tgtL - v.gainL) * gainAlpha;
            v.gainR += (tgtR - v.gainR) * gainAlpha;

            if (! wanted && v.gainL < 1.0e-5f && v.gainR < 1.0e-5f)
            {
                v.running = false;
                continue;
            }
            v.running = true;

            // vibrato + drift
            const float vib = std::sin (kTwoPi * v.vibPhase);
            v.vibPhase += params.vibRate * v.vibRateMul * (float) kControlRate / fs;
            if (v.vibPhase >= 1.0f) v.vibPhase -= 1.0f;

            v.driftCountdown -= kControlRate;
            if (v.driftCountdown <= 0)
            {
                v.driftTarget    = rand01 (v.rng) * 2.0f - 1.0f;
                v.driftCountdown = (int) ((0.25f + 0.5f * rand01 (v.rng)) * fs);
            }
            v.drift += (v.driftTarget - v.drift) * driftAlpha;

            const float cents = (float) v.interval * 100.0f
                              + v.detuneStatic * params.detuneCents
                              + v.drift * (1.5f + 0.15f * params.detuneCents)
                              + params.vibDepth * v.vibDepthMul * vib;
            v.ratio = std::exp2 (cents / 1200.0f);

            // onset offset, slewed so moving the knob doesn't click
            const float targetDelay = v.timingFrac * params.timingMs * 0.001f * fs;
            v.baseDelay += (targetDelay - v.baseDelay) * delayAlpha;
        }
    }

    // Jump the read head by jv samples of delay (+ = further back in time), choosing the
    // offset within +-range whose recent waveform best matches what we're playing now.
    void startJump (Voice& v, float jv)
    {
        const int ia = (int) w - (int) (v.baseDelay + v.dA);          // current read position
        const int ib = ia - (int) jv;                                 // candidate centre
        const uint32_t m = mask;
        auto at = [this, m] (int i) { return buf[(uint32_t) i & m]; };

        auto score = [&] (int delta, int stride)
        {
            const int a0 = ia - lenWin, b0 = ib - delta - lenWin;
            float sc = 0.0f, en = 1.0e-9f;
            for (int k = 0; k < lenWin; k += stride)
            {
                const float x = at (a0 + k), y = at (b0 + k);
                sc += x * y;
                en += y * y;
            }
            return sc / std::sqrt (en);
        };

        int   best  = 0;
        float bestS = -1.0e30f;
        for (int d = -range; d <= range; d += 4)
        {
            const float sc = score (d, 4);
            if (sc > bestS) { bestS = sc; best = d; }
        }
        for (int d = best - 3; d <= best + 3; ++d)
        {
            if (d < -range || d > range) continue;
            const float sc = score (d, 1);
            if (sc > bestS - 1.0e-9f && d != best) { bestS = std::max (bestS, sc); best = d; }
        }

        v.dB      = v.dA + jv + (float) best;
        v.fadePos = 0;
        v.fading  = true;
    }

    // 4-point Hermite interpolated read, d samples behind the newest sample (d >= 2)
    inline float read (float d) const
    {
        const int      di = (int) d;
        const float    t  = d - (float) di;
        const uint32_t i0 = w - (uint32_t) di;

        const float xm1 = buf[(i0 + 1) & mask];
        const float x0  = buf[i0 & mask];
        const float x1  = buf[(i0 - 1) & mask];
        const float x2  = buf[(i0 - 2) & mask];

        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    Voice voices[kMaxVoices];
    Params params;
    std::vector<float> buf, fadeTable;
    uint32_t mask = 0, w = 0;
    int   size = 0, latency = 0, ctl = 0, lastN = -1;
    int   lenFade = 384, lenWin = 480, range = 320;
    float fs = 48000.0f, minDelay = 3.0f, jump = 1440.0f, dLow = 740.0f;
    float driftAlpha = 0.01f, delayAlpha = 0.01f, gainAlpha = 0.05f, lastVowel = -1.0f;
};

} // namespace choir

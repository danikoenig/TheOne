#pragma once
// Effekt-Slots von «The One»: Delays, Chorus-Familie, Flanger, Phaser, Bitcrusher, Resonator.
#include "Params.h"
#include <vector>
#include <cstdint>

namespace TO
{
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void init (int minSize)
    {
        int n = 1; while (n < minSize) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float d) const;   // d in Samples (>= 1)
};

struct Smooth
{
    float v = 0; bool init = false;
    float step (float target, float c) { if (! init) { v = target; init = true; } v += c * (target - v); return v; }
    void snap() { init = false; }
};

class FxSlot
{
public:
    void prepare (double sampleRate);
    // p: fxP-Basis dieses Slots im Parameter-Array
    void process (float* L, float* R, int n, const float* p, double bpm);

private:
    void clearState();
    double sr = 44100.0;
    int lastType = -1;
    DelayLine dl[2];
    float lp[2] = { 0, 0 }, hp[2] = { 0, 0 };
    float ap1[2][2] = {}, ap2[2][2] = {};            // Echo: Allpass-Zustaende
    DelayLine apLine[2][2];
    float lfoPh[4] = { 0, 0, 0, 0 };
    float apS[2][12] = {};                           // Phaser-Allpaesse
    float apFb[2] = { 0, 0 };
    float svf[2][3][2] = {};                         // Resonator: Kanal, Band, s1/s2
    float holdVal[2] = { 0, 0 }; float holdCnt[2] = { 0, 0 };
    float tapeLp[2] = { 0, 0 };
    float chFb[2] = { 0, 0 };
    uint32_t rng = 1234567u;
    Smooth sTime[2], sMix, sFb, sTone, sP4, sP5, sRate;
    float noise() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (int32_t) rng * (1.0f / 2147483648.0f); }
};

class FxChain
{
public:
    void prepare (double sampleRate) { for (auto& s : slots) s.prepare (sampleRate); }
    void process (float* L, float* R, int n, const float* params, double bpm)
    {
        for (int i = 0; i < 3; ++i) slots[i].process (L, R, n, params + fxP (i, 0), bpm);
    }
private:
    FxSlot slots[3];
};
} // namespace TO

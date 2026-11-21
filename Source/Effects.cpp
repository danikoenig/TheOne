#include "Effects.h"
#include <algorithm>
#include <cmath>

namespace TO
{
static constexpr float kPi = 3.14159265358979f;

float DelayLine::read (float d) const
{
    const float rp = (float) w - d;
    const float fl = std::floor (rp);
    const float f = rp - fl;
    const int i1 = ((int) fl) & mask, i0 = (i1 - 1) & mask, i2 = (i1 + 1) & mask, i3 = (i1 + 2) & mask;
    const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
    const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

static inline float onePoleA (float fc, double sr) { return 1.0f - std::exp (-2.0f * kPi * fc / (float) sr); }
static inline float softClip (float x) { return x > 3.0f ? 1.0f : (x < -3.0f ? -1.0f : x * (27.0f + x * x) / (27.0f + 9.0f * x * x)); }

void FxSlot::prepare (double sampleRate)
{
    sr = sampleRate;
    const int maxD = (int) (2.6 * sr) + 8;
    dl[0].init (maxD); dl[1].init (maxD);
    for (auto& r : apLine) for (auto& l : r) l.init (2048);
    lastType = -1;
}

void FxSlot::clearState()
{
    dl[0].clear(); dl[1].clear();
    for (auto& r : apLine) for (auto& l : r) l.clear();
    for (int c = 0; c < 2; ++c)
    {
        lp[c] = hp[c] = 0; tapeLp[c] = 0; chFb[c] = 0; apFb[c] = 0; holdVal[c] = 0; holdCnt[c] = 0;
        for (auto& v : apS[c]) v = 0;
        for (auto& b : svf[c]) { b[0] = b[1] = 0; }
    }
    for (auto& s : sTime) s.snap();
    sMix.snap(); sFb.snap(); sTone.snap(); sP4.snap(); sP5.snap(); sRate.snap();
}

void FxSlot::process (float* L, float* R, int n, const float* p, double bpm)
{
    const int type = std::clamp ((int) std::lround (p[FX_TYPE]), 0, FX_NUM - 1);
    if (type != lastType) { clearState(); lastType = type; }
    if (type == FX_OFF) return;

    const bool sync = p[FX_SYNC] > 0.5f;
    const float mixT = p[FX_MIX];
    const float p1 = p[FX_P1], p2 = p[FX_P2], p3 = p[FX_P3], p4 = p[FX_P4], p5 = p[FX_P5];
    const float fsr = (float) sr;
    const float maxDelay = 2.5f * fsr;
    const float beatSec = (float) (60.0 / std::max (20.0, bpm));
    const float cs = 0.002f;   // Glaettung

    // Delay-Zeit (Sekunden) aus P1
    float baseTime = 0.01f * std::pow (200.0f, p1);
    if (sync) baseTime = kFxDivBeats[fxDivIndex (p1)] * beatSec;
    baseTime = std::clamp (baseTime, 0.0005f, 2.4f);
    // Modulationsrate (Hz) aus P1
    float modRate = 0.1f * std::pow (100.0f, p1);
    if (sync) modRate = (1.0f / beatSec) / std::max (0.0625f, kFxDivBeats[fxDivIndex (p1)]);

    const bool delayType = type >= FX_DELAY && type <= FX_ECHO;
    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i], inR = R[i];
        float wetL = 0, wetR = 0;
        const float mix = sMix.step (mixT, cs);

        if (delayType)
        {
            const float fb = sFb.step (p2 * 0.95f, cs);
            const float a = onePoleA (500.0f * std::pow (32.0f, sTone.step (p3, cs)), sr);
            float tL = baseTime, tR = baseTime;
            const float x = 0.5f * (inL + inR);
            float wobble = 0.0f;
            switch (type)
            {
                case FX_DELAY: tR = baseTime * (1.0f + 0.5f * p4); break;
                case FX_STEREO: tR = baseTime * (0.5f + p4); break;
                case FX_BBD:
                    lfoPh[0] += 0.7f / fsr; if (lfoPh[0] >= 1.0f) lfoPh[0] -= 1.0f;
                    wobble = std::sin (2.0f * kPi * lfoPh[0]) * p4 * 0.002f;
                    break;
                case FX_TAPE:
                    lfoPh[0] += 0.5f / fsr; if (lfoPh[0] >= 1.0f) lfoPh[0] -= 1.0f;
                    lfoPh[1] += 6.3f / fsr; if (lfoPh[1] >= 1.0f) lfoPh[1] -= 1.0f;
                    wobble = (std::sin (2.0f * kPi * lfoPh[0]) * 0.7f + std::sin (2.0f * kPi * lfoPh[1]) * 0.3f) * p4 * 0.0015f;
                    break;
                default: break;
            }
            const float sL = sTime[0].step (tL + wobble, 0.0008f), sR = sTime[1].step (tR + wobble, 0.0008f);
            const float dL = std::clamp (sL * fsr, 2.0f, maxDelay), dR = std::clamp (sR * fsr, 2.0f, maxDelay);
            const float oL = dl[0].read (dL), oR = dl[1].read (dR);
            float wL = oL, wR = oR;
            lp[0] += a * (oL - lp[0]); lp[1] += a * (oR - lp[1]);
            float fL = lp[0], fR = lp[1];
            float wL_in, wR_in;
            switch (type)
            {
                case FX_STEREO: wL_in = inL + fb * fL; wR_in = inR + fb * fR; break;
                case FX_PINGPONG:
                {
                    wL_in = x + fb * fR; wR_in = fL;
                    const float w = p4;      // Breite
                    wL = oL * (0.5f + 0.5f * w) + oR * (0.5f - 0.5f * w);
                    wR = oR * (0.5f + 0.5f * w) + oL * (0.5f - 0.5f * w);
                    break;
                }
                case FX_BBD:
                {
                    // zweites Tiefpass-Stueck in der Schleife + Quantisierung ("Eimerketten-Koernung")
                    tapeLp[0] += 0.35f * (fL - tapeLp[0]); tapeLp[1] += 0.35f * (fR - tapeLp[1]);
                    const float steps = std::pow (2.0f, 13.0f - 6.0f * p5);
                    const float nz = p5 * 0.004f;
                    wL_in = std::round ((x + fb * tapeLp[0]) * steps) / steps + noise() * nz;
                    wR_in = std::round ((x + fb * tapeLp[1]) * steps) / steps + noise() * nz;
                    wL_in = softClip (wL_in); wR_in = softClip (wR_in);
                    break;
                }
                case FX_TAPE:
                {
                    const float drive = 1.0f + p5 * 4.0f;
                    hp[0] += 0.0008f * (fL - hp[0]); hp[1] += 0.0008f * (fR - hp[1]);
                    wL_in = softClip ((x + fb * (fL - hp[0])) * drive) / drive;
                    wR_in = softClip ((x + fb * (fR - hp[1])) * drive) / drive;
                    break;
                }
                case FX_ECHO:
                {
                    // Feder-Diffusion: Allpass-Kette in der Rueckkopplung
                    float yL = fL, yR = fR;
                    for (int s = 0; s < 2; ++s)
                    {
                        const float g = 0.3f + 0.45f * p4;
                        const float d1 = 190.0f + 130.0f * s, d2 = 233.0f + 150.0f * s;
                        float bl = apLine[0][s].read (s == 0 ? d1 : d2), br = apLine[1][s].read (s == 0 ? d1 : d2);
                        const float vl = yL - g * bl, vr = yR - g * br;
                        apLine[0][s].push (vl); apLine[1][s].push (vr);
                        yL = bl + g * vl; yR = br + g * vr;
                    }
                    wL_in = x + fb * yL; wR_in = x + fb * yR;
                    break;
                }
                default: wL_in = x + fb * fL; wR_in = x + fb * fR; break;
            }
            dl[0].push (wL_in); dl[1].push (wR_in);
            wetL = wL; wetR = wR;
            L[i] = inL + wetL * mix;
            R[i] = inR + wetR * mix;
            continue;
        }

        if (type == FX_CHORUS || type == FX_DUALCHORUS || type == FX_ENSEMBLE)
        {
            const int taps = type == FX_CHORUS ? 1 : (type == FX_DUALCHORUS ? 2 : 3);
            const float rate = sRate.step (modRate, 0.001f);
            const float depth = sP4.step (p2, cs) * 0.006f;                 // bis 6 ms
            const float base = 0.005f + 0.025f * p3;                         // 5..30 ms
            const float spread = p4;
            lfoPh[0] += rate / fsr; if (lfoPh[0] >= 1.0f) lfoPh[0] -= 1.0f;
            lfoPh[1] += 5.2f / fsr; if (lfoPh[1] >= 1.0f) lfoPh[1] -= 1.0f;
            const float fbk = type == FX_CHORUS ? p5 * 0.6f : 0.0f;
            const float vib = type == FX_CHORUS ? 0.0f : p5 * 0.0012f;
            dl[0].push (inL + fbk * chFb[0]); dl[1].push (inR + fbk * chFb[1]);
            float sum[2] = { 0, 0 };
            for (int c = 0; c < 2; ++c)
                for (int t = 0; t < taps; ++t)
                {
                    float ph = lfoPh[0] + (float) t / (float) taps + (c == 1 ? 0.25f * spread * 2.0f : 0.0f);
                    ph -= std::floor (ph);
                    float tm = base + depth * std::sin (2.0f * kPi * ph) + vib * std::sin (2.0f * kPi * lfoPh[1]);
                    sum[c] += dl[c].read (std::clamp (tm * fsr, 2.0f, maxDelay));
                }
            wetL = sum[0] / (float) taps; wetR = sum[1] / (float) taps;
            chFb[0] = wetL; chFb[1] = wetR;
            L[i] = inL * (1.0f - mix) + wetL * mix;
            R[i] = inR * (1.0f - mix) + wetR * mix;
            continue;
        }

        if (type == FX_FLANGER)
        {
            const float rate = sRate.step (modRate, 0.001f);
            const float fb = (sFb.step (p3, cs) * 2.0f - 1.0f) * 0.92f;
            const float manual = 0.0004f + 0.008f * p4;
            const float depth = p2 * manual * 0.9f;
            lfoPh[0] += rate / fsr; if (lfoPh[0] >= 1.0f) lfoPh[0] -= 1.0f;
            float wet[2];
            const float in[2] = { inL, inR };
            for (int c = 0; c < 2; ++c)
            {
                float ph = lfoPh[0] + (c == 1 ? 0.5f * p5 : 0.0f); ph -= std::floor (ph);
                const float tm = manual + depth * std::sin (2.0f * kPi * ph);
                const float y = dl[c].read (std::clamp (tm * fsr, 2.0f, maxDelay));
                dl[c].push (in[c] + fb * chFb[c]);
                chFb[c] = softClip (y);
                wet[c] = y;
            }
            L[i] = inL * (1.0f - mix) + wet[0] * mix;
            R[i] = inR * (1.0f - mix) + wet[1] * mix;
            continue;
        }

        if (type == FX_PHASER6 || type == FX_PHASER12)
        {
            const int stages = type == FX_PHASER6 ? 6 : 12;
            const float rate = sRate.step (modRate, 0.001f);
            const float fbk = sFb.step (p3, cs) * 0.9f;
            const float fc0 = 200.0f * std::pow (20.0f, p4);
            lfoPh[0] += rate / fsr; if (lfoPh[0] >= 1.0f) lfoPh[0] -= 1.0f;
            const float in[2] = { inL, inR };
            float out[2];
            for (int c = 0; c < 2; ++c)
            {
                float ph = lfoPh[0] + (c == 1 ? 0.5f * p5 : 0.0f); ph -= std::floor (ph);
                const float f = std::clamp (fc0 * std::exp2 (p2 * 2.5f * std::sin (2.0f * kPi * ph)), 30.0f, 0.45f * fsr);
                const float tn = std::tan (kPi * f / fsr);
                const float a = (tn - 1.0f) / (tn + 1.0f);
                float x = in[c] + fbk * apFb[c];
                for (int s = 0; s < stages; ++s)
                {
                    const float y = a * x + apS[c][s];
                    apS[c][s] = x - a * y;
                    x = y;
                }
                apFb[c] = softClip (x);
                out[c] = x;
            }
            L[i] = inL * (1.0f - mix) + out[0] * mix;
            R[i] = inR * (1.0f - mix) + out[1] * mix;
            continue;
        }

        if (type == FX_CRUSHER)
        {
            const float bits = 24.0f - 22.0f * p1;
            const float steps = std::pow (2.0f, bits - 1.0f);
            const float holdSamples = 1.0f + (fsr / 200.0f - 1.0f) * p2 * p2;
            const float drive = 1.0f + 7.0f * p5;
            const float a = onePoleA (1000.0f * std::pow (20.0f, p3), sr);
            const float in[2] = { inL, inR };
            float out[2];
            for (int c = 0; c < 2; ++c)
            {
                holdCnt[c] += 1.0f;
                if (holdCnt[c] >= holdSamples)
                {
                    holdCnt[c] -= holdSamples;
                    holdCnt[c] += p4 * noise() * 0.5f * holdSamples;
                    if (holdCnt[c] < 0.0f) holdCnt[c] = 0.0f;
                    const float x = softClip (in[c] * drive) / (drive > 1.0f ? std::sqrt (drive) : 1.0f);
                    holdVal[c] = std::round (x * steps) / steps;
                }
                lp[c] += a * (holdVal[c] - lp[c]);
                out[c] = lp[c];
            }
            L[i] = inL * (1.0f - mix) + out[0] * mix;
            R[i] = inR * (1.0f - mix) + out[1] * mix;
            continue;
        }

        if (type == FX_RESONATOR)
        {
            const float q = 2.0f + 80.0f * p4 * p4;
            const float k = 1.0f / q;
            const float gainC = 1.0f + 0.6f * std::sqrt (q);
            lfoPh[0] += 0.25f / fsr; if (lfoPh[0] >= 1.0f) lfoPh[0] -= 1.0f;
            const float pb[3] = { p1, p2, p3 };
            const float in[2] = { inL, inR };
            float out[2] = { 0, 0 };
            for (int b = 0; b < 3; ++b)
            {
                float ph = lfoPh[0] + (float) b / 3.0f; ph -= std::floor (ph);
                const float f = std::clamp (60.0f * std::pow (130.0f, pb[b]) * std::exp2 (p5 * 1.5f * std::sin (2.0f * kPi * ph)), 30.0f, 0.45f * fsr);
                const float g = std::tan (kPi * f / fsr);
                for (int c = 0; c < 2; ++c)
                {
                    float& s1 = svf[c][b][0]; float& s2 = svf[c][b][1];
                    const float hpv = (in[c] - (k + g) * s1 - s2) / (1.0f + g * (k + g));
                    const float bp = g * hpv + s1;
                    const float lpv = g * bp + s2;
                    s1 = g * hpv + bp; s2 = g * bp + lpv;
                    out[c] += bp * k * gainC * 0.5f;
                }
            }
            for (int c = 0; c < 2; ++c) if (! std::isfinite (out[c])) { out[c] = 0; for (auto& bb : svf[c]) bb[0] = bb[1] = 0; }
            L[i] = inL * (1.0f - mix) + out[0] * mix;
            R[i] = inR * (1.0f - mix) + out[1] * mix;
            continue;
        }
    }
}
} // namespace TO

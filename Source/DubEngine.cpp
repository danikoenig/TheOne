#include "DubEngine.h"

namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr int subBlock = 32;
    constexpr double lfoOctPerFull = 8.2;   // Oktaven Hub bei Amount 100 %
    constexpr float  satMakeupDb = 7.6f;    // am Ableton-Rack gemessen: Analog Clip des Saturators klingt um diesen Betrag lauter
    // Pegelangleichung der anderen Saturation-Typen an Analog Clip (bei Drive 15 dB, gleiche Lautheit)
    constexpr float  satTrimDb[5] = { 0.0f, 3.4f, 4.6f, 4.4f, 1.7f };
    constexpr double lfoBias = 0.2;         // Versatz nach unten (der LFO schwingt staerker nach unten als nach oben)

    inline int nextPow2 (int v) { int n = 1; while (n < v) n <<= 1; return n; }
    inline float softLimitFb (float x)   // Sicherung im Feedback (bei 200 % wird es sonst unendlich laut)
    {
        const float a = std::fabs (x);
        if (a < 0.9f) return x;
        const float y = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
        return x < 0.0f ? -y : y;
    }
}

void DubEngine::Biquad::lowpass (double fs, double f, double q)
{
    const double w = 2.0 * pi * f / fs, c = std::cos (w), a = std::sin (w) / (2.0 * q);
    set ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + a, -2 * c, 1 - a);
}
void DubEngine::Biquad::highpass (double fs, double f, double q)
{
    const double w = 2.0 * pi * f / fs, c = std::cos (w), a = std::sin (w) / (2.0 * q);
    set ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + a, -2 * c, 1 - a);
}
void DubEngine::Biquad::bandpass (double fs, double f, double q)
{
    const double w = 2.0 * pi * f / fs, c = std::cos (w), a = std::sin (w) / (2.0 * q);
    set (a, 0, -a, 1 + a, -2 * c, 1 - a);
}

float DubEngine::Line::read (double d) const
{
    // d = Verzoegerung in Samples (>= 4), Hermite-Interpolation
    const double pos = (double) w - d;
    const double fl = std::floor (pos);
    const float f = (float) (pos - fl);
    const int i = (int) fl;
    const float y0 = buf[(size_t) ((i - 1) & mask)], y1 = buf[(size_t) (i & mask)],
                y2 = buf[(size_t) ((i + 1) & mask)], y3 = buf[(size_t) ((i + 2) & mask)];
    const float c0 = y1, c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

void DubEngine::prepare (double sampleRate)
{
    fs = sampleRate;
    const int size = nextPow2 ((int) std::ceil (fs * (DubDSP::maxDelaySec + 0.1)) + 16);
    for (auto& l : line) l.init (size);
    lookN = std::max (1, (int) std::lround (fs * DubDSP::lookaheadSec));
    lookL.assign ((size_t) lookN, 0.0f); lookR = lookL; dryL = lookL; dryR = lookL;
    needRing.assign ((size_t) lookN, 1.0f); minRing.assign ((size_t) lookN, 1.0f);
    reset();
}

void DubEngine::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        line[c].clear();
        head[c] = Head();
        dHp[c].clear(); dLp[c].clear(); af[c].clear();
        for (auto& b : low[c]) b.clear();
        tapeLp[c] = dcX[c] = dcY[c] = 0.0f;
    }
    // Low Cut: Butterworth 8. Ordnung = vier Stufen mit diesen Guetefaktoren (48 dB/Okt)
    static const double q8[4] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
    for (int c = 0; c < 2; ++c)
        for (int s = 0; s < 4; ++s)
            low[c][s].highpass (fs, DubDSP::lowCutHz, q8[s]);
    std::fill (lookL.begin(), lookL.end(), 0.0f); std::fill (lookR.begin(), lookR.end(), 0.0f);
    std::fill (dryL.begin(), dryL.end(), 0.0f);   std::fill (dryR.begin(), dryR.end(), 0.0f);
    std::fill (needRing.begin(), needRing.end(), 1.0f); std::fill (minRing.begin(), minRing.end(), 1.0f);
    maAvgSum = (double) lookN; gState = 1.0f; lw = 0;
    modPhase = lfoPhase = 0.0;
    smoothInit = false;
}

float DubEngine::readHead (Line& ln, Head& h, double target, int travel, double mod)
{
    const double minD = 4.0, maxD = (double) DubDSP::maxDelaySec * fs;
    auto clampD = [&] (double d) { return std::min (std::max (d, minD), maxD); };

    if (travel == 2)   // Jump
    {
        h.cur = target; h.fading = false;
        return ln.read (clampD (h.cur + mod));
    }
    if (travel == 0)   // Repitch: die Zeit gleitet, dabei aendert sich die Tonhoehe
    {
        h.fading = false;
        const double k = 1.0 - std::exp (-1.0 / (0.15 * fs));
        h.cur += (target - h.cur) * k;
        return ln.read (clampD (h.cur + mod));
    }
    // Fade: bei neuer Zeit wird zwischen alter und neuer Position ueberblendet
    if (! h.fading && std::fabs (target - h.cur) > 0.5)
    {
        h.fading = true; h.fadeTo = target; h.fadePos = 0.0;
    }
    if (! h.fading) return ln.read (clampD (h.cur + mod));
    const float f = (float) h.fadePos;
    const float y = ln.read (clampD (h.cur + mod)) * (1.0f - f) + ln.read (clampD (h.fadeTo + mod)) * f;
    h.fadePos += 1.0 / (0.05 * fs);
    if (h.fadePos >= 1.0) { h.cur = h.fadeTo; h.fading = false; }
    return y;
}

float DubEngine::saturate (float x, int type, int ch)
{
    switch (type)
    {
        default:
        case 0:   // Analog Clip: weiches Clipping, kubisch
        {
            if (x >= 1.0f) return 1.0f;
            if (x <= -1.0f) return -1.0f;
            return 1.5f * (x - x * x * x / 3.0f);   // normiert: +/-1 bei |x| = 1
        }
        case 1:   // Soft: tanh
            return std::tanh (x);
        case 2:   // Tape: tanh, Hoehen werden sanft abgesenkt
        {
            const float y = std::tanh (0.9f * x);
            tapeLp[ch] += 0.45f * (y - tapeLp[ch]);
            return tapeLp[ch];
        }
        case 3:   // Tube: asymmetrisch (gerade Obertoene), Gleichanteil wird entfernt
        {
            const float y = x >= 0.0f ? std::tanh (x) : std::tanh (0.6f * x) / 0.6f * 0.75f;
            const float o = y - dcX[ch] + 0.995f * dcY[ch];
            dcX[ch] = y; dcY[ch] = o;
            return o;
        }
        case 4:   // Fold: Wavefolder
            return std::sin (1.2f * x);
    }
}

void DubEngine::process (float* L, float* R, int numSamples, const DubParams& p)
{
    const float dFs = (float) fs;

    // Zeiten (Samples)
    auto timeSamples = [&] (int div, float ms)
    {
        double sec;
        if (p.timeMode == 2) sec = ms * 0.001;
        else
        {
            const double bpm = std::max (20.0, (double) (p.timeMode == 0 ? p.hostBpm : p.bpm));
            sec = DubDSP::sixteenths[std::min (std::max (div, 0), DubDSP::numDivisions - 1)] * 60.0 / bpm / 4.0;
        }
        sec = std::min (std::max (sec, 0.001), (double) DubDSP::maxDelaySec);
        return sec * fs;
    };
    const double tgtL = timeSamples (p.divL, p.msL);
    const double tgtR = p.dual ? timeSamples (p.divR, p.msR) : tgtL;

    if (! smoothInit)
    {
        sFb = p.feedback * 0.01f; sDrive = DubDSP::toLin (p.driveDb); sSatOut = DubDSP::toLin (p.satOutDb);
        sMix = p.mix * 0.01f; sOut = DubDSP::toLin (p.outputDb); sAfF = p.afFreq;
        sDelF = p.delFilterFreq; sDelW = p.delFilterWidth;
        head[0].cur = tgtL; head[1].cur = tgtR;   // beim Start nicht von 0 aus gleiten
        smoothInit = true;
    }
    if (p.pingPong != prevPingPong) { head[1].cur = tgtR; prevPingPong = p.pingPong; }

    const float satGain = DubDSP::toLin (satMakeupDb + satTrimDb[std::min (std::max (p.satType, 0), 4)]);
    const float kSm = 1.0f - std::exp (-(float) subBlock / (dFs * 0.02f));
    const float relK = 1.0f - std::exp (-1.0f / (dFs * 0.3f));
    float blockPeak = 0.0f;

    for (int s0 = 0; s0 < numSamples; s0 += subBlock)
    {
        const int n = std::min (subBlock, numSamples - s0);

        // Parameter glaetten, Filter nachfuehren
        sFb += (p.feedback * 0.01f - sFb) * kSm;
        sDrive += (DubDSP::toLin (p.driveDb) - sDrive) * kSm;
        sSatOut += (DubDSP::toLin (p.satOutDb) - sSatOut) * kSm;
        sMix += (p.mix * 0.01f - sMix) * kSm;
        sOut += (DubDSP::toLin (p.outputDb) - sOut) * kSm;
        sDelF += (p.delFilterFreq - sDelF) * kSm;
        sDelW += (p.delFilterWidth - sDelW) * kSm;
        sAfF += (p.afFreq - sAfF) * kSm;

        // Delay-Filter: HP unten und LP oben, symmetrisch um Freq (Breite in Oktaven)
        {
            const double half = std::pow (2.0, 0.5 * (double) sDelW);
            const double hp = std::min (std::max ((double) sDelF / half, 20.0), 18000.0);
            const double lp = std::min (std::max ((double) sDelF * half, 60.0), 0.45 * fs);
            for (int c = 0; c < 2; ++c) { dHp[c].highpass (fs, hp, 0.7071); dLp[c].lowpass (fs, lp, 0.7071); }
        }
        // Auto Filter: Bandpass mit sinusfoermigem LFO. Hub und Form sind am Ableton-Rack gemessen:
        // bei Amount 16.5 laeuft die Mittenfrequenz (863 Hz) etwa von 280 bis 1800 Hz, der Hub nach unten ist groesser
        // als nach oben. Der LFO laeuft frei (die Phase wird nicht neu gestartet).
        {
            lfoPhase += (double) p.lfoRate * n / fs; lfoPhase -= std::floor (lfoPhase);
            const double oct = (double) p.lfoAmount * 0.01 * lfoOctPerFull * (std::sin (2.0 * pi * lfoPhase) - lfoBias);
            const double f = std::min (std::max ((double) sAfF * std::pow (2.0, oct), 20.0), 0.45 * fs);
            for (int c = 0; c < 2; ++c) af[c].bandpass (fs, f, std::max (0.1, (double) p.afQ));
        }

        const float modDepthSamples = p.modDepth * 0.01f * 0.008f * dFs;   // 100 % = +/- 8 ms
        const double modInc = (double) p.modRate / fs;

        for (int i = 0; i < n; ++i)
        {
            const int idx = s0 + i;
            const float inL = L[idx], inR = R[idx];

            // LFO auf die Delay-Zeit (Dreieck)
            modPhase += modInc; modPhase -= std::floor (modPhase);
            const double tri = 4.0 * std::fabs (modPhase - 0.5) - 1.0;
            const double mod = tri * modDepthSamples;

            // Delay lesen, Filter im Feedback
            float dl = readHead (line[0], head[0], tgtL, p.travel, mod);
            float dr = readHead (line[1], head[1], p.pingPong ? tgtL : tgtR, p.travel, mod);
            dl = dLp[0].process (dHp[0].process (dl));
            dr = dLp[1].process (dHp[1].process (dr));

            // Schreiben: Ping Pong = L bekommt den Eingang und das Echo von R, R das Echo von L
            if (p.pingPong)
            {
                line[0].write (softLimitFb (0.5f * (inL + inR) + sFb * dr));
                line[1].write (softLimitFb (dl));
            }
            else
            {
                line[0].write (softLimitFb (inL + sFb * dl));
                line[1].write (softLimitFb (inR + sFb * dr));
            }

            // Saturator -> Auto Filter -> Low Cut
            float w[2] = { dl, dr };
            for (int c = 0; c < 2; ++c)
            {
                float x = saturate (w[c] * sDrive, p.satType, c) * sSatOut * satGain;
                x = af[c].process (x);
                for (auto& b : low[c]) x = b.process (x);
                w[c] = x;
            }

            // Limiter mit Lookahead (Ceiling 0 dBFS), Trockensignal wird um die Latenz verzoegert
            const float peak = std::max (std::fabs (w[0]), std::fabs (w[1]));
            const float need = peak > 1.0f ? 1.0f / peak : 1.0f;
            needRing[(size_t) lw] = need;
            float mn = 1.0f;
            for (float v : needRing) mn = std::min (mn, v);
            maAvgSum += (double) mn - (double) minRing[(size_t) lw];
            minRing[(size_t) lw] = mn;
            const float avg = (float) (maAvgSum / lookN);
            gState = std::min (avg, gState + (1.0f - gState) * relK);
            const float outL = lookL[(size_t) lw] * gState, outR = lookR[(size_t) lw] * gState;
            const float dryOutL = dryL[(size_t) lw], dryOutR = dryR[(size_t) lw];
            lookL[(size_t) lw] = w[0]; lookR[(size_t) lw] = w[1];
            dryL[(size_t) lw] = inL;   dryR[(size_t) lw] = inR;
            if (++lw >= lookN) lw = 0;

            const float yL = (dryOutL * (1.0f - sMix) + outL * sMix) * sOut;
            const float yR = (dryOutR * (1.0f - sMix) + outR * sMix) * sOut;
            L[idx] = yL; R[idx] = yR;
            blockPeak = std::max (blockPeak, std::max (std::fabs (yL), std::fabs (yR)));
        }
    }
    outPeak.store (blockPeak);
}

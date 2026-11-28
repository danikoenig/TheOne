#pragma once
// Uebernommen aus Dani Koenigs Plugin «Dub Delay» (danikoenig/DubDelay), als Effekt-Typ DUB DELAY in den FX-Slots.

#include <atomic>
#include <cmath>
#include <vector>
#include <algorithm>

// Reiner C++-DSP ohne JUCE-Abhaengigkeit (damit er separat getestet werden kann).
// Kette (wie im Ableton-Rack "DKO Dub Delay II"):
//   Delay (Filter im Feedback) -> Saturator -> Auto Filter (Bandpass + LFO) -> Low Cut 100 Hz (48 dB/Okt) -> Limiter
namespace DubDSP
{
    constexpr int   numDivisions = 8;
    constexpr int   sixteenths[numDivisions] = { 1, 2, 3, 4, 5, 6, 8, 16 };   // wie Ableton Delay
    constexpr float maxDelaySec = 3.5f;
    constexpr float lookaheadSec = 0.003f;   // Limiter-Lookahead = Plugin-Latenz
    constexpr float lowCutHz = 100.0f;

    inline float toLin (float db) { return std::pow (10.0f, db / 20.0f); }
}

struct DubParams
{
    // Delay
    int   timeMode = 0;        // 0 = Host, 1 = BPM (manuell), 2 = ms
    float hostBpm = 120.0f, bpm = 120.0f;
    int   divL = 2, divR = 2;  // Index in sixteenths (2 = 3/16)
    float msL = 375.0f, msR = 375.0f;
    bool  dual = false;        // L und R getrennt
    bool  pingPong = true;
    int   travel = 0;          // 0 = Repitch, 1 = Fade, 2 = Jump
    float feedback = 80.0f;    // 0..200 %
    float delFilterFreq = 1420.0f, delFilterWidth = 5.65f;   // Filter im Delay (Ableton: Freq und Width in Oktaven)
    float modRate = 0.5f, modDepth = 0.0f;                   // Delay-Modulation (LFO auf die Zeit)
    // Saturator
    int   satType = 0;         // 0 Analog Clip, 1 Soft, 2 Tape, 3 Tube, 4 Fold
    float driveDb = 15.0f, satOutDb = -10.0f;
    // Auto Filter (Bandpass)
    float afFreq = 863.0f, afQ = 1.58f;
    float lfoAmount = 16.5f;   // %
    float lfoRate = 0.06f;     // Hz
    // Ausgang
    float mix = 100.0f;        // %
    float outputDb = 0.0f;
};

class DubEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    // Verarbeitet zwei Kanaele in place (stereo)
    void process (float* l, float* r, int numSamples, const DubParams& p);
    int  getLatencySamples() const { return lookN; }

    std::atomic<float> outPeak { 0.0f };   // Spitze des letzten Blocks (linear), fuer die Anzeige

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
        void clear() { z1 = z2 = 0; }
        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
        { b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0; }
        void lowpass (double fs, double f, double q);
        void highpass (double fs, double f, double q);
        void bandpass (double fs, double f, double q);   // 0 dB im Maximum
    };

    struct Line
    {
        std::vector<float> buf;
        int mask = 0, w = 0;
        void init (int size) { buf.assign ((size_t) size, 0.0f); mask = size - 1; w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        float read (double d) const;     // Hermite
        void write (float x) { buf[(size_t) (w & mask)] = x; ++w; }
    };

    struct Head   // Delay-Zeit-Verhalten: Repitch (gleitet), Fade (Ueberblendung), Jump
    {
        double cur = 0, fadeTo = 0, fadePos = 1.0;
        bool fading = false;
    };

    float readHead (Line& line, Head& h, double target, int travel, double mod);
    float saturate (float x, int type, int ch);

    double fs = 44100.0;
    Line line[2];
    Head head[2];
    Biquad dHp[2], dLp[2];                 // Filter im Delay
    Biquad af[2];                          // Auto Filter (Bandpass)
    Biquad low[2][4];                      // Low Cut 100 Hz, 8. Ordnung
    float tapeLp[2] = { 0, 0 }, dcX[2] = { 0, 0 }, dcY[2] = { 0, 0 };
    double modPhase = 0.0, lfoPhase = 0.0;
    bool prevPingPong = false;

    // geglaettete Werte
    float sFb = 0.8f, sDrive = 5.6f, sSatOut = 0.316f, sMix = 1.0f, sOut = 1.0f, sAfF = 863.0f, sDelF = 1420.0f, sDelW = 5.65f;
    bool smoothInit = false;

    // Limiter
    int lookN = 0, lw = 0;
    std::vector<float> lookL, lookR, dryL, dryR, needRing, minRing;
    double maAvgSum = 0.0;
    float gState = 1.0f;
};

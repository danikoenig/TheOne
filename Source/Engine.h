#pragma once
// Synth-Engine von «The One»: 16 Stimmen, 3 VCOs, Ring Mod, Noise, SVF + Ladder, 3 EGs, 4 LFOs.
// Bewusst frei von JUCE, damit sie einzeln getestet werden kann.
#include "Params.h"
#include <array>
#include <cstdint>
#include <vector>

namespace TO
{
constexpr int kMaxVoices = 16;

struct Rng
{
    uint32_t s = 2463534242u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }   // -1..1
};

struct Osc
{
    double ph = 0.0;
};

struct Env
{
    enum St { Idle, Delay, Attack, Hold, Decay, Sustain, Release };
    struct Cfg { float delay = 0, att = 0.001f, hold = 0, dec = 0.1f, sus = 1, rel = 0.1f, cA = 0, cD = 0, cR = 0; bool loop = false, latch = false; };
    St st = Idle;
    float level = 0, t = 0, from = 0;
    bool gate = false;

    void enter (St s) { st = s; t = 0; if (s == Attack || s == Release) from = level; }
    void gateOn (bool retrigger) { gate = true; if (retrigger || st == Idle) enter (Delay); }
    void gateOff (const Cfg& c) { gate = false; if (! c.latch && st != Idle && st != Release) enter (Release); }
    float tick (const Cfg& c, float dt);
};

struct Lfo
{
    float ph = 0, sh = 0;
};

struct Svf { float s1 = 0, s2 = 0; };
struct Ladder { float s[4] = { 0, 0, 0, 0 }; };

struct Voice
{
    bool active = false, gate = false, sustained = false, mono = false;
    int key = -1;
    uint64_t order = 0;
    float note = 60, target = 60, vel = 1, detuneCents = 0, pan = 0;
    uint32_t age = 0;
    Osc osc[3];
    double subPh = 0.0;
    float out[3] = { 0, 0, 0 };   // letzter Ausgang je VCO (fuer FM)
    Env env[3];                   // 0 Filter, 1 Amp, 2 Mod
    Env nenv;                     // Noise
    Lfo lfo[4];
    Svf svfA, svfB;
    Ladder lad;
    Rng rng;
    float red = 0, wprev = 0;
    float gA = 0.1f, gB = 0.1f, kS = 1, gL = 0.1f, kL = 1;
    int ci = 0;
};

class Synth
{
public:
    void prepare (double sampleRate);
    void setParams (const float* p) { P = p; }
    void setChord (const std::vector<int>& intervals) { chord = intervals; if (chord.empty()) chord.push_back (0); }

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();
    void panic();   // sofort alles stumm, Pedal/Stack zuruecksetzen
    void setPedal (bool down);
    void setBend (float v) { bend = v; }       // -1..1
    void setModWheel (float v) { modWheel = v; }
    void process (float* L, float* R, int n, double bpm);   // addiert nicht, ueberschreibt

    int activeVoices() const { int c = 0; for (auto& v : voices) if (v.active) ++c; return c; }

private:
    struct LfoC { int wave = 0; float var = 0; bool bi = true, reset = false; float inc = 0, delay = 0, fade = 0, startPh = 0; };
    void startNote (int key, int note, float vel, int unisonIdx, int unisonCount, int chordIdx, bool retrigger, bool monoVoice);
    int allocVoice (int polyCount);
    void releaseVoice (Voice& v);
    void releaseKey (int key);
    void releaseSustained();
    void retargetMono (int key, bool retrigger);
    void monoStart (int key, float vel);
    void renderChunk (float* L, float* R, int n, double bpm);
    void renderVoice (Voice& v, float* L, float* R, int n, const float (*gl)[64]);

    double sr = 44100.0;
    const float* P = nullptr;
    std::array<Voice, kMaxVoices> voices;
    std::vector<int> chord { 0 };
    std::vector<int> monoStack;
    Lfo glfo[4];
    Rng grng;
    uint64_t orderCounter = 0;
    float bend = 0, modWheel = 0, lastNote = -1;
    bool pedal = false;
    int keysDown = 0;
    float monoVel = 1;
    LfoC lc[4];
    Env::Cfg ecfg[4];
    bool lastHold = false;
};
} // namespace TO

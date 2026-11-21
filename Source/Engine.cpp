#include "Engine.h"
#include <algorithm>
#include <cmath>

namespace TO
{
static constexpr float kPi = 3.14159265358979f;

//==============================================================================
// Huellkurve (DAHDSR mit Kurven, Loop, Latch)
static inline float shapeCurve (float x, float c)
{
    // c > 0: schnell am Anfang, langsam am Ende (analog-typisch); c < 0: umgekehrt; 0 = linear
    if (c >= 0.0f) return 1.0f - std::pow (1.0f - x, 1.0f + 3.0f * c);
    return std::pow (x, 1.0f - 3.0f * c);
}

float Env::tick (const Cfg& c, float dt)
{
    for (int guard = 0; guard < 8; ++guard)
    {
        switch (st)
        {
            case Idle: level = 0.0f; return level;
            case Delay:
                if (c.delay <= 0.0f || t >= c.delay) { enter (Attack); continue; }
                t += dt; return level;
            case Attack:
                if (c.att <= dt) { level = 1.0f; enter (Hold); continue; }
                level = from + (1.0f - from) * shapeCurve (t / c.att, c.cA);
                t += dt;
                if (t >= c.att) { level = 1.0f; enter (Hold); }
                return level;
            case Hold:
                if (c.hold <= 0.0f || t >= c.hold) { enter (Decay); continue; }
                t += dt; return level;
            case Decay:
                if (c.dec <= dt) { level = c.sus; enter (Sustain); continue; }
                level = 1.0f - (1.0f - c.sus) * shapeCurve (t / c.dec, c.cD);
                t += dt;
                if (t >= c.dec) { level = c.sus; enter (Sustain); }
                return level;
            case Sustain:
                level = c.sus;
                if (c.loop && (gate || c.latch)) { enter (Release); continue; }
                return level;
            case Release:
                if (c.rel <= dt) { level = 0.0f; t = c.rel; }
                else { level = from * (1.0f - shapeCurve (t / c.rel, c.cR)); t += dt; }
                if (t >= c.rel)
                {
                    level = 0.0f;
                    if (c.loop && (gate || c.latch)) enter (Delay); else st = Idle;
                }
                return level;
        }
    }
    return level;
}

//==============================================================================
// Oszillator-Hilfen: Skew-Dreieck (BLAMP) + Puls (BLEP)
static inline float blep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
static inline float blamp (float d, float dt)
{
    if (d >= 0.0f && d < dt) { const float t = d / dt - 1.0f; return -(1.0f / 3.0f) * t * t * t * dt; }
    if (d < 0.0f && d > -dt) { const float t = d / dt + 1.0f; return (1.0f / 3.0f) * t * t * t * dt; }
    return 0.0f;
}
static inline float wrapDist (float p, float k)
{
    float d = p - k;
    if (d > 0.5f) d -= 1.0f;
    if (d < -0.5f) d += 1.0f;
    return d;
}
static inline float triNaive (float p, float s)
{
    return p < s ? -1.0f + 2.0f * p / s : 1.0f - 2.0f * (p - s) / (1.0f - s);
}
static inline float naiveComposite (float p, float s, float duty, float mix)
{
    return triNaive (p, s) * (1.0f - mix) + (p < duty ? 1.0f : -1.0f) * mix;
}
static inline float genWave (float p, float dt, float s, float duty, float mix, float& raw)
{
    float w = triNaive (p, s);
    const float dm = 2.0f / s + 2.0f / (1.0f - s);
    w += dm * (blamp (wrapDist (p, 0.0f), dt) - blamp (wrapDist (p, s), dt));
    float q = p < duty ? 1.0f : -1.0f;
    q += blep (p, dt);
    float pd = p - duty; if (pd < 0.0f) pd += 1.0f;
    q -= blep (pd, dt);
    raw = w;
    return w * (1.0f - mix) + q * mix;
}

static inline float softTanh (float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

//==============================================================================
// LFO
static inline float lfoWave (int wave, float var, float ph, Lfo& s, bool wrapped, Rng& r)
{
    switch (wave)
    {
        case 0: { const float tri = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
                  const float sn = -std::cos (2.0f * kPi * ph);
                  return tri + var * (sn - tri); }
        case 1: { const float w = 0.5f - 0.45f * var; return ph < w ? 1.0f : -1.0f; }
        case 2: { const float saw = 2.0f * ph - 1.0f, ramp = 1.0f - 2.0f * ph; return saw + var * (ramp - saw); }
        default: { if (wrapped) s.sh = r.next();
                   const float nz = var > 0.001f ? r.next() : 0.0f;
                   return s.sh + var * (nz - s.sh); }
    }
}

//==============================================================================
void Synth::prepare (double sampleRate)
{
    sr = sampleRate;
    uint32_t seed = 12345u;
    for (auto& v : voices) { v = Voice(); seed = seed * 1664525u + 1013904223u; v.rng.s = seed | 1u; }
    grng.s = 99991u;
    for (auto& l : glfo) l = Lfo();
    monoStack.clear();
    keysDown = 0; lastNote = -1; pedal = false; lastHold = false;
}

int Synth::allocVoice (int polyCount)
{
    polyCount = std::clamp (polyCount, 1, kMaxVoices);
    for (int i = 0; i < polyCount; ++i) if (! voices[(size_t) i].active) return i;
    int best = -1; float bestLevel = 1e9f;
    for (int i = 0; i < polyCount; ++i)
    {
        auto& v = voices[(size_t) i];
        if (! v.gate && ! v.sustained && v.env[1].level < bestLevel) { bestLevel = v.env[1].level; best = i; }
    }
    if (best >= 0) return best;
    uint64_t oldest = ~0ull;
    for (int i = 0; i < polyCount; ++i)
        if (voices[(size_t) i].order < oldest) { oldest = voices[(size_t) i].order; best = i; }
    return best < 0 ? 0 : best;
}

void Synth::startNote (int key, int note, float vel, int u, int U, int ci, bool retrigger, bool monoVoice)
{
    const int poly = (int) std::lround (P[VOI0 + V_POLY]);
    const int idx = allocVoice (poly);
    Voice& v = voices[(size_t) idx];
    if (v.active && ! v.pending && v.env[1].level > 0.002f && ! (monoVoice && v.mono && v.gate))
    {
        // hoerbare Stimme wird geklaut: erst ausblenden, dann im naechsten Block starten
        v.pend = { key, note, u, U, ci, idx, vel, retrigger, monoVoice, false };
        v.pending = true; v.fade = kFade;
        v.key = key; v.mono = monoVoice; v.gate = true; v.sustained = false;
        v.order = ++orderCounter;
        lastNote = (float) note;
        return;
    }
    startOn (idx, key, note, vel, u, U, ci, retrigger, monoVoice, false);
}

void Synth::startOn (int idx, int key, int note, float vel, int u, int U, int ci, bool retrigger, bool monoVoice, bool fresh)
{
    Voice& v = voices[(size_t) idx];
    if (fresh) v.active = false;
    const bool wasActive = v.active;
    v.key = key; v.mono = monoVoice; v.ci = ci; v.slide = false;
    v.order = ++orderCounter; v.gate = true; v.sustained = false; v.age = 0;
    v.target = (float) note;
    const bool glide = P[VOI0 + V_GLIDEON] > 0.5f;
    v.note = (glide && lastNote >= 0.0f && wasActive == false) ? lastNote + (float) (note - key) : (glide && wasActive ? v.note : (float) note);
    const float det = P[VOI0 + V_DETUNE];
    float pos = U > 1 ? ((float) u / (float) (U - 1)) * 2.0f - 1.0f : 0.0f;
    v.detuneCents = pos * det * 80.0f + (U > 1 ? v.rng.next() * det * 3.0f : 0.0f);
    const float spread = P[VOI0 + V_SPREAD];
    v.pan = U > 1 ? spread * pos : spread * 0.5f * std::sin ((float) (idx * 2 + ci) * 2.4f);
    v.vel = vel;
    if (! wasActive)
    {
        for (int k = 0; k < 3; ++k)
            v.osc[k].ph = P[oscP (k, O_RESET)] > 0.5f ? 0.0 : (double) (0.5f * (v.rng.next() + 1.0f));
        v.svfA = Svf(); v.svfB = Svf(); v.lad = Ladder();
        for (auto& e : v.env) { e.level = 0; e.st = Env::Idle; }
        v.nenv.level = 0; v.nenv.st = Env::Idle;
    }
    else
        for (int k = 0; k < 3; ++k) if (P[oscP (k, O_RESET)] > 0.5f) v.osc[k].ph = 0.0;
    v.subPh = 0.0;
    for (int l = 0; l < 4; ++l) { v.lfo[l].ph = P[lfoP (l, L_PHASE)]; v.lfo[l].sh = v.rng.next(); }
    for (auto& e : v.env) e.gateOn (retrigger);
    v.nenv.gateOn (retrigger);
    v.active = true;
}

void Synth::releaseVoice (Voice& v)
{
    if (v.pending) { v.pend.relEarly = true; return; }
    v.gate = false; v.sustained = false;
    for (int i = 0; i < 3; ++i) v.env[i].gateOff (ecfg[i]);
    v.nenv.gateOff (ecfg[3]);
}

void Synth::releaseKey (int key)
{
    const bool hold = P[VOI0 + V_HOLD] > 0.5f;
    for (auto& v : voices)
        if (v.active && v.gate && ! v.mono && v.key == key)
        {
            if (pedal || hold) v.sustained = true;
            else releaseVoice (v);
        }
}

void Synth::releaseSustained()
{
    for (auto& v : voices)
        if (v.active && v.sustained) releaseVoice (v);
}

void Synth::setPedal (bool down)
{
    pedal = down;
    if (! down && P != nullptr && P[VOI0 + V_HOLD] < 0.5f) releaseSustained();
}

void Synth::retargetMono (int key, bool retrigger)
{
    const bool glide = P[VOI0 + V_GLIDEON] > 0.5f;
    for (auto& v : voices)
        if (v.active && v.mono && v.gate)
        {
            const int iv = chord[(size_t) std::clamp (v.ci, 0, (int) chord.size() - 1)];
            v.target = (float) (key + iv);
            v.key = key;
            if (! glide) v.note = v.target;
            if (retrigger) for (auto& e : v.env) { e.gateOn (true); }
            if (retrigger) v.nenv.gateOn (true);
        }
}

void Synth::monoStart (int key, float vel)
{
    monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), key), monoStack.end());
    monoStack.push_back (key);
    bool legato = false;
    for (auto& v : voices) if (v.active && v.mono && v.gate) legato = true;
    const bool multi = P[VOI0 + V_MULTI] > 0.5f;
    const bool chordOn = P[VOI0 + V_CHORD] > 0.5f;
    const int U = P[VOI0 + V_UNISON] > 0.5f ? std::clamp ((int) std::lround (P[VOI0 + V_UCOUNT]), 1, kMaxVoices) : 1;
    const int nc = chordOn ? (int) chord.size() : 1;
    int have = 0;
    for (auto& v : voices) if (v.active && v.mono && v.gate) ++have;
    if (legato && have == nc * U) { retargetMono (key, multi); lastNote = (float) key; return; }
    for (auto& v : voices) if (v.active && v.mono) releaseVoice (v);
    for (int ci = 0; ci < nc; ++ci)
        for (int u = 0; u < U; ++u)
            startNote (key, key + (chordOn ? chord[(size_t) ci] : 0), vel, u, U, ci, true, true);
    lastNote = (float) key;
}

void Synth::noteOn (int note, float velocity)
{
    if (P == nullptr) return;
    const bool hold = P[VOI0 + V_HOLD] > 0.5f;
    if (hold && ! pedal && keysDown == 0) releaseSustained();
    ++keysDown;
    if (P[VOI0 + V_MONO] > 0.5f) { monoStart (note, velocity); return; }
    const bool chordOn = P[VOI0 + V_CHORD] > 0.5f;
    const int poly = (int) std::lround (P[VOI0 + V_POLY]);
    int U = P[VOI0 + V_UNISON] > 0.5f ? std::clamp ((int) std::lround (P[VOI0 + V_UCOUNT]), 1, kMaxVoices) : 1;
    U = std::min (U, std::max (1, poly));
    const int nc = chordOn ? (int) chord.size() : 1;
    // gleiche Taste nochmal: alte Stimmen dieser Taste zuerst loesen
    for (auto& v : voices) if (v.active && v.key == note && ! v.mono) releaseVoice (v);
    for (int ci = 0; ci < nc; ++ci)
        for (int u = 0; u < U; ++u)
            startNote (note, note + (chordOn ? chord[(size_t) ci] : 0), velocity, u, U, ci, true, false);
    lastNote = (float) note;
}

void Synth::noteOff (int note)
{
    if (P == nullptr) return;
    keysDown = std::max (0, keysDown - 1);
    if (P[VOI0 + V_MONO] > 0.5f)
    {
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        const bool hold = P[VOI0 + V_HOLD] > 0.5f;
        if (! monoStack.empty()) { retargetMono (monoStack.back(), false); return; }
        for (auto& v : voices)
            if (v.active && v.mono && v.gate)
            {
                if (pedal || hold) v.sustained = true; else releaseVoice (v);
            }
        return;
    }
    releaseKey (note);
}

void Synth::slide (int fromKey, int toKey)
{
    for (auto& v : voices)
        if (v.active && v.gate && v.key == fromKey)
        {
            v.target = (float) toKey + (v.target - (float) v.key);
            v.key = toKey;
            v.slide = true;
        }
    for (auto& k : monoStack) if (k == fromKey) k = toKey;
    lastNote = (float) toKey;
}

void Synth::allNotesOff()
{
    for (auto& v : voices) if (v.active) releaseVoice (v);
    monoStack.clear();
    keysDown = 0;
}

void Synth::panic()
{
    for (auto& v : voices) { v.active = false; v.gate = false; v.sustained = false; v.pending = false; }
    monoStack.clear();
    keysDown = 0;
    pedal = false;
}

//==============================================================================
void Synth::process (float* L, float* R, int n, double bpm)
{
    for (int i = 0; i < n; ++i) { L[i] = 0.0f; R[i] = 0.0f; }
    if (P == nullptr) return;
    int pos = 0;
    while (pos < n)
    {
        const int c = std::min (64, n - pos);
        renderChunk (L + pos, R + pos, c, bpm);
        pos += c;
    }
}

void Synth::renderChunk (float* L, float* R, int n, double bpm)
{
    // Hold-Taste losgelassen: haengende Noten loesen
    const bool hold = P[VOI0 + V_HOLD] > 0.5f;
    if (lastHold && ! hold && ! pedal) releaseSustained();
    lastHold = hold;

    // Huellkurven-Konfiguration
    for (int i = 0; i < 3; ++i)
    {
        const float* e = P + ENV0 + i * E_N;
        const float sc = e[E_SCALE];
        Env::Cfg& c = ecfg[i];
        c.delay = delayTimeSec (e[E_DELAY]) * sc;
        c.att = envTimeSec (e[E_ATT]) * (e[E_ATT] <= 0.0005f ? 1.0f : sc);
        c.hold = delayTimeSec (e[E_HOLD]) * sc;
        c.dec = envTimeSec (e[E_DEC]) * (e[E_DEC] <= 0.0005f ? 1.0f : sc);
        c.sus = e[E_SUS];
        c.rel = envTimeSec (e[E_REL]) * (e[E_REL] <= 0.0005f ? 1.0f : sc);
        c.cA = e[E_ACURVE]; c.cD = e[E_DCURVE]; c.cR = e[E_RCURVE];
        c.loop = e[E_LOOP] > 0.5f; c.latch = e[E_LATCH] > 0.5f;
    }
    {
        const float* nz = P + NOI0;
        Env::Cfg& c = ecfg[3];
        c = Env::Cfg();
        c.att = envTimeSec (nz[N_ATT]);
        c.dec = envTimeSec (nz[N_REL]);
        c.rel = envTimeSec (nz[N_REL]);
        c.sus = nz[N_SUS] > 0.5f ? 1.0f : 0.0f;
        c.cA = 0.3f; c.cD = 0.5f; c.cR = 0.5f;
    }
    // LFO-Konfiguration
    static const float lo[4] = { 0.001f, 0.01f, 0.1f, 1.0f }, hi[4] = { 1.0f, 10.0f, 100.0f, 1000.0f };
    for (int l = 0; l < 4; ++l)
    {
        const float* q = P + LFO0 + l * L_N;
        LfoC& c = lc[l];
        c.wave = (int) std::lround (q[L_WAVE]);
        c.var = q[L_VAR];
        c.bi = q[L_POL] > 0.5f;
        c.reset = q[L_RESET] > 0.5f;
        c.delay = lfoDelaySec (q[L_DELAY]);
        c.fade = lfoDelaySec (q[L_FADE]);
        c.startPh = q[L_PHASE];
        float freq;
        if (q[L_SYNC] > 0.5f)
        {
            float beats = kDivBeats[divIndex (q[L_RATE])];
            const int tm = (int) std::lround (q[L_TIMING]);
            if (tm == 1) beats *= 1.5f; else if (tm == 2) beats *= 2.0f / 3.0f;
            freq = (float) (bpm / 60.0) / beats;
        }
        else
        {
            const int r = std::clamp ((int) std::lround (q[L_RANGE]), 0, 3);
            freq = lo[r] * std::pow (hi[r] / lo[r], q[L_RATE]);
        }
        c.inc = std::min (0.49f, freq / (float) sr);
    }

    // globale LFOs (fuer Stimmen ohne Note-On-Reset)
    float gl[4][64];
    for (int i = 0; i < n; ++i)
        for (int l = 0; l < 4; ++l)
        {
            Lfo& s = glfo[l];
            s.ph += lc[l].inc;
            bool wrapped = false;
            if (s.ph >= 1.0f) { s.ph -= 1.0f; wrapped = true; }
            const float o = lfoWave (lc[l].wave, lc[l].var, s.ph, s, wrapped, grng);
            gl[l][i] = lc[l].bi ? o : (o + 1.0f) * 0.5f;
        }

    // ausgeblendete Stimmen jetzt mit der neuen Note starten
    for (auto& v : voices)
        if (v.pending && v.fade <= 0)
        {
            const auto p = v.pend;
            v.pending = false;
            startOn (p.idx, p.key, p.note, p.vel, p.u, p.U, p.ci, p.retrig, p.mono, true);
            if (p.relEarly) { if (pedal || (P != nullptr && P[VOI0 + V_HOLD] > 0.5f)) v.sustained = true; else releaseVoice (v); }
        }

    for (auto& v : voices)
        if (v.active) renderVoice (v, L, R, n, gl);
}

//==============================================================================
void Synth::renderVoice (Voice& v, float* L, float* R, int n, const float (*gl)[64])
{
    const float* p = P;
    const float fsr = (float) sr;
    const float dtS = 1.0f / fsr;
    const int l4dest = (int) std::lround (p[XTR0 + X_L4DEST]);
    const float l4amt = p[XTR0 + X_L4AMT];
    const int medest = (int) std::lround (p[XTR0 + X_MEDEST]);
    const float meamt = p[XTR0 + X_MEAMT];
    const float vibDepth = p[COM0 + C_LFO1] * p[COM0 + C_LFO1] * 12.0f + modWheel * p[COM0 + C_MODDEPTH];
    const float bendSemis = bend * p[COM0 + C_BEND];
    const float mePitch = p[COM0 + C_MEPITCH] * 24.0f;
    const int ringSrc = (int) std::lround (p[COM0 + C_RINGSRC]);
    const float lfo3Ang = p[COM0 + C_LFO3ANG] * 0.5f, lfo3Pw = p[COM0 + C_LFO3PW] * 0.5f;
    const int fmRoute = (int) std::lround (p[COM0 + C_FMROUTE]);
    const bool serial = p[FIL0 + F_ROUTE] > 0.5f;
    const float fmix = p[FIL0 + F_MIX];
    const float gainSvf = std::min (1.0f, 2.0f * (1.0f - fmix)), gainLad = std::min (1.0f, 2.0f * fmix);
    float glideCoef = 1.0f;
    const float slideCoef = 1.0f - std::exp (-1.0f / (0.045f * fsr));
    if (p[VOI0 + V_GLIDEON] > 0.5f)
    {
        const float x = p[VOI0 + V_GLIDE];
        glideCoef = 1.0f - std::exp (-1.0f / ((0.003f + x * x * 2.5f) * fsr));
    }
    const float noiseColorMix = p[NOI0 + N_MIX];
    const int noisePair = (int) std::lround (p[NOI0 + N_PAIR]);
    float lvl[6], rs[6], rl[6];
    for (int j = 0; j < 6; ++j)
    {
        lvl[j] = p[MIX0 + MX_LVL + j];
        rs[j] = p[MIX0 + MX_SVF + j] > 0.5f ? 1.0f : 0.0f;
        rl[j] = p[MIX0 + MX_LAD + j] > 0.5f ? 1.0f : 0.0f;
    }
    const float subMul = p[MIX0 + MX_SUBOCT] > 0.5f ? 0.25f : 0.5f;
    const bool subSq = p[MIX0 + MX_SUBWAVE] > 0.5f;
    const int svfMode = (int) std::lround (p[FIL0 + F_SVF_MODE]);
    const bool svfSer = p[FIL0 + F_SVF_SER] > 0.5f;
    const int ladMode = (int) std::lround (p[FIL0 + F_LAD_MODE]);
    const int ladSlope = (int) std::lround (p[FIL0 + F_LAD_SLOPE]);
    const float vib0 = 0.0f;
    (void) vib0;

    float envOut[3] = { 0, 0, 0 };
    for (int i = 0; i < n; ++i)
    {
        ++v.age;
        { const float gc = v.slide ? std::min (glideCoef, slideCoef) : glideCoef; v.note = gc < 1.0f ? v.note + (v.target - v.note) * gc : v.target; }

        // Huellkurven
        for (int k = 0; k < 3; ++k)
        {
            const float x = v.env[k].tick (ecfg[k], dtS);
            const float va = p[envP (k, E_VEL)];
            envOut[k] = va >= 0.0f ? x * (1.0f - va * (1.0f - v.vel)) : x * (1.0f + va * v.vel);
        }
        const float en = v.nenv.tick (ecfg[3], dtS);
        const float egF = envOut[0], egA = envOut[1], egM = envOut[2];

        // LFOs (mit Delay/Fade je Stimme)
        float lv[4];
        const float tsec = (float) v.age * dtS;
        for (int l = 0; l < 4; ++l)
        {
            float raw;
            if (lc[l].reset)
            {
                Lfo& s = v.lfo[l];
                s.ph += lc[l].inc;
                bool wrapped = false;
                if (s.ph >= 1.0f) { s.ph -= 1.0f; wrapped = true; }
                const float o = lfoWave (lc[l].wave, lc[l].var, s.ph, s, wrapped, v.rng);
                raw = lc[l].bi ? o : (o + 1.0f) * 0.5f;
            }
            else raw = gl[l][i];
            float g = 1.0f;
            if (tsec < lc[l].delay) g = 0.0f;
            else if (lc[l].fade > 0.0f) g = std::min (1.0f, (tsec - lc[l].delay) / lc[l].fade);
            lv[l] = raw * g;
        }

        // Tonhoehe
        const float vib = lv[0] * vibDepth;
        const float extra = bendSemis + vib + mePitch * egM + (l4dest == 0 ? lv[3] * l4amt * 12.0f : 0.0f);
        float inc[3], raw[3], val[3];
        const float notePitch = v.note + v.detuneCents * 0.01f;
        for (int k = 0; k < 3; ++k)
        {
            const float trk = p[oscP (k, O_TRACK)];
            const float semis = (60.0f + (notePitch - 60.0f) * trk) + extra - 69.0f
                                + 12.0f * (p[oscP (k, O_OCT)] - 2.0f) + p[oscP (k, O_FREQ)];
            const float f = 440.0f * std::exp2 (semis * (1.0f / 12.0f)) + p[oscP (k, O_BEAT)];
            inc[k] = std::clamp (f * dtS, 0.0f, 0.45f);
        }
        float fm = p[COM0 + C_FM] + (medest == 3 ? egM * meamt : 0.0f);
        if (fm > 0.0f)
        {
            const int src = fmRoute == 2 ? 2 : 0;
            const int dst = fmRoute == 0 ? 1 : (fmRoute == 1 ? 2 : 0);
            inc[dst] = std::clamp (inc[dst] + 8.0f * fm * v.out[src] * inc[src], 0.0f, 0.49f);
        }

        // Oszillatoren
        float masterE = -1.0f;
        for (int k = 0; k < 3; ++k)
        {
            Osc& o = v.osc[k];
            const double prev = o.ph;
            o.ph += (double) inc[k];
            float syncJump = 0.0f, syncT = 2.0f;
            bool didSync = false;

            float angle = p[oscP (k, O_ANGLE)] + lv[2] * lfo3Ang + (medest == 1 ? egM * meamt * 0.5f : 0.0f)
                          + (l4dest == 6 ? lv[3] * l4amt * 0.5f : 0.0f);
            float pw = p[oscP (k, O_PW)] + lv[2] * lfo3Pw + (medest == 2 ? egM * meamt * 0.5f : 0.0f)
                       + (l4dest == 5 ? lv[3] * l4amt * 0.5f : 0.0f);
            float mix = p[oscP (k, O_MIX)] + (l4dest == 7 ? lv[3] * l4amt * 0.5f : 0.0f);
            angle = std::clamp (angle, 0.0f, 1.0f);
            pw = std::clamp (pw, 0.0f, 1.0f);
            mix = std::clamp (mix, 0.0f, 1.0f);
            const float duty = 0.02f + 0.96f * pw;
            const float dtk = std::max (inc[k], 1e-6f);
            float s;
            if (p[oscP (k, O_WAVE)] > 0.5f)   // SAW: Reset-Zeit 6 us .. 1 ms
            {
                const float resetT = 6e-6f + angle * (1e-3f - 6e-6f);
                const float r = std::max (resetT * (inc[k] * fsr), 2.0f * dtk);
                s = std::clamp (1.0f - r, 2.0f * dtk, 1.0f - 2.0f * dtk);
            }
            else
                s = std::clamp (0.5f + 0.5f * angle, 2.0f * dtk, 1.0f - 2.0f * dtk);

            if (k == 0)
            {
                if (o.ph >= 1.0) { o.ph -= 1.0; if (inc[0] > 1e-7f) masterE = (float) (o.ph / (double) inc[0]); }
            }
            else
            {
                const bool syncOn = p[oscP (k, O_SYNC)] > 0.5f && masterE >= 0.0f;
                if (syncOn)
                {
                    double phAt = prev + (double) inc[k] * (1.0 - (double) masterE);
                    if (phAt >= 1.0) phAt -= 1.0;
                    const float before = naiveComposite ((float) phAt, s, duty, mix);
                    const float after = naiveComposite (0.0f, s, duty, mix);
                    syncJump = after - before;
                    o.ph = (double) inc[k] * (double) masterE;
                    syncT = (float) o.ph;
                    didSync = true;
                }
                else if (o.ph >= 1.0) o.ph -= 1.0;
            }
            float r2;
            float w = genWave ((float) o.ph, dtk, s, duty, mix, r2);
            if (didSync && syncT < dtk)
            {
                const float t = syncT / dtk;
                w += 0.5f * syncJump * (t + t - t * t - 1.0f);
            }
            raw[k] = r2; val[k] = w; v.out[k] = w;
        }

        // Noise
        const float white = v.rng.next();
        v.red = 0.985f * v.red + 0.1f * white;
        const float redS = v.red * 1.7f, purple = (white - v.wprev) * 0.7f;
        v.wprev = white;
        float nl, nr;
        if (noisePair == 0) { nl = redS; nr = white; }
        else if (noisePair == 1) { nl = redS; nr = purple; }
        else { nl = white; nr = purple; }
        float noise = (nl + noiseColorMix * (nr - nl)) * en;
        if (medest == 5) noise *= std::max (0.0f, 1.0f + egM * meamt);

        // Mixer
        const float ring = ringSrc == 0 ? raw[0] * raw[1] : raw[1] * raw[2];
        // Sub-Oszillator: folgt VCO 1, eine oder zwei Oktaven tiefer
        float sub;
        {
            const float sinc = std::max (inc[0] * subMul, 1e-7f);
            v.subPh += (double) sinc;
            if (v.subPh >= 1.0) v.subPh -= 1.0;
            const float sp = (float) v.subPh;
            if (subSq)
            {
                sub = (sp < 0.5f ? 0.7f : -0.7f);
                sub += 0.7f * blep (sp, sinc);
                const float q = sp < 0.5f ? sp + 0.5f : sp - 0.5f;
                sub -= 0.7f * blep (q, sinc);
            }
            else sub = std::sin (6.2831853f * sp);
        }
        const float sig[6] = { val[0] * lvl[0], val[1] * lvl[1], val[2] * lvl[2], ring * lvl[3], noise * lvl[4], sub * lvl[5] };
        float busS = 0.0f, busL = 0.0f;
        for (int j = 0; j < 6; ++j) { busS += sig[j] * rs[j]; busL += sig[j] * rl[j]; }

        // Filter-Koeffizienten (alle 4 Samples)
        if ((i & 3) == 0)
        {
            const float l2 = lv[1];
            const float l4c = l4dest == 1 ? lv[3] * l4amt : 0.0f;
            const float resMod = (l4dest == 2 ? lv[3] * l4amt * 0.5f : 0.0f) + (medest == 4 ? egM * meamt * 0.5f : 0.0f);
            const float keyT = (notePitch - 60.0f) * (1.0f / 12.0f);
            const float fmS = v.out[(int) std::lround (p[FIL0 + F_SVF_FMSRC])];
            const float fmL = v.out[(int) std::lround (p[FIL0 + F_LAD_FMSRC])];
            const float octS = p[FIL0 + F_SVF_KEY] * keyT + 7.0f * p[FIL0 + F_SVF_EG] * egF + 3.0f * p[FIL0 + F_SVF_LFO] * l2
                               + 5.0f * p[FIL0 + F_SVF_FM] * fmS + 3.0f * l4c;
            const float octL = p[FIL0 + F_LAD_KEY] * keyT + 7.0f * p[FIL0 + F_LAD_EG] * egF + 3.0f * p[FIL0 + F_LAD_LFO] * l2
                               + 5.0f * p[FIL0 + F_LAD_FM] * fmL + 3.0f * l4c;
            const float maxF = std::min (20000.0f, 0.46f * fsr);
            const float baseS = 4.32193f + 9.96578f * p[FIL0 + F_SVF_CUT] + octS;
            const float baseL = 4.32193f + 9.96578f * p[FIL0 + F_LAD_CUT] + octL;
            const float fA = std::clamp (std::exp2 (baseS), 10.0f, maxF);
            const float fB = std::clamp (std::exp2 (baseS + p[FIL0 + F_SVF_SPACE] * 2.0f), 10.0f, maxF);
            const float fL = std::clamp (std::exp2 (baseL), 10.0f, maxF);
            v.gA = std::tan (kPi * fA * dtS);
            v.gB = std::tan (kPi * fB * dtS);
            v.gL = std::tan (kPi * fL * dtS);
            const float rS = std::clamp (p[FIL0 + F_SVF_RES] + resMod, 0.0f, 1.0f);
            const float rL = std::clamp (p[FIL0 + F_LAD_RES] + resMod, 0.0f, 1.0f);
            v.kS = 2.0f - 1.96f * rS;
            v.kL = 4.2f * rL;
        }

        // SVF (zwei Elemente)
        float svfOut = 0.0f;
        {
            auto tick = [&] (Svf& s, float x, float g, float k) -> float
            {
                const float hp = (x - (k + g) * s.s1 - s.s2) / (1.0f + g * (k + g));
                const float bp = g * hp + s.s1;
                const float lp = g * bp + s.s2;
                s.s1 = g * hp + bp;
                s.s2 = g * bp + lp;
                switch (svfMode)
                {
                    case 0: return lp + hp;
                    case 1: return bp * k;
                    case 2: return hp;
                    default: return lp;
                }
            };
            const float a = tick (v.svfA, busS, v.gA, v.kS);
            svfOut = svfSer ? tick (v.svfB, a, v.gB, v.kS) : 0.5f * (a + tick (v.svfB, busS, v.gB, v.kS));
        }

        // Ladder
        float ladOut;
        {
            const float x = busL + (serial ? svfOut : 0.0f);
            const float G = v.gL / (1.0f + v.gL), a = 1.0f - G;
            const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
            float* s = v.lad.s;
            const float Sg = G3 * a * s[0] + G2 * a * s[1] + G * a * s[2] + a * s[3];
            const float u = softTanh ((x - v.kL * Sg) / (1.0f + v.kL * G4));
            const float y1 = G * u + a * s[0];
            const float y2 = G * y1 + a * s[1];
            const float y3 = G * y2 + a * s[2];
            const float y4 = G * y3 + a * s[3];
            s[0] = 2.0f * y1 - s[0]; s[1] = 2.0f * y2 - s[1]; s[2] = 2.0f * y3 - s[2]; s[3] = 2.0f * y4 - s[3];
            if (ladMode == 0)
                ladOut = ladSlope == 0 ? y1 : (ladSlope == 1 ? y2 : (ladSlope == 2 ? y3 : y4));
            else
            {
                switch (ladSlope)
                {
                    case 0: ladOut = u - y1; break;
                    case 1: ladOut = u - 2.0f * y1 + y2; break;
                    case 2: ladOut = u - 3.0f * y1 + 3.0f * y2 - y3; break;
                    default: ladOut = u - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4; break;
                }
            }
        }

        float y = svfOut * gainSvf + ladOut * gainLad;
        if (! std::isfinite (y)) { v.svfA = Svf(); v.svfB = Svf(); v.lad = Ladder(); y = 0.0f; }

        // VCA + Pan
        float amp = egA * 0.3f;
        if (l4dest == 3) amp *= std::max (0.0f, 1.0f + 0.5f * l4amt * lv[3]);
        y *= amp;
        if (v.pending) { if (v.fade > 0) { y *= (float) v.fade / (float) kFade; --v.fade; } else y = 0.0f; }
        const float pan = std::clamp (v.pan + (l4dest == 4 ? lv[3] * l4amt : 0.0f), -1.0f, 1.0f);
        const float ang = (pan + 1.0f) * 0.25f * kPi;
        L[i] += y * std::cos (ang) * 1.41421356f;
        R[i] += y * std::sin (ang) * 1.41421356f;

        if (v.env[1].st == Env::Idle && ! v.gate) { v.active = false; break; }
    }
}
} // namespace TO

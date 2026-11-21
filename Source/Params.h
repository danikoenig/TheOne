#pragma once
// Parameter-Tabelle von «The One»: eine Quelle fuer Engine, Prozessor und GUI.
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>

namespace TO
{
enum Kind { kFloat, kBool, kChoice };
enum Disp { dNum, dPct, dCut, dTime, dDelay, dSemi, dHz, dCents, dInt, dBpm, dTen, dCurve, dLfoRate, dBeatHz, dDb, dStep, dOct, dBend };

struct PDef
{
    std::string id, name;
    float mn = 0, mx = 1, def = 0;
    Kind kind = kFloat;
    Disp disp = dNum;
    std::vector<std::string> choices;
};

// ---- Feld-Indizes je Modul --------------------------------------------------
enum { O_OCT, O_FREQ, O_BEAT, O_WAVE, O_ANGLE, O_PW, O_MIX, O_SYNC, O_TRACK, O_RESET, O_N };
enum { MX_LVL = 0, MX_SVF = 6, MX_LAD = 12, MX_SUBOCT = 18, MX_SUBWAVE = 19, MX_N = 20 };   // Kanaele: OSC1, OSC2, OSC3, RING, NOISE, SUB
enum { C_LFO1, C_MEPITCH, C_RINGSRC, C_LFO3ANG, C_LFO3PW, C_FM, C_FMROUTE, C_BEND, C_MODDEPTH, C_N };
enum { N_ATT, N_REL, N_SUS, N_PAIR, N_MIX, N_N };
enum { F_SVF_CUT, F_SVF_RES, F_SVF_MODE, F_SVF_SPACE, F_SVF_SER, F_SVF_KEY, F_SVF_EG, F_SVF_LFO, F_SVF_FM, F_SVF_FMSRC,
       F_LAD_CUT, F_LAD_RES, F_LAD_MODE, F_LAD_SLOPE, F_LAD_KEY, F_LAD_EG, F_LAD_LFO, F_LAD_FM, F_LAD_FMSRC,
       F_LINK, F_ROUTE, F_MIX, F_N };
enum { E_DELAY, E_ATT, E_HOLD, E_DEC, E_SUS, E_REL, E_ACURVE, E_DCURVE, E_RCURVE, E_LOOP, E_LATCH, E_SCALE, E_VEL, E_N };
enum { L_RATE, L_RANGE, L_WAVE, L_VAR, L_POL, L_SYNC, L_TIMING, L_DELAY, L_FADE, L_RESET, L_PHASE, L_N };
enum { X_L4DEST, X_L4AMT, X_MEDEST, X_MEAMT, X_N };
enum { V_POLY, V_MONO, V_UNISON, V_UCOUNT, V_DETUNE, V_GLIDEON, V_GLIDE, V_HOLD, V_CHORD, V_LEARN, V_SILENT, V_SPREAD, V_MULTI, V_N };
enum { FX_TYPE, FX_MIX, FX_P1, FX_P2, FX_P3, FX_P4, FX_P5, FX_SYNC, FX_N };
enum { OUT_VOL, OUT_TEMPO, OUT_N };
constexpr int kSeqSteps = 16;
enum { SQ_ON, SQ_LATCH, SQ_RATE, SQ_LEN, SQ_GATE, SQ_ORDER, SQ_RND, SQ_RNDP, SQ_RNDO, SQ_RNDA, SQ_VELRND, SQ_PITCH0, SQ_SLIDE0 = SQ_PITCH0 + kSeqSteps, SQ_ACC0 = SQ_SLIDE0 + kSeqSteps, SQ_MODE0 = SQ_ACC0 + kSeqSteps, SQ_OCT0 = SQ_MODE0 + kSeqSteps, SQ_N = SQ_OCT0 + kSeqSteps };

constexpr int OSC0 = 0;
constexpr int MIX0 = OSC0 + 3 * O_N;
constexpr int COM0 = MIX0 + MX_N;
constexpr int NOI0 = COM0 + C_N;
constexpr int FIL0 = NOI0 + N_N;
constexpr int ENV0 = FIL0 + F_N;
constexpr int LFO0 = ENV0 + 3 * E_N;
constexpr int XTR0 = LFO0 + 4 * L_N;
constexpr int VOI0 = XTR0 + X_N;
constexpr int FXS0 = VOI0 + V_N;
constexpr int OUT0 = FXS0 + 3 * FX_N;
constexpr int SEQ0 = OUT0 + OUT_N;
constexpr int NUM_PARAMS = SEQ0 + SQ_N;
// Sequencer-Raster (Schlaege pro Puls)
constexpr int kSeqRates = 7;
constexpr float kSeqBeats[kSeqRates] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f, 1.0f / 12.0f };

constexpr int oscP (int i, int f) { return OSC0 + i * O_N + f; }
constexpr int envP (int i, int f) { return ENV0 + i * E_N + f; }
constexpr int lfoP (int i, int f) { return LFO0 + i * L_N + f; }
constexpr int fxP  (int i, int f) { return FXS0 + i * FX_N + f; }

// ---- Wert-Abbildungen (Engine und GUI nutzen dieselben) ---------------------
inline float cutoffHz (float x)            { return 20.0f * std::pow (1000.0f, x); }
inline float envTimeSec (float x)          { return x <= 0.0005f ? 0.0002f : 0.001f * std::pow (10000.0f, x); }
inline float delayTimeSec (float x)        { return x <= 0.0005f ? 0.0f : 0.001f * std::pow (10000.0f, x); }
inline float lfoDelaySec (float x)         { return x <= 0.0005f ? 0.0f : 0.001f * std::pow (10000.0f, x); }

constexpr int kNumDivs = 13;
constexpr float kDivBeats[kNumDivs] = { 64, 32, 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f };
inline const char* divName (int i)
{
    static const char* n[kNumDivs] = { "64", "32", "16", "8", "4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };
    return n[i < 0 ? 0 : (i >= kNumDivs ? kNumDivs - 1 : i)];
}
inline int divIndex (float x) { int i = (int) std::lround (x * (kNumDivs - 1)); return i < 0 ? 0 : (i > kNumDivs - 1 ? kNumDivs - 1 : i); }

// Effekt-Sync: Notenwerte mit punktiert (D) und triolisch (T), nach Laenge sortiert (in Schlaegen)
constexpr int kFxDivs = 15;
constexpr float kFxDivBeats[kFxDivs] = { 4.0f, 3.0f, 2.0f, 1.5f, 4.0f / 3.0f, 1.0f, 0.75f, 2.0f / 3.0f, 0.5f, 0.375f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f, 1.0f / 12.0f };
inline const char* fxDivName (int i)
{
    static const char* n[kFxDivs] = { "1/1", "1/2 D", "1/2", "1/4 D", "1/2 T", "1/4", "1/8 D", "1/4 T", "1/8", "1/16 D", "1/8 T", "1/16", "1/16 T", "1/32", "1/32 T" };
    return n[i < 0 ? 0 : (i >= kFxDivs ? kFxDivs - 1 : i)];
}
inline int fxDivIndex (float x) { int i = (int) std::lround (x * (kFxDivs - 1)); return i < 0 ? 0 : (i > kFxDivs - 1 ? kFxDivs - 1 : i); }

// ---- Effekt-Typen und Beschriftung der fuenf Regler -------------------------
enum FxType { FX_OFF, FX_DELAY, FX_STEREO, FX_PINGPONG, FX_BBD, FX_TAPE, FX_ECHO, FX_CHORUS, FX_DUALCHORUS, FX_ENSEMBLE,
              FX_FLANGER, FX_PHASER6, FX_PHASER12, FX_CRUSHER, FX_RESONATOR, FX_NUM };

inline const char* fxTypeName (int t)
{
    static const char* n[FX_NUM] = { "OFF", "DELAY", "STEREO DELAY", "PING PONG", "BBD DELAY", "TAPE DELAY", "ECHO", "CHORUS",
                                     "DUAL CHORUS", "ENSEMBLE", "FLANGER", "PHASER 6", "PHASER 12", "BIT CRUSHER", "RESONATOR" };
    return n[t < 0 ? 0 : (t >= FX_NUM ? FX_NUM - 1 : t)];
}
inline const char* fxLabel (int type, int p)   // p = 0..4
{
    static const char* L[FX_NUM][5] = {
        { "", "", "", "", "" },
        { "TIME", "FEEDBACK", "TONE", "SPREAD", "" },
        { "TIME L", "FEEDBACK", "TONE", "R RATIO", "" },
        { "TIME", "FEEDBACK", "TONE", "WIDTH", "" },
        { "TIME", "FEEDBACK", "TONE", "WOBBLE", "GRIT" },
        { "TIME", "FEEDBACK", "TONE", "WOW", "SATURATE" },
        { "TIME", "FEEDBACK", "TONE", "DIFFUSE", "" },
        { "RATE", "DEPTH", "TIME", "SPREAD", "FEEDBACK" },
        { "RATE", "DEPTH", "TIME", "SPREAD", "VIBRATO" },
        { "RATE", "DEPTH", "TIME", "SPREAD", "VIBRATO" },
        { "RATE", "DEPTH", "FEEDBACK", "MANUAL", "SPREAD" },
        { "RATE", "DEPTH", "FEEDBACK", "SWEEP", "SPREAD" },
        { "RATE", "DEPTH", "FEEDBACK", "SWEEP", "SPREAD" },
        { "BITS", "RATE", "TONE", "JITTER", "DRIVE" },
        { "LOW", "MID", "HIGH", "RESO", "MOD" } };
    return L[type < 0 ? 0 : (type >= FX_NUM ? FX_NUM - 1 : type)][p];
}

// ---- Tabelle bauen ----------------------------------------------------------
inline std::vector<PDef> makeDefs()
{
    std::vector<PDef> d;
    d.reserve (NUM_PARAMS);
    auto F = [&] (const std::string& id, const std::string& nm, float mn, float mx, float df, Disp dp = dNum)
    { PDef p; p.id = id; p.name = nm; p.mn = mn; p.mx = mx; p.def = df; p.kind = kFloat; p.disp = dp; d.push_back (p); };
    auto B = [&] (const std::string& id, const std::string& nm, bool df)
    { PDef p; p.id = id; p.name = nm; p.mn = 0; p.mx = 1; p.def = df ? 1.0f : 0.0f; p.kind = kBool; d.push_back (p); };
    auto C = [&] (const std::string& id, const std::string& nm, std::vector<std::string> ch, int df)
    { PDef p; p.id = id; p.name = nm; p.mn = 0; p.mx = (float) ch.size() - 1; p.def = (float) df; p.kind = kChoice; p.choices = ch; d.push_back (p); };

    // Oszillatoren
    for (int i = 0; i < 3; ++i)
    {
        const std::string s = "osc" + std::to_string (i + 1) + "_", n = "OSC " + std::to_string (i + 1) + " ";
        C (s + "oct", n + "Octave", { "32'", "16'", "8'", "4'", "2'" }, 2);
        F (s + "freq", n + "Frequency", -7, 7, 0, dSemi);
        F (s + "beat", n + "Beat", -7, 7, 0, dBeatHz);
        C (s + "wave", n + "Wave", { "TRI", "SAW" }, 1);
        F (s + "angle", n + "Wave Angle", 0, 1, 0, dTen);
        F (s + "pw", n + "Pulse Width", 0, 1, 0.5f, dTen);
        F (s + "mix", n + "Pulse Mix", 0, 1, 0, dTen);
        B (s + "sync", n + "Hard Sync", false);
        F (s + "track", n + "Pitch Tracking", 0, 2, 1, dPct);
        B (s + "reset", n + "Phase Reset", false);
    }
    // Mixer
    const char* ch[6] = { "OSC 1", "OSC 2", "OSC 3", "RING MOD", "NOISE", "SUB" };
    for (int i = 0; i < 6; ++i) F ("mix_lvl" + std::to_string (i), std::string ("Mixer ") + ch[i], 0, 1, i == 0 ? 0.8f : 0.0f, dTen);
    for (int i = 0; i < 6; ++i) B ("mix_svf" + std::to_string (i), std::string ("Mixer ") + ch[i] + " > SVF", false);
    for (int i = 0; i < 6; ++i) B ("mix_lad" + std::to_string (i), std::string ("Mixer ") + ch[i] + " > Ladder", true);
    C ("sub_oct", "Sub Octave", { "-1", "-2" }, 1);
    C ("sub_wave", "Sub Wave", { "SINE", "SQUARE" }, 0);
    // Gemeinsame Oszillator-Modulation
    F ("c_lfo1", "LFO 1 > Pitch", 0, 1, 0, dTen);
    F ("c_mepitch", "Mod EG > Pitch", -1, 1, 0, dTen);
    C ("c_ringsrc", "Ring Mod Sources", { "1-2", "2-3" }, 0);
    F ("c_lfo3ang", "LFO 3 > Wave Angle", 0, 1, 0, dTen);
    F ("c_lfo3pw", "LFO 3 > Pulse Width", 0, 1, 0, dTen);
    F ("c_fm", "FM Amount", 0, 1, 0, dTen);
    C ("c_fmroute", "FM Route", { "1-2", "1-3", "3-1" }, 0);
    F ("c_bend", "Pitch Bend Range", 0, 24, 2, dBend);
    F ("c_moddepth", "Mod Wheel Vibrato", 0, 12, 1, dSemi);
    // Noise
    F ("n_att", "Noise Attack", 0, 1, 0, dTime);
    F ("n_rel", "Noise Release", 0, 1, 0.5f, dTime);
    B ("n_sus", "Noise Sustain", true);
    C ("n_pair", "Noise Color", { "RED+WHITE", "RED+PURPLE", "WHITE+PURPLE" }, 0);
    F ("n_mix", "Noise Color Mix", 0, 1, 0.5f, dTen);
    // Filter
    F ("svf_cut", "SVF Cutoff", 0, 1, 1.0f, dCut);
    F ("svf_res", "SVF Resonance", 0, 1, 0, dTen);
    C ("svf_mode", "SVF Mode", { "NOTCH", "BP", "HP", "LP" }, 3);
    F ("svf_space", "SVF Spacing", -1, 1, 0, dPct);
    B ("svf_ser", "SVF Series", false);
    F ("svf_key", "SVF Key Track", -2, 2, 0.5f, dPct);
    F ("svf_eg", "SVF EG Amount", -1, 1, 0, dTen);
    F ("svf_lfo", "SVF LFO 2 Amount", 0, 1, 0, dTen);
    F ("svf_fm", "SVF FM Amount", 0, 1, 0, dTen);
    C ("svf_fmsrc", "SVF FM Source", { "OSC 1", "OSC 2", "OSC 3" }, 0);
    F ("lad_cut", "Ladder Cutoff", 0, 1, 0.72f, dCut);
    F ("lad_res", "Ladder Resonance", 0, 1, 0.1f, dTen);
    C ("lad_mode", "Ladder Mode", { "LP", "HP" }, 0);
    C ("lad_slope", "Ladder Slope", { "6 dB", "12 dB", "18 dB", "24 dB" }, 3);
    F ("lad_key", "Ladder Key Track", -2, 2, 0.5f, dPct);
    F ("lad_eg", "Ladder EG Amount", -1, 1, 0.35f, dTen);
    F ("lad_lfo", "Ladder LFO 2 Amount", 0, 1, 0, dTen);
    F ("lad_fm", "Ladder FM Amount", 0, 1, 0, dTen);
    C ("lad_fmsrc", "Ladder FM Source", { "OSC 1", "OSC 2", "OSC 3" }, 0);
    B ("f_link", "Link Cutoff", false);
    C ("f_route", "Filter Route", { "PAR", "SER" }, 0);
    F ("f_mix", "Filter Mix", 0, 1, 0.5f, dTen);
    // Huellkurven: 0 = Filter, 1 = Amp, 2 = Mod
    const char* en[3] = { "Filter EG", "Amp EG", "Mod EG" };
    const char* ei[3] = { "feg_", "aeg_", "meg_" };
    for (int i = 0; i < 3; ++i)
    {
        const std::string s = ei[i], n = std::string (en[i]) + " ";
        F (s + "delay", n + "Delay", 0, 1, 0, dDelay);
        F (s + "att", n + "Attack", 0, 1, i == 1 ? 0.12f : 0.0f, dTime);
        F (s + "hold", n + "Hold", 0, 1, 0, dDelay);
        F (s + "dec", n + "Decay", 0, 1, i == 1 ? 0.55f : 0.62f, dTime);
        F (s + "sus", n + "Sustain", 0, 1, i == 1 ? 0.8f : 0.3f, dTen);
        F (s + "rel", n + "Release", 0, 1, i == 1 ? 0.45f : 0.4f, dTime);
        F (s + "acurve", n + "Attack Curve", -1, 1, 0.3f, dCurve);
        F (s + "dcurve", n + "Decay Curve", -1, 1, 0.6f, dCurve);
        F (s + "rcurve", n + "Release Curve", -1, 1, 0.6f, dCurve);
        B (s + "loop", n + "Loop", false);
        B (s + "latch", n + "Latch", false);
        F (s + "scale", n + "Time Scale", 0.1f, 10, 1, dNum);
        F (s + "vel", n + "Velocity", -1, 1, i == 1 ? 0.4f : (i == 0 ? 0.3f : 0.0f), dPct);
    }
    // LFOs
    for (int i = 0; i < 4; ++i)
    {
        const std::string s = "lfo" + std::to_string (i + 1) + "_", n = "LFO " + std::to_string (i + 1) + " ";
        F (s + "rate", n + "Rate", 0, 1, 0.5f, dLfoRate);
        C (s + "range", n + "Range", { "SLOW", "LOW", "MEDIUM", "HIGH" }, 1);
        C (s + "wave", n + "Wave", { "TRI/SIN", "PULSE", "SAW/RAMP", "S&H/NOISE" }, 0);
        F (s + "var", n + "Variation", 0, 1, 0, dPct);
        C (s + "pol", n + "Polarity", { "UNI", "BI" }, 1);
        B (s + "sync", n + "Sync", false);
        C (s + "timing", n + "Sync Timing", { "STRAIGHT", "DOTTED", "TRIPLET" }, 0);
        F (s + "delay", n + "Delay", 0, 1, 0, dDelay);
        F (s + "fade", n + "Fade In", 0, 1, 0, dDelay);
        B (s + "reset", n + "Phase Reset", false);
        F (s + "phase", n + "Start Phase", 0, 1, 0, dPct);
    }
    C ("x_l4dest", "LFO 4 Destination", { "PITCH", "CUTOFF", "RESO", "AMP", "PAN", "PULSE W", "WAVE ANGLE", "OSC MIX" }, 0);
    F ("x_l4amt", "LFO 4 Amount", 0, 1, 0, dTen);
    C ("x_medest", "Mod EG Destination", { "NONE", "WAVE ANGLE", "PULSE W", "FM AMT", "RESO", "NOISE LVL" }, 0);
    F ("x_meamt", "Mod EG Amount", -1, 1, 0, dTen);
    // Stimmen
    F ("v_poly", "Polyphony", 1, 16, 16, dInt);
    B ("v_mono", "Mono", false);
    B ("v_unison", "Unison", false);
    F ("v_ucount", "Unison Count", 2, 16, 4, dInt);
    F ("v_detune", "Unison Detune", 0, 1, 0.3f, dTen);
    B ("v_glideon", "Glide", false);
    F ("v_glide", "Glide Time", 0, 1, 0.3f, dTen);
    B ("v_hold", "Hold", false);
    B ("v_chord", "Chord", false);
    B ("v_learn", "Chord Learn", false);
    B ("v_silent", "Silent Learn", false);
    F ("v_spread", "Stereo Spread", 0, 1, 0.5f, dTen);
    B ("v_multi", "Multi Trigger", true);
    // Effekte
    std::vector<std::string> fxNames;
    for (int t = 0; t < FX_NUM; ++t) fxNames.push_back (fxTypeName (t));
    for (int i = 0; i < 3; ++i)
    {
        const std::string s = "fx" + std::to_string (i + 1) + "_", n = "FX " + std::to_string (i + 1) + " ";
        C (s + "type", n + "Type", fxNames, 0);
        F (s + "mix", n + "Mix", 0, 1, 0.3f, dPct);
        for (int k = 0; k < 5; ++k) F (s + "p" + std::to_string (k + 1), n + "Param " + std::to_string (k + 1), 0, 1, 0.5f, dPct);
        B (s + "sync", n + "Sync", false);
    }
    F ("out_vol", "Master Volume", 0, 1, 0.8f, dTen);
    F ("out_tempo", "Tempo (ohne Host)", 40, 240, 120, dBpm);
    // Step-Sequencer (8 Stufen, Metropolis-Prinzip: Pitch, Pulse Count, Gate Type)
    B ("seq_on", "Seq On", false);
    B ("seq_latch", "Seq Latch", false);
    C ("seq_rate", "Seq Rate", { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T" }, 3);
    F ("seq_len", "Seq Length", 1, 16, 16, dInt);
    F ("seq_gate", "Seq Gate Length", 0.05f, 1, 0.5f, dPct);
    C ("seq_order", "Seq Order", { "FWD", "REV", "PING", "RND" }, 0);
    F ("seq_rnd", "Seq Random Amount", 0, 1, 0.3f, dPct);
    B ("seq_rndp", "Seq Random Pitch", false);
    B ("seq_rndo", "Seq Random Octave", false);
    B ("seq_rnda", "Seq Random Accent", false);
    F ("seq_velrnd", "Seq Random Velocity", 0, 1, 0, dPct);
    const int defP[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < kSeqSteps; ++i) F ("seq_p" + std::to_string (i + 1), "Seq Pitch " + std::to_string (i + 1), -24, 24, (float) defP[i], dStep);
    for (int i = 0; i < kSeqSteps; ++i) B ("seq_sl" + std::to_string (i + 1), "Seq Slide " + std::to_string (i + 1), false);
    for (int i = 0; i < kSeqSteps; ++i) B ("seq_ac" + std::to_string (i + 1), "Seq Accent " + std::to_string (i + 1), false);
    for (int i = 0; i < kSeqSteps; ++i) C ("seq_m" + std::to_string (i + 1), "Seq Step " + std::to_string (i + 1), { "ON", "MUTE", "SKIP" }, 0);
    for (int i = 0; i < kSeqSteps; ++i) F ("seq_o" + std::to_string (i + 1), "Seq Octave " + std::to_string (i + 1), -1, 1, 0, dOct);
    return d;
}
} // namespace TO

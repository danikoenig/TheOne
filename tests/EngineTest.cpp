// Lokaler Test der Engine (ohne JUCE): rendert Noten und schreibt WAV-Dateien.
#include "../Source/Engine.h"
#include "../Source/Effects.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <cmath>
#include <xmmintrin.h>
using namespace TO;

static std::vector<PDef> defs;
static std::vector<float> P;
static int idxOf (const std::string& id) { for (size_t i = 0; i < defs.size(); ++i) if (defs[i].id == id) return (int) i; fprintf (stderr, "unknown %s\n", id.c_str()); return 0; }
static void set (const std::string& id, float v) { P[(size_t) idxOf (id)] = v; }

static void wav (const char* fn, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    std::ofstream f (fn, std::ios::binary);
    uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, riff = 36 + dataBytes, fmt = 16, rate = (uint32_t) sr, br = rate * 4;
    uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    f.write ("RIFF", 4); f.write ((char*) &riff, 4); f.write ("WAVEfmt ", 8); f.write ((char*) &fmt, 4);
    f.write ((char*) &pcm, 2); f.write ((char*) &ch, 2); f.write ((char*) &rate, 4); f.write ((char*) &br, 4);
    f.write ((char*) &ba, 2); f.write ((char*) &bits, 2); f.write ("data", 4); f.write ((char*) &dataBytes, 4);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::max (-32767.0f, std::min (32767.0f, L[i] * 32767.0f));
        int16_t b = (int16_t) std::max (-32767.0f, std::min (32767.0f, R[i] * 32767.0f));
        f.write ((char*) &a, 2); f.write ((char*) &b, 2);
    }
}

static void reset()
{
    P.assign ((size_t) NUM_PARAMS, 0.0f);
    for (size_t i = 0; i < defs.size(); ++i) P[i] = defs[i].def;
}

struct Stat { float peak = 0, rms = 0; bool bad = false; };
static Stat stat (const std::vector<float>& a, size_t from = 0, size_t to = 0)
{
    Stat s; if (to == 0) to = a.size(); double e = 0;
    for (size_t i = from; i < to; ++i) { if (! std::isfinite (a[i])) s.bad = true; s.peak = std::max (s.peak, std::fabs (a[i])); e += a[i] * a[i]; }
    s.rms = (float) std::sqrt (e / std::max<size_t> (1, to - from));
    return s;
}

// Rendert: Noten (note, startSek, endSek), Gesamtdauer
struct Ev { int note; float on, off; float vel; };
static void render (Synth& s, const std::vector<Ev>& evs, float dur, std::vector<float>& L, std::vector<float>& R, int sr = 48000, double bpm = 120)
{
    const int N = (int) (dur * sr);
    L.assign ((size_t) N, 0); R.assign ((size_t) N, 0);
    s.prepare (sr); s.setParams (P.data());
    int pos = 0; const int B = 128;
    std::vector<bool> on (evs.size(), false), off (evs.size(), false);
    while (pos < N)
    {
        for (size_t e = 0; e < evs.size(); ++e)
        {
            if (! on[e] && pos >= (int) (evs[e].on * sr)) { s.noteOn (evs[e].note, evs[e].vel); on[e] = true; }
            if (on[e] && ! off[e] && pos >= (int) (evs[e].off * sr)) { s.noteOff (evs[e].note); off[e] = true; }
        }
        const int c = std::min (B, N - pos);
        s.process (L.data() + pos, R.data() + pos, c, bpm);
        pos += c;
    }
}

int main (int argc, char** argv)
{
    _mm_setcsr (_mm_getcsr() | 0x8040);
    defs = makeDefs();
    if ((int) defs.size() != NUM_PARAMS) { printf ("FAIL: defs %zu != %d\n", defs.size(), NUM_PARAMS); return 1; }
    printf ("params: %d\n", NUM_PARAMS);
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { printf ("%s %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++fails; };
    std::vector<float> L, R;
    Synth syn;

    // 1) Init-Sound: eine Note
    reset();
    render (syn, { { 60, 0.1f, 1.0f, 1.0f } }, 2.0f, L, R);
    auto s1 = stat (L);
    check (! s1.bad && s1.peak > 0.05f && s1.peak < 1.0f, "init: Note klingt, endlich, kein Clipping");
    auto tail = stat (L, (size_t) (1.9f * 48000));
    check (tail.peak < 1e-3f, "init: Release klingt ab");
    wav ("/tmp/t_init.wav", L, R, 48000);

    // 2) 16 Stimmen Pad: Akkord, Unison, SVF+Ladder
    reset();
    set ("v_unison", 1); set ("v_ucount", 4); set ("v_detune", 0.5f);
    set ("mix_lvl1", 0.6f); set ("mix_lvl2", 0.6f);
    set ("osc2_beat", 0.6f); set ("osc3_beat", -0.7f);
    set ("aeg_att", 0.55f); set ("aeg_rel", 0.7f); set ("lad_cut", 0.55f);
    render (syn, { { 48, 0.1f, 3.0f, 0.9f }, { 55, 0.1f, 3.0f, 0.9f }, { 60, 0.1f, 3.0f, 0.9f }, { 63, 0.1f, 3.0f, 0.9f } }, 5.0f, L, R);
    auto s2 = stat (L); check (! s2.bad && s2.peak < 4.0f, "pad: endlich"); printf ("     pad peak %.3f rms %.3f active@end %d\n", s2.peak, s2.rms, syn.activeVoices());
    wav ("/tmp/t_pad.wav", L, R, 48000);

    // 3) Ladder Selbstoszillation
    reset();
    set ("mix_lvl0", 0.0f); set ("lad_res", 1.0f); set ("lad_cut", 0.5f); set ("lad_key", 1.0f);
    set ("lad_eg", 0); set ("aeg_att", 0); set ("aeg_sus", 1);
    // Anstoss ueber Noise
    set ("mix_lvl4", 0.3f); set ("n_att", 0); set ("n_rel", 0.05f); set ("n_sus", 0);
    set ("mix_lad4", 1);
    render (syn, { { 60, 0.1f, 1.5f, 1.0f } }, 2.0f, L, R);
    auto s3 = stat (L, (size_t) (1.0f * 48000), (size_t) (1.4f * 48000));
    check (! s3.bad && s3.rms > 0.01f, "ladder: schwingt selbst bei res=1");
    wav ("/tmp/t_selfosc.wav", L, R, 48000);

    // 4) Alle Modi / Quellen durchprobieren (Stabilitaet)
    int combos = 0;
    for (int wave = 0; wave < 2; ++wave)
    for (int svf = 0; svf < 4; ++svf)
    for (int slope = 0; slope < 4; ++slope)
    for (int lm = 0; lm < 2; ++lm)
    {
        reset();
        set ("osc1_wave", (float) wave); set ("osc2_wave", (float) (1 - wave)); set ("osc3_wave", (float) wave);
        set ("mix_lvl1", 0.7f); set ("mix_lvl2", 0.7f); set ("mix_lvl3", 0.5f); set ("mix_lvl4", 0.3f);
        for (int j = 0; j < 5; ++j) { set ("mix_svf" + std::to_string (j), 1); set ("mix_lad" + std::to_string (j), 1); }
        set ("osc2_sync", 1); set ("osc3_sync", 1); set ("osc2_oct", 3); set ("osc3_freq", 5);
        set ("c_fm", 0.6f); set ("svf_fm", 0.4f); set ("lad_fm", 0.4f);
        set ("svf_mode", (float) svf); set ("lad_slope", (float) slope); set ("lad_mode", (float) lm);
        set ("svf_res", 0.9f); set ("lad_res", 0.9f); set ("svf_space", 0.5f); set ("svf_ser", (float) (svf & 1)); set ("f_route", (float) (slope & 1));
        set ("lfo2_rate", 0.8f); set ("svf_lfo", 0.5f); set ("lad_lfo", 0.5f);
        set ("x_l4dest", (float) ((svf + slope) % 8)); set ("x_l4amt", 0.7f); set ("x_medest", (float) ((slope + lm) % 6)); set ("x_meamt", 0.6f);
        render (syn, { { 36, 0.0f, 0.4f, 1.0f }, { 72, 0.1f, 0.5f, 0.5f }, { 96, 0.2f, 0.6f, 1.0f } }, 1.0f, L, R);
        auto st = stat (L); auto sr_ = stat (R);
        ++combos;
        if (st.bad || sr_.bad || st.peak > 20.0f) { printf ("   bad combo wave%d svf%d slope%d lm%d peak %.2f\n", wave, svf, slope, lm, st.peak); ++fails; }
    }
    printf ("     %d Kombinationen geprueft\n", combos);

    // 5) Alias-Test: Saw bei hoher Note gegen naive Referenz
    reset();
    set ("lad_cut", 1.0f); set ("lad_res", 0); set ("lad_eg", 0); set ("lad_key", 0); set ("aeg_att", 0); set ("aeg_sus", 1);
    set ("mix_lvl0", 0.8f); set ("osc1_angle", 0);
    render (syn, { { 100, 0.05f, 1.0f, 1.0f } }, 1.2f, L, R);   // ~2637 Hz
    wav ("/tmp/t_saw_hi.wav", L, R, 48000);

    // 6) Chord Memory
    reset();
    syn.prepare (48000); syn.setParams (P.data());
    set ("v_chord", 1);
    syn.setChord ({ 0, 4, 7, 12 });
    render (syn, { { 60, 0.1f, 1.0f, 1.0f } }, 1.5f, L, R);
    auto s6 = stat (L); check (! s6.bad && s6.peak > 0.05f, "chord: spielt");
    wav ("/tmp/t_chord.wav", L, R, 48000);

    // 7) Mono + Glide + Legato
    reset(); set ("v_mono", 1); set ("v_glideon", 1); set ("v_glide", 0.5f);
    render (syn, { { 48, 0.1f, 0.9f, 1.0f }, { 60, 0.5f, 1.2f, 1.0f } }, 1.6f, L, R);
    auto s7 = stat (L); check (! s7.bad && s7.peak > 0.05f, "mono/glide: spielt");
    wav ("/tmp/t_mono.wav", L, R, 48000);

    // 8) Stimmenlimit: 40 Noten gleichzeitig
    reset(); set ("v_poly", 16);
    std::vector<Ev> many; for (int i = 0; i < 40; ++i) many.push_back ({ 40 + i, 0.05f + 0.01f * (float) i, 1.0f, 0.8f });
    render (syn, many, 1.5f, L, R);
    auto s8 = stat (L); check (! s8.bad && syn.activeVoices() <= 16, "40 Noten: max 16 Stimmen, kein Absturz");

    // 9) Envelope Loop / LFO sync
    reset(); set ("feg_loop", 1); set ("feg_dec", 0.3f); set ("feg_rel", 0.3f); set ("lfo1_sync", 1); set ("lfo1_rate", 0.8f); set ("c_lfo1", 0.5f);
    render (syn, { { 60, 0.1f, 1.5f, 1.0f } }, 2.0f, L, R);
    check (! stat (L).bad, "loop-EG + sync-LFO: stabil");


    // 10) Effekte: jeder Typ mit Extremwerten
    for (int ty = 1; ty < FX_NUM; ++ty)
        for (int extreme = 0; extreme < 3; ++extreme)
        {
            reset();
            FxChain chain; chain.prepare (48000);
            std::vector<float> a (48000 * 2), b (48000 * 2);
            for (size_t i = 0; i < a.size(); ++i) { float ph = std::fmod ((float) i * 220.0f / 48000.0f, 1.0f); a[i] = (i < 12000) ? (ph * 2 - 1) * 0.5f : 0.0f; b[i] = a[i]; }
            P[(size_t) fxP (0, FX_TYPE)] = (float) ty;
            float v = extreme == 0 ? 0.0f : (extreme == 1 ? 0.5f : 1.0f);
            P[(size_t) fxP (0, FX_MIX)] = extreme == 2 ? 1.0f : 0.5f;
            for (int k = 0; k < 5; ++k) P[(size_t) fxP (0, FX_P1 + k)] = v;
            P[(size_t) fxP (0, FX_SYNC)] = (float) (extreme == 1);
            for (int pos = 0; pos < (int) a.size(); pos += 100) chain.process (a.data() + pos, b.data() + pos, 100, P.data(), 120.0);
            auto sa = stat (a), sb = stat (b);
            bool ok = ! sa.bad && ! sb.bad && sa.peak < 8.0f;
            if (! ok) { printf ("FAIL fx %s extreme %d peak %.2f\n", fxTypeName (ty), extreme, sa.peak); ++fails; }
            if (extreme == 1) printf ("     fx %-13s peak %.2f tail %.4f\n", fxTypeName (ty), sa.peak, stat (a, 80000).peak);
        }

    printf (fails ? "\n%d FEHLER\n" : "\nalle Tests ok\n", fails);
    return fails ? 1 : 0;
}

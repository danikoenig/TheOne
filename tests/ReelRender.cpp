// Reel-Renderer fuer The One: spielt ein festes Drehbuch im Plugin ab,
// schreibt Audio (WAV, 48 kHz) und GUI-Frames (JPG, 30 fps, 2200x850).
// Build: cmake -DTHEONE_TEST_SRC=tests/ReelRender.cpp ...   Aufruf: TheOneTest <out-dir>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <functional>
#include <vector>
#include <algorithm>

using namespace juce;

static constexpr double FPS = 30.0, SR = 48000.0, BPM = 120.0;
static constexpr double BEAT = 60.0 / BPM;             // 0.5 s
static constexpr double TOTAL_BEATS = 360.0;           // 90 Takte = 180 s
static constexpr double GUI_UNTIL_BEATS = 344.0;       // danach Endkarte (Takt 86)

struct FixedPlayHead : AudioPlayHead
{
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (BPM);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setIsPlaying (true);
        return p;
    }
};

struct Note { double b; int key; double len; float vel; };
struct Act  { double b; std::function<void()> fn; };
struct Ramp { double b0, b1; String id; float v0, v1; };

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: TheOneTest <out-dir>\n"); return 1; }
    const File outDir { String (argv[1]) };
    outDir.getChildFile ("frames").createDirectory();

    {   // Startfenster aus
        PropertiesFile::Options o; o.applicationName = "The One"; o.folderName = "Dani Koenig"; o.filenameSuffix = ".settings"; o.osxLibrarySubFolder = "Application Support";
        PropertiesFile pf (o); pf.setValue ("showAtStartup", false); pf.saveIfNeeded();
    }

    TheOneProcessor p;
    FixedPlayHead ph;
    p.setPlayHead (&ph);
    const int blk = (int) (SR / FPS);   // 1600 Samples pro Frame
    p.setPlayConfigDetails (0, 2, SR, blk);
    p.prepareToPlay (SR, blk);

    std::vector<Note> notes;
    std::vector<Act> acts;
    std::vector<Ramp> ramps;
    auto set = [&] (const char* id, float v)
    {
        if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v));
        else std::printf ("unknown %s\n", id);
    };
    auto at = [&] (double b, std::function<void()> fn) { acts.push_back ({ b, std::move (fn) }); };
    auto S  = [&] (double b, const char* id, float v) { String s (id); at (b, [&set, s, v] { set (s.toRawUTF8(), v); }); };
    auto R  = [&] (double b0, double b1, const char* id, float v0, float v1) { ramps.push_back ({ b0, b1, id, v0, v1 }); };
    auto N  = [&] (double b, int key, double len, float vel = 0.85f) { notes.push_back ({ b, key, len, vel }); };
    auto chord = [&] (double b, std::vector<int> ks, double len, float vel = 0.8f) { for (int k : ks) N (b, k, len, vel); };
    auto prog = [&] (double b, int i) { at (b, [&p, i] { p.setCurrentProgram (i); }); };
    const float DIV8D = 6.0f / 14.0f;     // Effekt-Sync 1/8 D
    const float LFO8 = 7.0f / 12.0f;      // LFO-Sync 1/8

    // ---------------- Hook: grosses Pad (Takt 0-3)
    prog (0, 1);
    chord (0.1, { 45, 52, 55, 59, 60 }, 11.2);

    // ---------------- Oszillatoren (Takt 3-10, Beat 6-20)
    prog (12, 0);
    S (12, "osc1_wave", 0); S (12, "osc1_angle", 0);
    for (int bar = 3; bar < 10; ++bar) N (bar * 4.0 + 0.02, 45, 3.9, 0.8f);
    R (14, 22, "osc1_angle", 0.0f, 1.0f);
    S (22, "osc2_wave", 1); S (22, "osc2_beat", 0.5f);
    R (22, 26, "mix_lvl1", 0.0f, 0.6f);
    S (26, "osc3_mix", 1); S (26, "osc3_oct", 3);
    R (26, 28, "mix_lvl2", 0.0f, 0.45f);
    R (27, 32, "osc3_pw", 0.5f, 0.1f);
    R (32, 36, "mix_lvl5", 0.0f, 0.7f);

    // ---------------- Filter (Takt 10-17, Beat 20-34)
    const int riff[8] = { 45, 45, 57, 45, 43, 45, 48, 45 };
    for (int bar = 10; bar < 22; ++bar)
        for (int k = 0; k < 8; ++k) N (bar * 4.0 + k * 0.5, riff[k], 0.4, k % 4 == 0 ? 0.95f : 0.7f);
    S (40, "lad_res", 0.55f);
    R (40, 52, "lad_cut", 0.3f, 0.85f);
    R (52, 56, "lad_cut", 0.85f, 0.42f);
    at (56, [&set] { for (int i : { 0, 1, 2, 5 }) { set (("mix_svf" + String (i)).toRawUTF8(), 1); set (("mix_lad" + String (i)).toRawUTF8(), 0); } });
    S (56, "svf_mode", 1); S (56, "svf_res", 0.6f);
    R (56, 68, "svf_cut", 0.3f, 0.8f);

    // ---------------- LFOs (Takt 17-22, Beat 34-44)
    at (68, [&set] { for (int i : { 0, 1, 2, 5 }) { set (("mix_svf" + String (i)).toRawUTF8(), 0); set (("mix_lad" + String (i)).toRawUTF8(), 1); } });
    S (68, "lad_cut", 0.42f); S (68, "lfo2_sync", 1); S (68, "lfo2_rate", LFO8);
    R (68, 72, "lad_lfo", 0.0f, 0.55f);
    S (76, "lfo2_timing", 2);
    S (80, "lfo2_wave", 1);
    S (84, "lfo2_timing", 1);

    // ---------------- Chord Memory (Takt 22-30, Beat 44-60)
    prog (88, 0);
    S (88, "mix_lvl1", 0.5f); S (88, "osc2_beat", 0.3f);
    S (88, "lad_cut", 0.55f); S (88, "lad_res", 0.2f); S (88, "lad_eg", 0.45f);
    S (88, "feg_dec", 0.52f); S (88, "feg_sus", 0.1f);
    S (88, "aeg_dec", 0.64f); S (88, "aeg_sus", 0.22f); S (88, "aeg_rel", 0.38f); S (88, "out_vol", 0.95f);
    S (88, "fx1_type", 3); S (88, "fx1_sync", 1); S (88, "fx1_p1", DIV8D); S (88, "fx1_p2", 0.45f); S (88, "fx1_mix", 0.3f);
    S (88.1, "v_silent", 1); S (88.2, "v_learn", 1);
    { int k = 0; for (int key : { 57, 60, 64, 67, 71 }) N (88.4 + 0.16 * k++, key, 0.6, 0.8f); }
    S (90, "v_learn", 0); S (90.2, "v_chord", 1); S (90.2, "v_silent", 0);
    const int roots[4] = { 57, 53, 55, 52 };
    for (int bar = 22; bar < 30; ++bar)
        for (double off : { 0.5, 1.5, 2.5, 3.5 })
            N (bar * 4.0 + off, roots[(bar - 22) % 4], 0.3, 0.85f);

    // ---------------- Pads (Takt 30-37, Beat 60-74)
    prog (120, 1);
    S (120, "out_vol", 0.72f);
    chord (120.04, { 45, 52, 55, 59, 60 }, 7.8);
    chord (128.04, { 41, 48, 52, 57 }, 7.8);
    chord (136.04, { 48, 55, 59, 64 }, 7.8);
    chord (144.04, { 43, 50, 55, 59 }, 3.8);
    R (120, 148, "lad_cut", 0.4f, 0.66f);

    // ---------------- Bass (Takt 37-42, Beat 74-84)
    prog (148, 3);
    S (148, "out_vol", 0.8f);
    const int bl[8] = { 45, 45, 57, 45, 0, 43, 45, 48 };
    for (int bar = 37; bar < 42; ++bar)
        for (int k = 0; k < 8; ++k)
            if (bl[k] > 0) N (bar * 4.0 + k * 0.5, bar == 39 ? bl[k] - 4 : bl[k], 0.35, k == 0 ? 1.0f : 0.75f);

    // ---------------- Lead (Takt 42-47, Beat 84-94)
    prog (168, 2);
    N (168, 64, 3.1); N (171, 62, 1.1); N (172, 60, 4.1); N (176, 59, 2.1); N (178, 60, 2.1); N (180, 64, 4.1); N (184, 69, 3.8);
    at (184.6, [&p] { p.uiMod.store (0.7f); });
    at (187.8, [&p] { p.uiMod.store (0.0f); });

    // ---------------- Step-Sequencer (Takt 47-82, Beat 94-164)
    prog (188, 0);
    S (188, "out_vol", 0.85f);
    S (188, "lad_res", 0.65f); S (188, "lad_cut", 0.3f); S (188, "lad_eg", 0.55f);
    S (188, "feg_att", 0.0f); S (188, "feg_dec", 0.38f); S (188, "feg_sus", 0.05f);
    S (188, "aeg_att", 0.0f); S (188, "aeg_dec", 0.5f); S (188, "aeg_sus", 0.7f); S (188, "aeg_rel", 0.15f);
    S (188, "mix_lvl5", 0.4f); S (188, "sub_oct", 0);
    const int pat[16] = { 0, 0, 12, 0, 0, 12, 10, 0, 0, 0, 12, 0, 3, 5, 7, 12 };
    for (int i = 0; i < 16; ++i) S (188, ("seq_p" + String (i + 1)).toRawUTF8(), (float) pat[i]);
    S (188, "seq_rate", 3); S (188, "seq_len", 16); S (188, "seq_gate", 0.45f); S (188, "seq_latch", 1);
    S (188.04, "seq_on", 1);
    N (188.1, 45, 0.5);
    R (188, 208, "lad_cut", 0.28f, 0.48f);
    // Pitch + Oktave live (Beat 104-112)
    S (208, "seq_p4", 7); S (210, "seq_p8", 10); S (212, "seq_o6", 1); S (214, "seq_o14", -1);
    S (216, "seq_p12", 3); S (218, "seq_p16", 15); S (220, "seq_o2", 1); S (222, "seq_p10", -2);
    // Slide + Accent (Beat 112-120)
    S (224, "seq_ac1", 1); S (225, "seq_ac5", 1); S (226, "seq_ac9", 1); S (227, "seq_ac13", 1);
    S (232, "seq_sl3", 1); S (233, "seq_sl7", 1); S (234, "seq_sl11", 1); S (235, "seq_sl15", 1);
    // Grundton wandert ab Takt 56 (Beat 112): A A F G
    const int keys4[4] = { 45, 45, 41, 43 };
    for (int bar = 56; bar < 82; bar += 2) N (bar * 4.0 + 0.01, keys4[((bar - 56) / 2) % 4], 0.2);
    // Random (Beat 120-128)
    S (240, "seq_rndp", 1); S (242, "seq_rnda", 1); S (244, "seq_rndo", 1);
    R (246, 252, "seq_rnd", 0.3f, 0.55f);
    R (248, 252, "seq_velrnd", 0.0f, 0.5f);
    R (240, 256, "lad_cut", 0.48f, 0.58f);
    // Order (Beat 128-136)
    S (256, "seq_order", 2); S (262, "seq_order", 3); S (268, "seq_order", 0);
    S (270, "seq_rndp", 0); S (270, "seq_rndo", 0);
    // Effekte (Beat 136-152)
    S (272, "fx1_type", 3); S (272, "fx1_sync", 1); S (272, "fx1_p1", DIV8D); S (272, "fx1_p2", 0.45f); S (272, "fx1_mix", 0.35f);
    S (280, "fx2_type", 11); S (280, "fx2_p1", 0.3f); S (280, "fx2_mix", 0.35f); S (272, "out_vol", 0.95f);
    S (288, "fx3_type", 14); S (288, "fx3_mix", 0.25f); S (300, "out_vol", 1.0f);
    R (288, 296, "fx3_p4", 0.5f, 0.8f);
    // Finale (Beat 152-164)
    S (304, "lad_res", 0.75f);
    R (304, 320, "lad_cut", 0.35f, 0.8f);
    R (320, 328, "lad_cut", 0.8f, 0.42f);
    // Ausklang (Beat 164-180)
    S (328, "seq_on", 0);
    prog (328.1, 1);
    S (328.2, "out_vol", 0.72f);
    chord (328.4, { 45, 52, 55, 59, 60 }, 19, 0.75f);

    std::stable_sort (acts.begin(), acts.end(), [] (auto& a, auto& b) { return a.b < b.b; });   // gleiche Zeit: Reihenfolge wie notiert (Programm zuerst)

    // ---------------- Editor
    std::unique_ptr<TheOneEditor> ed (dynamic_cast<TheOneEditor*> (p.createEditor()));
    ed->setSize (TheOneEditor::kW, TheOneEditor::kH);

    AudioBuffer<float> buf (2, blk);
    AudioBuffer<float> all (2, (int) (TOTAL_BEATS * BEAT * SR) + blk);
    all.clear();
    const int nFrames = (int) std::lround (TOTAL_BEATS * BEAT * FPS);
    const int guiFrames = (int) std::lround (GUI_UNTIL_BEATS * BEAT * FPS);
    size_t ai = 0;
    JPEGImageFormat jpg; jpg.setQuality (0.9f);

    for (int f = 0; f < nFrames; ++f)
    {
        const double t0 = (double) f / FPS, t1 = (double) (f + 1) / FPS;
        const double b0 = t0 / BEAT, b1 = t1 / BEAT;
        while (ai < acts.size() && acts[ai].b < b1) acts[ai++].fn();
        for (auto& r : ramps)
            if (b0 >= r.b0 - 1e-9 && b0 <= r.b1 + 0.1)
            {
                const double u = jlimit (0.0, 1.0, (b0 - r.b0) / (r.b1 - r.b0));
                set (r.id.toRawUTF8(), (float) (r.v0 + (r.v1 - r.v0) * u));
            }
        MidiBuffer midi;
        for (auto& n : notes)
        {
            const double on = n.b * BEAT, off = (n.b + n.len) * BEAT;
            if (on >= t0 && on < t1) midi.addEvent (MidiMessage::noteOn (1, n.key, n.vel), (int) ((on - t0) * SR));
            if (off >= t0 && off < t1) midi.addEvent (MidiMessage::noteOff (1, n.key), (int) ((off - t0) * SR));
        }
        buf.clear();
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) all.copyFrom (c, f * blk, buf, c, 0, blk);

        if (f < guiFrames)
        {
            ed->tickForTest();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            File fo = outDir.getChildFile ("frames").getChildFile (String (f).paddedLeft ('0', 4) + ".jpg");
            fo.deleteFile();
            FileOutputStream os (fo);
            jpg.writeImageToStream (img, os);
        }
        if (f % 300 == 0) std::printf ("frame %d / %d\n", f, nFrames), std::fflush (stdout);
    }

    File wav = outDir.getChildFile ("audio.wav");
    wav.deleteFile();
    WavAudioFormat fmt;
    std::unique_ptr<AudioFormatWriter> w (fmt.createWriterFor (new FileOutputStream (wav), SR, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (all, 0, nFrames * blk);
    w.reset();
    std::printf ("done: %d frames, audio peak %.3f\n", nFrames, all.getMagnitude (0, nFrames * blk));
    ed.reset();
    return 0;
}

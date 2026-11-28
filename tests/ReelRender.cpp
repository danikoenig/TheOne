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
static constexpr double TOTAL_BEATS = 356.0;           // 178 s (unter der Shorts-Grenze)
static constexpr double GUI_UNTIL_BEATS = 332.0;       // danach Endkarte (166 s)

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

    // ================= Reel v2 (178 s, 120 BPM, Beat = 0.5 s) =================
    // ---------------- Hook: grosses Pad (Beat 0-12)
    prog (0, 1);
    S (0, "out_vol", 0.755f);
    chord (0.1, { 45, 52, 55, 59, 60 }, 11.6);

    // ---------------- Oszillatoren (Beat 12-36)
    prog (12, 0);
    S (12, "osc1_wave", 0); S (12, "osc1_angle", 0);
    for (int bar = 3; bar < 9; ++bar) N (bar * 4.0 + 0.02, 45, 3.9, 0.8f);
    R (14, 22, "osc1_angle", 0.0f, 1.0f);
    S (22, "osc2_wave", 1); S (22, "osc2_beat", 0.5f);
    R (22, 26, "mix_lvl1", 0.0f, 0.6f);
    S (26, "osc3_mix", 1); S (26, "osc3_oct", 3);
    R (26, 28, "mix_lvl2", 0.0f, 0.45f);
    R (27, 32, "osc3_pw", 0.5f, 0.1f);
    R (32, 36, "mix_lvl5", 0.0f, 0.7f);

    // ---------------- Filter + LFO (Beat 36-76)
    const int riff[8] = { 45, 45, 57, 45, 43, 45, 48, 45 };
    for (int bar = 9; bar < 19; ++bar)
        for (int k = 0; k < 8; ++k) N (bar * 4.0 + k * 0.5, riff[k], 0.4, k % 4 == 0 ? 0.95f : 0.7f);
    S (36, "lad_res", 0.55f);
    R (36, 48, "lad_cut", 0.3f, 0.85f);
    R (48, 52, "lad_cut", 0.85f, 0.42f);
    at (52, [&set] { for (int i : { 0, 1, 2, 5 }) { set (("mix_svf" + String (i)).toRawUTF8(), 1); set (("mix_lad" + String (i)).toRawUTF8(), 0); } });
    S (52, "svf_mode", 1); S (52, "svf_res", 0.6f);
    R (52, 60, "svf_cut", 0.3f, 0.8f);
    at (60, [&set] { for (int i : { 0, 1, 2, 5 }) { set (("mix_svf" + String (i)).toRawUTF8(), 0); set (("mix_lad" + String (i)).toRawUTF8(), 1); } });
    S (60, "lad_cut", 0.42f); S (60, "lfo2_sync", 1); S (60, "lfo2_rate", LFO8);
    R (60, 64, "lad_lfo", 0.0f, 0.55f);
    S (66, "lfo2_timing", 2);
    S (70, "lfo2_wave", 1);
    S (73, "lfo2_timing", 1);

    // ---------------- Akkordtypen, Inversion, Voicing, Bass (Beat 76-120)
    prog (76, 0);
    S (76, "out_vol", 0.95f);
    S (76, "mix_lvl1", 0.5f); S (76, "osc2_beat", 0.3f);
    S (76, "lad_cut", 0.55f); S (76, "lad_res", 0.2f); S (76, "lad_eg", 0.45f);
    S (76, "feg_dec", 0.52f); S (76, "feg_sus", 0.1f);
    S (76, "aeg_dec", 0.64f); S (76, "aeg_sus", 0.22f); S (76, "aeg_rel", 0.38f);
    S (76, "fx1_type", 3); S (76, "fx1_sync", 1); S (76, "fx1_p1", DIV8D); S (76, "fx1_p2", 0.45f); S (76, "fx1_mix", 0.3f);
    S (76, "v_ctype", 10); S (76, "v_chord", 1);                 // Minor 9
    S (84, "v_ctype", 3);                                         // Major 7
    S (90, "v_ctype", 11);                                        // Dominant 9
    S (95, "v_ctype", 13);                                        // Sus 4
    S (98, "v_ctype", 10);                                        // Minor 9
    S (100, "v_cinv", 1); S (104, "v_cvoice", 1); S (108, "v_cbass", 1); S (112, "v_cvoice", 3); S (116, "v_cinv", 0);
    const int roots[4] = { 57, 53, 55, 52 };
    for (int bar = 19; bar < 30; ++bar)
        for (double off : { 0.5, 1.5, 2.5, 3.5 })
            N (bar * 4.0 + off, roots[(bar - 19) % 4], 0.3, 0.85f);
    R (116, 119.8, "fx1_mix", 0.3f, 0.0f);
    R (116, 119.8, "fx1_p2", 0.45f, 0.0f);

    // ---------------- Presets + Pads (Beat 120-144)
    prog (120, 1);
    S (120, "out_vol", 0.68f);
    chord (120.04, { 45, 52, 55, 59, 60 }, 7.8);
    chord (128.04, { 41, 48, 52, 57 }, 7.8);
    chord (136.04, { 48, 55, 59, 64 }, 3.8);
    chord (140.04, { 43, 50, 55, 59 }, 3.8);
    R (120, 144, "lad_cut", 0.4f, 0.66f);

    // ---------------- Bass + Knack Clip (Beat 144-164)
    prog (144, 3);
    S (144, "out_vol", 0.7f);
    const int bl[8] = { 45, 45, 57, 45, 0, 43, 45, 48 };
    for (int bar = 36; bar < 41; ++bar)
        for (int k = 0; k < 8; ++k)
            if (bl[k] > 0) N (bar * 4.0 + k * 0.5, bar == 38 ? bl[k] - 4 : bl[k], 0.35, k == 0 ? 1.0f : 0.75f);
    R (150, 158, "out_clip", 0.0f, 0.6f);

    // ---------------- Lead (Beat 164-180)
    prog (164, 2);
    S (164, "out_vol", 0.62f);
    N (164, 64, 3.1); N (167, 62, 1.1); N (168, 60, 4.1); N (172, 59, 2.1); N (174, 60, 2.1); N (176, 69, 3.8);
    at (176.6, [&p] { p.uiMod.store (0.7f); });
    at (179.8, [&p] { p.uiMod.store (0.0f); });

    // ---------------- Step-Sequencer (Beat 180-304)
    prog (180, 0);
    S (180, "out_vol", 0.85f);
    S (180, "lad_res", 0.65f); S (180, "lad_cut", 0.3f); S (180, "lad_eg", 0.55f);
    S (180, "feg_att", 0.0f); S (180, "feg_dec", 0.38f); S (180, "feg_sus", 0.05f);
    S (180, "aeg_att", 0.0f); S (180, "aeg_dec", 0.5f); S (180, "aeg_sus", 0.7f); S (180, "aeg_rel", 0.15f);
    S (180, "mix_lvl5", 0.4f); S (180, "sub_oct", 0);
    const int pat[16] = { 0, 0, 12, 0, 0, 12, 10, 0, 0, 0, 12, 0, 3, 5, 7, 12 };
    for (int i = 0; i < 16; ++i) S (180, ("seq_p" + String (i + 1)).toRawUTF8(), (float) pat[i]);
    S (180, "seq_rate", 3); S (180, "seq_len", 16); S (180, "seq_gate", 0.45f); S (180, "seq_latch", 1);
    S (180.04, "seq_on", 1);
    N (180.1, 45, 0.5);
    R (180, 200, "lad_cut", 0.28f, 0.48f);
    // Pitch + Oktave live
    S (200, "seq_p4", 7); S (201.5, "seq_p8", 10); S (203, "seq_o6", 1); S (204.5, "seq_o14", -1);
    S (206, "seq_p12", 3); S (207.5, "seq_p16", 15); S (209, "seq_o2", 1); S (210.5, "seq_p10", -2);
    // Slide + Accent
    S (212, "seq_ac1", 1); S (213, "seq_ac5", 1); S (214, "seq_ac9", 1); S (215, "seq_ac13", 1);
    S (217, "seq_sl3", 1); S (218, "seq_sl7", 1); S (219, "seq_sl11", 1); S (220, "seq_sl15", 1);
    const int keys4[4] = { 45, 45, 41, 43 };
    for (int bar = 53; bar < 76; bar += 2) N (bar * 4.0 + 0.01, keys4[((bar - 53) / 2) % 4], 0.4);
    // Random
    S (224, "seq_rndp", 1); S (225, "seq_rnda", 1); S (226, "seq_rndo", 1);
    R (227, 230, "seq_rnd", 0.3f, 0.55f);
    R (228, 230, "seq_velrnd", 0.0f, 0.5f);
    R (224, 236, "lad_cut", 0.48f, 0.58f);
    // Order
    S (236, "seq_order", 2); S (240, "seq_order", 3); S (244, "seq_order", 0);
    S (246, "seq_rndp", 0); S (246, "seq_rndo", 0);
    // Effekte
    S (248, "out_vol", 0.95f);
    S (248, "fx1_type", 3); S (248, "fx1_sync", 1); S (248, "fx1_p1", DIV8D); S (248, "fx1_p2", 0.45f); S (248, "fx1_mix", 0.35f);
    S (254, "fx2_type", 11); S (254, "fx2_p1", 0.3f); S (254, "fx2_mix", 0.35f);
    S (260, "fx3_type", 14); S (260, "fx3_mix", 0.25f); S (260, "out_vol", 1.0f);
    R (260, 266, "fx3_p4", 0.5f, 0.8f);
    // Finale
    S (276, "lad_res", 0.75f);
    R (276, 292, "lad_cut", 0.35f, 0.8f);
    R (292, 304, "lad_cut", 0.8f, 0.42f);

    // ---------------- Ausklang: eine Taste, grosser Akkord (Beat 304-356)
    S (304, "seq_on", 0);
    prog (304.05, 1);
    S (304.1, "out_vol", 0.48f);
    S (304.1, "v_ucount", 2);
    S (304.1, "v_chord", 1); S (304.1, "v_ctype", 10); S (304.1, "v_cvoice", 3); S (304.1, "v_cbass", 1);
    N (304.3, 45, 7.6, 0.75f);
    N (312.3, 41, 7.6, 0.75f);
    N (320.3, 45, 20.0, 0.75f);

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

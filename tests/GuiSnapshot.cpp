// Lokaler Test: Prozessor + Editor erzeugen, Audio rendern, Oberflaeche als PNG speichern.
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <vector>
#include <cmath>
using namespace juce;

struct TestPlayHead : AudioPlayHead
{
    bool playing = false; double ppq = 0;
    Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setBpm (120.0); p.setIsPlaying (playing); p.setPpqPosition (ppq); return p; }
};

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    TheOneProcessor p;
    p.setPlayConfigDetails (0, 2, 48000, 512);
    p.prepareToPlay (48000, 512);
    const String outPng = argc > 1 ? argv[1] : "/tmp/theone_gui.png";

    // Test: TRI gegen SAW, Sub-Oszillator
    {
        TheOneProcessor q;
        q.setPlayConfigDetails (0, 2, 48000, 512);
        q.prepareToPlay (48000, 512);
        auto qs = [&] (const char* id, float v) { if (auto* a = q.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); else printf ("unknown %s\n", id); };
        auto goertz = [] (const std::vector<float>& x, double f) { double re = 0, im = 0; for (size_t i = 0; i < x.size(); ++i) { const double w = 2.0 * 3.14159265358979 * f * (double) i / 48000.0; re += x[i] * std::cos (w); im += x[i] * std::sin (w); } return std::sqrt (re * re + im * im) / (double) x.size(); };
        auto render = [&] (float sw, float sub) {
            q.setCurrentProgram (0);
            qs ("lad_cut", 1.0f); qs ("lad_eg", 0.0f); qs ("lad_res", 0.0f); qs ("osc1_wave", sw); qs ("osc1_angle", 0.0f);
            qs ("mix_lvl5", sub); qs ("sub_oct", 0); qs ("aeg_att", 0.0f); qs ("aeg_sus", 1.0f);
            q.keyboardState.noteOn (1, 57, 1.0f);
            AudioBuffer<float> b (2, 512); MidiBuffer m; std::vector<float> o;
            for (int i = 0; i < 60; ++i) { m.clear(); b.clear(); q.processBlock (b, m); if (i >= 20) for (int k = 0; k < 512; ++k) o.push_back (b.getSample (0, k)); }
            q.keyboardState.allNotesOff (1);
            for (int i = 0; i < 100; ++i) { m.clear(); b.clear(); q.processBlock (b, m); }
            return o;
        };
        auto tri = render (0, 0), saw = render (1, 0), sb = render (0, 0.8f);
        printf ("TRI  f %.4f 2f %.4f 3f %.4f\n", goertz (tri, 220), goertz (tri, 440), goertz (tri, 660));
        printf ("SAW  f %.4f 2f %.4f 3f %.4f\n", goertz (saw, 220), goertz (saw, 440), goertz (saw, 660));
        printf ("SUB  110Hz %.4f 220Hz %.4f\n", goertz (sb, 110), goertz (sb, 220));
        for (int pi = 0; pi < q.getNumPrograms(); ++pi)
        {
            q.setCurrentProgram (pi);
            q.keyboardState.noteOn (1, 48, 1.0f);
            AudioBuffer<float> b (2, 512); MidiBuffer m; float pk = 0;
            for (int i = 0; i < 100; ++i) { m.clear(); b.clear(); q.processBlock (b, m); pk = jmax (pk, b.getMagnitude (0, 512)); }
            q.panic();
            for (int i = 0; i < 4; ++i) { m.clear(); b.clear(); q.processBlock (b, m); }
            printf ("program %s peak %.3f after-panic peak %.5f voices %d\n", q.getProgramName (pi).toRawUTF8(), pk, b.getMagnitude (0, 512), q.voicesInUse.load());
        }
    }

    // Test: Sequencer
    {
        TheOneProcessor q;
        q.setPlayConfigDetails (0, 2, 48000, 512);
        q.prepareToPlay (48000, 512);
        auto qs = [&] (const char* id, float v) { if (auto* a = q.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); else printf ("unknown %s\n", id); };
        qs ("seq_on", 1); qs ("seq_rate", 3); qs ("seq_len", 4); qs ("aeg_att", 0); qs ("aeg_rel", 0.05f);
        qs ("seq_sl1", 1); qs ("seq_p2", 7); qs ("seq_ac3", 1); qs ("seq_sl4", 1); qs ("seq_o4", 1);
        AudioBuffer<float> b (2, 512); MidiBuffer m;
        q.keyboardState.noteOn (1, 48, 1.0f);
        String stages; int last = -9; float pk = 0;
        for (int i = 0; i < 280; ++i) { m.clear(); b.clear(); q.processBlock (b, m); pk = jmax (pk, b.getMagnitude (0, 512)); const int s = q.seqStage.load(); if (s != last) { stages << s; last = s; } }
        q.keyboardState.noteOff (1, 48, 0.0f);
        float tail = 0;
        for (int i = 0; i < 200; ++i) { m.clear(); b.clear(); q.processBlock (b, m); if (i > 100) tail = jmax (tail, b.getMagnitude (0, 512)); }
        // Skip/Mute/Random
        qs ("seq_m2", 2); qs ("seq_m3", 1); qs ("seq_order", 0);
        q.keyboardState.noteOn (1, 48, 1.0f); stages = ""; last = -9;
        for (int i = 0; i < 200; ++i) { m.clear(); b.clear(); q.processBlock (b, m); const int s = q.seqStage.load(); if (s != last) { stages << s << " "; last = s; } }
        printf ("skip2/mute3 fwd: %s\n", stages.toRawUTF8());
        q.keyboardState.noteOff (1, 48, 0.0f); for (int i = 0; i < 20; ++i) { m.clear(); b.clear(); q.processBlock (b, m); }
        for (int o = 1; o < 4; ++o)
        {
            qs ("seq_order", (float) o); qs ("seq_len", 6);
            q.keyboardState.noteOn (1, 48, 1.0f); stages = ""; last = -9;
            for (int i = 0; i < 250; ++i) { m.clear(); b.clear(); q.processBlock (b, m); const int s = q.seqStage.load(); if (s != last) { stages << s << " "; last = s; } }
            printf ("order %d: %s\n", o, stages.toRawUTF8());
            q.keyboardState.noteOff (1, 48, 0.0f); for (int i = 0; i < 20; ++i) { m.clear(); b.clear(); q.processBlock (b, m); }
        }
        printf ("seq stages %s peak %.3f tail-after-release %.5f stage %d\n", stages.toRawUTF8(), pk, tail, q.seqStage.load());
    }

    // Test: Slide (kein Neuanschlag, Tonhoehe gleitet)
    for (int sl = 0; sl < 2; ++sl)
    {
        TheOneProcessor q;
        q.setPlayConfigDetails (0, 2, 48000, 256);
        q.prepareToPlay (48000, 256);
        auto qs = [&] (const char* id, float v) { if (auto* a = q.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        qs ("seq_on", 1); qs ("seq_rate", 0); qs ("seq_len", 2); qs ("seq_gate", 0.5f); qs ("seq_p2", 12); qs ("seq_sl1", (float) sl);
        qs ("aeg_att", 0); qs ("aeg_dec", 0.3f); qs ("aeg_sus", 0.6f); qs ("aeg_rel", 0.1f); qs ("out_tempo", 120);
        q.keyboardState.noteOn (1, 48, 1.0f);
        AudioBuffer<float> b (2, 256); MidiBuffer m; int maxV = 0; float minLvl = 9;
        for (int i = 0; i < 250; ++i) { m.clear(); b.clear(); q.processBlock (b, m); maxV = jmax (maxV, q.voicesInUse.load());
            if (i > 60 && i < 120) minLvl = jmin (minLvl, b.getRMSLevel (0, 0, 256)); }
        printf ("slide=%d maxVoices %d minLevel(around step change) %.4f\n", sl, maxV, minLvl);
    }

    // Test: FX-Sync-Werte
    for (int i = 0; i < TO::kFxDivs; ++i) printf ("%s=%.3f ", TO::fxDivName (i), TO::kFxDivBeats[i]);
    printf ("\n");

    // Klick-Test: Pad-Akkordwechsel
    for (int variant = 0; variant < 4; ++variant)
    {
        TheOneProcessor q;
        q.setPlayConfigDetails (0, 2, 48000, 64);
        q.prepareToPlay (48000, 64);
        q.setCurrentProgram (1);
        auto qs = [&] (const char* id, float v) { if (auto* a = q.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        if (variant == 1) qs ("v_unison", 0);
        if (variant == 2) { qs ("fx1_type", 0); qs ("fx2_type", 0); }
        if (variant == 3) qs ("v_poly", 8);
        const int c1[5] = { 45, 52, 55, 59, 60 }, c2[4] = { 41, 48, 52, 57 };
        std::vector<float> outv;
        AudioBuffer<float> b (2, 64);
        for (int pos = 0; pos < 48000 * 6; pos += 64)
        {
            MidiBuffer m;
            if (pos == 0) for (int k : c1) m.addEvent (MidiMessage::noteOn (1, k, 0.8f), 0);
            if (pos == 64 * 2925) for (int k : c1) m.addEvent (MidiMessage::noteOff (1, k), 0);   // ~3.9 s
            if (pos == 64 * 3000) for (int k : c2) m.addEvent (MidiMessage::noteOn (1, k, 0.8f), 1);   // 4.0 s
            b.clear(); q.processBlock (b, m);
            for (int i = 0; i < 64; ++i) outv.push_back (b.getSample (0, i));
        }
        float atChange = 0, steady = 0;
        for (size_t i = 2; i < outv.size(); ++i)
        {
            const float d2 = std::fabs (outv[i] - 2 * outv[i - 1] + outv[i - 2]);
            if (i > 192000 - 200 && i < 192000 + 4800) atChange = jmax (atChange, d2);
            if (i > 192000 + 48000 && i < 192000 + 96000) steady = jmax (steady, d2);
        }
        printf ("click variant %d: change %.4f steady %.4f\n", variant, atChange, steady);
    }

    // Akkord-Test: Typen, Inversion, Voicing, Bass
    {
        auto pr = [] (const char* label, std::vector<int> v) { String s; for (int x : v) s << x << " "; printf ("chord %-22s %s\n", label, s.toRawUTF8()); };
        pr ("Minor 9", TheOneProcessor::buildChord ({ 0 }, 10, 0, 0, 0));
        pr ("Minor 9 inv1", TheOneProcessor::buildChord ({ 0 }, 10, 1, 0, 0));
        pr ("Major 7 open", TheOneProcessor::buildChord ({ 0 }, 3, 0, 1, 0));
        pr ("Major 7 drop2", TheOneProcessor::buildChord ({ 0 }, 3, 0, 2, 0));
        pr ("Major 7 wide", TheOneProcessor::buildChord ({ 0 }, 3, 0, 3, 0));
        pr ("Minor bass -1", TheOneProcessor::buildChord ({ 0 }, 2, 0, 0, 1));
        pr ("Learned 0,3,7 bass -2", TheOneProcessor::buildChord ({ 0, 3, 7 }, 0, 0, 0, 2));
    }
    // Silent-Learn-Test: SILENT allein lernt ohne Ton
    {
        TheOneProcessor q;
        q.setPlayConfigDetails (0, 2, 48000, 512);
        q.prepareToPlay (48000, 512);
        auto qs = [&] (const char* id, float v) { if (auto* a = q.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        AudioBuffer<float> b (2, 512); MidiBuffer m;
        qs ("v_silent", 1);
        m.clear(); b.clear(); q.processBlock (b, m);
        m.addEvent (MidiMessage::noteOn (1, 57, 0.8f), 0); m.addEvent (MidiMessage::noteOn (1, 60, 0.8f), 10); m.addEvent (MidiMessage::noteOn (1, 64, 0.8f), 20);
        float pk = 0; b.clear(); q.processBlock (b, m); pk = b.getMagnitude (0, 512);
        for (int i = 0; i < 20; ++i) { m.clear(); b.clear(); q.processBlock (b, m); pk = jmax (pk, b.getMagnitude (0, 512)); }
        m.clear(); for (int k : { 57, 60, 64 }) m.addEvent (MidiMessage::noteOff (1, k), 0);
        b.clear(); q.processBlock (b, m);
        qs ("v_silent", 0); m.clear(); b.clear(); q.processBlock (b, m);
        printf ("silent learn: peak while learning %.5f learned %s\n", pk, q.chordToString().toRawUTF8());
    }

    // Clip-Test: Bass mit CLIP 0 / 50 / 100 %
    for (float cl : { 0.0f, 0.5f, 1.0f })
    {
        TheOneProcessor q;
        q.setPlayConfigDetails (0, 2, 48000, 512);
        q.prepareToPlay (48000, 512);
        q.setCurrentProgram (3);
        if (auto* a = q.apvts.getParameter ("out_clip")) a->setValueNotifyingHost (cl);
        AudioBuffer<float> b (2, 512); MidiBuffer m; double sq = 0; float pk = 0; int n = 0;
        for (int i = 0; i < 200; ++i)
        {
            m.clear(); if (i % 12 == 0) m.addEvent (MidiMessage::noteOn (1, 45, 1.0f), 0); if (i % 12 == 8) m.addEvent (MidiMessage::noteOff (1, 45), 0);
            b.clear(); q.processBlock (b, m);
            for (int k = 0; k < 512; ++k) { const float v = b.getSample (0, k); sq += v * v; ++n; pk = jmax (pk, std::fabs (v)); }
        }
        printf ("clip %.0f%%: rms %.1f dB peak %.2f latency %d name %s\n", cl * 100, 10 * std::log10 (sq / n), pk, q.getLatencySamples(), q.currentPresetName().toRawUTF8());
    }

    // Preset-Test: speichern, aendern, laden
    {
        TheOneProcessor q;
        q.prepareToPlay (48000, 512);
        auto qs = [&] (const char* id, float v) { if (auto* a = q.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        auto qg = [&] (const char* id) { return q.apvts.getRawParameterValue (id)->load(); };
        qs ("lad_cut", 0.33f); qs ("seq_p5", 7); q.setChordFromString ("0,3,7");
        const bool ok = q.savePreset ("Test Preset", "Bass");
        qs ("lad_cut", 0.9f); qs ("seq_p5", 0); q.setChordFromString ("0,4");
        auto list = q.scanPresets(); int found = -1;
        for (int i = 0; i < list.size(); ++i) if (list[i].name == "Test Preset") found = i;
        const bool ld = found >= 0 && q.loadPreset (list[found]);
        printf ("preset: saved %d found %d (cat '%s') loaded %d cut %.2f seq_p5 %.0f chord %s name %s count %d\n", ok, found, found >= 0 ? list[found].category.toRawUTF8() : "", ld,
                qg ("lad_cut"), qg ("seq_p5"), q.chordToString().toRawUTF8(), q.currentPresetName().toRawUTF8(), list.size());
        if (found >= 0) list[found].file.deleteFile();
        TheOneProcessor::presetRoot().getChildFile ("Bass").deleteFile();
    }

    // Transport-Test: Sequencer folgt Start/Stop der DAW
    {
        TheOneProcessor q;
        TestPlayHead tp;
        q.setPlayHead (&tp);
        q.setPlayConfigDetails (0, 2, 48000, 512);
        q.prepareToPlay (48000, 512);
        auto qs = [&] (const char* id, float v) { if (auto* a = q.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        qs ("seq_on", 1); qs ("seq_latch", 1);
        AudioBuffer<float> b (2, 512); MidiBuffer m;
        auto run = [&] (int blocks) { float pk = 0; for (int i = 0; i < blocks; ++i) { m.clear(); b.clear(); q.processBlock (b, m); if (tp.playing) tp.ppq += 512.0 / 48000.0 * 2.0; pk = jmax (pk, b.getMagnitude (0, 512)); } return pk; };
        q.keyboardState.noteOn (1, 48, 1.0f); run (2); q.keyboardState.noteOff (1, 48, 0.0f);
        const float free = run (60); const int s1 = q.seqStage.load();
        tp.playing = true; tp.ppq = 0; const float playA = run (60); const int s2 = q.seqStage.load();
        tp.playing = false; run (2); const float stopped = run (60); const int s3 = q.seqStage.load();
        tp.playing = true; tp.ppq = 0; const float playB = run (60); const int s4 = q.seqStage.load();
        printf ("transport: free %.3f stage %d | play %.3f stage %d | stop %.5f stage %d | play again %.3f stage %d\n", free, s1, playA, s2, stopped, s3, playB, s4);
    }

    // Bass-Demo (WAV zum Abhoeren)
    {
        TheOneProcessor q;
        q.setPlayConfigDetails (0, 2, 48000, 512);
        q.prepareToPlay (48000, 512);
        q.setCurrentProgram (3);
        const int line[16] = { 45, 0, 57, 45, 0, 45, 48, 0, 45, 0, 43, 45, 0, 55, 45, 0 };
        AudioBuffer<float> all (2, 48000 * 9); all.clear();
        AudioBuffer<float> b (2, 512);
        const double step = 0.125 * 48000;   // 1/16 bei 120 BPM
        int last = -1;
        for (int pos = 0; pos + 512 <= all.getNumSamples(); pos += 512)
        {
            MidiBuffer m;
            for (int s = 0; s < 64; ++s)
            {
                const int on = (int) (s * step), off = (int) (s * step + step * 0.8);
                const int n = line[s % 16];
                if (n > 0 && on >= pos && on < pos + 512) m.addEvent (MidiMessage::noteOn (1, n, s % 4 == 0 ? 1.0f : 0.75f), on - pos);
                if (n > 0 && off >= pos && off < pos + 512) m.addEvent (MidiMessage::noteOff (1, n), off - pos);
            }
            b.clear(); q.processBlock (b, m);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, b, c, 0, 512);
            (void) last;
        }
        File wf ("/tmp/bass_demo.wav"); wf.deleteFile();
        WavAudioFormat fmt;
        std::unique_ptr<AudioFormatWriter> w (fmt.createWriterFor (new FileOutputStream (wf), 48000, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        printf ("bass demo peak %.3f\n", all.getMagnitude (0, all.getNumSamples()));
    }

    // Zustand: Akkord, 3 Oszillatoren, Effekte, damit das Bild etwas zeigt
    auto set = [&] (const char* id, float v) { if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); else printf ("unknown %s\n", id); };
    set ("mix_lvl1", 0.6f); set ("mix_lvl2", 0.5f); set ("osc2_beat", 0.5f); set ("osc3_beat", -0.6f); set ("osc2_wave", 0); set ("osc2_angle", 0.4f);
    set ("seq_on", 0); set ("seq_sl3", 1); set ("seq_ac1", 1); set ("seq_ac5", 1); set ("seq_o6", 1); set ("seq_p6", 7); set ("fx1_type", 1); set ("fx2_type", 7); set ("fx3_type", 14);
    set ("v_chord", 1); set ("v_unison", 1);
    p.setChordFromString ("0,4,7,12");

    AudioBuffer<float> buf (2, 512);
    MidiBuffer midi;
    p.keyboardState.noteOn (1, 60, 0.8f);
    p.keyboardState.noteOn (1, 67, 0.8f);
    float peak = 0;
    for (int i = 0; i < 80; ++i)
    {
        midi.clear();
        buf.clear();
        p.processBlock (buf, midi);
        peak = jmax (peak, buf.getMagnitude (0, 512));
    }
    printf ("peak %.3f voices %d gen %llx %llx\n", peak, p.voicesInUse.load(), (unsigned long long) p.genLo.load(), (unsigned long long) p.genHi.load());

    {
        PropertiesFile::Options o; o.applicationName = "The One"; o.folderName = "Dani Koenig"; o.filenameSuffix = ".settings"; o.osxLibrarySubFolder = "Application Support";
        PropertiesFile pf (o); pf.setValue ("showAtStartup", ! (argc > 2 && String (argv[2]) == "noscreen")); pf.saveIfNeeded();
    }
    std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1760, 680);
    MessageManager::getInstance()->runDispatchLoopUntil (300);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    File f (outPng);
    f.deleteFile();
    FileOutputStream fo (f);
    PNGImageFormat png;
    png.writeImageToStream (img, fo);
    printf ("snapshot %dx%d -> %s\n", img.getWidth(), img.getHeight(), outPng.toRawUTF8());

    ed->setSize (2200, 850);
    MessageManager::getInstance()->runDispatchLoopUntil (200);
    auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    File f2 (outPng.replace (".png", "_full.png"));
    f2.deleteFile();
    FileOutputStream fo2 (f2);
    png.writeImageToStream (img2, fo2);
    ed.reset();
    return 0;
}

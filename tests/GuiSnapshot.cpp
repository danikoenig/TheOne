// Lokaler Test: Prozessor + Editor erzeugen, Audio rendern, Oberflaeche als PNG speichern.
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <vector>
#include <cmath>
using namespace juce;

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

    // Zustand: Akkord, 3 Oszillatoren, Effekte, damit das Bild etwas zeigt
    auto set = [&] (const char* id, float v) { if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); else printf ("unknown %s\n", id); };
    set ("mix_lvl1", 0.6f); set ("mix_lvl2", 0.5f); set ("osc2_beat", 0.5f); set ("osc3_beat", -0.6f); set ("osc2_wave", 0); set ("osc2_angle", 0.4f);
    set ("seq_on", 1); set ("seq_sl3", 1); set ("seq_ac1", 1); set ("seq_ac5", 1); set ("seq_o6", 1); set ("seq_p6", 7); set ("fx1_type", 1); set ("fx2_type", 7); set ("fx3_type", 14);
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
    printf ("peak %.3f voices %d\n", peak, p.voicesInUse.load());

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

// Lokaler Test: Prozessor + Editor erzeugen, Audio rendern, Oberflaeche als PNG speichern.
#include "../Source/PluginProcessor.h"
#include <cstdio>
using namespace juce;

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    TheOneProcessor p;
    p.setPlayConfigDetails (0, 2, 48000, 512);
    p.prepareToPlay (48000, 512);
    const String outPng = argc > 1 ? argv[1] : "/tmp/theone_gui.png";

    // Zustand: Akkord, 3 Oszillatoren, Effekte, damit das Bild etwas zeigt
    auto set = [&] (const char* id, float v) { if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); else printf ("unknown %s\n", id); };
    set ("mix_lvl1", 0.6f); set ("mix_lvl2", 0.5f); set ("osc2_beat", 0.5f); set ("osc3_beat", -0.6f); set ("osc2_wave", 0); set ("osc2_angle", 0.4f);
    set ("fx1_type", 1); set ("fx2_type", 7); set ("fx3_type", 14);
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

    std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1760, 704);
    MessageManager::getInstance()->runDispatchLoopUntil (300);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    File f (outPng);
    f.deleteFile();
    FileOutputStream fo (f);
    PNGImageFormat png;
    png.writeImageToStream (img, fo);
    printf ("snapshot %dx%d -> %s\n", img.getWidth(), img.getHeight(), outPng.toRawUTF8());

    ed->setSize (2200, 880);
    MessageManager::getInstance()->runDispatchLoopUntil (200);
    auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    File f2 (outPng.replace (".png", "_full.png"));
    f2.deleteFile();
    FileOutputStream fo2 (f2);
    png.writeImageToStream (img2, fo2);
    ed.reset();
    return 0;
}

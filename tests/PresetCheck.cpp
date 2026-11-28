// Pegel- und Plausibilitaetspruefung aller Werks-Presets (lokal): rendert je Preset eine Probe und misst RMS/Peak.
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <cmath>
using namespace juce;

struct PH : AudioPlayHead
{
    Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setBpm (120.0); p.setTimeSignature (TimeSignature { 4, 4 }); p.setIsPlaying (true); return p; }
};

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0; const int blk = 512;
    const File wavDir = argc > 1 ? File (String (argv[1])) : File();
    for (int prg = 0;; ++prg)
    {
        TheOneProcessor p; PH ph; p.setPlayHead (&ph);
        if (prg >= p.getNumPrograms()) break;
        p.setPlayConfigDetails (0, 2, sr, blk); p.prepareToPlay (sr, blk);
        p.setCurrentProgram (prg);
        const String name = p.getProgramName (prg);
        const bool mono = name.startsWith ("Bass") || name.startsWith ("Lead") || name.startsWith ("Seq") || name.startsWith ("Chord");
        const int total = (int) (sr * 4.0), noteOff = (int) (sr * 3.0);
        AudioBuffer<float> out (2, total); out.clear();
        double sum = 0; float peak = 0; bool bad = false;
        for (int pos = 0; pos < total; pos += blk)
        {
            const int n = std::min (blk, total - pos);
            AudioBuffer<float> b (2, n); b.clear();
            MidiBuffer m;
            auto notes = mono ? std::vector<int> { 45 } : std::vector<int> { 57, 60, 64, 67 };
            if (mono && ! name.startsWith ("Seq") && ! name.startsWith ("Chord"))
            {   // kleine Linie fuer Bass/Lead: Achtel
                for (int k = 0; k < 6; ++k)
                {
                    const int on = (int) (k * 0.5 * sr), off = (int) ((k * 0.5 + 0.4) * sr);
                    const int key = (name.startsWith ("Lead") ? 60 : 36) + (k % 3 == 2 ? 7 : (k % 3) * 3);
                    if (on >= pos && on < pos + n) m.addEvent (MidiMessage::noteOn (1, key, (uint8) 100), on - pos);
                    if (off >= pos && off < pos + n) m.addEvent (MidiMessage::noteOff (1, key), off - pos);
                }
            }
            else
            {
                if (pos == 0) for (int k : notes) m.addEvent (MidiMessage::noteOn (1, k, (uint8) 100), 0);
                if (noteOff >= pos && noteOff < pos + n) for (int k : notes) m.addEvent (MidiMessage::noteOff (1, k), noteOff - pos);
            }
            p.processBlock (b, m);
            for (int c = 0; c < 2; ++c)
            {
                out.copyFrom (c, pos, b, c, 0, n);
                for (int i = 0; i < n; ++i)
                {
                    const float v = b.getSample (c, i);
                    if (! std::isfinite (v)) bad = true;
                    if (pos < noteOff) sum += v * v;
                    peak = std::max (peak, std::abs (v));
                }
            }
        }
        const double rms = std::sqrt (sum / (2.0 * noteOff));
        std::printf ("%2d  %-28s rms %6.1f dB  peak %5.2f %s\n", prg, name.toRawUTF8(), 20 * std::log10 (rms + 1e-9), peak, bad ? "NaN!" : "");
        if (wavDir != File())
        {
            wavDir.createDirectory();
            auto f = wavDir.getChildFile (String (prg).paddedLeft ('0', 2) + " " + File::createLegalFileName (name) + ".wav");
            f.deleteFile();
            WavAudioFormat wf;
            if (auto w = std::unique_ptr<AudioFormatWriter> (wf.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0)))
                w->writeFromAudioSampleBuffer (out, 0, total);
        }
    }
    return 0;
}

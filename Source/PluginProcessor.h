#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include "Params.h"
#include "Engine.h"
#include "Effects.h"

class TheOneProcessor : public juce::AudioProcessor
{
public:
    TheOneProcessor();
    ~TheOneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "The One"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    std::vector<TO::PDef> defs;

    // GUI-Zugriff
    std::atomic<float> uiBend { 0.0f }, uiMod { 0.0f };
    std::atomic<int> voicesInUse { 0 };
    std::atomic<float> hostBpm { 120.0f };
    std::array<std::atomic<float>, 1024> scope;
    std::atomic<int> scopeW { 0 };
    std::array<std::atomic<int>, 16> chordNotes;
    std::atomic<int> chordCount { 1 };
    std::atomic<bool> learnActive { false };
    std::atomic<int> learnCount { 0 };
    std::atomic<float> outLevel { 0.0f };
    std::atomic<int> seqStage { -1 };   // laufende Stufe fuer die Anzeige
    std::atomic<uint64_t> genLo { 0 }, genHi { 0 };   // Noten, die der Synth gerade spielt (Akkorde, Sequencer)

    void setChordFromString (const juce::String&);
    juce::String chordToString() const;
    void clearChord();
    void panic();   // Message-Thread: alles stumm, Hold aus

    // ---- Presets (Message-Thread). Dateien *.theone im Ordner Music/Dani Koenig/The One/Presets, Unterordner = Kategorie
    struct PresetEntry { juce::String name, category; juce::File file; int factory = -1; };
    static juce::File presetRoot();
    juce::Array<PresetEntry> scanPresets() const;
    bool savePreset (const juce::String& name, const juce::String& category);
    bool loadPreset (const PresetEntry&);
    juce::String currentPresetName() const { return apvts.state.getProperty ("presetName", "Init").toString(); }
    juce::File currentPresetFile() const { return juce::File (apvts.state.getProperty ("presetFile", "").toString()); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const std::vector<TO::PDef>&);
    void applyChord (const std::vector<int>&);

    std::atomic<bool> panicFlag { false };
    void runSequencer (juce::MidiBuffer&, int numSamples, double bpm, bool learn, bool hostPlaying, double ppq);
    bool prevHostPlaying = false, seqHadRoot = false;
    juce::Array<int> seqKeys;
    bool seqRunning = false;
    int seqRoot = 60, seqStageIdx = 0, seqSounding = -1;
    bool seqPrevSlide = false;
    double seqToPulse = 0.0, seqOffIn = -1.0;
    float seqVel = 0.8f;
    int seqDir = 1;
    juce::Random seqRng;
    int currentProgram = 0;
    TO::Synth synth;
    TO::FxChain fx;
    std::vector<std::atomic<float>*> raw;
    std::array<float, (size_t) TO::NUM_PARAMS> P;
    juce::AudioBuffer<float> scratch;
    std::array<bool, 128> down {};
    std::vector<int> learnNotes;
    bool lastLearn = false;
    float midiBend = 0.0f, midiMod = 0.0f;
    double sampleRate = 44100.0;
    juce::SpinLock chordLock;
    bool chordPending = false;
    std::vector<int> pendingChord;
    std::vector<int> learnedIv { 0 };      // gelernter Akkord (Audio-Thread)
    int lastChordKey = -1;
    bool learnedDirty = true;
public:
    static std::vector<int> buildChord (const std::vector<int>& learned, int type, int inv, int voicing, int bass);
private:
    int scopeDecim = 0;
    float limState = 1.0f;
    juce::SmoothedValue<float> volS, clipS;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;   // Knack-Clipper, 4x
    // Ausgangs-Limiter mit 1.5 ms Vorschau (verzerrt nicht wie ein Clipper)
    std::vector<float> laL, laR;
    int laPos = 0, laLen = 72;
    float limGain = 1.0f, limAtt = 0.9f, limRel = 0.9999f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheOneProcessor)
};

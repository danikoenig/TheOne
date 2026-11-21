#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
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

    void setChordFromString (const juce::String&);
    juce::String chordToString() const;
    void clearChord();
    void panic();   // Message-Thread: alles stumm, Hold aus

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const std::vector<TO::PDef>&);
    void applyChord (const std::vector<int>&);

    std::atomic<bool> panicFlag { false };
    void runSequencer (juce::MidiBuffer&, int numSamples, double bpm, bool learn);
    juce::Array<int> seqKeys;
    bool seqRunning = false;
    int seqRoot = 60, seqStageIdx = 0, seqPulseIdx = 0, seqSounding = -1;
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
    int scopeDecim = 0;
    float limState = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheOneProcessor)
};

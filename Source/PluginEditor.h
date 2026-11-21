#pragma once
#include "PluginProcessor.h"
#include "Look.h"

class TheOneEditor : public juce::AudioProcessorEditor
{
public:
    explicit TheOneEditor (TheOneProcessor&);
    ~TheOneEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override {}

    static constexpr int kW = 2200, kH = 850;
    class Content;
private:
    TheOneProcessor& proc;
    std::unique_ptr<Content> content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheOneEditor)
};

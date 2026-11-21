#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "Params.h"

namespace Col
{
    const juce::Colour panelTop    { 0xff17181c };
    const juce::Colour panelBottom { 0xff0d0e11 };
    const juce::Colour line        { 0xff6a6f7a };
    const juce::Colour text        { 0xffeceef2 };
    const juce::Colour textDim     { 0xff7d828c };
    const juce::Colour led         { 0xffff9a4d };
    const juce::Colour ledOff      { 0xff3a2e22 };
    const juce::Colour knobBody    { 0xff26282e };
    const juce::Colour knobRim     { 0xff4a4e58 };
    const juce::Colour lcdBg       { 0xff121317 };
    const juce::Colour lcdLine     { 0xff2b2e35 };
}

using ShowFn = std::function<void (const juce::String&, const juce::String&)>;

class TheOneLook : public juce::LookAndFeel_V4
{
public:
    TheOneLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

void drawLed (juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour c = Col::led);

// Drehregler im Stil des Originals (Label oben, Wert erscheint im LCD)
class KnobCtl : public juce::Component
{
public:
    KnobCtl (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& label, int size, ShowFn show, bool big = false);
    KnobCtl (const juce::String& label, int size, ShowFn show, juce::NormalisableRange<double> range, double def, const juce::String& fullName);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { announce(); }
    void mouseDown (const juce::MouseEvent&) override { announce(); }
    void mouseDrag (const juce::MouseEvent&) override { announce(); }
    void setLabel (const juce::String& s) { label = s; repaint(); }
    void setValueText (std::function<juce::String (double)> f) { valueText = f; }
    juce::Slider slider;
    juce::String fullName;
private:
    void announce();
    void init (int size, bool big);
    juce::String label;
    ShowFn show;
    std::function<juce::String (double)> valueText;
    std::unique_ptr<juce::SliderParameterAttachment> att;
    int labelH = 13;
};

class LedToggle : public juce::Component
{
public:
    LedToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& label);
    explicit LedToggle (const juce::String& label);   // ohne Parameter
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setState (bool b) { if (b != on) { on = b; repaint(); } }
    bool getState() const { return on; }
    std::function<void (bool)> onChange;
private:
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::RangedAudioParameter* param = nullptr;
    juce::String label;
    bool on = false;
public:
    juce::Colour acc = Col::led;
};

class SelectorCtl : public juce::Component
{
public:
    SelectorCtl (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, bool vertical, std::vector<juce::String> labels = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::RangedAudioParameter* param = nullptr;
    std::vector<juce::String> labels;
    bool vertical;
    int current = 0;
public:
    juce::Colour acc = Col::led;
};

class ComboCtl : public juce::Component
{
public:
    ComboCtl (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void resized() override { box.setBounds (getLocalBounds()); }
    juce::ComboBox box;
private:
    std::unique_ptr<juce::ComboBoxParameterAttachment> att;
};

class WheelCtl : public juce::Component
{
public:
    WheelCtl (const juce::String& label, bool springBack, std::function<void (float)> cb);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { set (e); }
    void mouseDrag (const juce::MouseEvent& e) override { set (e); }
    void mouseUp (const juce::MouseEvent&) override { if (spring) { value = 0.0f; cb (0.0f); repaint(); } }
private:
    void set (const juce::MouseEvent& e);
    juce::String label; bool spring; std::function<void (float)> cb; float value = 0.0f;
};

// Wellenform-Fenster eines VCOs (wie das OLED im Original)
class WaveView : public juce::Component
{
public:
    WaveView (juce::AudioProcessorValueTreeState& apvts, int osc);
    void paint (juce::Graphics&) override;
private:
    std::atomic<float>* wave; std::atomic<float>* angle; std::atomic<float>* pw; std::atomic<float>* mix;
};

class KeyboardView : public juce::MidiKeyboardComponent
{
public:
    KeyboardView (juce::MidiKeyboardState& s) : juce::MidiKeyboardComponent (s, juce::MidiKeyboardComponent::horizontalKeyboard) {}
protected:
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver, juce::Colour line, juce::Colour text) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver, juce::Colour fill) override;
};

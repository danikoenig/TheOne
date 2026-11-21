#include "Look.h"
#include <cmath>

using namespace juce;

static Font F (float h, bool bold = true) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

TheOneLook::TheOneLook()
{
    setColour (ComboBox::backgroundColourId, Colour (0xff0c0c0e));
    setColour (ComboBox::outlineColourId, Col::line.withAlpha (0.55f));
    setColour (ComboBox::textColourId, Col::text);
    setColour (ComboBox::arrowColourId, Col::led);
    setColour (PopupMenu::backgroundColourId, Colour (0xff101012));
    setColour (PopupMenu::textColourId, Col::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Col::led.darker (0.3f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextButton::buttonColourId, Colour (0xff1b1b1e));
    setColour (TextButton::textColourOffId, Col::text);
}

void drawLed (Graphics& g, Rectangle<float> r, bool on)
{
    const auto c = r.getCentre();
    if (on)
    {
        g.setGradientFill (ColourGradient (Col::led.withAlpha (0.45f), c.x, c.y, Col::led.withAlpha (0.0f), c.x + r.getWidth() * 1.1f, c.y, true));
        g.fillEllipse (r.expanded (r.getWidth() * 0.9f));
    }
    g.setColour (on ? Col::led : Col::ledOff);
    g.fillEllipse (r);
    g.setColour (Colours::white.withAlpha (on ? 0.45f : 0.12f));
    g.fillEllipse (r.reduced (r.getWidth() * 0.28f).translated (-r.getWidth() * 0.08f, -r.getWidth() * 0.1f));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (r, 0.8f);
}

void TheOneLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, Slider& s)
{
    const bool big = (bool) s.getProperties().getWithDefault ("big", false);
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float R = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Skalenstriche
    const int ticks = 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (float) i / (float) (ticks - 1) * (endAngle - startAngle);
        const float r0 = R - 1.0f, r1 = R - (i % 5 == 0 ? 4.5f : 3.2f);
        g.setColour (Col::text.withAlpha (i % 5 == 0 ? 0.75f : 0.4f));
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, 1.0f);
    }
    const float bodyR = R - (big ? 7.0f : 6.0f);
    auto body = Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (c);
    // Schatten
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (body.expanded (1.5f).translated (0.0f, 1.5f));
    // Koerper (Zylinder von oben)
    g.setGradientFill (ColourGradient (Colour (0xff3b3b40), body.getX(), body.getY(), Colour (0xff0a0a0b), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    // Rillen am Rand
    const int ridges = big ? 40 : 28;
    for (int i = 0; i < ridges; ++i)
    {
        const float a = (float) i / (float) ridges * MathConstants<float>::twoPi;
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawLine (c.x + std::sin (a) * bodyR * 0.9f, c.y - std::cos (a) * bodyR * 0.9f, c.x + std::sin (a) * bodyR, c.y - std::cos (a) * bodyR, 0.8f);
    }
    g.setColour (Col::knobRim.withAlpha (0.8f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    // Kappe
    auto cap = body.reduced (bodyR * 0.22f);
    g.setGradientFill (ColourGradient (Colour (0xff2b2b2f), cap.getX(), cap.getY(), Colour (0xff111113), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (Colours::white.withAlpha (s.isMouseOverOrDragging() ? 0.22f : 0.1f));
    g.drawEllipse (cap, 0.8f);
    // Zeiger
    const float r0 = bodyR * 0.30f, r1 = bodyR * 0.93f;
    g.setColour (Col::text);
    g.drawLine (c.x + std::sin (angle) * r0, c.y - std::cos (angle) * r0, c.x + std::sin (angle) * r1, c.y - std::cos (angle) * r1, big ? 2.8f : 2.2f);
}

void TheOneLook::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (Colour (0xff0c0c0e));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.hasKeyboardFocus (false) ? Col::led : Col::line.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    Path p; const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (Col::led);
    g.fillPath (p);
}
void TheOneLook::positionComboBoxText (ComboBox& box, Label& l) { l.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2); l.setFont (getComboBoxFont (box)); }
Font TheOneLook::getComboBoxFont (ComboBox&) { return F (11.0f); }
Font TheOneLook::getPopupMenuFont() { return F (13.0f, false); }

void TheOneLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? Colour (0xff35353a) : (hi ? Colour (0xff27272b) : Colour (0xff1b1b1e)));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Col::line.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}
void TheOneLook::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (Col::text);
    g.setFont (F (jmin (11.0f, (float) b.getHeight() - 6.0f)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

//==============================================================================
KnobCtl::KnobCtl (AudioProcessorValueTreeState& apvts, const String& paramId, const String& lbl, int size, ShowFn sh, bool big)
    : label (lbl), show (sh)
{
    auto* p = apvts.getParameter (paramId);
    fullName = p->getName (64);
    init (size, big);
    att = std::make_unique<SliderParameterAttachment> (*p, slider);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    valueText = [p] (double v) { return p->getText (p->convertTo0to1 ((float) v), 64); };
}

KnobCtl::KnobCtl (const String& lbl, int size, ShowFn sh, NormalisableRange<double> range, double def, const String& name)
    : fullName (name), label (lbl), show (sh)
{
    init (size, false);
    slider.setNormalisableRange (range);
    slider.setDoubleClickReturnValue (true, def);
    slider.setValue (def, dontSendNotification);
}

void KnobCtl::init (int size, bool big)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (180);
    slider.getProperties().set ("big", big);
    slider.addMouseListener (this, false);
    addAndMakeVisible (slider);
    labelH = 13;
    setSize (size, size + labelH);
}

void KnobCtl::announce()
{
    if (show) show (fullName, valueText ? valueText (slider.getValue()) : String (slider.getValue(), 2));
}

void KnobCtl::paint (Graphics& g)
{
    g.setColour (label.isEmpty() ? Col::textDim.withAlpha (0.4f) : Col::text);
    g.setFont (F (jmin (10.0f, (float) getWidth() * 0.2f + 4.5f)));
    g.drawText (label, 0, 0, getWidth(), labelH, Justification::centred, false);
}
void KnobCtl::resized() { slider.setBounds (0, labelH, getWidth(), getWidth()); }

//==============================================================================
LedToggle::LedToggle (AudioProcessorValueTreeState& apvts, const String& id, const String& lbl) : label (lbl)
{
    param = apvts.getParameter (id);
    att = std::make_unique<ParameterAttachment> (*param, [this] (float v) { on = v > 0.5f; repaint(); });
    att->sendInitialUpdate();
}
LedToggle::LedToggle (const String& lbl) : label (lbl) {}

void LedToggle::paint (Graphics& g)
{
    const float h = jmin (15.0f, (float) getHeight());
    auto body = Rectangle<float> (0.5f, ((float) getHeight() - h) * 0.5f + 0.5f, 26.0f, h - 1.0f);
    g.setColour (Colour (0xff0e0e10));
    g.fillRoundedRectangle (body.expanded (1.5f), 3.0f);
    g.setGradientFill (on ? ColourGradient (Col::led.brighter (0.25f), 0, body.getY(), Col::led.darker (0.5f), 0, body.getBottom(), false)
                          : ColourGradient (Colour (0xff2e2e33), 0, body.getY(), Colour (0xff17171a), 0, body.getBottom(), false));
    g.fillRoundedRectangle (body, 2.5f);
    if (on)
    {
        g.setColour (Col::led.withAlpha (0.25f));
        g.drawRoundedRectangle (body.expanded (2.5f), 4.0f, 2.0f);
    }
    g.setColour (Colours::white.withAlpha (on ? 0.35f : 0.08f));
    g.drawRoundedRectangle (body.reduced (0.5f), 2.5f, 0.8f);
    g.setColour (Col::text);
    g.setFont (F (9.5f));
    g.drawText (label, Rectangle<int> (32, 0, getWidth() - 32, getHeight()), Justification::centredLeft, false);
}
void LedToggle::mouseDown (const MouseEvent&)
{
    if (param != nullptr) att->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    else { on = ! on; repaint(); if (onChange) onChange (on); }
}

//==============================================================================
SelectorCtl::SelectorCtl (AudioProcessorValueTreeState& apvts, const String& id, bool vert, std::vector<String> lbls)
    : labels (std::move (lbls)), vertical (vert)
{
    param = apvts.getParameter (id);
    if (labels.empty())
        if (auto* cp = dynamic_cast<AudioParameterChoice*> (param))
            for (auto& s : cp->choices) labels.push_back (s);
    att = std::make_unique<ParameterAttachment> (*param, [this] (float v) { current = (int) std::lround (v); repaint(); });
    att->sendInitialUpdate();
}

void SelectorCtl::paint (Graphics& g)
{
    const int n = (int) labels.size();
    g.setFont (F (9.0f));
    for (int i = 0; i < n; ++i)
    {
        Rectangle<float> cell = vertical ? Rectangle<float> (0, (float) i * (float) getHeight() / (float) n, (float) getWidth(), (float) getHeight() / (float) n)
                                         : Rectangle<float> ((float) i * (float) getWidth() / (float) n, 0, (float) getWidth() / (float) n, (float) getHeight());
        const float d = 7.0f;
        if (vertical)
        {
            drawLed (g, Rectangle<float> (d, d).withCentre ({ cell.getX() + 5.0f, cell.getCentreY() }), i == current);
            g.setColour (i == current ? Col::text : Col::text.withAlpha (0.78f));
            g.drawText (labels[(size_t) i], cell.withTrimmedLeft (13.0f).toNearestInt(), Justification::centredLeft, false);
        }
        else
        {
            drawLed (g, Rectangle<float> (d, d).withCentre ({ cell.getCentreX(), cell.getY() + d * 0.5f + 1.0f }), i == current);
            g.setColour (i == current ? Col::text : Col::text.withAlpha (0.78f));
            g.drawText (labels[(size_t) i], cell.withTrimmedTop (d + 1.0f).toNearestInt(), Justification::centredTop, false);
        }
    }
}
void SelectorCtl::mouseDown (const MouseEvent& e)
{
    const int n = (int) labels.size();
    const int i = vertical ? (int) (e.position.y / ((float) getHeight() / (float) n)) : (int) (e.position.x / ((float) getWidth() / (float) n));
    att->setValueAsCompleteGesture ((float) jlimit (0, n - 1, i));
}

//==============================================================================
ComboCtl::ComboCtl (AudioProcessorValueTreeState& apvts, const String& id)
{
    auto* p = apvts.getParameter (id);
    if (auto* cp = dynamic_cast<AudioParameterChoice*> (p))
    {
        int i = 1;
        for (auto& s : cp->choices) box.addItem (s, i++);
    }
    addAndMakeVisible (box);
    att = std::make_unique<ComboBoxParameterAttachment> (*p, box);
}

//==============================================================================
WheelCtl::WheelCtl (const String& l, bool springBack, std::function<void (float)> c) : label (l), spring (springBack), cb (c) {}

void WheelCtl::set (const MouseEvent& e)
{
    const float t = jlimit (0.0f, 1.0f, 1.0f - (e.position.y - 6.0f) / ((float) getHeight() - 12.0f));
    value = spring ? t * 2.0f - 1.0f : t;
    cb (value);
    repaint();
}

void WheelCtl::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto wheel = r.withTrimmedBottom (14.0f);
    g.setColour (Colours::black);
    g.fillRoundedRectangle (wheel.expanded (2.0f), 6.0f);
    g.setGradientFill (ColourGradient (Colour (0xff0b0b0c), wheel.getX(), 0, Colour (0xff0b0b0c), wheel.getRight(), 0, false));
    g.fillRoundedRectangle (wheel, 5.0f);
    // Walze mit Rillen, Position verschiebt die Rillen
    g.saveState();
    g.reduceClipRegion (wheel.toNearestInt());
    const float pos = spring ? (value + 1.0f) * 0.5f : value;
    for (int i = -2; i < 30; ++i)
    {
        const float y = wheel.getBottom() - (float) i * 7.0f + pos * 28.0f - 14.0f;
        const float k = (y - wheel.getY()) / wheel.getHeight();
        g.setColour (Colour (0xff4a4a50).withAlpha (0.55f + 0.4f * std::sin (k * MathConstants<float>::pi)));
        g.fillRect (wheel.getX() + 4.0f, y, wheel.getWidth() - 8.0f, 2.0f);
    }
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.8f), 0, wheel.getY(), Colours::transparentBlack, 0, wheel.getY() + 22.0f, false));
    g.fillRect (wheel);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.8f), 0, wheel.getBottom(), Colours::transparentBlack, 0, wheel.getBottom() - 22.0f, false));
    g.fillRect (wheel);
    g.restoreState();
    g.setColour (Col::text);
    g.setFont (F (9.0f));
    g.drawText (label, getLocalBounds().removeFromBottom (14), Justification::centred);
}

//==============================================================================
WaveView::WaveView (AudioProcessorValueTreeState& apvts, int osc)
{
    const String s = "osc" + String (osc + 1) + "_";
    wave = apvts.getRawParameterValue (s + "wave");
    angle = apvts.getRawParameterValue (s + "angle");
    pw = apvts.getRawParameterValue (s + "pw");
    mix = apvts.getRawParameterValue (s + "mix");
}

void WaveView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff020203));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Col::line.withAlpha (0.45f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    auto a = r.reduced (8.0f, 8.0f);
    const float ang = angle->load(), mx = mix->load(), duty = 0.02f + 0.96f * pw->load();
    const bool saw = wave->load() > 0.5f;
    const float s = saw ? jlimit (0.02f, 0.98f, 1.0f - (6e-6f + ang * 1e-3f) * 120.0f) : jlimit (0.02f, 0.98f, 1.0f - ang);
    Path p;
    const int N = 200;
    for (int i = 0; i <= N; ++i)
    {
        const float ph = (float) i / (float) N;
        float ph2 = ph * 2.0f;      // zwei Perioden zeigen
        ph2 -= std::floor (ph2);
        const float tri = ph2 < s ? -1.0f + 2.0f * ph2 / s : 1.0f - 2.0f * (ph2 - s) / (1.0f - s);
        const float pul = ph2 < duty ? 1.0f : -1.0f;
        const float v = tri * (1.0f - mx) + pul * mx;
        const float x = a.getX() + ph * a.getWidth(), y = a.getCentreY() - v * a.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (Colours::white.withAlpha (0.18f));
    g.strokePath (p, PathStrokeType (3.5f, PathStrokeType::curved));
    g.setColour (Colours::white);
    g.strokePath (p, PathStrokeType (1.4f, PathStrokeType::curved));
}

//==============================================================================
void KeyboardView::drawWhiteNote (int, Graphics& g, Rectangle<float> a, bool isDown, bool isOver, Colour lineColour, Colour)
{
    g.setGradientFill (isDown ? ColourGradient (Col::led.brighter (0.2f), 0, a.getY(), Col::led.darker (0.35f), 0, a.getBottom(), false)
                              : ColourGradient (Colour (0xfffbfbf8), 0, a.getY(), isOver ? Colour (0xffe9e9e4) : Colour (0xffdcdcd6), 0, a.getBottom(), false));
    g.fillRect (a);
    g.setColour (lineColour);
    g.fillRect (a.withWidth (1.0f));
    g.setColour (Colours::black.withAlpha (0.18f));
    g.fillRect (a.withTop (a.getBottom() - 4.0f));
}
void KeyboardView::drawBlackNote (int, Graphics& g, Rectangle<float> a, bool isDown, bool isOver, Colour)
{
    auto k = a.reduced (0.5f, 0.0f);
    g.setColour (Colours::black);
    g.fillRect (k.expanded (1.0f, 0.0f).withTrimmedTop (-1.0f));
    g.setGradientFill (isDown ? ColourGradient (Col::led, 0, k.getY(), Col::led.darker (0.5f), 0, k.getBottom(), false)
                              : ColourGradient (isOver ? Colour (0xff3b3b40) : Colour (0xff2a2a2e), 0, k.getY(), Colour (0xff09090a), 0, k.getBottom(), false));
    g.fillRect (k);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.fillRect (k.withTrimmedBottom (k.getHeight() * 0.8f).reduced (3.0f, 0.0f));
}

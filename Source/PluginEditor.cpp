#include "PluginEditor.h"
#include <cmath>

using namespace juce;
using namespace TO;

static Font F (float h, bool bold = true) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

//==============================================================================
// LCD in der Mitte
class LcdView : public Component
{
public:
    explicit LcdView (TheOneProcessor& p) : proc (p) {}
    void setReadout (const String& name, const String& value) { rName = name; rValue = value; repaint(); }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff121317));
        g.fillRoundedRectangle (r, 6.0f);
        auto s = r.reduced (6.0f);
        g.setGradientFill (ColourGradient (Colour (0xff121317), 0, s.getY(), Colour (0xff0d0e11), 0, s.getBottom(), false));
        g.fillRoundedRectangle (s, 3.0f);
        g.setColour (Col::lcdLine);
        g.drawRoundedRectangle (s, 3.0f, 1.0f);
        auto a = s.reduced (12.0f, 10.0f);

        // Kopf
        g.setColour (Col::text);
        g.setFont (F (31.0f).withExtraKerningFactor (0.16f));
        g.drawText ("THE ONE", a.withHeight (36.0f).toNearestInt(), Justification::centredLeft, false);
        g.setColour (Col::textDim);
        g.setFont (F (9.5f).withExtraKerningFactor (0.12f));
        g.drawText ("POLYPHONIC SYNTHESIZER  -  by Dani Koenig", Rectangle<int> ((int) a.getX(), (int) a.getY() + 38, (int) a.getWidth(), 14), Justification::centredLeft, false);
        // rechts: Tempo und Stimmen
        g.setColour (Col::text);
        g.setFont (F (12.0f));
        g.drawText (String (proc.hostBpm.load(), 1) + " BPM", Rectangle<int> ((int) a.getRight() - 120, (int) a.getY() + 2, 120, 16), Justification::centredRight, false);
        g.setColour (Col::textDim);
        g.drawText ("VOICES " + String (proc.voicesInUse.load()) + " / " + String ((int) std::lround (proc.apvts.getRawParameterValue ("v_poly")->load())),
                    Rectangle<int> ((int) a.getRight() - 120, (int) a.getY() + 20, 120, 16), Justification::centredRight, false);

        g.setColour (Col::lcdLine);
        g.drawLine (a.getX(), a.getY() + 60.0f, a.getRight(), a.getY() + 60.0f);

        // Anzeige des zuletzt bedienten Reglers (roter Balken wie im Original)
        auto bar = Rectangle<float> (a.getX(), a.getY() + 70.0f, a.getWidth(), 22.0f);
        g.setGradientFill (ColourGradient (Col::led.darker (0.15f), 0, bar.getY(), Col::led.darker (0.6f), 0, bar.getBottom(), false));
        g.fillRect (bar);
        g.setColour (Colours::white);
        g.setFont (F (12.5f).withExtraKerningFactor (0.06f));
        g.drawText (rName.isEmpty() ? String ("INIT") : rName.toUpperCase(), bar.reduced (8.0f, 0.0f).toNearestInt(), Justification::centredLeft, false);
        g.setFont (F (34.0f));
        g.drawText (rValue.isEmpty() ? String ("-") : rValue, Rectangle<int> ((int) a.getX(), (int) a.getY() + 98, (int) a.getWidth(), 42), Justification::centred, false);

        // Oszilloskop
        auto sc = Rectangle<float> (a.getX(), a.getY() + 150.0f, a.getWidth(), 100.0f);
        g.setColour (Colour (0xff0d0e11));
        g.fillRect (sc);
        g.setColour (Col::lcdLine);
        g.drawRect (sc, 1.0f);
        g.drawHorizontalLine ((int) sc.getCentreY(), sc.getX(), sc.getRight());
        {
            const int W = 1024, start = proc.scopeW.load();
            const int N = 512;
            // steigende Nulldurchgang suchen
            int off = 0;
            for (int i = 1; i < 400; ++i)
            {
                const float a0 = proc.scope[(size_t) ((start + i - 1) & (W - 1))].load();
                const float a1 = proc.scope[(size_t) ((start + i) & (W - 1))].load();
                if (a0 <= 0.0f && a1 > 0.0f) { off = i; break; }
            }
            Path p;
            for (int i = 0; i < N; ++i)
            {
                const float v = proc.scope[(size_t) ((start + off + i) & (W - 1))].load();
                const float x = sc.getX() + sc.getWidth() * (float) i / (float) (N - 1);
                const float y = sc.getCentreY() - jlimit (-1.0f, 1.0f, v * 1.6f) * sc.getHeight() * 0.46f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (Col::led.withAlpha (0.25f));
            g.strokePath (p, PathStrokeType (3.0f));
            g.setColour (Colour (0xffffe2cc));
            g.strokePath (p, PathStrokeType (1.2f));
        }

        // Akkord
        g.setColour (Col::textDim);
        g.setFont (F (9.5f).withExtraKerningFactor (0.1f));
        const bool chordOn = proc.apvts.getRawParameterValue ("v_chord")->load() > 0.5f;
        auto cr = Rectangle<int> ((int) a.getX(), (int) a.getY() + 262, (int) a.getWidth(), 14);
        g.drawText ("CHORD MEMORY", cr, Justification::centredLeft, false);
        g.setColour (chordOn ? Col::led : Col::textDim);
        g.drawText (chordOn ? "ON" : "OFF", cr, Justification::centredRight, false);
        String s2;
        const int n = proc.chordCount.load();
        for (int i = 0; i < n; ++i)
        {
            const int v = proc.chordNotes[(size_t) i].load();
            s2 += (i == 0 && v == 0 ? String ("0") : (v > 0 ? "+" + String (v) : String (v))) + "  ";
        }
        g.setColour (Col::text);
        g.setFont (F (13.0f));
        if (proc.learnActive.load())
        {
            g.setColour (Col::led);
            g.drawText ("LEARN - " + String (proc.learnCount.load()) + " NOTES", Rectangle<int> (cr.getX(), cr.getY() + 16, cr.getWidth(), 18), Justification::centredLeft, false);
        }
        else g.drawText (s2.trim(), Rectangle<int> (cr.getX(), cr.getY() + 16, cr.getWidth(), 18), Justification::centredLeft, false);
    }

private:
    TheOneProcessor& proc;
    String rName, rValue;
};

//==============================================================================
// Startfenster (wie "Getting Started" bei Falcon), abschaltbar
static std::unique_ptr<PropertiesFile> openSettings()
{
    PropertiesFile::Options o;
    o.applicationName = "The One";
    o.folderName = "Dani Koenig";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return std::make_unique<PropertiesFile> (o);
}

class StartScreen : public Component
{
public:
    std::function<void (int)> onPick;
    std::function<void()> onClose;

    StartScreen (TheOneProcessor& p) : proc (p), showToggle ("SHOW AT STARTUP")
    {
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            auto* b = new TextButton (proc.getProgramName (i));
            rows.add (b); addAndMakeVisible (b);
            b->setColour (TextButton::buttonColourId, Colour (0xff26282e));
            b->setColour (TextButton::buttonOnColourId, Colour (0xff3a3e47));
            b->onClick = [this, i] { if (onPick) onPick (i); };
        }
        addAndMakeVisible (showToggle);
        auto s = openSettings();
        showToggle.setState (s->getBoolValue ("showAtStartup", true));
        showToggle.onChange = [] (bool b) { auto st = openSettings(); st->setValue ("showAtStartup", b); st->saveIfNeeded(); };
        addAndMakeVisible (closeBtn);
        closeBtn.setButtonText ("CLOSE");
        closeBtn.onClick = [this] { if (onClose) onClose(); };
    }
    static bool shouldShow() { return openSettings()->getBoolValue ("showAtStartup", true); }

    Rectangle<int> card() const { const int h = 150 + rows.size() * 50; return Rectangle<int> (0, 0, 620, h).withCentre (getLocalBounds().getCentre().translated (0, -40)); }
    void resized() override
    {
        auto c = card();
        for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds (c.getX() + 30, c.getY() + 80 + i * 50, c.getWidth() - 60, 44);
        showToggle.setBounds (c.getX() + 30, c.getBottom() - 50, 200, 20);
        closeBtn.setBounds (c.getRight() - 140, c.getBottom() - 56, 110, 30);
    }
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black.withAlpha (0.62f));
        auto c = card().toFloat();
        g.setColour (Colour (0xff1d1f24)); g.fillRoundedRectangle (c, 10.0f);
        g.setColour (Colour (0xff2b2e35)); g.drawRoundedRectangle (c, 10.0f, 1.5f);
        g.setColour (Col::text);
        g.setFont (F (26.0f));
        g.drawText ("Getting Started", c.withHeight (60.0f).withTrimmedLeft (30.0f).translated (0, 12).toNearestInt(), Justification::centredLeft, false);
        g.setColour (Col::textDim);
        g.setFont (F (12.0f, false));
        g.drawText ("Pick a starting point. Everything stays editable.", c.withHeight (20.0f).withTrimmedLeft (30.0f).translated (0, 54).toNearestInt(), Justification::centredLeft, false);
    }
    void mouseDown (const MouseEvent& e) override { if (! card().contains (e.getPosition()) && onClose) onClose(); }

private:
    TheOneProcessor& proc;
    OwnedArray<TextButton> rows;
    LedToggle showToggle;
    TextButton closeBtn;
};

//==============================================================================
// Vertikaler Fader (Sequencer), Wert erscheint im LCD
class FaderCtl : public Component
{
public:
    FaderCtl (AudioProcessorValueTreeState& apvts, const String& id, ShowFn s, bool notched, Colour a) : show (s), notch (notched), acc (a)
    {
        param = apvts.getParameter (id);
        att = std::make_unique<ParameterAttachment> (*param, [this] (float) { repaint(); });
        att->sendInitialUpdate();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto track = Rectangle<float> (r.getCentreX() - 3.0f, r.getY() + 6.0f, 6.0f, r.getHeight() - 12.0f);
        g.setColour (Colour (0xff121317)); g.fillRoundedRectangle (track, 3.0f);
        g.setColour (Colour (0xff2b2e35)); g.drawRoundedRectangle (track, 3.0f, 1.0f);
        if (notch)
        {
            const int steps = (int) std::lround (param->getNormalisableRange().end - param->getNormalisableRange().start);
            g.setColour (Col::textDim.withAlpha (0.5f));
            for (int i = 0; i <= steps; ++i) { const float y = track.getBottom() - track.getHeight() * (float) i / (float) steps; g.fillRect (track.getRight() + 2.0f, y - 0.5f, 3.0f, 1.0f); }
        }
        else
        {
            g.setColour (Col::textDim.withAlpha (0.6f));
            g.fillRect (track.getRight() + 2.0f, track.getCentreY() - 0.5f, 6.0f, 1.0f);
        }
        const float v = param->getValue();
        const float y = track.getBottom() - v * track.getHeight();
        const float cw = jmin (20.0f, r.getWidth() - 4.0f);
        auto cap = Rectangle<float> (r.getCentreX() - cw * 0.5f, y - 5.0f, cw, 10.0f);
        g.setGradientFill (ColourGradient (Colour (0xff3a3e47), 0, cap.getY(), Colour (0xff1d1f24), 0, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (acc);
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 6.0f, 2.0f);
    }
    void mouseDown (const MouseEvent& e) override { att->beginGesture(); setFrom (e); }
    void mouseDrag (const MouseEvent& e) override { setFrom (e); }
    void mouseUp (const MouseEvent&) override { att->endGesture(); }
    void mouseDoubleClick (const MouseEvent&) override { att->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue())); announce(); }
private:
    void setFrom (const MouseEvent& e)
    {
        const float t = jlimit (0.0f, 1.0f, 1.0f - (e.position.y - 6.0f) / ((float) getHeight() - 12.0f));
        att->setValueAsPartOfGesture (param->convertFrom0to1 (t));
        announce();
    }
    void announce() { if (show) show (param->getName (64), param->getCurrentValueAsText()); }
    ShowFn show;
    bool notch;
    Colour acc;
    RangedAudioParameter* param = nullptr;
    std::unique_ptr<ParameterAttachment> att;
};

//==============================================================================
// Step-Taste: Klick schaltet ON -> MUTE -> SKIP
class StepModeBtn : public Component
{
public:
    StepModeBtn (AudioProcessorValueTreeState& apvts, const String& id, ShowFn s, Colour a) : show (s), acc (a)
    {
        param = apvts.getParameter (id);
        att = std::make_unique<ParameterAttachment> (*param, [this] (float v) { mode = (int) std::lround (v); repaint(); });
        att->sendInitialUpdate();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        if (mode == 0) { g.setColour (acc); g.fillRoundedRectangle (r, 3.0f); }
        else { g.setColour (Colour (0xff121317)); g.fillRoundedRectangle (r, 3.0f); g.setColour (mode == 1 ? acc.withAlpha (0.8f) : Col::textDim.withAlpha (0.5f)); g.drawRoundedRectangle (r, 3.0f, 1.0f); }
        g.setColour (mode == 0 ? Colour (0xff17181c) : (mode == 1 ? acc : Col::textDim));
        g.setFont (F (7.5f));
        g.drawText (mode == 0 ? "ON" : (mode == 1 ? "MUTE" : "SKIP"), getLocalBounds(), Justification::centred, false);
    }
    void mouseDown (const MouseEvent&) override
    {
        att->setValueAsCompleteGesture ((float) ((mode + 1) % 3));
        if (show) show (param->getName (64), param->getCurrentValueAsText());
    }
private:
    ShowFn show;
    Colour acc;
    int mode = 0;
    RangedAudioParameter* param = nullptr;
    std::unique_ptr<ParameterAttachment> att;
};

//==============================================================================
struct Mod { String title; Rectangle<int> r; Colour acc; };
namespace Acc
{
    const Colour osc   { 0xffff9a4d };   // Orange
    const Colour lfo   { 0xffa78bfa };   // Violett
    const Colour filt  { 0xff4dd0e1 };   // Tuerkis
    const Colour env   { 0xff5ee6a8 };   // Gruen
    const Colour fx    { 0xff4fb8ee };   // Blau
    const Colour voice { 0xffffd166 };   // Gelb
    const Colour out   { 0xff9fa8da };   // Lavendel
    const Colour seq   { 0xfff0485a };   // Rot
}
struct Cap { String text; int x, y; float size; bool center; };

class TheOneEditor::Content : public Component, private Timer
{
public:
    explicit Content (TheOneProcessor& p)
        : proc (p), apvts (p.apvts), lcd (p), keys (p.keyboardState),
          pitchWheel ("PITCH", true, [this] (float v) { proc.uiBend.store (v); }),
          modWheel ("MOD", false, [this] (float v) { proc.uiMod.store (v); })
    {
        setLookAndFeel (&look);
        show = [this] (const String& n, const String& v) { lcd.setReadout (n, v); };
        buildLeft();
        buildCenter();
        buildRight();
        addAndMakeVisible (lcd);
        lcd.setBounds (838, 26, 394, 310);
        addAndMakeVisible (pitchWheel); addAndMakeVisible (modWheel);
        pitchWheel.setBounds (62, 750, 54, 124);
        modWheel.setBounds (126, 750, 54, 124);
        addAndMakeVisible (keys);
        keys.setAvailableRange (36, 96);
        keys.setScrollButtonsVisible (false);
        keys.setKeyWidth (1960.0f / 36.0f);
        keys.setVelocity (0.8f, true);
        keys.setBounds (190, 752, 1960, 122);
        keys.setWantsKeyboardFocus (false);
        startTimerHz (30);
        start = std::make_unique<StartScreen> (proc);
        addChildComponent (*start);
        start->setBounds (0, 0, kW0, kH0);
        start->onPick = [this] (int i) { proc.setCurrentProgram (i); if (progBox) progBox->setSelectedId (i + 1, dontSendNotification); showStart (false); };
        start->onClose = [this] { showStart (false); };
        if (StartScreen::shouldShow()) showStart (true);
    }
    void showStart (bool on) { start->setVisible (on); if (on) start->toFront (false); }
    ~Content() override { setLookAndFeel (nullptr); }

    //------------------------------------------------------------------ Hilfen
    int ox = 0, oy = 0;
    Colour cur = Acc::osc;
    void M (const String& title, int x, int y, int w, int h, Colour a) { cur = a; mods.push_back ({ title, { x, y, w, h }, a }); ox = x; oy = y; }
    void Ctext (const String& t, int x, int y, float size = 9.0f, bool center = false) { caps.push_back ({ t, ox + x, oy + y, size, center }); }

    KnobCtl* K (const String& id, const String& label, int x, int y, int size, bool big = false)
    {
        auto* k = new KnobCtl (apvts, id, label, size, show, big);
        k->slider.setColour (Slider::rotarySliderFillColourId, cur);
        owned.add (k); addAndMakeVisible (k);
        k->setTopLeftPosition (ox + x, oy + y);
        return k;
    }
    SelectorCtl* S (const String& id, int x, int y, int w, int h, bool vertical, std::vector<String> labels = {})
    {
        auto* s = new SelectorCtl (apvts, id, vertical, std::move (labels));
        s->acc = cur;
        owned.add (s); addAndMakeVisible (s);
        s->setBounds (ox + x, oy + y, w, h);
        return s;
    }
    LedToggle* T (const String& id, const String& label, int x, int y, int w = 70, int h = 16)
    {
        auto* t = new LedToggle (apvts, id, label);
        t->acc = cur;
        owned.add (t); addAndMakeVisible (t);
        t->setBounds (ox + x, oy + y, w, h);
        return t;
    }
    ComboCtl* Cb (const String& id, int x, int y, int w, int h = 20)
    {
        auto* c = new ComboCtl (apvts, id);
        owned.add (c); addAndMakeVisible (c);
        c->setBounds (ox + x, oy + y, w, h);
        return c;
    }

    //------------------------------------------------------------------ Aufbau
    void buildLeft()
    {
        // LFOs
        for (int i = 0; i < 4; ++i)
        {
            const String s = "lfo" + String (i + 1) + "_";
            M ("LFO " + String (i + 1), 50 + (int) std::lround (i * 192.67), 14, 182, 166, Acc::lfo);
            S (s + "wave", 8, 26, 76, 50, true, { "TRI/SIN", "PULSE", "SAW/RAMP", "S&H" });
            K (s + "rate", "RATE", 86, 24, 54);
            S (s + "timing", 146, 26, 34, 36, true, { "STR", "DOT", "TRI" });
            S (s + "range", 146, 70, 34, 48, true, { "SLOW", "LOW", "MED", "HIGH" });
            K (s + "var", "VAR", 8, 88, 40);
            K (s + "delay", "DELAY", 54, 88, 40);
            K (s + "fade", "FADE", 100, 88, 40);
            T (s + "sync", "SYNC", 4, 146, 58, 15);
            T (s + "pol", "BIPOLAR", 62, 146, 60, 15);
            T (s + "reset", "RST", 122, 146, 60, 15);
        }
        // Oszillatoren
        for (int i = 0; i < 3; ++i)
        {
            const String s = "osc" + String (i + 1) + "_";
            M ("OSCILLATOR " + String (i + 1), 50 + i * 204, 190, 194, 292, Acc::osc);
            auto* wv = new WaveView (apvts, i); owned.add (wv); addAndMakeVisible (wv); wv->setBounds (ox + 8, oy + 24, 178, 60); waves.add (wv);
            K (s + "freq", "FREQ", 8, 92, 52);
            K (s + "beat", "BEAT", 70, 92, 52);
            K (s + "angle", "WAVE ANGLE", 132, 92, 52);
            K (s + "pw", "PULSE WIDTH", 8, 164, 52);
            K (s + "mix", "MIX", 70, 164, 52);
            K (s + "track", "TRACK", 132, 164, 52);
            S (s + "oct", 8, 240, 176, 20, false);
            S (s + "wave", 8, 266, 54, 20, false);
            if (i > 0) T (s + "sync", "SYNC", 66, 268, 58, 15);
            T (s + "reset", "RESET", 124, 268, 62, 15);
        }
        M ("OSC COMMON", 662, 190, 148, 292, Acc::osc);
        K ("c_lfo1", "LFO1>PIT", 10, 24, 48);
        K ("c_mepitch", "EG>PIT", 88, 24, 48);
        K ("c_lfo3ang", "LFO3>ANG", 10, 92, 48);
        K ("c_lfo3pw", "LFO3>PW", 88, 92, 48);
        K ("c_fm", "FM AMT", 10, 160, 48);
        K ("c_bend", "BEND", 88, 160, 48);
        Ctext ("RING MOD SOURCES", 10, 222);
        S ("c_ringsrc", 8, 234, 132, 20, false);
        Ctext ("FM ROUTE", 10, 257);
        S ("c_fmroute", 8, 269, 132, 20, false);

        // Noise
        M ("NOISE", 50, 492, 190, 242, Acc::osc);
        K ("n_att", "ATTACK", 10, 24, 48);
        K ("n_rel", "RELEASE", 70, 24, 48);
        K ("n_mix", "COLOR", 130, 24, 48);
        Ctext ("COLOR", 10, 102);
        S ("n_pair", 10, 116, 110, 44, true, { "RED+WHITE", "RED+PURPLE", "WHITE+PURPLE" });
        T ("n_sus", "SUSTAIN", 10, 176, 80, 16);

        // Stimmen
        M ("VOICE ALLOCATION", 250, 492, 560, 134, Acc::voice);
        K ("v_poly", "POLY", 14, 24, 48);
        K ("v_ucount", "UNISON", 76, 24, 48);
        K ("v_detune", "DETUNE", 138, 24, 48);
        K ("v_spread", "SPREAD", 200, 24, 48);
        K ("v_glide", "GLIDE", 262, 24, 48);
        T ("v_mono", "MONO", 340, 30, 66);
        T ("v_unison", "UNISON", 340, 54, 76);
        T ("v_glideon", "GLIDE ON", 340, 78, 80);
        T ("v_multi", "MULTI TRIG", 430, 30, 96);

        // Akkord
        M ("CHORD / HOLD", 250, 636, 560, 98, Acc::voice);
        T ("v_hold", "HOLD", 14, 34, 66, 16);
        T ("v_chord", "CHORD", 100, 34, 72, 16);
        T ("v_learn", "LEARN", 192, 34, 72, 16);
        T ("v_silent", "SILENT LEARN", 284, 34, 110, 16);
        auto* clr = new TextButton ("CLEAR");
        owned.add (clr); addAndMakeVisible (clr);
        clr->setBounds (ox + 420, oy + 31, 70, 22);
        clr->onClick = [this] { proc.clearChord(); };
        Ctext ("Learn: Noten spielen, Learn aus, Chord an. Jede Taste spielt dann den Akkord, starr transponiert.", 14, 66, 9.0f);
    }

    void buildCenter()
    {
        M ("DISPLAY", 830, 14, 410, 330, Acc::osc);
        for (int i = 0; i < 3; ++i)
        {
            const String s = "fx" + String (i + 1) + "_";
            M ("EFFECT " + String (i + 1), 830, 354 + i * 130, 410, 120, Acc::fx);
            auto* cb = Cb (s + "type", 10, 22, 150, 20);
            (void) cb;
            T (s + "sync", "SYNC", 172, 24, 56, 16);
            FxUI u;
            u.knobs[0] = K (s + "mix", "MIX", 10, 50, 50);
            for (int k = 0; k < 5; ++k) u.knobs[k + 1] = K (s + "p" + String (k + 1), "", 10 + (k + 1) * 66, 50, 50);
            u.typeParam = apvts.getRawParameterValue (s + "type");
            u.syncParam = apvts.getRawParameterValue (s + "sync");
            fx.push_back (u);
            for (int k = 0; k < 6; ++k)
            {
                const int slot = i, idx = k;
                if (k >= 1)
                    u.knobs[k]->setValueText ([this, slot, idx] (double x) { return fxText (slot, idx - 1, (float) x); });
            }
        }
        // Zaehler fuer Beschriftung
        for (int i = 0; i < 3; ++i) { auto& u = fx[(size_t) i]; for (int k = 1; k < 6; ++k) { const int slot = i, idx = k; u.knobs[k]->setValueText ([this, slot, idx] (double x) { return fxText (slot, idx - 1, (float) x); }); } }
    }

    String fxText (int slot, int k, float x)
    {
        auto& u = fx[(size_t) slot];
        const int type = (int) std::lround (u.typeParam->load());
        const bool sync = u.syncParam->load() > 0.5f;
        char b[48];
        const bool delayT = type >= FX_DELAY && type <= FX_ECHO;
        const bool modT = (type >= FX_CHORUS && type <= FX_PHASER12);
        if (k == 0)
        {
            if (delayT)
            {
                if (sync) return divName (divIndex (x));
                const float t = 0.01f * std::pow (200.0f, x);
                if (t < 1.0f) snprintf (b, sizeof (b), "%.0f ms", t * 1000.0f); else snprintf (b, sizeof (b), "%.2f s", t);
                return b;
            }
            if (modT)
            {
                if (sync) return divName (divIndex (x));
                snprintf (b, sizeof (b), "%.2f Hz", 0.1f * std::pow (100.0f, x)); return b;
            }
            if (type == FX_CRUSHER) { snprintf (b, sizeof (b), "%.1f BIT", 24.0f - 22.0f * x); return b; }
            if (type == FX_RESONATOR) { snprintf (b, sizeof (b), "%.0f Hz", 60.0f * std::pow (130.0f, x)); return b; }
        }
        if (type == FX_RESONATOR && (k == 1 || k == 2)) { snprintf (b, sizeof (b), "%.0f Hz", 60.0f * std::pow (130.0f, x)); return b; }
        snprintf (b, sizeof (b), "%.0f %%", x * 100.0f);
        return b;
    }

    void buildRight()
    {
        // Mixer
        M ("MIXER", 1260, 14, 300, 146, Acc::osc);
        const char* nm[6] = { "OSC 1", "OSC 2", "OSC 3", "RING", "NOISE", "SUB" };
        for (int i = 0; i < 6; ++i)
        {
            K ("mix_lvl" + String (i), nm[i], 8 + i * 49, 18, 36);
            T ("mix_svf" + String (i), "S", 5 + i * 49, 66, 44, 14);
            T ("mix_lad" + String (i), "L", 5 + i * 49, 84, 44, 14);
        }
        Ctext ("S = SVF   L = LADDER", 150, 104);
        Ctext ("SUB OCTAVE", 8, 104);
        S ("sub_oct", 8, 116, 80, 24, false);
        Ctext ("SUB WAVE", 100, 104);
        S ("sub_wave", 100, 116, 110, 24, false);
        // Zusatz-Modulation
        M ("EXTRA MODULATION", 1260, 170, 300, 94, Acc::lfo);
        Ctext ("LFO 4 DESTINATION", 10, 18);
        Cb ("x_l4dest", 10, 30, 130, 18);
        K ("x_l4amt", "AMT", 152, 12, 30);
        Ctext ("MOD EG DESTINATION", 10, 54);
        Cb ("x_medest", 10, 66, 130, 18);
        K ("x_meamt", "AMT", 152, 50, 30);
        K ("c_moddepth", "WHEEL", 230, 30, 40);

        // SVF
        M ("STATE VARIABLE FILTER", 1570, 14, 190, 250, Acc::filt);
        svfCut = K ("svf_cut", "CUTOFF", 10, 24, 62, true);
        K ("svf_res", "RESO", 80, 30, 46);
        K ("svf_space", "SPACING", 134, 30, 46);
        K ("svf_key", "KEY TRK", 10, 106, 46);
        S ("svf_mode", 70, 108, 70, 52, true);
        T ("svf_ser", "SERIES", 10, 172, 80, 16);
        Ctext ("FM SOURCE", 10, 196);
        S ("svf_fmsrc", 10, 208, 168, 20, false, { "OSC 1", "OSC 2", "OSC 3" });

        // Ladder
        M ("LADDER FILTER", 1770, 14, 190, 250, Acc::filt);
        ladCut = K ("lad_cut", "CUTOFF", 10, 24, 62, true);
        K ("lad_res", "RESO", 80, 30, 46);
        K ("lad_key", "KEY TRK", 134, 30, 46);
        Ctext ("MODE", 10, 104);
        S ("lad_mode", 10, 116, 100, 20, false);
        Ctext ("SLOPE dB", 10, 142);
        S ("lad_slope", 10, 154, 170, 20, false, { "6", "12", "18", "24" });
        Ctext ("FM SOURCE", 10, 196);
        S ("lad_fmsrc", 10, 208, 168, 20, false, { "OSC 1", "OSC 2", "OSC 3" });

        // Filter gemeinsam
        M ("FILTER COMMON", 1970, 14, 180, 250, Acc::filt);
        fEg = new KnobCtl ("EG AMT", 46, show, NormalisableRange<double> (-1.0, 1.0), 0.0, "Filter EG Amount");
        fLfo = new KnobCtl ("LFO 2", 46, show, NormalisableRange<double> (0.0, 1.0), 0.0, "Filter LFO 2 Amount");
        fFm = new KnobCtl ("FM AMT", 46, show, NormalisableRange<double> (0.0, 1.0), 0.0, "Filter FM Amount");
        KnobCtl* fk[3] = { fEg, fLfo, fFm };
        for (int i = 0; i < 3; ++i)
        {
            fk[i]->slider.setColour (Slider::rotarySliderFillColourId, cur);
            owned.add (fk[i]); addAndMakeVisible (fk[i]);
            fk[i]->setTopLeftPosition (ox + 8 + i * 56, oy + 24);
            fk[i]->setValueText ([i] (double v) { return String (v * 10.0, 1); });
            fk[i]->slider.onDragStart = [this, i] { gestureFilter (i, true); };
            fk[i]->slider.onDragEnd = [this, i] { gestureFilter (i, false); };
            fk[i]->slider.onValueChange = [this, i, fk] { if (! syncing) applyFilterMod (i, fk[i]->slider.getValue()); };
        }
        modSvf = new LedToggle ("SVF MOD"); modLad = new LedToggle ("LADDER MOD");
        owned.add (modSvf); owned.add (modLad);
        addAndMakeVisible (modSvf); addAndMakeVisible (modLad);
        modSvf->setBounds (ox + 6, oy + 104, 78, 16); modLad->setBounds (ox + 84, oy + 104, 94, 16);
        modSvf->setState (true); modLad->setState (true);
        modSvf->acc = cur; modLad->acc = cur;
        T ("f_link", "LINK CUTOFF", 8, 130, 110, 16);
        Ctext ("ROUTE", 8, 156);
        S ("f_route", 8, 168, 90, 20, false);
        K ("f_mix", "MIX", 110, 150, 56);
        svfCut->slider.onValueChange = [this] { linkMoved (0); };
        ladCut->slider.onValueChange = [this] { linkMoved (1); };

        // Huellkurven: Attack - Decay - Sustain - Release als Hauptreihe, Delay/Hold als Zusatz
        const char* en[3] = { "FILTER ENVELOPE", "AMPLIFIER ENVELOPE", "MODULATION ENVELOPE" };
        for (int i = 0; i < 3; ++i)
        {
            const String s = i == 0 ? "feg_" : (i == 1 ? "aeg_" : "meg_");
            M (en[i], 1260 + i * 300, 274, 290, 200, Acc::env);
            K (s + "att", "ATTACK", 12, 22, 46);
            K (s + "dec", "DECAY", 80, 22, 46);
            K (s + "sus", "SUSTAIN", 148, 22, 46);
            K (s + "rel", "RELEASE", 216, 22, 46);
            Ctext ("EXTRA STAGES", 12, 80);
            K (s + "delay", "DELAY", 12, 92, 38);
            K (s + "hold", "HOLD", 66, 92, 38);
            K (s + "vel", "VEL", 120, 92, 38);
            K (s + "scale", "TIME x", 174, 92, 38);
            Ctext ("CURVES  (A / D / R)", 12, 140);
            K (s + "acurve", "A", 12, 152, 28);
            K (s + "dcurve", "D", 60, 152, 28);
            K (s + "rcurve", "R", 108, 152, 28);
            T (s + "loop", "LOOP", 190, 146, 66, 15);
            T (s + "latch", "LATCH", 190, 168, 70, 15);
        }
        // Ausgang, Programme, Panic
        M ("OUTPUT", 1260, 484, 220, 250, Acc::out);
        K ("out_vol", "VOLUME", 14, 24, 60);
        K ("out_tempo", "TEMPO", 132, 30, 50);
        Ctext ("START PROGRAM", 12, 118);
        progBox = new ComboBox();
        owned.add (progBox); addAndMakeVisible (progBox);
        for (int i = 0; i < proc.getNumPrograms(); ++i) progBox->addItem (proc.getProgramName (i), i + 1);
        progBox->setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
        progBox->setBounds (ox + 12, oy + 130, 196, 22);
        progBox->onChange = [this] { proc.setCurrentProgram (progBox->getSelectedId() - 1); };
        auto* st = new TextButton ("START SCREEN");
        owned.add (st); addAndMakeVisible (st);
        st->setBounds (ox + 12, oy + 162, 94, 24);
        st->onClick = [this] { showStart (true); };
        auto* pan = new TextButton ("PANIC");
        owned.add (pan); addAndMakeVisible (pan);
        pan->setColour (TextButton::textColourOffId, Colour (0xffff1f3f));
        pan->setBounds (ox + 114, oy + 162, 94, 24);
        pan->onClick = [this] { proc.panic(); };
        Ctext ("Panic: alle Noten stumm, Hold aus.", 12, 200);
        Ctext ("Ohne Host gilt der TEMPO-Wert.", 12, 214);

        // Step-Sequencer (Metropolis-Prinzip), 16 Stufen
        M ("STEP SEQUENCER", 1490, 484, 660, 250, Acc::seq);
        T ("seq_on", "SEQ ON", 8, 22, 88, 16);
        T ("seq_latch", "LATCH", 8, 40, 88, 16);
        Ctext ("RATE", 8, 60, 8.5f);
        Cb ("seq_rate", 8, 71, 88, 18);
        Ctext ("ORDER", 8, 92, 8.5f);
        Cb ("seq_order", 8, 103, 88, 18);
        K ("seq_len", "STEPS", 4, 124, 30);
        K ("seq_gate", "GATE", 52, 124, 30);
        for (int i = 0; i < kSeqSteps; ++i)
        {
            const int x = 104 + i * 34;
            const String n (i + 1);
            stageX[i] = ox + x;
            auto* fp = new FaderCtl (apvts, "seq_p" + n, show, false, cur); owned.add (fp); addAndMakeVisible (fp); fp->setBounds (ox + x + 4, oy + 31, 26, 62);
            auto* fo = new FaderCtl (apvts, "seq_o" + n, show, true, cur);  owned.add (fo); addAndMakeVisible (fo); fo->setBounds (ox + x + 4, oy + 96, 26, 30);
            auto* fc = new FaderCtl (apvts, "seq_c" + n, show, true, cur);  owned.add (fc); addAndMakeVisible (fc); fc->setBounds (ox + x + 4, oy + 130, 26, 46);
            S ("seq_g" + n, x + 12, 180, 18, 36, true, { "", "", "", "" });
            auto* sm = new StepModeBtn (apvts, "seq_m" + n, show, cur); owned.add (sm); addAndMakeVisible (sm); sm->setBounds (ox + x + 3, oy + 222, 29, 18);
        }
    }

    //------------------------------------------------------------------ Filter-Modulation (EG/LFO2/FM gemeinsam fuer beide Filter)
    static const char* fid (int which, bool svfSide)
    {
        static const char* a[3][2] = { { "svf_eg", "lad_eg" }, { "svf_lfo", "lad_lfo" }, { "svf_fm", "lad_fm" } };
        return a[which][svfSide ? 0 : 1];
    }
    void applyFilterMod (int which, double v)
    {
        for (int side = 0; side < 2; ++side)
            if ((side == 0 && modSvf->getState()) || (side == 1 && modLad->getState()))
                if (auto* p = apvts.getParameter (fid (which, side == 0))) p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
    }
    void gestureFilter (int which, bool begin)
    {
        for (int side = 0; side < 2; ++side)
            if ((side == 0 && modSvf->getState()) || (side == 1 && modLad->getState()))
                if (auto* p = apvts.getParameter (fid (which, side == 0))) { if (begin) p->beginChangeGesture(); else p->endChangeGesture(); }
    }
    void syncFilterKnobs()
    {
        KnobCtl* fk[3] = { fEg, fLfo, fFm };
        const bool svfSide = modSvf->getState() || ! modLad->getState();
        syncing = true;
        for (int i = 0; i < 3; ++i)
            if (! fk[i]->slider.isMouseButtonDown())
                fk[i]->slider.setValue (apvts.getRawParameterValue (fid (i, svfSide))->load(), dontSendNotification);
        syncing = false;
    }
    void linkMoved (int which)
    {
        if (linkGuard) return;
        auto* thisK = which == 0 ? svfCut : ladCut;
        const float now = (float) thisK->slider.getValue();
        const bool linkOn = apvts.getRawParameterValue ("f_link")->load() > 0.5f;
        if (linkOn && thisK->slider.isMouseButtonDown())
        {
            const float delta = now - lastCut[which];
            auto* other = which == 0 ? ladCut : svfCut;
            const float nv = jlimit (0.0f, 1.0f, (float) other->slider.getValue() + delta);
            linkGuard = true;
            other->slider.setValue (nv, sendNotificationSync);
            linkGuard = false;
            lastCut[1 - which] = nv;
        }
        lastCut[which] = now;
    }

    //------------------------------------------------------------------ Timer
    void timerCallback() override
    {
        lcd.repaint();
        for (auto* w : waves) w->repaint();
        for (int i = 0; i < 3; ++i)
        {
            auto& u = fx[(size_t) i];
            const int type = (int) std::lround (u.typeParam->load());
            if (type != u.lastType)
            {
                u.lastType = type;
                for (int k = 0; k < 5; ++k)
                {
                    const String l = fxLabel (type, k);
                    u.knobs[k + 1]->setLabel (l);
                    u.knobs[k + 1]->setAlpha (l.isEmpty() ? 0.25f : 1.0f);
                    u.knobs[k + 1]->setInterceptsMouseClicks (l.isNotEmpty(), l.isNotEmpty());
                }
                u.knobs[0]->setAlpha (type == FX_OFF ? 0.25f : 1.0f);
            }
        }
        syncFilterKnobs();
        repaint (1490, 484, 660, 40);
        if (! svfCut->slider.isMouseButtonDown() && ! ladCut->slider.isMouseButtonDown())
        { lastCut[0] = (float) svfCut->slider.getValue(); lastCut[1] = (float) ladCut->slider.getValue(); }
    }

    //------------------------------------------------------------------ Zeichnen
    void paint (Graphics& g) override
    {
        const auto W = (float) kW0, H = (float) kH0;
        // Panel
        g.setGradientFill (ColourGradient (Col::panelTop, 0, 0, Col::panelBottom, 0, 744, false));
        g.fillRect (0.0f, 0.0f, W, 744.0f);
        g.setColour (Colour (0xff0d0e11));
        g.fillRect (0.0f, 744.0f, W, H - 744.0f);
        // feine Linie am Panelrand
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine (743, 40.0f, W - 40.0f);
        // Seitenteile (dunkel, wie die anderen Plugins)
        for (int side = 0; side < 2; ++side)
        {
            const float x0 = side == 0 ? 0.0f : W - 40.0f;
            g.setColour (Colour (0xff1d1f24));
            g.fillRect (x0, 0.0f, 40.0f, H);
            g.setColour (Colour (0xff2b2e35));
            g.drawVerticalLine (side == 0 ? 39 : (int) x0, 0.0f, H);
        }
        // Module
        for (auto& m : mods)
        {
            auto r = m.r.toFloat();
            g.setColour (Colour (0xff1d1f24));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (Colour (0xff2b2e35));
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
            g.setFont (F (10.5f).withExtraKerningFactor (0.1f));
            const float tw = F (10.5f).withExtraKerningFactor (0.1f).getStringWidthFloat (m.title) + 24.0f;
            g.setColour (Colour (0xff1d1f24));
            g.fillRect (r.getX() + 9.0f, r.getY() - 2.0f, tw, 5.0f);
            auto dot = Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 18.0f, r.getY() });
            g.setColour (m.acc.withAlpha (0.35f)); g.fillEllipse (dot.expanded (3.0f));
            g.setColour (m.acc); g.fillEllipse (dot);
            g.setColour (Col::text);
            g.drawText (m.title, Rectangle<int> (m.r.getX() + 26, m.r.getY() - 8, (int) tw, 16), Justification::centredLeft, false);
        }
        for (auto& c : caps)
        {
            g.setColour (Col::textDim);
            g.setFont (F (c.size));
            g.drawText (c.text, Rectangle<int> (c.x, c.y, 520, 12), c.center ? Justification::centred : Justification::centredLeft, false);
        }
        // Sequencer: Legende, Stufennummern und Lauf-LED
        {
            const int mx = 1490, my = 484;
            g.setColour (Col::textDim);
            g.setFont (F (7.5f));
            const char* gl[4] = { "REST", "ONE", "MULTI", "HOLD" };
            for (int k = 0; k < 4; ++k) g.drawText (gl[k], Rectangle<int> (mx + 40, my + 180 + k * 9, 64, 9), Justification::centredRight, false);
            g.drawText ("TYPE", Rectangle<int> (mx + 8, my + 193, 40, 10), Justification::centredLeft, false);
            g.drawText ("STEP", Rectangle<int> (mx + 40, my + 226, 64, 10), Justification::centredRight, false);
            const int cur = proc.seqStage.load();
            const int len = (int) std::lround (apvts.getRawParameterValue ("seq_len")->load());
            for (int i = 0; i < TO::kSeqSteps; ++i)
            {
                g.setColour (i < len ? Col::text : Col::textDim.withAlpha (0.4f));
                g.setFont (F (8.5f));
                g.drawText (String (i + 1), Rectangle<int> (stageX[i], my + 9, 34, 10), Justification::centred, false);
                drawLed (g, Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) stageX[i] + 17.0f, (float) my + 24.0f }), i == cur, Acc::seq);
            }
        }
    }

    static constexpr int kW0 = TheOneEditor::kW, kH0 = TheOneEditor::kH;

private:
    struct FxUI { KnobCtl* knobs[6] {}; std::atomic<float>* typeParam = nullptr; std::atomic<float>* syncParam = nullptr; int lastType = -2; };

    TheOneProcessor& proc;
    AudioProcessorValueTreeState& apvts;
    TheOneLook look;
    ShowFn show;
    LcdView lcd;
    KeyboardView keys;
    WheelCtl pitchWheel, modWheel;
    OwnedArray<Component> owned;
    Array<WaveView*> waves;
    std::vector<Mod> mods;
    std::vector<Cap> caps;
    std::vector<FxUI> fx;
    KnobCtl *svfCut = nullptr, *ladCut = nullptr, *fEg = nullptr, *fLfo = nullptr, *fFm = nullptr;
    LedToggle *modSvf = nullptr, *modLad = nullptr;
    ComboBox* progBox = nullptr;
    std::unique_ptr<StartScreen> start;
    int stageX[TO::kSeqSteps] {};
    int lastSeqStage = -2;
    bool syncing = false, linkGuard = false;
    float lastCut[2] = { 0, 0 };
};

//==============================================================================
TheOneEditor::TheOneEditor (TheOneProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    content = std::make_unique<Content> (p);
    addAndMakeVisible (*content);
    content->setBounds (0, 0, kW, kH);
    setResizable (true, true);
    setResizeLimits (880, 352, 3400, 1360);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (1760, 704);
}

TheOneEditor::~TheOneEditor() {}

void TheOneEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content->setTransform (AffineTransform::scale (s));
    content->setBounds (0, 0, kW, kH);
}

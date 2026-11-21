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
        g.setColour (Colour (0xff0a0a0b));
        g.fillRoundedRectangle (r, 6.0f);
        auto s = r.reduced (6.0f);
        g.setGradientFill (ColourGradient (Colour (0xff0b0b0d), 0, s.getY(), Colour (0xff020203), 0, s.getBottom(), false));
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
        g.setColour (Colour (0xff060607));
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
            g.setColour (Colour (0xffffd9d9));
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
struct Mod { String title; Rectangle<int> r; };
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
        lcd.setBounds (830 + 6, 14 + 6, 398, 330);
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
    }
    ~Content() override { setLookAndFeel (nullptr); }

    //------------------------------------------------------------------ Hilfen
    int ox = 0, oy = 0;
    void M (const String& title, int x, int y, int w, int h) { mods.push_back ({ title, { x, y, w, h } }); ox = x; oy = y; }
    void Ctext (const String& t, int x, int y, float size = 9.0f, bool center = false) { caps.push_back ({ t, ox + x, oy + y, size, center }); }

    KnobCtl* K (const String& id, const String& label, int x, int y, int size, bool big = false)
    {
        auto* k = new KnobCtl (apvts, id, label, size, show, big);
        owned.add (k); addAndMakeVisible (k);
        k->setTopLeftPosition (ox + x, oy + y);
        return k;
    }
    SelectorCtl* S (const String& id, int x, int y, int w, int h, bool vertical, std::vector<String> labels = {})
    {
        auto* s = new SelectorCtl (apvts, id, vertical, std::move (labels));
        owned.add (s); addAndMakeVisible (s);
        s->setBounds (ox + x, oy + y, w, h);
        return s;
    }
    LedToggle* T (const String& id, const String& label, int x, int y, int w = 70, int h = 16)
    {
        auto* t = new LedToggle (apvts, id, label);
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
            M ("LFO " + String (i + 1), 50 + i * 190, 14, 184, 166);
            S (s + "wave", 8, 26, 76, 50, true, { "TRI/SIN", "PULSE", "SAW/RAMP", "S&H" });
            K (s + "rate", "RATE", 86, 24, 54);
            S (s + "timing", 146, 26, 34, 36, true, { "STR", "DOT", "TRI" });
            S (s + "range", 146, 70, 34, 48, true, { "SLOW", "LOW", "MED", "HIGH" });
            K (s + "var", "VAR", 8, 88, 40);
            K (s + "delay", "DELAY", 54, 88, 40);
            K (s + "fade", "FADE", 100, 88, 40);
            T (s + "sync", "SYNC", 4, 146, 58, 15);
            T (s + "pol", "BIPOLAR", 62, 146, 60, 15);
            T (s + "reset", "RESET", 122, 146, 60, 15);
        }
        // Oszillatoren
        for (int i = 0; i < 3; ++i)
        {
            const String s = "osc" + String (i + 1) + "_";
            M ("OSCILLATOR " + String (i + 1), 50 + i * 200, 190, 194, 292);
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
        M ("OSCILLATOR COMMON", 650, 190, 160, 292);
        K ("c_lfo1", "LFO1>PIT", 10, 24, 50);
        K ("c_mepitch", "EG>PIT", 98, 24, 50);
        K ("c_lfo3ang", "LFO3>ANG", 10, 92, 50);
        K ("c_lfo3pw", "LFO3>PW", 98, 92, 50);
        K ("c_fm", "FM AMT", 10, 160, 50);
        K ("c_bend", "BEND", 98, 160, 50);
        Ctext ("RING MOD SOURCES", 10, 232);
        S ("c_ringsrc", 10, 244, 140, 20, false);
        Ctext ("FM ROUTE", 10, 266);
        S ("c_fmroute", 10, 278 - 8, 140, 20, false);

        // Noise
        M ("NOISE", 50, 492, 190, 242);
        K ("n_att", "ATTACK", 10, 24, 48);
        K ("n_rel", "RELEASE", 70, 24, 48);
        K ("n_mix", "COLOR", 130, 24, 48);
        Ctext ("COLOR", 10, 102);
        S ("n_pair", 10, 116, 110, 44, true, { "RED+WHITE", "RED+PURPLE", "WHITE+PURPLE" });
        T ("n_sus", "SUSTAIN", 10, 176, 80, 16);

        // Stimmen
        M ("VOICE ALLOCATION", 250, 492, 560, 134);
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
        M ("CHORD / HOLD", 250, 636, 560, 98);
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
        for (int i = 0; i < 3; ++i)
        {
            const String s = "fx" + String (i + 1) + "_";
            M ("EFFECT " + String (i + 1), 830, 354 + i * 126, 410, 122);
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
        M ("MIXER", 1260, 14, 300, 128);
        const char* nm[5] = { "OSC 1", "OSC 2", "OSC 3", "RING", "NOISE" };
        for (int i = 0; i < 5; ++i)
        {
            K ("mix_lvl" + String (i), nm[i], 8 + i * 58, 22, 46);
            T ("mix_svf" + String (i), "SVF", 6 + i * 58, 88, 52, 14);
            T ("mix_lad" + String (i), "LAD", 6 + i * 58, 106, 52, 14);
        }
        // Zusatz-Modulation
        M ("EXTRA MODULATION", 1260, 150, 300, 114);
        Ctext ("LFO 4 DESTINATION", 10, 22);
        Cb ("x_l4dest", 10, 34, 130);
        K ("x_l4amt", "AMOUNT", 150, 22, 40);
        Ctext ("MOD EG DESTINATION", 10, 66);
        Cb ("x_medest", 10, 78, 130);
        K ("x_meamt", "AMOUNT", 150, 64, 40);
        K ("c_moddepth", "WHEEL", 230, 22, 40);

        // SVF
        M ("STATE VARIABLE FILTER", 1570, 14, 190, 250);
        svfCut = K ("svf_cut", "CUTOFF", 10, 24, 62, true);
        K ("svf_res", "RESO", 80, 30, 46);
        K ("svf_space", "SPACING", 134, 30, 46);
        K ("svf_key", "KEY TRK", 10, 106, 46);
        S ("svf_mode", 70, 108, 70, 52, true);
        T ("svf_ser", "SERIES", 10, 172, 80, 16);
        Ctext ("FM SOURCE", 10, 196);
        S ("svf_fmsrc", 10, 208, 168, 20, false, { "OSC 1", "OSC 2", "OSC 3" });

        // Ladder
        M ("LADDER FILTER", 1770, 14, 190, 250);
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
        M ("FILTER COMMON", 1970, 14, 180, 250);
        fEg = new KnobCtl ("EG AMT", 46, show, NormalisableRange<double> (-1.0, 1.0), 0.0, "Filter EG Amount");
        fLfo = new KnobCtl ("LFO 2 AMT", 46, show, NormalisableRange<double> (0.0, 1.0), 0.0, "Filter LFO 2 Amount");
        fFm = new KnobCtl ("FM AMT", 46, show, NormalisableRange<double> (0.0, 1.0), 0.0, "Filter FM Amount");
        KnobCtl* fk[3] = { fEg, fLfo, fFm };
        for (int i = 0; i < 3; ++i)
        {
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
        T ("f_link", "LINK CUTOFF", 8, 130, 110, 16);
        Ctext ("ROUTE", 8, 156);
        S ("f_route", 8, 168, 90, 20, false);
        K ("f_mix", "MIX", 110, 150, 56);
        svfCut->slider.onValueChange = [this] { linkMoved (0); };
        ladCut->slider.onValueChange = [this] { linkMoved (1); };

        // Huellkurven
        const char* en[3] = { "FILTER ENVELOPE", "AMPLIFIER ENVELOPE", "MODULATION ENVELOPE" };
        for (int i = 0; i < 3; ++i)
        {
            const String s = i == 0 ? "feg_" : (i == 1 ? "aeg_" : "meg_");
            M (en[i], 1260 + i * 300, 274, 290, 200);
            K (s + "delay", "DELAY", 8, 24, 46);
            K (s + "att", "ATTACK", 62, 24, 46);
            K (s + "hold", "HOLD", 116, 24, 46);
            K (s + "dec", "DECAY", 8, 96, 46);
            K (s + "sus", "SUSTAIN", 62, 96, 46);
            K (s + "rel", "RELEASE", 116, 96, 46);
            K (s + "acurve", "A CURVE", 184, 24, 38);
            K (s + "dcurve", "D CURVE", 236, 24, 38);
            K (s + "rcurve", "R CURVE", 184, 82, 38);
            K (s + "scale", "TIME x", 236, 82, 38);
            K (s + "vel", "VELOCITY", 184, 140, 38);
            T (s + "loop", "LOOP", 8, 170, 60, 15);
            T (s + "latch", "LATCH", 76, 170, 66, 15);
        }
        // Ausgang und Typenschild
        M ("OUTPUT", 1260, 484, 300, 250);
        K ("out_vol", "MASTER VOLUME", 20, 36, 84, true);
        K ("out_tempo", "TEMPO", 140, 46, 60);
        Ctext ("Mit Host-Tempo laeuft Sync automatisch.", 14, 190);
        Ctext ("Ohne Host (Standalone) gilt dieser Wert.", 14, 202);
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
        g.setColour (Colour (0xff070708));
        g.fillRect (0.0f, 744.0f, W, H - 744.0f);
        // feine Linie am Panelrand
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine (743, 40.0f, W - 40.0f);
        // Holzwangen
        for (int side = 0; side < 2; ++side)
        {
            const float x0 = side == 0 ? 0.0f : W - 40.0f;
            auto r = Rectangle<float> (x0, 0.0f, 40.0f, H);
            g.setGradientFill (ColourGradient (Colour (0xff3a2112), x0, 0, Colour (0xff6a4327), x0 + 40.0f, 0, false));
            g.fillRect (r);
            Random rnd (side + 7);
            for (int i = 0; i < 90; ++i)
            {
                const float x = x0 + rnd.nextFloat() * 40.0f;
                g.setColour (Colour (rnd.nextBool() ? 0xff24130a : 0xff8a5a36).withAlpha (0.12f + rnd.nextFloat() * 0.16f));
                g.drawLine (x, 0.0f, x + (rnd.nextFloat() - 0.5f) * 6.0f, H, 0.6f + rnd.nextFloat());
            }
            g.setColour (Colours::black.withAlpha (0.55f));
            g.drawVerticalLine (side == 0 ? 39 : (int) x0, 0.0f, H);
        }
        // Module
        for (auto& m : mods)
        {
            auto r = m.r.toFloat();
            g.setColour (Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (Col::line.withAlpha (0.5f));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
            g.setFont (F (10.5f).withExtraKerningFactor (0.1f));
            const float tw = F (10.5f).withExtraKerningFactor (0.1f).getStringWidthFloat (m.title) + 12.0f;
            g.setColour (Colour (0xff0f0f11));
            g.fillRect (r.getX() + 9.0f, r.getY() - 2.0f, tw, 5.0f);
            g.setColour (Col::text);
            g.drawText (m.title, Rectangle<int> (m.r.getX() + 14, m.r.getY() - 8, (int) tw, 16), Justification::centredLeft, false);
        }
        for (auto& c : caps)
        {
            g.setColour (Col::textDim);
            g.setFont (F (c.size));
            g.drawText (c.text, Rectangle<int> (c.x, c.y, 520, 12), c.center ? Justification::centred : Justification::centredLeft, false);
        }
        // Typenschild
        auto plate = Rectangle<float> (1580.0f, 494.0f, 560.0f, 230.0f);
        g.setGradientFill (ColourGradient (Colour (0xff151518), plate.getX(), plate.getY(), Colour (0xff0a0a0b), plate.getRight(), plate.getBottom(), false));
        g.fillRoundedRectangle (plate, 5.0f);
        g.setColour (Col::line.withAlpha (0.35f));
        g.drawRoundedRectangle (plate, 5.0f, 1.0f);
        g.setColour (Col::text);
        g.setFont (F (88.0f).withExtraKerningFactor (0.14f));
        g.drawText ("THE ONE", plate.withTrimmedTop (36.0f).withHeight (100.0f).toNearestInt(), Justification::centred, false);
        g.setColour (Col::led);
        g.fillRect (plate.getX() + 60.0f, plate.getY() + 150.0f, plate.getWidth() - 120.0f, 2.0f);
        g.setColour (Col::textDim);
        g.setFont (F (13.0f).withExtraKerningFactor (0.22f));
        g.drawText ("16-VOICE POLYPHONIC SYNTHESIZER", plate.withTrimmedTop (162.0f).withHeight (20.0f).toNearestInt(), Justification::centred, false);
        g.setFont (F (12.0f).withExtraKerningFactor (0.12f));
        g.drawText ("by Dani Koenig", plate.withTrimmedTop (186.0f).withHeight (20.0f).toNearestInt(), Justification::centred, false);
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

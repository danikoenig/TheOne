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
        g.setFont (F (21.0f).withExtraKerningFactor (0.16f));
        g.drawText ("THE ONE", a.withHeight (24.0f).toNearestInt(), Justification::centredLeft, false);
        g.setColour (Col::textDim);
        g.setFont (F (9.0f).withExtraKerningFactor (0.1f));
        g.drawText ("by Dani Koenig  v" JucePlugin_VersionString, Rectangle<int> ((int) a.getX() + 112, (int) a.getY() + 6, 130, 14), Justification::centredLeft, false);
        // rechts: Tempo und Stimmen
        g.setColour (Col::text);
        g.setFont (F (11.0f));
        g.drawText (String (proc.hostBpm.load(), 1) + " BPM   VOICES " + String (proc.voicesInUse.load()) + " / " + String ((int) std::lround (proc.apvts.getRawParameterValue ("v_poly")->load())),
                    Rectangle<int> ((int) a.getRight() - 170, (int) a.getY() + 4, 170, 16), Justification::centredRight, false);

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
        auto sc = Rectangle<float> (a.getX(), a.getY() + 150.0f, a.getWidth(), 108.0f);
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

        // Learn-Hinweis nur waehrend Chord Learn
        if (proc.learnActive.load())
        {
            g.setColour (Col::led);
            g.setFont (F (13.0f));
            g.drawText ("CHORD LEARN - " + String (proc.learnCount.load()) + " NOTES", Rectangle<int> ((int) a.getX(), (int) a.getY() + 266, (int) a.getWidth(), 18), Justification::centredLeft, false);
        }
    }

private:
    TheOneProcessor& proc;
    String rName, rValue;
};

//==============================================================================
// Vertikaler Fader (Sequencer), Wert erscheint im LCD
class FaderCtl : public Component
{
public:
    FaderCtl (AudioProcessorValueTreeState& apvts, const String& id, ShowFn s, bool notched, Colour a, bool valueBelow = false) : show (s), notch (notched), acc (a), below (valueBelow)
    {
        param = apvts.getParameter (id);
        att = std::make_unique<ParameterAttachment> (*param, [this] (float) { repaint(); });
        att->sendInitialUpdate();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (below)
        {
            const int v = (int) std::lround (param->convertFrom0to1 (param->getValue()));
            g.setColour (v == 0 ? Col::textDim : acc);
            g.setFont (F (9.5f));
            g.drawText (v > 0 ? "+" + String (v) : String (v), r.removeFromBottom (13.0f).toNearestInt(), Justification::centred, false);
        }
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
        const float t = jlimit (0.0f, 1.0f, 1.0f - (e.position.y - 6.0f) / ((float) getHeight() - (below ? 13.0f : 0.0f) - 12.0f));
        att->setValueAsPartOfGesture (param->convertFrom0to1 (t));
        announce();
    }
    void announce() { if (show) show (param->getName (64), param->getCurrentValueAsText()); }
    ShowFn show;
    bool notch;
    Colour acc;
    bool below = false;
    RangedAudioParameter* param = nullptr;
    std::unique_ptr<ParameterAttachment> att;
};

//==============================================================================
// Step-Taste: Klick schaltet ON -> MUTE -> SKIP
class StepModeBtn : public Component
{
public:
    StepModeBtn (AudioProcessorValueTreeState& apvts, const String& id, ShowFn s, Colour a, StringArray lbl = { "ON", "MUTE", "SKIP" }, int filled = 0)
        : show (s), acc (a), labels (lbl), filledState (filled)
    {
        param = apvts.getParameter (id);
        att = std::make_unique<ParameterAttachment> (*param, [this] (float v) { mode = (int) std::lround (v); repaint(); });
        att->sendInitialUpdate();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const bool filled = filledState < 0 ? mode != 0 : mode == filledState;
        if (filled) { g.setColour (acc); g.fillRoundedRectangle (r, 3.0f); }
        else { g.setColour (Colour (0xff121317)); g.fillRoundedRectangle (r, 3.0f); g.setColour (filledState >= 0 && labels.size() > 2 && mode == 1 ? acc.withAlpha (0.8f) : Col::textDim.withAlpha (0.5f)); g.drawRoundedRectangle (r, 3.0f, 1.0f); }
        g.setColour (filled ? Colour (0xff17181c) : (filledState >= 0 && labels.size() > 2 && mode == 1 ? acc : Col::textDim));
        g.setFont (F (filledState < 0 ? 9.5f : 7.5f));
        g.drawText (labels[jlimit (0, labels.size() - 1, mode)], getLocalBounds(), Justification::centred, false);
    }
    void mouseDown (const MouseEvent&) override
    {
        att->setValueAsCompleteGesture ((float) ((mode + 1) % labels.size()));
        if (show) show (param->getName (64), param->getCurrentValueAsText());
    }
private:
    ShowFn show;
    Colour acc;
    StringArray labels;
    int filledState = 0;
    int mode = 0;
    RangedAudioParameter* param = nullptr;
    std::unique_ptr<ParameterAttachment> att;
};

//==============================================================================
// Oktav-Taste pro Step: Klick schaltet 0 -> +1 -> -1 -> 0
class OctBtn : public Component
{
public:
    OctBtn (AudioProcessorValueTreeState& apvts, const String& id, ShowFn s, Colour a) : show (s), acc (a)
    {
        param = apvts.getParameter (id);
        att = std::make_unique<ParameterAttachment> (*param, [this] (float v) { val = (int) std::lround (v); repaint(); });
        att->sendInitialUpdate();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        if (val != 0) { g.setColour (acc.withAlpha (val > 0 ? 1.0f : 0.55f)); g.fillRoundedRectangle (r, 3.0f); }
        else { g.setColour (Colour (0xff121317)); g.fillRoundedRectangle (r, 3.0f); g.setColour (Col::textDim.withAlpha (0.5f)); g.drawRoundedRectangle (r, 3.0f, 1.0f); }
        g.setColour (val != 0 ? Colour (0xff17181c) : Col::textDim);
        g.setFont (F (8.5f));
        g.drawText (val > 0 ? "+1" : (val < 0 ? "-1" : "0"), getLocalBounds(), Justification::centred, false);
    }
    void mouseDown (const MouseEvent&) override
    {
        const int nv = val == 0 ? 1 : (val > 0 ? -1 : 0);
        att->setValueAsCompleteGesture ((float) nv);
        if (show) show (param->getName (64), param->getCurrentValueAsText());
    }
private:
    ShowFn show;
    Colour acc;
    int val = 0;
    RangedAudioParameter* param = nullptr;
    std::unique_ptr<ParameterAttachment> att;
};

//==============================================================================
// Preset-Leiste im Display: blaettern, Menue, Speichern, Loeschen
class PresetBar : public Component
{
public:
    std::function<void (const String&, const String&)> show;
    explicit PresetBar (TheOneProcessor& p) : proc (p)
    {
        for (auto* b : { &prev, &next, &name, &save, &del }) addAndMakeVisible (b);
        name.setColour (TextButton::buttonColourId, Colour (0xff0d0e11));
        name.setColour (TextButton::textColourOffId, Col::text);
        prev.onClick = [this] { step (-1); };
        next.onClick = [this] { step (1); };
        name.onClick = [this] { openMenu(); };
        save.onClick = [this] { askSave(); };
        del.onClick = [this] { askDelete(); };
        refresh();
    }
    void resized() override
    {
        auto r = getLocalBounds();
        prev.setBounds (r.removeFromLeft (24));
        r.removeFromLeft (3);
        del.setBounds (r.removeFromRight (40));
        r.removeFromRight (3);
        save.setBounds (r.removeFromRight (48));
        r.removeFromRight (3);
        next.setBounds (r.removeFromRight (24));
        r.removeFromRight (3);
        name.setBounds (r);
    }
    void refresh()
    {
        const auto f = proc.currentPresetFile();
        const bool user = f.existsAsFile();
        String label = proc.currentPresetName();
        if (user && f.getParentDirectory() != TheOneProcessor::presetRoot())
            label = f.getParentDirectory().getRelativePathFrom (TheOneProcessor::presetRoot()) + " / " + label;
        if (label != shown) { shown = label; name.setButtonText (label + String::fromUTF8 ("  \xe2\x96\xbe")); }
        del.setEnabled (user);
    }

private:
    int currentIndex (const Array<TheOneProcessor::PresetEntry>& list) const
    {
        const auto f = proc.currentPresetFile();
        for (int i = 0; i < list.size(); ++i)
            if (f.existsAsFile() ? list[i].file == f : (list[i].factory >= 0 && list[i].name == proc.currentPresetName())) return i;
        return -1;
    }
    void load (const TheOneProcessor::PresetEntry& e)
    {
        if (proc.loadPreset (e) && show) show ("Preset", e.name);
        refresh();
    }
    void step (int d)
    {
        const auto list = proc.scanPresets();
        if (list.isEmpty()) return;
        const int i = currentIndex (list);
        load (list[((i < 0 ? 0 : i + d) % list.size() + list.size()) % list.size()]);
    }
    void openMenu()
    {
        menuList = proc.scanPresets();
        PopupMenu m, fac;
        std::map<String, PopupMenu> cats;
        PopupMenu top;
        const int cur = currentIndex (menuList);
        for (int i = 0; i < menuList.size(); ++i)
        {
            const auto& e = menuList[i];
            if (e.factory >= 0) fac.addItem (i + 1, e.name, true, i == cur);
            else if (e.category.isEmpty()) top.addItem (i + 1, e.name, true, i == cur);
            else cats[e.category].addItem (i + 1, e.name, true, i == cur);
        }
        m.addSectionHeader ("FACTORY");
        {   // Werks-Presets nach Kategorie ("Pad - Name") in Untermenues
            std::vector<std::pair<String, PopupMenu>> fcats;
            for (int i = 0; i < menuList.size(); ++i)
            {
                const auto& e = menuList[i];
                if (e.factory < 0) continue;
                const int dash = e.name.indexOf (" - ");
                if (dash < 0) { m.addItem (i + 1, e.name, true, i == cur); continue; }
                const String cat = e.name.substring (0, dash), nm = e.name.substring (dash + 3);
                auto it = std::find_if (fcats.begin(), fcats.end(), [&] (auto& c) { return c.first == cat; });
                if (it == fcats.end()) { fcats.push_back ({ cat, PopupMenu() }); it = fcats.end() - 1; }
                it->second.addItem (i + 1, nm, true, i == cur);
            }
            for (auto& [c, sub] : fcats) m.addSubMenu (c, sub, true, nullptr, [&] { for (int i = 0; i < menuList.size(); ++i) if (i == cur && menuList[i].factory >= 0 && menuList[i].name.startsWith (c + " - ")) return true; return false; }());
        }
        if (! cats.empty() || top.getNumItems() > 0)
        {
            m.addSectionHeader ("USER");
            for (auto& [c, sub] : cats) m.addSubMenu (c, sub);
            for (int i = 0; i < menuList.size(); ++i) if (menuList[i].factory < 0 && menuList[i].category.isEmpty()) m.addItem (i + 1, menuList[i].name, true, i == cur);
        }
        m.addSeparator();
        m.addItem (10000, "Open preset folder");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&name), [this] (int r)
        {
            if (r == 10000) { TheOneProcessor::presetRoot().startAsProcess(); return; }
            if (r > 0 && r <= menuList.size()) load (menuList[r - 1]);
        });
    }
    void askSave()
    {
        const auto f = proc.currentPresetFile();
        const bool user = f.existsAsFile();
        String cat = user && f.getParentDirectory() != TheOneProcessor::presetRoot() ? f.getParentDirectory().getRelativePathFrom (TheOneProcessor::presetRoot()) : String();
        auto* w = new AlertWindow ("Save preset", "Name and category (category = subfolder, may stay empty)", MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", user ? proc.currentPresetName() : String(), "Name");
        w->addTextEditor ("cat", cat, "Category");
        w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        SafePointer<PresetBar> self (this);
        w->enterModalState (true, ModalCallbackFunction::create ([self, w] (int r)
        {
            if (r != 1 || self == nullptr) return;
            const String n = w->getTextEditorContents ("name").trim(), c = w->getTextEditorContents ("cat").trim();
            if (n.isEmpty()) return;
            auto dir = TheOneProcessor::presetRoot();
            if (c.isNotEmpty()) dir = dir.getChildFile (File::createLegalFileName (c));
            const auto target = dir.getChildFile (File::createLegalFileName (n) + ".theone");
            auto doSave = [self, n, c]
            {
                if (self == nullptr) return;
                if (self->proc.savePreset (n, c) && self->show) self->show ("Preset saved", n);
                self->refresh();
            };
            if (target.existsAsFile())
                AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Overwrite preset?", "\"" + n + "\" already exists. Replace it?", "Replace", "Cancel", nullptr,
                                              ModalCallbackFunction::create ([doSave] (int ok) { if (ok == 1) doSave(); }));
            else doSave();
        }), true);
    }
    void askDelete()
    {
        const auto f = proc.currentPresetFile();
        if (! f.existsAsFile()) return;
        SafePointer<PresetBar> self (this);
        AlertWindow::showOkCancelBox (MessageBoxIconType::WarningIcon, "Delete preset?", "\"" + f.getFileNameWithoutExtension() + "\" will be moved to the trash.", "Delete", "Cancel", nullptr,
            ModalCallbackFunction::create ([self, f] (int ok)
            {
                if (ok != 1 || self == nullptr) return;
                if (! f.moveToTrash()) f.deleteFile();
                self->proc.apvts.state.setProperty ("presetFile", "", nullptr);
                self->proc.apvts.state.setProperty ("presetName", "Untitled", nullptr);
                self->refresh();
            }));
    }

    TheOneProcessor& proc;
    TextButton prev { "<" }, next { ">" }, name, save { "SAVE" }, del { "DEL" };
    Array<TheOneProcessor::PresetEntry> menuList;
    String shown;
};

//==============================================================================
// Raster der Akkordtypen (LED + Name), gebunden an v_ctype
class ChordGrid : public Component
{
public:
    ChordGrid (AudioProcessorValueTreeState& apvts, ShowFn s, Colour a) : show (s), acc (a)
    {
        param = apvts.getParameter ("v_ctype");
        att = std::make_unique<ParameterAttachment> (*param, [this] (float v) { sel = (int) std::lround (v); repaint(); });
        att->sendInitialUpdate();
    }
    void paint (Graphics& g) override
    {
        for (int i = 0; i < 19; ++i)
        {
            auto c = cell (i).toFloat();
            const bool on = i == sel;
            if (on) { g.setColour (acc.withAlpha (0.12f)); g.fillRoundedRectangle (c.reduced (1.0f, 2.0f), 3.0f); }
            drawLed (g, Rectangle<float> (7.0f, 7.0f).withCentre ({ c.getX() + 9.0f, c.getCentreY() }), on, acc);
            g.setColour (on ? acc : (i == 0 ? Col::textDim : Col::text));
            g.setFont (F (9.5f));
            g.drawText (names[i], c.withTrimmedLeft (17.0f).toNearestInt(), Justification::centredLeft, false);
        }
    }
    void mouseDown (const MouseEvent& e) override
    {
        for (int i = 0; i < 19; ++i)
            if (cell (i).contains (e.getPosition()))
            {
                att->setValueAsCompleteGesture ((float) i);
                if (show) show ("Chord Type", param->getCurrentValueAsText());
                return;
            }
    }
private:
    Rectangle<int> cell (int i) const
    {
        const int cols = 5, cw = getWidth() / cols, ch = getHeight() / 4;
        return { (i % cols) * cw, (i / cols) * ch, cw, ch };
    }
    static constexpr const char* names[19] = { "LEARNED", "Major", "Minor", "Maj 7", "Min 7", "Dom 7", "Maj 6", "Min 6", "Add 9", "Maj 9",
                                               "Min 9", "Dom 9", "Sus 2", "Sus 4", "Dim", "Dim 7", "Half-dim", "Aug", "Power" };
    ShowFn show;
    Colour acc;
    int sel = 0;
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
    const Colour seq   { 0xffff8a3d };   // Orange
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
        // Pitch-Bend-Bereich direkt bei den Raedern
        ox = 0; oy = 0; cur = Acc::osc;
        K ("c_bend", "BEND", 162, 754, 50);
        addAndMakeVisible (lcd);
        addAndMakeVisible (presets);
        presets.setBounds (852, 70, 374, 24);
        presets.show = [this] (const String& n, const String& v) { lcd.setReadout (n, v); };
        lcd.setBounds (838, 26, 394, 310);
        addAndMakeVisible (pitchWheel); addAndMakeVisible (modWheel);
        pitchWheel.setBounds (50, 750, 50, 94);
        modWheel.setBounds (106, 750, 50, 94);
        addAndMakeVisible (keys);
        keys.setAvailableRange (36, 96);
        keys.setScrollButtonsVisible (false);
        keys.setKeyWidth (1928.0f / 36.0f);
        keys.setVelocity (0.8f, true);
        keys.setBounds (222, 752, 1928, 92);
        keys.setWantsKeyboardFocus (false);
        keys.generated = [this] (int n) { const uint64_t m = n < 64 ? proc.genLo.load() : proc.genHi.load(); return ((m >> (n & 63)) & 1) != 0; };
        startTimerHz (30);
    }
    ~Content() override { setLookAndFeel (nullptr); }
    void tick() { timerCallback(); }

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
            {
                auto* rk = K (s + "rate", "RATE", 86, 24, 54);
                auto* syncP = apvts.getRawParameterValue (s + "sync");
                auto* timP = apvts.getRawParameterValue (s + "timing");
                auto* rateP = apvts.getParameter (s + "rate");
                rk->setValueText ([syncP, timP, rateP] (double v) -> String
                {
                    if (syncP->load() < 0.5f) return rateP->getText (rateP->convertTo0to1 ((float) v), 32);
                    const float beats = kDivBeats[divIndex ((float) v)];
                    String n = beats >= 4.0f ? String ((int) std::lround (beats / 4.0f)) + (beats >= 8.0f ? " BARS" : " BAR")
                                             : "1/" + String ((int) std::lround (4.0f / beats));
                    const int tm = (int) std::lround (timP->load());
                    return n + (tm == 1 ? " D" : (tm == 2 ? " T" : ""));
                });
            }
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
            M ("OSCILLATOR " + String (i + 1), 50 + i * 204, 190, 194, 284, Acc::osc);
            auto* wv = new WaveView (apvts, i); owned.add (wv); addAndMakeVisible (wv); wv->setBounds (ox + 8, oy + 22, 178, 58); waves.add (wv);
            K (s + "freq", "FREQ", 8, 88, 52);
            K (s + "beat", "BEAT", 70, 88, 52);
            K (s + "angle", "WAVE ANGLE", 132, 88, 52);
            K (s + "pw", "PULSE WIDTH", 8, 158, 52);
            K (s + "mix", "MIX", 70, 158, 52);
            K (s + "track", "TRACK", 132, 158, 52);
            S (s + "oct", 6, 232, 130, 20, false);
            S (s + "wave", 140, 232, 48, 20, false);
            if (i > 0) T (s + "sync", "SYNC", 8, 260, 60, 15);
            T (s + "reset", "PHASE RESET", 80, 260, 110, 15);
        }
        M ("OSC COMMON", 662, 190, 148, 284, Acc::osc);
        K ("c_lfo1", "LFO1>PIT", 10, 24, 48);
        K ("c_mepitch", "EG>PIT", 88, 24, 48);
        K ("c_lfo3ang", "LFO3>ANG", 10, 88, 48);
        K ("c_lfo3pw", "LFO3>PW", 88, 88, 48);
        K ("c_fm", "FM AMT", 10, 152, 48);
        K ("c_moddepth", "WHEEL VIB", 88, 152, 48);
        Ctext ("RING MOD SOURCES", 10, 214);
        S ("c_ringsrc", 8, 226, 132, 20, false);
        Ctext ("FM ROUTE", 10, 249);
        S ("c_fmroute", 8, 260, 132, 20, false);

        // Noise
        M ("NOISE", 50, 484, 190, 250, Acc::osc);
        K ("n_att", "ATTACK", 10, 24, 48);
        K ("n_rel", "RELEASE", 70, 24, 48);
        K ("n_mix", "COLOR", 130, 24, 48);
        Ctext ("COLOR", 10, 102);
        S ("n_pair", 10, 116, 110, 44, true, { "RED+WHITE", "RED+PURPLE", "WHITE+PURPLE" });
        T ("n_sus", "SUSTAIN", 10, 176, 80, 16);

        // Stimmen
        M ("VOICE ALLOCATION", 250, 484, 560, 120, Acc::voice);
        K ("v_poly", "POLY", 14, 24, 52);
        K ("v_ucount", "UNISON", 80, 24, 52);
        K ("v_detune", "DETUNE", 146, 24, 52);
        K ("v_spread", "SPREAD", 212, 24, 52);
        K ("v_glide", "GLIDE", 278, 24, 52);
        T ("v_mono", "MONO", 352, 30, 66);
        T ("v_unison", "UNISON", 352, 54, 76);
        T ("v_glideon", "GLIDE ON", 352, 78, 80);
        T ("v_multi", "MULTI TRIG", 444, 30, 96);

        // Akkord: Typ-Raster wie bei Akkord-Generatoren, Inversion, Voicing, Bassnote (blau)
        M ("CHORD", 250, 614, 560, 120, Acc::fx);
        T ("v_chord", "CHORD ON", 8, 18, 100, 16);
        T ("v_hold", "HOLD", 8, 38, 100, 16);
        T ("v_learn", "LEARN", 8, 58, 100, 16);
        T ("v_silent", "SILENT", 8, 78, 100, 16);
        auto* clr = new TextButton ("CLEAR");
        owned.add (clr); addAndMakeVisible (clr);
        clr->setBounds (ox + 8, oy + 98, 96, 16);
        clr->onClick = [this] { proc.clearChord(); };
        auto* grid = new ChordGrid (apvts, show, cur);
        owned.add (grid); addAndMakeVisible (grid);
        grid->setBounds (ox + 114, oy + 16, 326, 98);
        Ctext ("INVERSION", 448, 14, 8.5f);
        Cb ("v_cinv", 446, 25, 106, 18);
        Ctext ("VOICING", 448, 47, 8.5f);
        Cb ("v_cvoice", 446, 58, 106, 18);
        auto* bassBtn = new StepModeBtn (apvts, "v_cbass", show, cur, { "BASS OFF", "BASS -1 OCT", "BASS -2 OCT" }, -1);
        owned.add (bassBtn); addAndMakeVisible (bassBtn);
        bassBtn->setBounds (ox + 446, oy + 88, 106, 24);
    }

    void buildCenter()
    {
        M ("DISPLAY", 830, 14, 410, 330, Acc::fx);
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
        const bool delayT = (type >= FX_DELAY && type <= FX_ECHO) || type == FX_DUB;
        if (type == FX_DUB && k > 0)
        {
            if (k == 1) snprintf (b, sizeof (b), "%.0f %%", dubFeedbackPct (x));
            else if (k == 2) { const float f = dubFilterHz (x); if (f < 1000.0f) snprintf (b, sizeof (b), "%.0f Hz", f); else snprintf (b, sizeof (b), "%.1f kHz", f / 1000.0f); }
            else if (k == 3) snprintf (b, sizeof (b), "%.1f dB", dubDriveDb (x));
            else snprintf (b, sizeof (b), "%.1f", dubLfoAmt (x));
            return b;
        }
        const bool modT = (type >= FX_CHORUS && type <= FX_PHASER12);
        if (k == 0)
        {
            if (delayT)
            {
                if (sync) return fxDivName (fxDivIndex (x));
                const float t = 0.01f * std::pow (200.0f, x);
                if (t < 1.0f) snprintf (b, sizeof (b), "%.0f ms", t * 1000.0f); else snprintf (b, sizeof (b), "%.2f s", t);
                return b;
            }
            if (modT)
            {
                if (sync) return fxDivName (fxDivIndex (x));
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
            K ("mix_lvl" + String (i), nm[i], 3 + i * 49, 16, 46);
            T ("mix_svf" + String (i), "S", 7 + i * 49, 78, 42, 13);
            T ("mix_lad" + String (i), "L", 7 + i * 49, 93, 42, 13);
        }
        Ctext ("SUB OCTAVE", 8, 110, 8.5f);
        S ("sub_oct", 8, 121, 70, 20, false);
        Ctext ("SUB WAVE", 92, 110, 8.5f);
        S ("sub_wave", 92, 121, 100, 20, false);
        Ctext ("S = SVF   L = LADDER", 206, 126, 8.0f);
        // Zusatz-Modulation
        M ("EXTRA MODULATION", 1260, 170, 300, 94, Acc::lfo);
        Ctext ("LFO 4 DESTINATION", 10, 18);
        Cb ("x_l4dest", 10, 30, 130, 18);
        K ("x_l4amt", "AMT", 152, 12, 30);
        Ctext ("MOD EG DESTINATION", 10, 54);
        Cb ("x_medest", 10, 66, 130, 18);
        K ("x_meamt", "AMT", 152, 50, 30);

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
        // Ausgang: KNACK-Clip oben, Volume darunter, kleiner Panic-Knopf (TEMPO nur in der Standalone-App)
        M ("OUTPUT", 1260, 484, 160, 250, Acc::fx);
        const bool standalone = proc.wrapperType == AudioProcessor::wrapperType_Standalone;
        const int kx = standalone ? 14 : 47;
        cur = Colour (0xffff5a3d);   // Knack
        K ("out_clip", "CLIP", kx, 16, 66, true);
        cur = Acc::fx;
        K ("out_vol", "VOLUME", kx, 106, 66, true);
        if (standalone) K ("out_tempo", "TEMPO", 104, 112, 42);
        auto* pan = new TextButton ("PANIC");
        owned.add (pan); addAndMakeVisible (pan);
        pan->setColour (TextButton::textColourOffId, Col::textDim);
        pan->setBounds (ox + 50, oy + 222, 60, 18);
        pan->onClick = [this] { proc.panic(); };

        // Step-Sequencer (Metropolis-Prinzip), 16 Stufen
        M ("STEP SEQUENCER", 1430, 484, 720, 250, Acc::seq);
        T ("seq_on", "SEQ ON", 4, 16, 68, 15);
        T ("seq_latch", "LATCH", 74, 16, 64, 15);
        Ctext ("RATE", 6, 38, 8.5f);
        Ctext ("ORDER", 74, 38, 8.5f);
        Cb ("seq_rate", 4, 49, 64, 18);
        Cb ("seq_order", 72, 49, 64, 18);
        K ("seq_len", "LEN", 2, 76, 30);
        K ("seq_gate", "GATE", 36, 76, 30);
        K ("seq_rnd", "RND", 70, 76, 30);
        K ("seq_velrnd", "VEL", 104, 76, 30);
        T ("seq_rndp", "RND", 4, 140, 60, 15);
        T ("seq_rndo", "RND", 4, 162, 60, 15);
        T ("seq_rnda", "RND", 4, 202, 60, 15);
        for (int i = 0; i < kSeqSteps; ++i)
        {
            const int x = 144 + i * 36;
            const String n (i + 1);
            stageX[i] = ox + x;
            auto* fp = new FaderCtl (apvts, "seq_p" + n, show, false, cur, true); owned.add (fp); addAndMakeVisible (fp); fp->setBounds (ox + x + 4, oy + 30, 28, 128);
            auto* fo = new OctBtn (apvts, "seq_o" + n, show, cur); owned.add (fo); addAndMakeVisible (fo); fo->setBounds (ox + x + 2, oy + 162, 32, 17);
            auto* sl = new StepModeBtn (apvts, "seq_sl" + n, show, cur, { "SLIDE", "SLIDE" }, 1); owned.add (sl); addAndMakeVisible (sl); sl->setBounds (ox + x + 2, oy + 182, 32, 17);
            auto* ac = new StepModeBtn (apvts, "seq_ac" + n, show, cur, { "ACC", "ACC" }, 1);     owned.add (ac); addAndMakeVisible (ac); ac->setBounds (ox + x + 2, oy + 202, 32, 17);
            auto* sm = new StepModeBtn (apvts, "seq_m" + n, show, cur); owned.add (sm); addAndMakeVisible (sm); sm->setBounds (ox + x + 2, oy + 222, 32, 18);
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
        {   // nach Chord Learn: Typ auf LEARNED und CHORD an
            const bool la = proc.learnActive.load();
            if (! la && lastLearnUi)
            {
                if (auto* p = apvts.getParameter ("v_ctype")) p->setValueNotifyingHost (0.0f);
                if (auto* p = apvts.getParameter ("v_chord")) p->setValueNotifyingHost (1.0f);
            }
            lastLearnUi = la;
        }
        presets.refresh();
        { const uint64_t lo = proc.genLo.load(), hi = proc.genHi.load(); if (lo != lastGenLo || hi != lastGenHi) { lastGenLo = lo; lastGenHi = hi; keys.repaint(); } }
        repaint (1430, 484, 720, 40);
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
            if (m.title == "DISPLAY")   // blauer, schimmernder Rahmen
            {
                for (int k = 6; k >= 1; --k)
                {
                    g.setColour (m.acc.withAlpha (0.05f + 0.02f * (float) (6 - k)));
                    g.drawRoundedRectangle (r.expanded ((float) k * 0.9f), 6.0f + (float) k, 1.8f);
                }
                g.setColour (m.acc.withAlpha (0.9f));
                g.drawRoundedRectangle (r, 6.0f, 1.6f);
                g.setColour (Colours::white.withAlpha (0.25f));
                g.drawRoundedRectangle (r.reduced (1.5f), 5.0f, 0.6f);
            }
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
            const int mx = 1430, my = 484;
            // 4er-Gruppen leicht absetzen
            for (int b = 0; b < 4; ++b)
            {
                g.setColour (b % 2 == 0 ? Colours::white.withAlpha (0.035f) : Colours::black.withAlpha (0.12f));
                g.fillRoundedRectangle (Rectangle<float> ((float) (mx + 144 + b * 144), (float) my + 5.0f, 144.0f, 240.0f).reduced (1.5f, 0.0f), 4.0f);
            }
            g.setColour (Col::textDim);
            g.setFont (F (7.5f));
            g.drawText ("SLIDE", Rectangle<int> (mx + 64, my + 185, 76, 10), Justification::centredRight, false);
            g.drawText ("ACCENT", Rectangle<int> (mx + 64, my + 205, 76, 10), Justification::centredRight, false);
            g.drawText ("OCTAVE", Rectangle<int> (mx + 64, my + 165, 76, 10), Justification::centredRight, false);
            g.drawText ("PITCH", Rectangle<int> (mx + 64, my + 143, 76, 10), Justification::centredRight, false);
            g.drawText ("STEP", Rectangle<int> (mx + 64, my + 226, 76, 10), Justification::centredRight, false);
            const int cur = proc.seqStage.load();
            const int len = (int) std::lround (apvts.getRawParameterValue ("seq_len")->load());
            for (int i = 0; i < TO::kSeqSteps; ++i)
            {
                g.setColour (i < len ? Col::text : Col::textDim.withAlpha (0.4f));
                g.setFont (F (8.5f));
                g.drawText (String (i + 1), Rectangle<int> (stageX[i], my + 9, 36, 10), Justification::centred, false);
                drawLed (g, Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) stageX[i] + 18.0f, (float) my + 24.0f }), i == cur, Acc::seq);
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
    PresetBar presets { proc };
    KeyboardView keys;
    WheelCtl pitchWheel, modWheel;
    OwnedArray<Component> owned;
    Array<WaveView*> waves;
    std::vector<Mod> mods;
    std::vector<Cap> caps;
    std::vector<FxUI> fx;
    KnobCtl *svfCut = nullptr, *ladCut = nullptr, *fEg = nullptr, *fLfo = nullptr, *fFm = nullptr;
    LedToggle *modSvf = nullptr, *modLad = nullptr;
    int stageX[TO::kSeqSteps] {};
    int lastSeqStage = -2;
    uint64_t lastGenLo = 0, lastGenHi = 0;
    bool lastLearnUi = false;
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
    setResizeLimits (880, 340, 3400, 1314);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (1760, 680);
}

TheOneEditor::~TheOneEditor() {}

void TheOneEditor::tickForTest() { content->tick(); }

void TheOneEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content->setTransform (AffineTransform::scale (s));
    content->setBounds (0, 0, kW, kH);
}

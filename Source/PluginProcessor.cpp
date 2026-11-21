#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

using namespace juce;

static String fmtValue (const TO::PDef& d, float v)
{
    using namespace TO;
    char b[64];
    switch (d.disp)
    {
        case dTen:    snprintf (b, sizeof (b), "%.1f", d.mn < 0 ? v * 10.0f : v * 10.0f); return b;
        case dPct:    snprintf (b, sizeof (b), "%.0f %%", v * 100.0f); return b;
        case dCurve:  snprintf (b, sizeof (b), "%+.0f %%", v * 100.0f); return b;
        case dCut:    { const float hz = cutoffHz (v); if (hz >= 1000.0f) snprintf (b, sizeof (b), "%.2f kHz", hz / 1000.0f); else snprintf (b, sizeof (b), "%.0f Hz", hz); return b; }
        case dTime:   { if (v <= 0.0005f) return "MIN"; const float t = envTimeSec (v); if (t < 1.0f) snprintf (b, sizeof (b), "%.0f ms", t * 1000.0f); else snprintf (b, sizeof (b), "%.2f s", t); return b; }
        case dDelay:  { if (v <= 0.0005f) return "OFF"; const float t = delayTimeSec (v); if (t < 1.0f) snprintf (b, sizeof (b), "%.0f ms", t * 1000.0f); else snprintf (b, sizeof (b), "%.2f s", t); return b; }
        case dSemi:   snprintf (b, sizeof (b), "%+.2f st", v); return b;
        case dBeatHz: snprintf (b, sizeof (b), "%+.2f Hz", v); return b;
        case dInt:    snprintf (b, sizeof (b), "%d", (int) std::lround (v)); return b;
        case dBpm:    snprintf (b, sizeof (b), "%.0f BPM", v); return b;
        case dLfoRate: snprintf (b, sizeof (b), "%.1f", v * 10.0f); return b;
        default:      snprintf (b, sizeof (b), "%.2f", v); return b;
    }
}

AudioProcessorValueTreeState::ParameterLayout TheOneProcessor::createLayout (const std::vector<TO::PDef>& defs)
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& d : defs)
    {
        const ParameterID pid (d.id, 1);
        if (d.kind == TO::kBool)
            layout.add (std::make_unique<AudioParameterBool> (pid, d.name, d.def > 0.5f));
        else if (d.kind == TO::kChoice)
        {
            StringArray ch; for (auto& c : d.choices) ch.add (c);
            layout.add (std::make_unique<AudioParameterChoice> (pid, d.name, ch, (int) d.def));
        }
        else
        {
            const TO::PDef dd = d;
            const float step = d.disp == TO::dInt ? 1.0f : 0.0f;
            auto attr = AudioParameterFloatAttributes().withStringFromValueFunction ([dd] (float v, int) { return fmtValue (dd, v); });
            layout.add (std::make_unique<AudioParameterFloat> (pid, d.name, NormalisableRange<float> (d.mn, d.mx, step), d.def, attr));
        }
    }
    return layout;
}

static std::vector<TO::PDef> gDefs() { return TO::makeDefs(); }

TheOneProcessor::TheOneProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "THEONE", createLayout (gDefs())),
      defs (gDefs())
{
    raw.resize ((size_t) TO::NUM_PARAMS, nullptr);
    for (size_t i = 0; i < defs.size(); ++i) raw[i] = apvts.getRawParameterValue (defs[i].id);
    P.fill (0.0f);
    for (auto& s : scope) s.store (0.0f);
    for (auto& c : chordNotes) c.store (0);
    chordNotes[0].store (0);
    scratch.setSize (2, 8192);
}

bool TheOneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == AudioChannelSet::stereo() && l.getMainInputChannelSet().isDisabled();
}

void TheOneProcessor::prepareToPlay (double sr, int blockSize)
{
    sampleRate = sr;
    synth.prepare (sr);
    fx.prepare (sr);
    scratch.setSize (2, std::max (blockSize, 8192), false, false, true);
    down.fill (false);
    lastLearn = false;
    limState = 1.0f;
}

void TheOneProcessor::applyChord (const std::vector<int>& iv)
{
    std::vector<int> c = iv;
    if (c.empty()) c.push_back (0);
    int i = 0;
    for (int v : c) if (i < 16) chordNotes[(size_t) i++].store (v);
    chordCount.store (std::min (16, (int) c.size()));
    {
        const SpinLock::ScopedLockType sl (chordLock);
        pendingChord = c; chordPending = true;
    }
}

void TheOneProcessor::clearChord() { applyChord ({ 0 }); }

String TheOneProcessor::chordToString() const
{
    StringArray a;
    const int n = chordCount.load();
    for (int i = 0; i < n; ++i) a.add (String (chordNotes[(size_t) i].load()));
    return a.joinIntoString (",");
}

void TheOneProcessor::setChordFromString (const String& s)
{
    std::vector<int> v;
    for (auto& t : StringArray::fromTokens (s, ",", ""))
        if (t.isNotEmpty()) v.push_back (t.getIntValue());
    applyChord (v);
}

void TheOneProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, numSamples);

    for (size_t i = 0; i < raw.size(); ++i) P[i] = raw[i]->load();
    synth.setParams (P.data());
    {
        const SpinLock::ScopedTryLockType sl (chordLock);
        if (sl.isLocked() && chordPending) { synth.setChord (pendingChord); chordPending = false; }
    }

    double bpm = P[(size_t) TO::OUT0 + TO::OUT_TEMPO];
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    hostBpm.store ((float) bpm);

    // Chord Learn: Flanke erkennen
    const bool learn = P[(size_t) TO::VOI0 + TO::V_LEARN] > 0.5f;
    const bool silent = P[(size_t) TO::VOI0 + TO::V_SILENT] > 0.5f;
    if (learn && ! lastLearn) { learnNotes.clear(); learnCount.store (0); }
    if (! learn && lastLearn)
    {
        if (! learnNotes.empty())
        {
            std::vector<int> iv;
            const int root = learnNotes.front();
            for (int n : learnNotes) iv.push_back (n - root);
            std::sort (iv.begin(), iv.end());
            iv.erase (std::unique (iv.begin(), iv.end()), iv.end());
            // Grundton (erste gespielte Note) steht fuer die Taste: Intervalle relativ dazu, 0 bleibt dabei
            applyChord (iv);
        }
    }
    lastLearn = learn;
    learnActive.store (learn);

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    float* L = scratch.getWritePointer (0);
    float* R = scratch.getWritePointer (1);
    int pos = 0;
    auto renderTo = [&] (int end)
    {
        if (end > pos)
        {
            synth.setBend (std::clamp (midiBend + uiBend.load(), -1.0f, 1.0f));
            synth.setModWheel (std::max (midiMod, uiMod.load()));
            synth.process (L + pos, R + pos, end - pos, bpm);
            pos = end;
        }
    };
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        renderTo (std::clamp (meta.samplePosition, 0, numSamples));
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            if (learn)
            {
                learnNotes.push_back (n); learnCount.store ((int) learnNotes.size());
            }
            if (! (learn && silent)) { synth.noteOn (n, m.getFloatVelocity()); down[(size_t) n] = true; }
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            if (down[(size_t) n]) { synth.noteOff (n); down[(size_t) n] = false; }
        }
        else if (m.isPitchWheel()) midiBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            if (cc == 1) midiMod = (float) m.getControllerValue() / 127.0f;
            else if (cc == 64) synth.setPedal (m.getControllerValue() >= 64);
        }
        if (m.isAllNotesOff() || m.isAllSoundOff()) { synth.allNotesOff(); down.fill (false); }
    }
    renderTo (numSamples);

    fx.process (L, R, numSamples, P.data(), bpm);

    // Master + sanfter Begrenzer
    const float vol = P[(size_t) TO::OUT0 + TO::OUT_VOL];
    const float gain = vol * vol * 2.2f;
    float peak = 0.0f;
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < numSamples; ++i)
    {
        float l = L[i] * gain, r = R[i] * gain;
        auto lim = [] (float x)
        {
            const float a = std::fabs (x);
            if (a <= 0.7f) return x;
            const float s = x < 0 ? -1.0f : 1.0f;
            return s * (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f));
        };
        l = lim (l); r = lim (r);
        outL[i] = l; if (outR) outR[i] = r;
        peak = std::max (peak, std::max (std::fabs (l), std::fabs (r)));
        if (++scopeDecim >= 4)
        {
            scopeDecim = 0;
            const int w = scopeW.load();
            scope[(size_t) w].store (0.5f * (l + r));
            scopeW.store ((w + 1) & 1023);
        }
    }
    outLevel.store (peak);
    voicesInUse.store (synth.activeVoices());
}

AudioProcessorEditor* TheOneProcessor::createEditor() { return new TheOneEditor (*this); }

void TheOneProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("chord", chordToString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TheOneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            apvts.replaceState (vt);
            if (vt.hasProperty ("chord")) setChordFromString (vt.getProperty ("chord").toString());
            // Learn nie aktiv laden
            if (auto* p = apvts.getParameter ("v_learn")) p->setValueNotifyingHost (0.0f);
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TheOneProcessor(); }

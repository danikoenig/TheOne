#include "PluginProcessor.h"
#include "FactoryPresets.h"
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
        case dStep:   snprintf (b, sizeof (b), "%+d st", (int) std::lround (v)); return b;
        case dBend:   snprintf (b, sizeof (b), "+/- %d st", (int) std::lround (v)); return b;
        case dOct:    { const int o = (int) std::lround (v); if (o == 0) return "0 oct"; snprintf (b, sizeof (b), "%+d oct", o); return b; }
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
            const float step = (d.disp == TO::dInt || d.disp == TO::dStep || d.disp == TO::dOct || d.disp == TO::dBend) ? 1.0f : 0.0f;
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
    volS.reset (sr, 0.03);
    volS.setCurrentAndTargetValue (0.64f * 3.2f);
    laLen = std::max (8, (int) std::lround (0.0015 * sr));
    laL.assign ((size_t) laLen, 0.0f); laR.assign ((size_t) laLen, 0.0f); laPos = 0;
    limGain = 1.0f;
    limAtt = std::exp (-3.0f / (float) laLen);
    limRel = std::exp (-1.0f / (0.15f * (float) sr));
    os = std::make_unique<dsp::Oversampling<float>> (2, 2, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os->initProcessing ((size_t) std::max (blockSize, 8192));
    os->reset();
    clipS.reset (sr * 4.0, 0.03); clipS.setCurrentAndTargetValue (1.0f);
    setLatencySamples (laLen + (int) std::lround (os->getLatencyInSamples()));
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

std::vector<int> TheOneProcessor::buildChord (const std::vector<int>& learned, int type, int inv, int voicing, int bass)
{
    static const std::vector<int> types[19] = {
        {}, { 0, 4, 7 }, { 0, 3, 7 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 4, 7, 10 }, { 0, 4, 7, 9 }, { 0, 3, 7, 9 },
        { 0, 4, 7, 14 }, { 0, 4, 7, 11, 14 }, { 0, 3, 7, 10, 14 }, { 0, 4, 7, 10, 14 }, { 0, 2, 7 }, { 0, 5, 7 },
        { 0, 3, 6 }, { 0, 3, 6, 9 }, { 0, 3, 6, 10 }, { 0, 4, 8 }, { 0, 7, 12 } };
    std::vector<int> n = (type <= 0 || type > 18) ? learned : types[type];
    if (n.empty()) n.push_back (0);
    std::sort (n.begin(), n.end());
    n.erase (std::unique (n.begin(), n.end()), n.end());
    const int cnt = (int) n.size();
    // Inversion: die untersten Toene eine Oktave nach oben
    for (int i = 0; i < std::min (inv, cnt - 1); ++i) n[(size_t) i] += 12;
    std::sort (n.begin(), n.end());
    // Voicing
    if (cnt >= 3)
    {
        if (voicing == 1)      for (int i = 1; i < cnt; i += 2) n[(size_t) i] += 12;                // Open: jeder zweite Ton hoch
        else if (voicing == 2) n[(size_t) cnt - 2] -= 12;                                         // Drop 2
        else if (voicing == 3) { for (int i = 1; i < cnt; i += 2) n[(size_t) i] += 12; n[0] -= 12; }   // Wide: Open, tiefster Ton eine Oktave runter
        std::sort (n.begin(), n.end());
    }
    // Bassnote unter dem Grundton der Taste
    if (bass > 0)
    {
        const int b = -12 * bass;
        if (std::find (n.begin(), n.end(), b) == n.end()) n.insert (n.begin(), b);
    }
    return n;
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

//==============================================================================
// Werks-Presets (eigene Sounds, keine Presets des Originals): Source/FactoryPresets.h
namespace { const std::vector<TO::Prog>& programs() { return TO::factoryPrograms(); } }

int TheOneProcessor::getNumPrograms() { return (int) programs().size(); }
const String TheOneProcessor::getProgramName (int i) { return String (programs()[(size_t) jlimit (0, getNumPrograms() - 1, i)].name); }

void TheOneProcessor::setCurrentProgram (int idx)
{
    idx = jlimit (0, getNumPrograms() - 1, idx);
    currentProgram = idx;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
        {
            const String id = rp->paramID;
            if (id.startsWith ("out_") || id == "v_hold" || id == "v_learn" || id == "v_silent") continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    const auto& prog = programs()[(size_t) idx];
    for (auto& kv : prog.v)
        if (auto* rp = apvts.getParameter (kv.first))
        {
            float v = kv.second;
            if (String (kv.first).startsWith ("mix_lvl")) v = jlimit (0.0f, 1.0f, v * prog.trim);   // Pegelabgleich
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        }
    apvts.state.setProperty ("presetName", String (programs()[(size_t) idx].name), nullptr);
    apvts.state.setProperty ("presetFile", "", nullptr);
}

void TheOneProcessor::panic()
{
    if (auto* h = apvts.getParameter ("v_hold")) h->setValueNotifyingHost (0.0f);
    keyboardState.allNotesOff (0);
    panicFlag.store (true);
}

//==============================================================================
// Step-Sequencer: gehaltene Taste transponiert die Sequenz (Grundton C3 = 60)
void TheOneProcessor::runSequencer (MidiBuffer& midi, int numSamples, double bpm, bool learn, bool hostPlaying, double ppq)
{
    using namespace TO;
    const bool transStart = hostPlaying && ! prevHostPlaying, transStop = ! hostPlaying && prevHostPlaying;
    prevHostPlaying = hostPlaying;
    const bool on = P[SEQ0 + SQ_ON] > 0.5f && ! learn;
    if (! on)
    {
        if (seqSounding >= 0) { midi.addEvent (MidiMessage::noteOff (1, seqSounding), 0); seqSounding = -1; }
        seqKeys.clear(); seqRunning = false; seqStage.store (-1);
        return;
    }
    const bool latch = P[SEQ0 + SQ_LATCH] > 0.5f;
    const int len = jlimit (1, kSeqSteps, (int) std::lround (P[SEQ0 + SQ_LEN]));
    const int order = (int) std::lround (P[SEQ0 + SQ_ORDER]);
    auto modeOf = [&] (int s) { return (int) std::lround (P[SEQ0 + SQ_MODE0 + s]); };
    auto nextStage = [&] (int s) -> int
    {
        int cand[kSeqSteps], nc = 0;
        for (int i = 0; i < len; ++i) if (modeOf (i) != 2) cand[nc++] = i;
        if (nc == 0) return jlimit (0, len - 1, s);
        if (nc == 1) return cand[0];
        if (order == 3)
        {
            int r = cand[seqRng.nextInt (nc)];
            for (int t = 0; t < 4 && r == s; ++t) r = cand[seqRng.nextInt (nc)];
            return r;
        }
        if (order == 2)
        {
            int i = s;
            for (int t = 0; t < 2 * len + 2; ++t)
            {
                int j = i + seqDir;
                if (j >= len || j < 0) { seqDir = -seqDir; j = i + seqDir; }
                i = jlimit (0, len - 1, j);
                if (modeOf (i) != 2) return i;
            }
            return cand[0];
        }
        const int d = order == 1 ? -1 : 1;
        for (int k = 1; k <= len; ++k) { const int i = ((s + d * k) % len + len) % len; if (modeOf (i) != 2) return i; }
        return cand[0];
    };
    auto firstStage = [&]() -> int
    {
        seqDir = 1;
        if (order == 1) return nextStage (0);          // rueckwaerts: letzte aktive Stufe
        if (order == 3) return nextStage (-1);
        for (int i = 0; i < len; ++i) if (modeOf (i) != 2) return i;   // vorwaerts / Ping: erste aktive Stufe
        return 0;
    };
    const double pulse = jmax (16.0, 60.0 / jmax (20.0, bpm) * (double) kSeqBeats[jlimit (0, kSeqRates - 1, (int) std::lround (P[SEQ0 + SQ_RATE]))] * sampleRate);
    const double gateLen = jmax (8.0, pulse * (double) P[SEQ0 + SQ_GATE]);

    MidiBuffer out;
    auto noteOffAt = [&] (int pos) { if (seqSounding >= 0) { out.addEvent (MidiMessage::noteOff (1, seqSounding), pos); seqSounding = -1; } };
    // Step ausloesen: Slide vom vorherigen Step gleitet ohne Neuanschlag
    auto firePulse = [&] (int pos)
    {
        const int st = seqStageIdx;
        if (modeOf (st) != 0) { noteOffAt (pos); seqOffIn = -1.0; seqPrevSlide = false; return; }   // MUTE/SKIP
        // Zufall: Pitch von einem anderen aktiven Step, Oktave springt, Accent kippt
        const float chance = P[SEQ0 + SQ_RND];
        auto roll = [&] { return chance > 0.0f && seqRng.nextFloat() < chance; };
        int pitch = (int) std::lround (P[SEQ0 + SQ_PITCH0 + st]);
        int oct = (int) std::lround (P[SEQ0 + SQ_OCT0 + st]);
        bool acc = P[SEQ0 + SQ_ACC0 + st] > 0.5f;
        if (P[SEQ0 + SQ_RNDP] > 0.5f && roll())
        {
            int cand[kSeqSteps], nc = 0;
            for (int i = 0; i < len; ++i) if (i != st && modeOf (i) == 0) cand[nc++] = i;
            if (nc > 0) pitch = (int) std::lround (P[SEQ0 + SQ_PITCH0 + cand[seqRng.nextInt (nc)]]);
        }
        if (P[SEQ0 + SQ_RNDO] > 0.5f && roll())
        {
            const int d = seqRng.nextBool() ? 1 : -1;
            oct = (oct + d > 1 || oct + d < -1) ? oct - d : oct + d;
        }
        if (P[SEQ0 + SQ_RNDA] > 0.5f && roll()) acc = ! acc;
        const int note = jlimit (0, 127, seqRoot + pitch + 12 * oct);
        float vel = acc ? 1.0f : 0.62f;
        if (const float vr = P[SEQ0 + SQ_VELRND]; vr > 0.0f)   // Velocity-Zufall: 0 = fest
            vel = jlimit (0.08f, 1.0f, vel * (1.0f + (seqRng.nextFloat() * 2.0f - 1.0f) * vr * 0.45f));
        if (seqPrevSlide && seqSounding >= 0)
        {
            if (note != seqSounding) out.addEvent (MidiMessage::controllerEvent (16, seqSounding, note), pos);
            seqSounding = note;
        }
        else
        {
            noteOffAt (pos);
            out.addEvent (MidiMessage::noteOn (1, note, vel), pos);
            seqSounding = note;
        }
        seqPrevSlide = P[SEQ0 + SQ_SLIDE0 + st] > 0.5f;
        seqOffIn = seqPrevSlide ? -1.0 : gateLen;
    };
    double cur = 0.0;
    auto advanceTo = [&] (int end)
    {
        const double e = (double) end;
        while (seqRunning)
        {
            const double nextPulse = cur + seqToPulse;
            const double nextOff = seqOffIn >= 0.0 ? cur + seqOffIn : 1e18;
            const double next = jmin (nextPulse, nextOff);
            if (next >= e) break;
            const double el = next - cur;
            seqToPulse -= el; if (seqOffIn >= 0.0) seqOffIn -= el;
            cur = next;
            const int at = jlimit (0, numSamples - 1, (int) cur);
            if (seqOffIn >= 0.0 && seqOffIn <= 1e-9) { noteOffAt (at); seqOffIn = -1.0; }
            if (seqToPulse <= 1e-9)
            {
                seqToPulse += pulse;
                seqStageIdx = nextStage (seqStageIdx);
                firePulse (at);
            }
        }
        if (seqRunning && e > cur)
        {
            const double el = e - cur;
            seqToPulse -= el; if (seqOffIn >= 0.0) seqOffIn -= el;
        }
        cur = jmax (cur, e);
    };

    if (seqStageIdx >= len) seqStageIdx = nextStage (seqStageIdx % len);
    const double rateBeats = (double) kSeqBeats[jlimit (0, kSeqRates - 1, (int) std::lround (P[SEQ0 + SQ_RATE]))];

    // DAW-Transport: Stop haelt den Sequencer an, Start startet ihn neu ab Step 1 (im Takt der DAW)
    if (transStop && seqRunning) { noteOffAt (0); seqRunning = false; seqOffIn = -1.0; }
    if (transStart && (seqRunning || (latch ? seqHadRoot : ! seqKeys.isEmpty())))
    {
        noteOffAt (0);
        seqRunning = true; seqStageIdx = firstStage(); seqOffIn = -1.0; seqPrevSlide = false;
        seqToPulse = pulse;
        firePulse (0);
    }
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = jlimit (0, numSamples - 1, meta.samplePosition);
        if (! m.isNoteOnOrOff()) { advanceTo (pos); out.addEvent (m, pos); continue; }
        advanceTo (pos);
        const int n = m.getNoteNumber();
        if (m.isNoteOn())
        {
            seqKeys.removeAllInstancesOf (n);
            seqKeys.add (n);
            seqVel = m.getFloatVelocity();
            const bool wasRunning = seqRunning;
            seqRoot = n;   // Taste = Grundton der Sequenz
            seqHadRoot = true;
            if (! wasRunning)
            {
                seqRunning = true; seqStageIdx = firstStage(); seqOffIn = -1.0; seqPrevSlide = false;
                seqToPulse = pulse;
                if (hostPlaying && ppq >= 0.0)
                {
                    // laeuft die DAW, rastet der naechste Step auf das Raster der DAW ein
                    const double ppqAt = ppq + (double) pos / sampleRate * bpm / 60.0;
                    const double fr = std::fmod (ppqAt, rateBeats);
                    const double rest = (rateBeats - fr) * 60.0 / jmax (20.0, bpm) * sampleRate;
                    if (rest > 8.0) seqToPulse = rest;
                }
                firePulse (pos);
            }
        }
        else
        {
            seqKeys.removeAllInstancesOf (n);
            if (! seqKeys.isEmpty()) seqRoot = seqKeys.getLast();
            else if (! latch) { noteOffAt (pos); seqRunning = false; }
        }
    }
    advanceTo (numSamples);
    seqStage.store (seqRunning ? seqStageIdx : -1);
    midi.swapWith (out);
}

void TheOneProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, numSamples);

    for (size_t i = 0; i < raw.size(); ++i) P[i] = raw[i]->load();
    synth.setParams (P.data());
    if (panicFlag.exchange (false)) { seqRunning = false; seqKeys.clear(); seqSounding = -1; seqStage.store (-1); synth.panic(); down.fill (false); fx.prepare (getSampleRate() > 0 ? getSampleRate() : 48000.0); }
    {
        const SpinLock::ScopedTryLockType sl (chordLock);
        if (sl.isLocked() && chordPending) { learnedIv = pendingChord; chordPending = false; learnedDirty = true; }
    }

    {   // Akkord aus Typ (oder gelerntem Akkord), Inversion, Voicing und Bassnote
        const int ty = (int) std::lround (P[(size_t) TO::VOI0 + TO::V_CTYPE]);
        const int inv = (int) std::lround (P[(size_t) TO::VOI0 + TO::V_CINV]);
        const int vo = (int) std::lround (P[(size_t) TO::VOI0 + TO::V_CVOICE]);
        const int ba = (int) std::lround (P[(size_t) TO::VOI0 + TO::V_CBASS]);
        const int key = ((ty * 8 + inv) * 8 + vo) * 8 + ba;
        if (key != lastChordKey || learnedDirty)
        {
            synth.setChord (buildChord (learnedIv, ty, inv, vo, ba));
            lastChordKey = key; learnedDirty = false;
        }
    }

    double bpm = P[(size_t) TO::OUT0 + TO::OUT_TEMPO];
    bool hostPlaying = false;
    double ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    hostBpm.store ((float) bpm);

    // Chord Learn: Flanke erkennen
    // LEARN lernt hoerbar, SILENT lernt ohne Ton: beide Tasten sind eigene Lernmodi
    const bool silent = P[(size_t) TO::VOI0 + TO::V_SILENT] > 0.5f;
    const bool learn = P[(size_t) TO::VOI0 + TO::V_LEARN] > 0.5f || silent;
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
    runSequencer (midi, numSamples, bpm, learn, hostPlaying, ppq);

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
        else if (m.isController() && m.getChannel() == 16)   // interner Sequencer-Slide
        {
            const int from = m.getControllerNumber(), to = m.getControllerValue();
            synth.slide (from, to);
            if (down[(size_t) from]) { down[(size_t) from] = false; down[(size_t) to] = true; }
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
    { uint64_t lo, hi; synth.soundingNotes (lo, hi); genLo.store (lo); genHi.store (hi); }

    fx.process (L, R, numSamples, P.data(), bpm);

    // KNACK-Clipper (4x Oversampling): CLIP treibt bis +24 dB in die Knack-Kurve, bei 0 unveraendert.
    // Bezugspegel = Ausgang bei VOLUME 8, damit der Clip-Einsatz nicht vom Volume abhaengt.
    {
        const float refG = 0.64f * 3.2f;
        const float clip = P[(size_t) TO::OUT0 + TO::OUT_CLIP];
        clipS.setTargetValue (Decibels::decibelsToGain (clip * 24.0f));
        float* chans[2] = { L, R };
        dsp::AudioBlock<float> blk (chans, 2, (size_t) numSamples);
        auto up = os->processSamplesUp (blk);
        const int nUp = (int) up.getNumSamples();
        float* uL = up.getChannelPointer (0);
        float* uR = up.getChannelPointer (1);
        constexpr float k = 6.8f;   // Knack SOFT/HARD in der Mitte
        auto sh = [] (float x) { const float ax = std::fabs (x); return x / std::pow (1.0f + std::pow (ax, k), 1.0f / k); };
        for (int i = 0; i < nUp; ++i)
        {
            const float d = clipS.getNextValue();
            if (d > 1.0005f)
            {
                uL[i] = sh (uL[i] * refG * d) / refG;
                uR[i] = sh (uR[i] * refG * d) / refG;
            }
        }
        os->processSamplesDown (blk);
    }

    // Master (Init um -9 dBFS, Akkorde mit Luft) + Limiter mit Vorschau, Decke -1 dBFS
    const float vol = P[(size_t) TO::OUT0 + TO::OUT_VOL];
    volS.setTargetValue (vol * vol * 3.2f);
    const float ceil = 0.89f;
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);
    auto lim = [] (float x)
    {
        const float a = std::fabs (x);
        if (a <= 0.95f) return x;   // nur Notbremse, der Limiter haelt die Spitzen bei -1 dBFS
        const float sg = x < 0 ? -1.0f : 1.0f;
        return sg * (0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f));
    };
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = volS.getNextValue();
        const float inL = L[i] * g, inR = R[i] * g;
        const float pk = std::max (std::fabs (inL), std::fabs (inR));
        const float target = pk > ceil ? ceil / pk : 1.0f;
        limGain = target < limGain ? target + (limGain - target) * limAtt : target + (limGain - target) * limRel;
        const float dl = laL[(size_t) laPos], dr = laR[(size_t) laPos];
        laL[(size_t) laPos] = inL; laR[(size_t) laPos] = inR;
        if (++laPos >= laLen) laPos = 0;
        float l = lim (dl * limGain), r = lim (dr * limGain);
        outL[i] = l; outR[i] = r;
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

//==============================================================================
File TheOneProcessor::presetRoot()
{
    auto d = File::getSpecialLocation (File::userMusicDirectory).getChildFile ("Dani Koenig").getChildFile ("The One").getChildFile ("Presets");
    d.createDirectory();
    return d;
}

Array<TheOneProcessor::PresetEntry> TheOneProcessor::scanPresets() const
{
    Array<PresetEntry> out;
    for (int i = 0; i < (int) programs().size(); ++i)
        out.add ({ String (programs()[(size_t) i].name), "Factory", File(), i });
    const auto root = presetRoot();
    Array<File> files = root.findChildFiles (File::findFiles, true, "*.theone");
    Array<PresetEntry> user;
    for (auto& f : files)
    {
        const auto parent = f.getParentDirectory();
        user.add ({ f.getFileNameWithoutExtension(), parent == root ? String() : parent.getRelativePathFrom (root), f, -1 });
    }
    std::sort (user.begin(), user.end(), [] (const PresetEntry& a, const PresetEntry& b)
               { return a.category != b.category ? a.category.compareIgnoreCase (b.category) < 0 : a.name.compareIgnoreCase (b.name) < 0; });
    out.addArray (user);
    return out;
}

bool TheOneProcessor::savePreset (const String& name, const String& category)
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto dir = presetRoot();
    if (category.trim().isNotEmpty()) dir = dir.getChildFile (File::createLegalFileName (category.trim()));
    dir.createDirectory();
    const auto f = dir.getChildFile (clean + ".theone");
    apvts.state.setProperty ("presetName", clean, nullptr);
    apvts.state.setProperty ("presetFile", f.getFullPathName(), nullptr);
    auto state = apvts.copyState();
    state.setProperty ("chord", chordToString(), nullptr);
    state.removeProperty ("presetFile", nullptr);
    if (auto xml = state.createXml()) return xml->writeTo (f);
    return false;
}

bool TheOneProcessor::loadPreset (const PresetEntry& e)
{
    if (e.factory >= 0)
    {
        setCurrentProgram (e.factory);
        apvts.state.setProperty ("presetName", e.name, nullptr);
        apvts.state.setProperty ("presetFile", "", nullptr);
        return true;
    }
    auto xml = XmlDocument::parse (e.file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto vt = ValueTree::fromXml (*xml);
    const float tempo = apvts.getRawParameterValue ("out_tempo")->load();
    apvts.replaceState (vt);
    if (vt.hasProperty ("chord")) setChordFromString (vt.getProperty ("chord").toString());
    if (auto* p = apvts.getParameter ("v_learn")) p->setValueNotifyingHost (0.0f);
    if (auto* p = apvts.getParameter ("out_tempo")) p->setValueNotifyingHost (p->convertTo0to1 (tempo));   // Tempo gehoert nicht zum Sound
    apvts.state.setProperty ("presetName", e.name, nullptr);
    apvts.state.setProperty ("presetFile", e.file.getFullPathName(), nullptr);
    return true;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TheOneProcessor(); }

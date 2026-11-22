#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
struct Preset
{
    const char* name;
    float autotune, speed, robot, formant, transpose, fine;
    bool monotone; // lock to the single note of the current key
};

const Preset kPresets[] = {
    // name              tune  speed robot  form  trans fine  mono
    { "Natural Tune",     55.f,  90.f,   0.f,  0.f,   0.f, 0.f, false },
    { "Pop Tune",        100.f,  30.f,   0.f,  0.f,   0.f, 0.f, false },
    { "Hard Tune",       100.f,   0.f,   0.f,  0.f,   0.f, 0.f, false },
    { "Robot Voice",     100.f,   0.f,  60.f,  0.f,   0.f, 0.f, false },
    { "Extreme Robot",   100.f,   0.f, 100.f,  0.f,   0.f, 0.f, false },
    { "Monotone Robot",  100.f,   0.f, 100.f,  0.f,   0.f, 0.f, true  },
    { "Deep Robot",      100.f,   0.f,  85.f, -4.f, -12.f, 0.f, false },
    { "Alien",           100.f,   0.f,  45.f,  5.f,   7.f, 0.f, false },
};

String fmtSigned (float v, int dp, const char* unit)
{
    return String (v > 0.0f ? "+" : "") + String (v, dp) + unit;
}
} // namespace

AutoladdProcessor::AutoladdProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "autoladd", createLayout())
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pAutotune = raw (ids::autotune);
    pSpeed = raw (ids::speed);
    pRobot = raw (ids::robot);
    pFormant = raw (ids::formant);
    pTranspose = raw (ids::transpose);
    pFine = raw (ids::fine);
    pMix = raw (ids::mix);
    pGain = raw (ids::gain);
    pSens = raw (ids::sensitivity);
    pGate = raw (ids::gate);
    pRefA = raw (ids::refA);
    pKey = raw (ids::key);
    pScale = raw (ids::scale);
    pBypass = raw (ids::bypass);
    pMidi = raw (ids::midiTarget);
    for (int i = 0; i < 12; ++i)
        pNote[i] = apvts.getRawParameterValue (ids::note (i));
}

AudioProcessorValueTreeState::ParameterLayout AutoladdProcessor::createLayout()
{
    using FloatP = AudioParameterFloat;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const char* name, NormalisableRange<float> range, float def,
                               std::function<String (float, int)> toText) {
        layout.add (std::make_unique<FloatP> (ParameterID { id, 1 }, name, range, def,
                                              AudioParameterFloatAttributes()
                                                  .withStringFromValueFunction (std::move (toText))
                                                  .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); })));
    };

    addFloat (ids::autotune, "Autotune", { 0.0f, 100.0f, 0.1f }, 100.0f,
              [] (float v, int) { return String (roundToInt (v)) + " %"; });
    addFloat (ids::speed, "Retune Speed", { 0.0f, 400.0f, 0.1f, 0.4f }, 0.0f,
              [] (float v, int) { return v < 0.5f ? String ("Instant") : String (roundToInt (v)) + " ms"; });
    addFloat (ids::robot, "Robot", { 0.0f, 100.0f, 0.1f }, 40.0f,
              [] (float v, int) { return String (roundToInt (v)) + " %"; });
    addFloat (ids::formant, "Formant", { -12.0f, 12.0f, 0.1f }, 0.0f,
              [] (float v, int) { return fmtSigned (v, 1, " st"); });
    addFloat (ids::transpose, "Transpose", { -24.0f, 24.0f, 1.0f }, 0.0f,
              [] (float v, int) { return fmtSigned (v, 0, " st"); });
    addFloat (ids::fine, "Fine Tune", { -100.0f, 100.0f, 1.0f }, 0.0f,
              [] (float v, int) { return fmtSigned (v, 0, " ct"); });
    addFloat (ids::mix, "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f,
              [] (float v, int) { return String (roundToInt (v)) + " %"; });
    addFloat (ids::gain, "Output Gain", { -24.0f, 12.0f, 0.1f }, 0.0f,
              [] (float v, int) { return fmtSigned (v, 1, " dB"); });
    addFloat (ids::sensitivity, "Sensitivity", { 0.0f, 100.0f, 0.1f }, 45.0f,
              [] (float v, int) { return String (roundToInt (v)) + " %"; });
    addFloat (ids::gate, "Gate", { -80.0f, -20.0f, 0.1f }, -60.0f,
              [] (float v, int) { return String (v, 0) + " dB"; });
    addFloat (ids::refA, "Reference A", { 415.0f, 466.0f, 0.1f }, 440.0f,
              [] (float v, int) { return String (v, 1) + " Hz"; });

    StringArray keys, scales;
    for (auto* n : autoladd::kNoteNames) keys.add (n);
    for (auto& s : autoladd::kScales) scales.add (s.name);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::key, 1 }, "Key", keys, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::scale, 1 }, "Scale", scales, 0));

    for (int i = 0; i < 12; ++i)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::note (i), 1 },
                                                          String ("Note ") + autoladd::kNoteNames[i], true));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::midiTarget, 1 }, "MIDI Note Target", false));
    return layout;
}

bool AutoladdProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

void AutoladdProcessor::prepareToPlay (double sampleRate, int)
{
    engineChannels = jlimit (1, 2, getTotalNumInputChannels());
    engine.prepare (sampleRate, 80.0, engineChannels);
    setLatencySamples (engine.getLatency());

    mixSmooth.reset (sampleRate, 0.02);
    gainSmooth.reset (sampleRate, 0.02);
    mixSmooth.setCurrentAndTargetValue (*pBypass > 0.5f ? 0.0f : *pMix * 0.01f);
    gainSmooth.setCurrentAndTargetValue (Decibels::decibelsToGain ((float) *pGain));

    std::fill (std::begin (heldCount), std::end (heldCount), 0);
    heldMask.store (0);
}

uint16_t AutoladdProcessor::getEffectiveMask() const
{
    const uint16_t held = heldMask.load (std::memory_order_relaxed);
    if (*pMidi > 0.5f && held != 0)
        return held;

    const int sc = jlimit (0, autoladd::kNumScales - 1, (int) *pScale);
    if (sc == autoladd::kCustomScaleIndex)
    {
        uint16_t m = 0;
        for (int i = 0; i < 12; ++i)
            if (*pNote[i] > 0.5f)
                m = (uint16_t) (m | (1u << i));
        return m;
    }
    return autoladd::absoluteMaskFor (sc, jlimit (0, 11, (int) *pKey));
}

void AutoladdProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = jmin (2, buffer.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);
    if (numCh == 0)
        return;

    // MIDI note target: the notes you hold become the notes the voice is tuned to
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
            ++heldCount[m.getNoteNumber() % 12];
        else if (m.isNoteOff())
            heldCount[m.getNoteNumber() % 12] = jmax (0, heldCount[m.getNoteNumber() % 12] - 1);
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            std::fill (std::begin (heldCount), std::end (heldCount), 0);
    }
    uint16_t held = 0;
    for (int i = 0; i < 12; ++i)
        if (heldCount[i] > 0)
            held = (uint16_t) (held | (1u << i));
    heldMask.store (held, std::memory_order_relaxed);

    autoladd::EngineParams p;
    p.amount = *pAutotune * 0.01f;
    p.speedMs = *pSpeed;
    p.robot = *pRobot * 0.01f;
    p.formantSemis = *pFormant;
    p.transposeSemis = *pTranspose + *pFine * 0.01f;
    p.refA = *pRefA;
    p.sensitivity = 0.05f + 0.35f * (*pSens * 0.01f);
    p.gateDb = *pGate;
    p.noteMask = getEffectiveMask();
    engine.setParams (p);

    mixSmooth.setTargetValue (*pBypass > 0.5f ? 0.0f : *pMix * 0.01f);
    gainSmooth.setTargetValue (Decibels::decibelsToGain ((float) *pGain));

    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float in[2] = { ch0[i], ch1 != nullptr ? ch1[i] : ch0[i] };
        float wet[2] {}, dry[2] {};
        engine.processSample (in, wet, dry);

        const float mix = mixSmooth.getNextValue();
        const float g = gainSmooth.getNextValue();
        ch0[i] = (dry[0] + mix * (wet[0] - dry[0])) * g;
        if (ch1 != nullptr)
        {
            const int c = engineChannels > 1 ? 1 : 0;
            ch1[i] = (dry[c] + mix * (wet[c] - dry[c])) * g;
        }
    }
}

PitchDisplay AutoladdProcessor::getPitchDisplay() const
{
    return { engine.getDisplayNoteIn(), engine.getDisplayNoteOut(), engine.getDisplayVoiced() };
}

void AutoladdProcessor::setParam (const String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float norm = p->convertTo0to1 (plainValue);
        if (std::abs (p->getValue() - norm) > 1.0e-6f)
            p->setValueNotifyingHost (norm);
    }
}

void AutoladdProcessor::setNoteToggles (uint16_t mask)
{
    for (int i = 0; i < 12; ++i)
        setParam (ids::note (i), ((mask >> i) & 1) ? 1.0f : 0.0f);
}

void AutoladdProcessor::setCustomMask (uint16_t mask)
{
    setNoteToggles (mask);
    setParam (ids::scale, (float) autoladd::kCustomScaleIndex);
}

const StringArray& AutoladdProcessor::getPresetNames()
{
    static const StringArray names = [] {
        StringArray s;
        for (auto& p : kPresets) s.add (p.name);
        return s;
    }();
    return names;
}

void AutoladdProcessor::applyPreset (int index)
{
    if (index < 0 || index >= (int) std::size (kPresets))
        return;
    const auto& p = kPresets[index];
    setParam (ids::autotune, p.autotune);
    setParam (ids::speed, p.speed);
    setParam (ids::robot, p.robot);
    setParam (ids::formant, p.formant);
    setParam (ids::transpose, p.transpose);
    setParam (ids::fine, p.fine);
    setParam (ids::mix, 100.0f);
    setParam (ids::bypass, 0.0f);
    if (p.monotone)
        setCustomMask ((uint16_t) (1u << jlimit (0, 11, (int) *pKey)));
}

UiSettings AutoladdProcessor::getUiSettings() const
{
    UiSettings s;
    const auto& st = apvts.state;
    s.theme = (int) st.getProperty ("uiTheme", s.theme);
    s.hue = (float) (double) st.getProperty ("uiHue", s.hue);
    s.glass = (float) (double) st.getProperty ("uiGlass", s.glass);
    s.scale = (float) (double) st.getProperty ("uiScale", s.scale);
    s.showNames = (bool) st.getProperty ("uiNames", s.showNames);
    return s;
}

void AutoladdProcessor::setUiSettings (const UiSettings& s)
{
    auto st = apvts.state;
    st.setProperty ("uiTheme", s.theme, nullptr);
    st.setProperty ("uiHue", (double) s.hue, nullptr);
    st.setProperty ("uiGlass", (double) s.glass, nullptr);
    st.setProperty ("uiScale", (double) s.scale, nullptr);
    st.setProperty ("uiNames", s.showNames, nullptr);
}

void AutoladdProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void AutoladdProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* AutoladdProcessor::createEditor()
{
    return new AutoladdEditor (*this);
}

// Entry point used by the plugin wrappers (not compiled into the offline test harness).
#ifdef JucePlugin_Name
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoladdProcessor();
}
#endif

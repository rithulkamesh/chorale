#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor::BusesProperties ChoraleProcessor::choraleBuses()
{
    auto buses = BusesProperties()
                     .withInput ("Input", juce::AudioChannelSet::mono(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     // Aux stems, disabled by default: stereo hosts see no change,
                     // multi-out hosts (Logic Multi-Output, Reaper, Bitwig...)
                     // enable them on demand.
                     .withOutput ("Lead", juce::AudioChannelSet::stereo(), false);
    for (int v = 1; v <= HarmonyEngine::kNumVoices; ++v)
        buses = buses.withOutput ("Voice " + juce::String (v),
                                  juce::AudioChannelSet::stereo(), false);
    return buses;
}

ChoraleProcessor::ChoraleProcessor()
    : AudioProcessor (choraleBuses()),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pDryWet = apvts.getRawParameterValue ("dryWet");
    pKeyRoot = apvts.getRawParameterValue ("keyRoot");
    pScale = apvts.getRawParameterValue ("scale");
    pCorrect = apvts.getRawParameterValue ("correct");
    pHumanize = apvts.getRawParameterValue ("humanize");
    pTone = apvts.getRawParameterValue ("tone");
    pWidth = apvts.getRawParameterValue ("width");
    pEchoTime = apvts.getRawParameterValue ("echoTime");
    pEchoFb = apvts.getRawParameterValue ("echoFb");
    pEchoMix = apvts.getRawParameterValue ("echoMix");
    for (int v = 0; v < kNumVoices; ++v)
    {
        const auto s = juce::String (v + 1);
        pMode[v] = apvts.getRawParameterValue ("v" + s + "Mode");
        pDegree[v] = apvts.getRawParameterValue ("v" + s + "Degree");
        pNote[v] = apvts.getRawParameterValue ("v" + s + "Note");
        pGain[v] = apvts.getRawParameterValue ("v" + s + "Gain");
        pPan[v] = apvts.getRawParameterValue ("v" + s + "Pan");
        pDetune[v] = apvts.getRawParameterValue ("v" + s + "Detune");
        pSolo[v] = apvts.getRawParameterValue ("v" + s + "Solo");
        pMute[v] = apvts.getRawParameterValue ("v" + s + "Mute");
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ChoraleProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        "dryWet", "Mix", NormalisableRange<float> (0.0f, 1.0f), 0.45f));
    layout.add (std::make_unique<AudioParameterChoice> (
        "keyRoot", "Key Root",
        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (
        "scale", "Scale",
        StringArray { "Auto", "Major", "Minor", "Dorian", "Phrygian", "Lydian",
                      "Mixolydian", "Locrian", "Chromatic" },
        0));
    layout.add (std::make_unique<AudioParameterChoice> (
        "correct", "Correct", StringArray { "Off", "Natural", "Hard" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (
        "humanize", "Humanize", NormalisableRange<float> (0.0f, 1.0f), 0.25f));
    layout.add (std::make_unique<AudioParameterFloat> (
        "tone", "Tone", NormalisableRange<float> (500.0f, 20000.0f, 0.0f, 0.35f), 20000.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        "width", "Width", NormalisableRange<float> (0.0f, 2.0f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        "echoTime", "Echo Time", NormalisableRange<float> (0.0f, 1000.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        "echoFb", "Echo Feedback", NormalisableRange<float> (0.0f, 0.9f), 0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (
        "echoMix", "Echo Mix", NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    StringArray degrees;
    const char* names[] = { "Oct", "7th", "6th", "5th", "4th", "3rd", "2nd" };
    for (int i = 0; i < 7; ++i)
        degrees.add (String (names[i]) + " Down");
    degrees.add ("Unison");
    for (int i = 6; i >= 0; --i)
        degrees.add (String (names[i]) + " Up");

    StringArray notes;
    const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int m = 36; m < 84; ++m) // C2..B5
        notes.add (String (pcs[m % 12]) + String (m / 12 - 1));

    // Default = "Pop Stack": 3rd up left, 5th up right.
    const int defMode[8] = { 1, 1, 0, 0, 0, 0, 0, 0 };
    const int defDegree[8] = { 9, 11, 7, 7, 7, 7, 7, 7 };
    const float defGain[8] = { 0.8f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f };
    const float defPan[8] = { -0.4f, 0.4f, -0.7f, 0.7f, -0.2f, 0.2f, -0.9f, 0.9f };

    for (int v = 0; v < kNumVoices; ++v)
    {
        const auto s = String (v + 1);
        layout.add (std::make_unique<AudioParameterChoice> (
            "v" + s + "Mode", "Voice " + s + " Mode",
            StringArray { "Off", "Scale", "Note", "MIDI" }, defMode[v]));
        layout.add (std::make_unique<AudioParameterChoice> (
            "v" + s + "Degree", "Voice " + s + " Interval", degrees, defDegree[v]));
        layout.add (std::make_unique<AudioParameterChoice> (
            "v" + s + "Note", "Voice " + s + " Note", notes, 57 - 36)); // A3
        layout.add (std::make_unique<AudioParameterFloat> (
            "v" + s + "Gain", "Voice " + s + " Gain",
            NormalisableRange<float> (0.0f, 1.0f), defGain[v]));
        layout.add (std::make_unique<AudioParameterFloat> (
            "v" + s + "Pan", "Voice " + s + " Pan",
            NormalisableRange<float> (-1.0f, 1.0f), defPan[v]));
        layout.add (std::make_unique<AudioParameterFloat> (
            "v" + s + "Detune", "Voice " + s + " Detune",
            NormalisableRange<float> (-50.0f, 50.0f), 0.0f));
        layout.add (std::make_unique<AudioParameterBool> (
            "v" + s + "Solo", "Voice " + s + " Solo", false));
        layout.add (std::make_unique<AudioParameterBool> (
            "v" + s + "Mute", "Voice " + s + " Mute", false));
    }
    return layout;
}

void ChoraleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    scratchIn.assign ((size_t) samplesPerBlock, 0.0f);
    scratchR.assign ((size_t) samplesPerBlock, 0.0f);
    setLatencySamples (engine.latencySamples());
}

bool ChoraleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono())
        return false;
    const auto main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo())
        return false;
    // Aux stem buses: each either disabled or stereo.
    for (int b = 1; b < layouts.outputBuses.size(); ++b)
    {
        const auto& set = layouts.outputBuses.getReference (b);
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void ChoraleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            engine.noteOn (msg.getNoteNumber());
        else if (msg.isNoteOff())
            engine.noteOff (msg.getNoteNumber());
    }

    HarmonySettings s;
    s.dryWet = *pDryWet;
    s.keyRoot = (int) *pKeyRoot;
    s.scaleMode = (int) *pScale;
    s.correct = (int) *pCorrect;
    s.humanize = *pHumanize;
    s.tone = *pTone;
    s.width = *pWidth;
    s.echoTime = *pEchoTime;
    s.echoFb = *pEchoFb;
    s.echoMix = *pEchoMix;
    for (int v = 0; v < kNumVoices; ++v)
        s.voices[v] = { (int) *pMode[v], (int) *pDegree[v], 36 + (int) *pNote[v],
                        *pGain[v], *pPan[v], *pDetune[v],
                        *pSolo[v] > 0.5f, *pMute[v] > 0.5f };
    engine.setSettings (s);

    if ((int) scratchIn.size() < n)
    {
        scratchIn.assign ((size_t) n, 0.0f); // host sent a bigger block than promised
        scratchR.assign ((size_t) n, 0.0f);
    }
    std::copy_n (buffer.getReadPointer (0), n, scratchIn.data());

    HarmonyEngine::MultiOut out;
    auto mainBus = getBusBuffer (buffer, false, 0);
    out.mainL = mainBus.getWritePointer (0);
    out.mainR = mainBus.getNumChannels() > 1 ? mainBus.getWritePointer (1) : scratchR.data();
    for (int b = 1; b < getBusCount (false); ++b)
    {
        if (! getBus (false, b)->isEnabled())
            continue;
        auto aux = getBusBuffer (buffer, false, b);
        if (aux.getNumChannels() < 2)
            continue;
        if (b == 1)
        {
            out.leadL = aux.getWritePointer (0);
            out.leadR = aux.getWritePointer (1);
        }
        else
        {
            out.voiceL[b - 2] = aux.getWritePointer (0);
            out.voiceR[b - 2] = aux.getWritePointer (1);
        }
    }
    engine.process (scratchIn.data(), out, n);

    const auto est = engine.lastPitch();
    uiF0.store (est.voiced ? est.f0 : 0.0f);
    uiRoot.store (engine.detectedRootPc());
    uiMinor.store (engine.detectedMinor());
    uiLevel.store (engine.inputLevel());
    for (int v = 0; v < kNumVoices; ++v)
    {
        uiVoiceHz[v].store (engine.voiceTargetHz (v));
        uiVoiceGain[v].store (engine.voiceLevel (v));
    }
}

void ChoraleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChoraleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ChoraleProcessor::createEditor()
{
    return new ChoraleEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChoraleProcessor();
}

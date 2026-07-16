#include "PluginProcessor.h"

OpenHarmonyProcessor::OpenHarmonyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pDryWet = apvts.getRawParameterValue ("dryWet");
    pKeyRoot = apvts.getRawParameterValue ("keyRoot");
    pScale = apvts.getRawParameterValue ("scale");
    pMidiMode = apvts.getRawParameterValue ("midiMode");
    for (int v = 0; v < 4; ++v)
    {
        const auto s = juce::String (v + 1);
        pInterval[v] = apvts.getRawParameterValue ("v" + s + "Interval");
        pGain[v] = apvts.getRawParameterValue ("v" + s + "Gain");
        pPan[v] = apvts.getRawParameterValue ("v" + s + "Pan");
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout OpenHarmonyProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        "dryWet", "Dry/Wet", NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    layout.add (std::make_unique<AudioParameterChoice> (
        "keyRoot", "Key Root",
        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (
        "scale", "Scale",
        StringArray { "Auto", "Major", "Minor", "Dorian", "Phrygian", "Lydian",
                      "Mixolydian", "Locrian", "Chromatic" },
        0));
    layout.add (std::make_unique<AudioParameterBool> ("midiMode", "MIDI Mode", false));

    const StringArray intervals { "Off", "Oct Down", "5th Down", "3rd Down",
                                  "Unison", "3rd Up", "5th Up", "Oct Up" };
    const int defaultInterval[4] = { 5, 6, 1, 0 }; // 3rd up, 5th up, oct down (muted), off
    const float defaultGain[4] = { 0.8f, 0.6f, 0.0f, 0.0f };
    const float defaultPan[4] = { -0.3f, 0.3f, -0.6f, 0.6f };

    for (int v = 0; v < 4; ++v)
    {
        const auto s = juce::String (v + 1);
        layout.add (std::make_unique<AudioParameterChoice> (
            "v" + s + "Interval", "Voice " + s + " Interval", intervals, defaultInterval[v]));
        layout.add (std::make_unique<AudioParameterFloat> (
            "v" + s + "Gain", "Voice " + s + " Gain",
            NormalisableRange<float> (0.0f, 1.0f), defaultGain[v]));
        layout.add (std::make_unique<AudioParameterFloat> (
            "v" + s + "Pan", "Voice " + s + " Pan",
            NormalisableRange<float> (-1.0f, 1.0f), defaultPan[v]));
    }
    return layout;
}

void OpenHarmonyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    scratchIn.assign ((size_t) samplesPerBlock, 0.0f);
    scratchR.assign ((size_t) samplesPerBlock, 0.0f);
    setLatencySamples (engine.latencySamples());
}

bool OpenHarmonyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void OpenHarmonyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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
    s.midiMode = *pMidiMode > 0.5f;
    for (int v = 0; v < 4; ++v)
        s.voices[v] = { (int) *pInterval[v], *pGain[v], *pPan[v] };
    engine.setSettings (s);

    if ((int) scratchIn.size() < n)
    {
        scratchIn.assign ((size_t) n, 0.0f); // host sent a bigger block than promised
        scratchR.assign ((size_t) n, 0.0f);
    }
    std::copy_n (buffer.getReadPointer (0), n, scratchIn.data());

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : scratchR.data();
    engine.process (scratchIn.data(), outL, outR, n);
}

void OpenHarmonyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OpenHarmonyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* OpenHarmonyProcessor::createEditor()
{
    // ponytail: generic parameter editor until step 9 (custom UI).
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenHarmonyProcessor();
}

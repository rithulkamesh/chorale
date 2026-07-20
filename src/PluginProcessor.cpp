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
      apvts (*this, &undoManager, "PARAMS", createParameterLayout())
{
    pDryWet = apvts.getRawParameterValue ("dryWet");
    pKeyRoot = apvts.getRawParameterValue ("keyRoot");
    pScale = apvts.getRawParameterValue ("scale");
    pCorrect = apvts.getRawParameterValue ("correct");
    pHumanize = apvts.getRawParameterValue ("humanize");
    pTone = apvts.getRawParameterValue ("tone");
    pWidth = apvts.getRawParameterValue ("width");
    pLatMode = apvts.getRawParameterValue ("latMode");
    for (int e = 0; e < 2; ++e)
    {
        const auto s = juce::String (e + 1);
        pEchoTime[e] = apvts.getRawParameterValue ("echo" + s + "Time");
        pEchoFb[e] = apvts.getRawParameterValue ("echo" + s + "Fb");
        pEchoMix[e] = apvts.getRawParameterValue ("echo" + s + "Mix");
        pVerbSize[e] = apvts.getRawParameterValue ("verb" + s + "Size");
        pVerbMix[e] = apvts.getRawParameterValue ("verb" + s + "Mix");
    }
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
        pEqOn[v] = apvts.getRawParameterValue ("v" + s + "EqOn");
        for (int b = 0; b < 8; ++b)
        {
            pEqF[v][b] = apvts.getRawParameterValue ("v" + s + "Eq" + juce::String (b + 1) + "F");
            pEqG[v][b] = apvts.getRawParameterValue ("v" + s + "Eq" + juce::String (b + 1) + "G");
        }
        pCompOn[v] = apvts.getRawParameterValue ("v" + s + "CompOn");
        pSatOn[v] = apvts.getRawParameterValue ("v" + s + "SatOn");
        pSatDrive[v] = apvts.getRawParameterValue ("v" + s + "SatDrive");
        pSatMix[v] = apvts.getRawParameterValue ("v" + s + "SatMix");
        pCompT[v] = apvts.getRawParameterValue ("v" + s + "CompT");
        pCompR[v] = apvts.getRawParameterValue ("v" + s + "CompR");
    }
    pMEqOn = apvts.getRawParameterValue ("mEqOn");
    for (int b = 0; b < 8; ++b)
    {
        pMEqF[b] = apvts.getRawParameterValue ("mEq" + juce::String (b + 1) + "F");
        pMEqG[b] = apvts.getRawParameterValue ("mEq" + juce::String (b + 1) + "G");
    }
    pMSatOn = apvts.getRawParameterValue ("mSatOn");
    pMSatDrive = apvts.getRawParameterValue ("mSatDrive");
    pMSatMix = apvts.getRawParameterValue ("mSatMix");
    pMCompOn = apvts.getRawParameterValue ("mCompOn");
    pMCompT = apvts.getRawParameterValue ("mCompT");
    pMCompR = apvts.getRawParameterValue ("mCompR");
    pMidiAdapt = apvts.getRawParameterValue ("midiAdapt");
    for (int g = 0; g < 4; ++g)
        pGainLevel[g] = apvts.getRawParameterValue ("gain" + juce::String (g + 1) + "Level");

    rebuildGraph();
}

//==============================================================================
juce::ValueTree ChoraleProcessor::graphTreeIfPresent() const
{
    return apvts.state.getChildWithName ("GRAPH");
}

juce::ValueTree ChoraleProcessor::graphTree()
{
    auto t = apvts.state.getChildWithName ("GRAPH");
    if (! t.isValid())
    {
        t = juce::ValueTree ("GRAPH");
        apvts.state.appendChild (t, nullptr);
    }
    // Materialize the default patch on first touch so edits start from it.
    if (t.getNumChildren() == 0)
        for (const auto& e : graph::defaultEdges())
        {
            juce::ValueTree edge ("EDGE");
            edge.setProperty ("from", e.from, nullptr);
            edge.setProperty ("to", e.to, nullptr);
            t.appendChild (edge, nullptr);
        }
    return t;
}

std::vector<graph::Edge> ChoraleProcessor::graphEdges() const
{
    const auto t = graphTreeIfPresent();
    if (! t.isValid())
        return graph::defaultEdges(); // untouched state -> default patch
    std::vector<graph::Edge> edges;
    for (const auto& c : t)
        if (c.hasType ("EDGE"))
            edges.push_back ({ (int) c.getProperty ("from"), (int) c.getProperty ("to") });
    // A GRAPH tree with no edges but placed nodes is a deliberately empty
    // patch; no children at all means "not initialized yet".
    if (edges.empty() && t.getNumChildren() == 0)
        return graph::defaultEdges();
    return edges;
}

bool ChoraleProcessor::graphAddEdge (int from, int to)
{
    auto edges = graphEdges();
    if (std::find (edges.begin(), edges.end(), graph::Edge { from, to }) != edges.end())
        return true;
    edges.push_back ({ from, to });
    if (! graph::compile (edges).valid)
        return false;
    auto t = graphTree();
    juce::ValueTree edge ("EDGE");
    edge.setProperty ("from", from, nullptr);
    edge.setProperty ("to", to, nullptr);
    t.appendChild (edge, nullptr);
    rebuildGraph();
    return true;
}

void ChoraleProcessor::graphRemoveEdge (int from, int to)
{
    auto t = graphTree();
    for (int i = t.getNumChildren(); --i >= 0;)
    {
        auto c = t.getChild (i);
        if (c.hasType ("EDGE") && (int) c.getProperty ("from") == from
            && (int) c.getProperty ("to") == to)
            t.removeChild (i, nullptr);
    }
    rebuildGraph();
}

void ChoraleProcessor::graphResetToDefault()
{
    auto t = apvts.state.getChildWithName ("GRAPH");
    if (t.isValid())
        apvts.state.removeChild (t, nullptr);
    rebuildGraph();
}

bool ChoraleProcessor::nodeOnCanvas (int id) const
{
    for (const auto& e : graphEdges())
        if (e.from == id || e.to == id)
            return true;
    const auto t = graphTreeIfPresent();
    if (t.isValid())
        for (const auto& c : t)
            if (c.hasType ("NODE") && (int) c.getProperty ("id") == id)
                return true;
    return false;
}

juce::Point<int> ChoraleProcessor::nodePos (int id, juce::Point<int> fallback) const
{
    const auto t = graphTreeIfPresent();
    if (t.isValid())
        for (const auto& c : t)
            if (c.hasType ("NODE") && (int) c.getProperty ("id") == id)
                return { (int) c.getProperty ("x"), (int) c.getProperty ("y") };
    return fallback;
}

void ChoraleProcessor::setNodePos (int id, juce::Point<int> p)
{
    auto t = graphTree();
    for (auto c : t)
        if (c.hasType ("NODE") && (int) c.getProperty ("id") == id)
        {
            c.setProperty ("x", p.x, nullptr);
            c.setProperty ("y", p.y, nullptr);
            return;
        }
    juce::ValueTree n ("NODE");
    n.setProperty ("id", id, nullptr);
    n.setProperty ("x", p.x, nullptr);
    n.setProperty ("y", p.y, nullptr);
    t.appendChild (n, nullptr);
}

void ChoraleProcessor::ensureNodeVisible (int id, juce::Point<int> p)
{
    if (! nodeOnCanvas (id))
        setNodePos (id, p);
}

namespace
{
float paramValue (const juce::ValueTree& state, const juce::String& id, float def)
{
    for (const auto& c : state)
        if (c.hasType ("PARAM") && c.getProperty ("id").toString() == id)
            return (float) c.getProperty ("value", def);
    return def;
}

void setParamValue (juce::ValueTree& state, const juce::String& id, float v)
{
    for (auto c : state)
        if (c.hasType ("PARAM") && c.getProperty ("id").toString() == id)
        {
            c.setProperty ("value", v, nullptr);
            return;
        }
    juce::ValueTree p ("PARAM");
    p.setProperty ("id", id, nullptr);
    p.setProperty ("value", v, nullptr);
    state.appendChild (p, nullptr);
}
} // namespace

void ChoraleProcessor::migrateState (juce::ValueTree& state)
{
    if (! state.isValid() || state.getChildWithName ("GRAPH").isValid())
        return;
    // Pre-1.2 states always serialized the wet-bus echo params; their absence
    // means this is just an untouched 1.2 state (nothing to do).
    if (paramValue (state, "echoTime", -1.0f) < 0.0f)
        return;

    const float echoTime = paramValue (state, "echoTime", 0.0f);
    const float echoFb = paramValue (state, "echoFb", 0.35f);
    const float echoMix = paramValue (state, "echoMix", 0.0f);
    const float verbSize = paramValue (state, "verbSize", 0.5f);
    const float verbMix = paramValue (state, "verbMix", 0.0f);
    float sendE[8], sendV[8], maxSendV = 0.0f;
    bool anyEchoSend = false, anyVerbSend = false;
    for (int v = 0; v < 8; ++v)
    {
        const auto s = juce::String (v + 1);
        sendE[v] = paramValue (state, "v" + s + "SendEcho", 0.0f);
        sendV[v] = paramValue (state, "v" + s + "SendVerb", 0.0f);
        anyEchoSend = anyEchoSend || sendE[v] > 0.05f;
        anyVerbSend = anyVerbSend || sendV[v] > 0.05f;
        maxSendV = std::max (maxSendV, sendV[v]);
    }
    // Same audibility gates the old wet bus used.
    const bool echoOn = echoTime > 1.0f && (echoMix > 0.001f || anyEchoSend);
    const bool verbOn = verbMix > 0.001f || anyVerbSend;

    // Rebuild the fixed chains as a wired patch: per voice, only the enabled
    // modules (bypassed ones passed through anyway). Lanes that fed echo /
    // reverb (via bus mix or their own send) route through the new nodes;
    // both nodes are additive so dry still reaches OUT at unity. Send levels
    // are approximated by the node's single mix knob.
    juce::ValueTree g ("GRAPH");
    auto addEdge = [&g] (int from, int to)
    {
        juce::ValueTree e ("EDGE");
        e.setProperty ("from", from, nullptr);
        e.setProperty ("to", to, nullptr);
        g.appendChild (e, nullptr);
    };
    for (int v = 0; v < 8; ++v)
    {
        const auto s = juce::String (v + 1);
        int cur = graph::kVoice0 + v;
        if (paramValue (state, "v" + s + "EqOn", 0.0f) > 0.5f)
        {
            addEdge (cur, graph::kEq0 + v);
            cur = graph::kEq0 + v;
        }
        if (paramValue (state, "v" + s + "CompOn", 0.0f) > 0.5f)
        {
            addEdge (cur, graph::kComp0 + v);
            cur = graph::kComp0 + v;
        }
        if (paramValue (state, "v" + s + "SatOn", 0.0f) > 0.5f)
        {
            addEdge (cur, graph::kSat0 + v);
            cur = graph::kSat0 + v;
        }
        int target = graph::kOut;
        if (verbOn && (verbMix > 0.001f || sendV[v] > 0.05f))
            target = graph::kVerb0;
        if (echoOn && (echoMix > 0.001f || sendE[v] > 0.05f))
            target = graph::kEcho0;
        addEdge (cur, target);
    }
    if (echoOn)
        addEdge (graph::kEcho0, verbOn ? graph::kVerb0 : graph::kOut);
    if (verbOn)
        addEdge (graph::kVerb0, graph::kOut);
    state.appendChild (g, nullptr);

    if (echoOn)
    {
        setParamValue (state, "echo1Time", echoTime);
        setParamValue (state, "echo1Fb", echoFb);
        // The old bus returned max(echoMix, 0.85 when driven by sends only).
        setParamValue (state, "echo1Mix", std::max (echoMix, anyEchoSend ? 0.85f : 0.0f));
    }
    if (verbOn)
    {
        setParamValue (state, "verb1Size", verbSize);
        setParamValue (state, "verb1Mix", std::max (verbMix, maxSendV));
    }
}

void ChoraleProcessor::rebuildGraph()
{
    auto plan = std::make_unique<graph::Plan> (graph::compile (graphEdges()));
    if (! plan->valid) // corrupt state: fall back rather than go silent
        *plan = graph::compile (graph::defaultEdges());
    activePlan.store (plan.get());
    planKeepalive.push_back (std::move (plan));
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
    layout.add (std::make_unique<AudioParameterChoice> (
        "latMode", "Latency", StringArray { "Studio", "Live" }, 0));

    // ECHO / VERB graph-node pool.
    for (int e = 0; e < 2; ++e)
    {
        const auto s = String (e + 1);
        layout.add (std::make_unique<AudioParameterFloat> (
            "echo" + s + "Time", "Echo " + s + " Time",
            NormalisableRange<float> (0.0f, 1000.0f), 350.0f));
        layout.add (std::make_unique<AudioParameterFloat> (
            "echo" + s + "Fb", "Echo " + s + " Feedback",
            NormalisableRange<float> (0.0f, 0.9f), 0.35f));
        layout.add (std::make_unique<AudioParameterFloat> (
            "echo" + s + "Mix", "Echo " + s + " Mix",
            NormalisableRange<float> (0.0f, 1.0f), 0.5f));
        layout.add (std::make_unique<AudioParameterFloat> (
            "verb" + s + "Size", "Reverb " + s + " Size",
            NormalisableRange<float> (0.0f, 1.0f), 0.5f));
        layout.add (std::make_unique<AudioParameterFloat> (
            "verb" + s + "Mix", "Reverb " + s + " Mix",
            NormalisableRange<float> (0.0f, 1.0f), 0.35f));
    }

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

        // Channel chain: EQ and compressor are opt-in modules.
        layout.add (std::make_unique<AudioParameterBool> (
            "v" + s + "EqOn", "Voice " + s + " EQ On", false));
        layout.add (std::make_unique<AudioParameterBool> (
            "v" + s + "CompOn", "Voice " + s + " Comp On", false));
        layout.add (std::make_unique<AudioParameterBool> (
            "v" + s + "SatOn", "Voice " + s + " Saturation On", false));
        layout.add (std::make_unique<AudioParameterFloat> (
            "v" + s + "SatDrive", "Voice " + s + " Sat Drive",
            NormalisableRange<float> (0.0f, 1.0f), 0.3f));
        layout.add (std::make_unique<AudioParameterFloat> (
            "v" + s + "SatMix", "Voice " + s + " Sat Mix",
            NormalisableRange<float> (0.0f, 1.0f), 1.0f));
        static const float defF[8] = { 80, 200, 500, 1200, 2500, 5000, 9000, 14000 };
        for (int b = 0; b < 8; ++b)
        {
            const auto bs = String (b + 1);
            layout.add (std::make_unique<AudioParameterFloat> (
                "v" + s + "Eq" + bs + "F", "Voice " + s + " EQ " + bs + " Freq",
                NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), defF[b]));
            layout.add (std::make_unique<AudioParameterFloat> (
                "v" + s + "Eq" + bs + "G", "Voice " + s + " EQ " + bs + " Gain",
                NormalisableRange<float> (-12.0f, 12.0f), 0.0f));
        }
        layout.add (std::make_unique<AudioParameterFloat> (
            "v" + s + "CompT", "Voice " + s + " Comp Threshold",
            NormalisableRange<float> (-40.0f, 0.0f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (
            "v" + s + "CompR", "Voice " + s + " Comp Ratio",
            NormalisableRange<float> (1.0f, 8.0f), 2.0f));
    }

    // Master section.
    {
        layout.add (std::make_unique<AudioParameterBool> ("mEqOn", "Master EQ On", false));
        static const float defF[8] = { 80, 200, 500, 1200, 2500, 5000, 9000, 14000 };
        for (int b = 0; b < 8; ++b)
        {
            const auto bs = String (b + 1);
            layout.add (std::make_unique<AudioParameterFloat> (
                "mEq" + bs + "F", "Master EQ " + bs + " Freq",
                NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), defF[b]));
            layout.add (std::make_unique<AudioParameterFloat> (
                "mEq" + bs + "G", "Master EQ " + bs + " Gain",
                NormalisableRange<float> (-12.0f, 12.0f), 0.0f));
        }
    }
    layout.add (std::make_unique<AudioParameterBool> ("mSatOn", "Master Saturation On", false));
    layout.add (std::make_unique<AudioParameterFloat> (
        "mSatDrive", "Master Sat Drive", NormalisableRange<float> (0.0f, 1.0f), 0.3f));
    layout.add (std::make_unique<AudioParameterFloat> (
        "mSatMix", "Master Sat Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<AudioParameterBool> ("mCompOn", "Master Comp On", false));
    layout.add (std::make_unique<AudioParameterFloat> (
        "mCompT", "Master Comp Threshold", NormalisableRange<float> (-40.0f, 0.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        "mCompR", "Master Comp Ratio", NormalisableRange<float> (1.0f, 8.0f), 2.0f));
    layout.add (std::make_unique<AudioParameterBool> (
        "midiAdapt", "MIDI Adapt", false));
    for (int g = 0; g < 4; ++g)
        layout.add (std::make_unique<AudioParameterFloat> (
            "gain" + String (g + 1) + "Level", "Gain " + String (g + 1),
            NormalisableRange<float> (0.0f, 2.0f), 1.0f));
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
    s.lowLatency = *pLatMode > 0.5f;
    for (int e = 0; e < 2; ++e)
    {
        s.echo[e].time = *pEchoTime[e];
        s.echo[e].fb = *pEchoFb[e];
        s.echo[e].mix = *pEchoMix[e];
        s.verb[e].size = *pVerbSize[e];
        s.verb[e].mix = *pVerbMix[e];
    }
    s.mCompOn = *pMCompOn > 0.5f;
    s.mCompThresh = *pMCompT;
    s.mCompRatio = *pMCompR;
    s.mSatOn = *pMSatOn > 0.5f;
    s.mSatDrive = *pMSatDrive;
    s.mSatMix = *pMSatMix;
    s.midiAdapt = *pMidiAdapt > 0.5f;
    for (int g = 0; g < 4; ++g)
        s.gainLevel[g] = *pGainLevel[g];
    engine.setGraph (activePlan.load());
    s.mEqOn = *pMEqOn > 0.5f;
    for (int b = 0; b < 8; ++b)
    {
        s.mEqF[b] = *pMEqF[b];
        s.mEqG[b] = *pMEqG[b];
    }
    for (int v = 0; v < kNumVoices; ++v)
    {
        auto& vs = s.voices[v];
        vs.mode = (int) *pMode[v];
        vs.degree = (int) *pDegree[v];
        vs.note = 36 + (int) *pNote[v];
        vs.gain = *pGain[v];
        vs.pan = *pPan[v];
        vs.detune = *pDetune[v];
        vs.solo = *pSolo[v] > 0.5f;
        vs.mute = *pMute[v] > 0.5f;
        vs.eqOn = *pEqOn[v] > 0.5f;
        for (int b = 0; b < 8; ++b)
        {
            vs.eqF[b] = *pEqF[v][b];
            vs.eqG[b] = *pEqG[v][b];
        }
        vs.compOn = *pCompOn[v] > 0.5f;
        vs.satOn = *pSatOn[v] > 0.5f;
        vs.satDrive = *pSatDrive[v];
        vs.satMix = *pSatMix[v];
        vs.compThresh = *pCompT[v];
        vs.compRatio = *pCompR[v];
    }
    engine.setSettings (s);
    if (getLatencySamples() != engine.latencySamples())
        setLatencySamples (engine.latencySamples());

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

void ChoraleProcessor::toggleAB()
{
    auto current = apvts.copyState();
    if (abStored.isValid())
        apvts.replaceState (abStored);
    // First toggle: the other slot starts as a copy, so nothing audibly jumps.
    abStored = current;
    abActive.store (abActive.load() ^ 1);
    rebuildGraph(); // the graph rides in the state tree
}

void ChoraleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("uiScale", (double) uiScale.load());
        copyXmlToBinary (*xml, destData);
    }
}

void ChoraleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        uiScale.store ((float) xml->getDoubleAttribute ("uiScale", 1.0));
        auto tree = juce::ValueTree::fromXml (*xml);
        migrateState (tree);
        apvts.replaceState (tree);
        rebuildGraph();
    }
}

juce::AudioProcessorEditor* ChoraleProcessor::createEditor()
{
    return new ChoraleEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChoraleProcessor();
}

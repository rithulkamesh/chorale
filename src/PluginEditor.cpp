#include "PluginEditor.h"
#include "Presets.h"

using namespace juce;

//==============================================================================
OpenHarmonyLookAndFeel::OpenHarmonyLookAndFeel()
{
    setColour (Slider::rotarySliderFillColourId, ui::kText);
    setColour (Label::textColourId, ui::kText);
    setColour (PopupMenu::backgroundColourId, ui::kPanelHi);
    setColour (PopupMenu::textColourId, ui::kText);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3a4254));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (PopupMenu::headerTextColourId, ui::kDim);
    setColour (ComboBox::textColourId, ui::kText);
}

void OpenHarmonyLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                               float pos, float a0, float a1, Slider& s)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = a0 + pos * (a1 - a0);
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);

    Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0, a0, a1, true);
    g.setColour (ui::kPanelHi);
    g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    Path fill;
    // Bipolar knobs (pan/detune) fill from centre.
    const bool bipolar = s.getMinimum() < 0.0;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (angle - from) > 0.01f)
    {
        fill.addCentredArc (centre.x, centre.y, radius, radius, 0,
                            jmin (from, angle), jmax (from, angle), true);
        g.setColour (accent);
        g.strokePath (fill, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const auto tip = centre.getPointOnCircumference (radius - 6.0f, angle);
    g.setColour (ui::kText);
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (tip));
}

void OpenHarmonyLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h,
                                               float pos, float, float, Slider::SliderStyle style,
                                               Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const float cx = (float) x + w / 2.0f;
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    g.setColour (ui::kPanelHi);
    g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) h, 3.0f);
    g.setColour (accent.withAlpha (0.85f));
    g.fillRoundedRectangle (cx - 3.0f, pos, 6.0f, (float) y + h - pos, 3.0f);
    g.setColour (ui::kText);
    g.fillRoundedRectangle (cx - 10.0f, pos - 3.0f, 20.0f, 6.0f, 3.0f);
}

void OpenHarmonyLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int,
                                           ComboBox& box)
{
    const auto r = Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (ui::kPanelHi);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? ui::kDim : Colour (0xff30364a));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h / 2.0f - 1.5f;
    arrow.addTriangle (ax, ay, ax + 7.0f, ay, ax + 3.5f, ay + 4.5f);
    g.setColour (ui::kDim);
    g.fillPath (arrow);
}

Font OpenHarmonyLookAndFeel::getComboBoxFont (ComboBox&) { return Font (FontOptions (13.0f)); }
Font OpenHarmonyLookAndFeel::getPopupMenuFont() { return Font (FontOptions (13.0f)); }

//==============================================================================
Visualizer::Visualizer (OpenHarmonyProcessor& p) : processor (p)
{
    particles.reserve (400);
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void Visualizer::spawn (float hz, float intensity, Colour c)
{
    if (particles.size() >= 380)
        return;
    // Pitch -> horizontal position, ~C2 (65 Hz) .. ~C6 (1046 Hz) log-mapped.
    const float nx = jlimit (0.03f, 0.97f, std::log2 (hz / 65.0f) / 4.0f);
    Particle pt;
    pt.x = nx * (float) getWidth() + rng.nextFloat() * 14.0f - 7.0f;
    pt.y = (float) getHeight() * (0.30f + rng.nextFloat() * 0.45f);
    pt.vx = (rng.nextFloat() - 0.5f) * 10.0f;
    pt.vy = -(5.0f + rng.nextFloat() * 14.0f);
    pt.age = 0.0f;
    pt.life = 1.2f + rng.nextFloat() * 1.6f;
    pt.size = 1.2f + rng.nextFloat() * 2.4f;
    pt.baseAlpha = jlimit (0.15f, 0.95f, 0.25f + intensity);
    pt.colour = c;
    particles.push_back (pt);
}

void Visualizer::timerCallback()
{
    constexpr float dt = 1.0f / 30.0f;
    time += dt;

    const float level = processor.uiLevel.load();
    smoothedLevel += 0.25f * (level - smoothedLevel);
    const float drive = jlimit (0.0f, 1.0f, smoothedLevel * 8.0f);

    // Lead voice particles (neutral white-blue) + one set per sounding harmony voice.
    if (const float f0 = processor.uiF0.load(); f0 > 0.0f && drive > 0.02f)
        for (int k = 0; k < 1 + (int) (drive * 2.0f); ++k)
            spawn (f0, drive * 0.7f, Colour (0xffdfe6f5));

    for (int v = 0; v < OpenHarmonyProcessor::kNumVoices; ++v)
    {
        const float hz = processor.uiVoiceHz[v].load();
        const float g = processor.uiVoiceGain[v].load();
        if (hz > 0.0f && g > 0.01f && drive > 0.02f)
            for (int k = 0; k < 1 + (int) (drive * g * 3.0f); ++k)
                spawn (hz, drive * g, ui::kVoice[v]);
    }

    // Sparse ambient dust so the panel never looks dead.
    if (particles.size() < 50 && rng.nextFloat() < 0.4f)
    {
        spawn (65.0f * std::exp2 (rng.nextFloat() * 4.0f), 0.08f, Colour (0xff39415a));
        particles.back().vy *= 0.4f;
    }

    for (auto& pt : particles)
    {
        pt.age += dt;
        // Gentle curl so the cloud swirls instead of rising uniformly.
        pt.vx += std::sin (time * 0.9f + pt.y * 0.020f) * 7.0f * dt;
        pt.vy += std::cos (time * 0.7f + pt.x * 0.015f) * 5.0f * dt;
        pt.x += pt.vx * dt;
        pt.y += pt.vy * dt;
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(),
                                     [this] (const Particle& pt)
                                     { return pt.age >= pt.life || pt.y < -8.0f
                                              || pt.x < -8.0f || pt.x > getWidth() + 8.0f; }),
                     particles.end());
    repaint();
}

void Visualizer::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0a0c10));
    g.fillRoundedRectangle (r, 10.0f);

    // Faint centre glow that breathes with input level.
    if (smoothedLevel > 0.002f)
    {
        const float glow = jlimit (0.0f, 0.10f, smoothedLevel * 1.2f);
        ColourGradient grad (Colour (0xff2a3350).withAlpha (glow), r.getCentreX(), r.getCentreY(),
                             Colours::transparentBlack, r.getCentreX(), r.getY(), true);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 10.0f);
    }

    for (const auto& pt : particles)
    {
        const float t = pt.age / pt.life;
        const float a = pt.baseAlpha * std::sin (3.14159f * jlimit (0.0f, 1.0f, t));
        if (a <= 0.01f)
            continue;
        // Soft halo + bright core reads as a glowing mote.
        g.setColour (pt.colour.withAlpha (a * 0.22f));
        g.fillEllipse (Rectangle<float> (pt.size * 3.6f, pt.size * 3.6f).withCentre ({ pt.x, pt.y }));
        g.setColour (pt.colour.withAlpha (a));
        g.fillEllipse (Rectangle<float> (pt.size, pt.size).withCentre ({ pt.x, pt.y }));
    }

    g.setColour (Colour (0xff232833));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
}

//==============================================================================
VoiceStrip::VoiceStrip (AudioProcessorValueTreeState& state, int voiceIndex)
    : apvts (state), index (voiceIndex)
{
    const auto id = String (index + 1);
    const auto accent = ui::kVoice[index];

    title.setText ("VOICE " + id, dontSendNotification);
    title.setFont (Font (FontOptions (12.0f, Font::bold)));
    title.setColour (Label::textColourId, accent);
    addAndMakeVisible (title);

    auto initCombo = [&] (ComboBox& c, const char* paramId, std::unique_ptr<ComboAtt>& att)
    {
        if (auto* p = dynamic_cast<AudioParameterChoice*> (apvts.getParameter ("v" + id + paramId)))
            c.addItemList (p->choices, 1);
        addAndMakeVisible (c);
        att = std::make_unique<ComboAtt> (apvts, "v" + id + paramId, c);
    };
    initCombo (mode, "Mode", modeAtt);
    initCombo (degree, "Degree", degreeAtt);
    initCombo (note, "Note", noteAtt);

    auto initKnob = [&] (Slider& s, const char* paramId, std::unique_ptr<SliderAtt>& att)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setColour (Slider::rotarySliderFillColourId, accent);
        addAndMakeVisible (s);
        att = std::make_unique<SliderAtt> (apvts, "v" + id + paramId, s);
    };
    initKnob (detune, "Detune", detuneAtt);
    initKnob (pan, "Pan", panAtt);

    gain.setSliderStyle (Slider::LinearVertical);
    gain.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    gain.setColour (Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible (gain);
    gainAtt = std::make_unique<SliderAtt> (apvts, "v" + id + "Gain", gain);

    for (auto* l : { &detuneLbl, &panLbl })
    {
        l->setFont (Font (FontOptions (9.5f)));
        l->setColour (Label::textColourId, ui::kDim);
        l->setJustificationType (Justification::centred);
        addAndMakeVisible (*l);
    }
    detuneLbl.setText ("DETUNE", dontSendNotification);
    panLbl.setText ("PAN", dontSendNotification);

    refresh();
}

void VoiceStrip::refresh()
{
    const int m = (int) apvts.getRawParameterValue ("v" + String (index + 1) + "Mode")->load();
    degree.setVisible (m == 1);
    note.setVisible (m == 2);
    const float alpha = m == 0 ? 0.35f : 1.0f;
    for (auto* c : getChildren())
        if (c != &mode && c != &title)
            c->setAlpha (alpha);
}

void VoiceStrip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::kPanel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (ui::kVoice[index].withAlpha (0.9f));
    g.fillRoundedRectangle (r.removeFromTop (3.0f).reduced (10.0f, 0.0f), 1.5f);
}

void VoiceStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (4);
    title.setBounds (r.removeFromTop (18));
    r.removeFromTop (4);
    mode.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);
    auto sel = r.removeFromTop (24);
    degree.setBounds (sel);
    note.setBounds (sel);
    r.removeFromTop (8);

    auto knobs = r.removeFromTop (56);
    auto left = knobs.removeFromLeft (knobs.getWidth() / 2);
    detune.setBounds (left.removeFromTop (44));
    detuneLbl.setBounds (left);
    pan.setBounds (knobs.removeFromTop (44));
    panLbl.setBounds (knobs);

    r.removeFromTop (6);
    gain.setBounds (r.withSizeKeepingCentre (28, r.getHeight()));
}

//==============================================================================
OpenHarmonyEditor::OpenHarmonyEditor (OpenHarmonyProcessor& p)
    : AudioProcessorEditor (p), processor (p), visualizer (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (visualizer);

    titleLbl.setText ("OpenHarmony", dontSendNotification);
    titleLbl.setFont (Font (FontOptions (21.0f, Font::bold)));
    titleLbl.setColour (Label::textColourId, ui::kText);
    addAndMakeVisible (titleLbl);

    preset.setTextWhenNothingSelected ("Presets...");
    {
        const char* lastCat = "";
        for (int i = 0; i < presets::kNumPresets; ++i)
        {
            if (String (lastCat) != presets::kPresets[i].category)
            {
                lastCat = presets::kPresets[i].category;
                preset.addSectionHeading (lastCat);
            }
            preset.addItem (presets::kPresets[i].name, i + 1);
        }
    }
    preset.onChange = [this]
    {
        if (preset.getSelectedId() > 0)
            applyPreset (preset.getSelectedId() - 1);
    };
    addAndMakeVisible (preset);

    auto initHeaderCombo = [&] (ComboBox& c, const char* paramId, auto& att)
    {
        if (auto* par = dynamic_cast<AudioParameterChoice*> (processor.apvts.getParameter (paramId)))
            c.addItemList (par->choices, 1);
        addAndMakeVisible (c);
        att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (
            processor.apvts, paramId, c);
    };
    initHeaderCombo (keyRoot, "keyRoot", keyAtt);
    initHeaderCombo (scale, "scale", scaleAtt);

    for (auto& [l, text] : std::initializer_list<std::pair<Label*, const char*>> {
             { &keyLbl, "KEY" }, { &scaleLbl, "SCALE" }, { &mixLbl, "MIX" } })
    {
        l->setText (text, dontSendNotification);
        l->setFont (Font (FontOptions (9.5f)));
        l->setColour (Label::textColourId, ui::kDim);
        l->setJustificationType (Justification::centred);
        addAndMakeVisible (*l);
    }

    mix.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    mix.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    mix.setColour (Slider::rotarySliderFillColourId, Colour (0xffb8c2dd));
    addAndMakeVisible (mix);
    mixAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "dryWet", mix);

    autoKeyLbl.setFont (Font (FontOptions (13.0f, Font::bold)));
    autoKeyLbl.setColour (Label::textColourId, Colour (0xff7ee08a));
    autoKeyLbl.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (autoKeyLbl);

    pitchLbl.setFont (Font (FontOptions (12.0f)));
    pitchLbl.setColour (Label::textColourId, ui::kDim);
    addAndMakeVisible (pitchLbl);

    latencyLbl.setFont (Font (FontOptions (11.0f)));
    latencyLbl.setColour (Label::textColourId, Colour (0xff565d70));
    latencyLbl.setJustificationType (Justification::centredRight);
    latencyLbl.setText ("46 ms lookahead - enable host latency compensation", dontSendNotification);
    addAndMakeVisible (latencyLbl);

    for (int v = 0; v < OpenHarmonyProcessor::kNumVoices; ++v)
        addAndMakeVisible (strips.add (new VoiceStrip (processor.apvts, v)));

    setSize (960, 620);
    startTimerHz (15);
}

OpenHarmonyEditor::~OpenHarmonyEditor()
{
    setLookAndFeel (nullptr);
}

void OpenHarmonyEditor::applyPreset (int i)
{
    if (i < 0 || i >= presets::kNumPresets)
        return;
    const auto& pre = presets::kPresets[i];

    const bool autoScale = (int) processor.apvts.getRawParameterValue ("scale")->load() == 0;
    const int rootPc = autoScale ? processor.uiRoot.load()
                                 : (int) processor.apvts.getRawParameterValue ("keyRoot")->load();
    const int scaleIdx = (int) processor.apvts.getRawParameterValue ("scale")->load();
    const bool minorish = autoScale ? processor.uiMinor.load()
                                    : (scaleIdx == 2 || scaleIdx == 3 || scaleIdx == 4 || scaleIdx == 7);

    auto set = [&] (const String& id, float value)
    {
        if (auto* par = processor.apvts.getParameter (id))
        {
            par->beginChangeGesture();
            par->setValueNotifyingHost (par->convertTo0to1 (value));
            par->endChangeGesture();
        }
    };

    set ("dryWet", pre.dryWet);
    for (int v = 0; v < OpenHarmonyProcessor::kNumVoices; ++v)
    {
        const auto& vs = pre.v[v];
        const auto id = String (v + 1);
        set ("v" + id + "Mode", (float) vs.mode);
        set ("v" + id + "Degree", (float) vs.degree);
        set ("v" + id + "Note", (float) (presets::resolveNote (vs.note, rootPc, minorish) - 36));
        set ("v" + id + "Gain", vs.gain);
        set ("v" + id + "Pan", vs.pan);
        set ("v" + id + "Detune", vs.detune);
    }
}

void OpenHarmonyEditor::timerCallback()
{
    static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const float f0 = processor.uiF0.load();
    if (f0 > 0.0f)
    {
        const double midi = 69.0 + 12.0 * std::log2 (f0 / 440.0);
        const int n = (int) std::lround (midi);
        const int cents = (int) std::lround ((midi - n) * 100.0);
        pitchLbl.setText (String (pcs[((n % 12) + 12) % 12]) + String (n / 12 - 1) + "  "
                              + (cents >= 0 ? "+" : "") + String (cents) + "c",
                          dontSendNotification);
    }
    else
        pitchLbl.setText ("-", dontSendNotification);

    const bool autoScale = (int) processor.apvts.getRawParameterValue ("scale")->load() == 0;
    autoKeyLbl.setText (autoScale ? String (pcs[processor.uiRoot.load() % 12])
                                        + (processor.uiMinor.load() ? " minor" : " major")
                                  : String(),
                        dontSendNotification);

    for (auto* s : strips)
        s->refresh();
}

void OpenHarmonyEditor::paint (Graphics& g)
{
    g.fillAll (ui::kBg);
    g.setColour (ui::kPanel.brighter (0.03f));
    g.fillRect (getLocalBounds().removeFromTop (60));
    g.setColour (Colour (0xff262c38));
    g.drawHorizontalLine (60, 0.0f, (float) getWidth());
}

void OpenHarmonyEditor::resized()
{
    auto r = getLocalBounds();

    auto header = r.removeFromTop (60).reduced (14, 0);
    titleLbl.setBounds (header.removeFromLeft (150));
    header.removeFromLeft (10);
    preset.setBounds (header.removeFromLeft (200).withSizeKeepingCentre (200, 26));
    header.removeFromLeft (18);

    auto keyArea = header.removeFromLeft (70);
    keyLbl.setBounds (keyArea.removeFromTop (16).translated (0, 8));
    keyRoot.setBounds (keyArea.withSizeKeepingCentre (70, 26).translated (0, 2));
    header.removeFromLeft (8);
    auto scaleArea = header.removeFromLeft (110);
    scaleLbl.setBounds (scaleArea.removeFromTop (16).translated (0, 8));
    scale.setBounds (scaleArea.withSizeKeepingCentre (110, 26).translated (0, 2));
    header.removeFromLeft (10);
    autoKeyLbl.setBounds (header.removeFromLeft (90));

    auto mixArea = header.removeFromRight (56);
    mix.setBounds (mixArea.removeFromTop (46).translated (0, 4));
    mixLbl.setBounds (mixArea);

    auto footer = r.removeFromBottom (30).reduced (14, 4);
    pitchLbl.setBounds (footer.removeFromLeft (160));
    latencyLbl.setBounds (footer);

    visualizer.setBounds (r.removeFromTop (190).reduced (14, 10));

    r.reduce (14, 4);
    r.removeFromBottom (6);
    const int gap = 10;
    const int stripW = (r.getWidth() - gap * 5) / 6;
    for (int v = 0; v < strips.size(); ++v)
        strips[v]->setBounds (r.getX() + v * (stripW + gap), r.getY(), stripW, r.getHeight());
}

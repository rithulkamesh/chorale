#include "PluginEditor.h"
#include "Presets.h"

using namespace juce;

namespace ui
{
String voiceLabel (AudioProcessorValueTreeState& apvts, int v)
{
    const auto id = String (v + 1);
    const int mode = (int) apvts.getRawParameterValue ("v" + id + "Mode")->load();
    if (mode == 0)
        return "-";
    if (mode == 3)
        return "MIDI";
    if (mode == 2)
    {
        static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int note = 36 + (int) apvts.getRawParameterValue ("v" + id + "Note")->load();
        return String (pcs[note % 12]) + String (note / 12 - 1);
    }
    const int deg = (int) apvts.getRawParameterValue ("v" + id + "Degree")->load() - 7;
    if (deg == 0)
        return "UNI";
    static const char* names[] = { "UNI", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH", "OCT" };
    return String (names[std::min (std::abs (deg), 7)]) + (deg > 0 ? String::charToString (0x2191)
                                                                   : String::charToString (0x2193));
}
} // namespace ui

//==============================================================================
ChoraleLookAndFeel::ChoraleLookAndFeel()
{
    setColour (Slider::rotarySliderFillColourId, ui::kText);
    setColour (Label::textColourId, ui::kText);
    setColour (PopupMenu::backgroundColourId, ui::kPanelHi);
    setColour (PopupMenu::textColourId, ui::kText);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3a4254));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (PopupMenu::headerTextColourId, ui::kDim);
    setColour (ComboBox::textColourId, ui::kText);
    setColour (TextButton::textColourOffId, ui::kDim);
    setColour (TextButton::textColourOnId, Colour (0xff14161c));
}

void ChoraleLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
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
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (angle - from) > 0.01f)
    {
        Path fill;
        fill.addCentredArc (centre.x, centre.y, radius, radius, 0,
                            jmin (from, angle), jmax (from, angle), true);
        g.setColour (accent);
        g.strokePath (fill, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const auto tip = centre.getPointOnCircumference (radius - 5.5f, angle);
    g.setColour (ui::kText);
    g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (tip));
}

void ChoraleLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int,
                                       ComboBox& box)
{
    const auto r = Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (ui::kPanelHi);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? ui::kDim : Colour (0xff2c3242));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h / 2.0f - 1.5f;
    arrow.addTriangle (ax, ay, ax + 7.0f, ay, ax + 3.5f, ay + 4.5f);
    g.setColour (ui::kDim);
    g.fillPath (arrow);
}

void ChoraleLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&,
                                               bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    Colour fill = ui::kPanelHi;
    if (b.getToggleState())
        fill = b.findColour (TextButton::buttonOnColourId);
    else if (highlighted)
        fill = ui::kPanelHi.brighter (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colour (0xff2c3242));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

Font ChoraleLookAndFeel::getComboBoxFont (ComboBox&) { return Font (FontOptions (13.0f)); }
Font ChoraleLookAndFeel::getPopupMenuFont() { return Font (FontOptions (13.0f)); }
Font ChoraleLookAndFeel::getTextButtonFont (TextButton&, int) { return Font (FontOptions (12.0f, Font::bold)); }

//==============================================================================
NoteChips::NoteChips (ChoraleProcessor& p, std::function<void (int)> cb)
    : processor (p), onSelect (std::move (cb))
{
    setInterceptsMouseClicks (true, false);
}

Rectangle<int> NoteChips::chipRect (int index) const
{
    constexpr int w = 58, h = 24, gap = 8;
    const int total = 9 * w + 8 * gap;
    const int x0 = (getWidth() - total) / 2;
    return { x0 + index * (w + gap), (getHeight() - h) / 2, w, h };
}

void NoteChips::paint (Graphics& g)
{
    for (int i = 0; i <= ChoraleProcessor::kNumVoices; ++i)
    {
        const auto r = chipRect (i).toFloat();
        const bool isLead = i == 0;
        const int v = i - 1;

        String label = isLead ? "LEAD" : ui::voiceLabel (processor.apvts, v);
        const bool off = ! isLead && label == "-";
        Colour c = isLead ? ui::kLead : ui::kVoice[v];
        if (off)
            c = Colour (0xff353b4a);

        g.setColour (off ? c.withAlpha (0.35f) : c.withAlpha (0.9f));
        g.fillRoundedRectangle (r, 12.0f);
        if (i == selected)
        {
            g.setColour (Colours::white);
            g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.6f);
        }
        g.setColour (off ? ui::kDim : Colour (0xff14161c));
        g.setFont (Font (FontOptions (11.0f, Font::bold)));
        g.drawText (label, r, Justification::centred);
    }
}

void NoteChips::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i <= ChoraleProcessor::kNumVoices; ++i)
        if (chipRect (i).contains (e.getPosition()))
        {
            selected = i;
            onSelect (i - 1); // -1 = lead
            repaint();
            return;
        }
}

//==============================================================================
StageView::StageView (ChoraleProcessor& p, std::function<void (int)> cb)
    : processor (p), onSelect (std::move (cb))
{
    particles.reserve (600);
    startTimerHz (30);
}

float StageView::offsetSemis (int v) const
{
    const auto id = String (v + 1);
    const int mode = (int) processor.apvts.getRawParameterValue ("v" + id + "Mode")->load();
    if (mode == 1)
    {
        static const int maj[7] = { 0, 2, 4, 5, 7, 9, 11 };
        const int steps = (int) processor.apvts.getRawParameterValue ("v" + id + "Degree")->load() - 7;
        int k = steps >= 0 ? steps / 7 : -((-steps + 6) / 7);
        return (float) (12 * k + maj[steps - 7 * k]);
    }
    if (mode == 2)
        return jlimit (-14.0f, 14.0f,
                       (float) (36 + (int) processor.apvts.getRawParameterValue ("v" + id + "Note")->load() - 57));
    if (mode == 3)
        return 9.0f;
    return 0.0f;
}

Point<float> StageView::bubblePos (int v) const
{
    const auto id = String (v + 1);
    const float pan = processor.apvts.getRawParameterValue ("v" + id + "Pan")->load();
    const float semis = offsetSemis (v);
    const float cx = (float) getWidth() / 2.0f, cy = (float) getHeight() - 34.0f;
    const float rMax = jmin ((float) getHeight() - 78.0f, (float) getWidth() / 2.0f - 46.0f);
    const float r = (0.22f + 0.72f * (semis + 14.0f) / 28.0f) * rMax;
    const float a = pan * 1.15f; // radians from vertical
    return { cx + r * std::sin (a), cy - r * std::cos (a) };
}

void StageView::spawn (Point<float> at, float intensity, Colour c)
{
    if (particles.size() >= 560)
        return;
    Particle pt;
    pt.x = at.x + (rng.nextFloat() - 0.5f) * 26.0f;
    pt.y = at.y + (rng.nextFloat() - 0.5f) * 26.0f;
    pt.vx = (rng.nextFloat() - 0.5f) * 9.0f;
    pt.vy = -(3.0f + rng.nextFloat() * 11.0f);
    pt.age = 0.0f;
    pt.life = 1.4f + rng.nextFloat() * 2.0f;
    pt.size = 0.7f + rng.nextFloat() * 1.1f;
    pt.baseAlpha = jlimit (0.12f, 0.95f, 0.22f + intensity);
    pt.twinkle = rng.nextFloat() * 6.28f;
    pt.colour = c;
    particles.push_back (pt);
}

void StageView::timerCallback()
{
    constexpr float dt = 1.0f / 30.0f;
    time += dt;

    smoothedLevel += 0.25f * (processor.uiLevel.load() - smoothedLevel);
    const float drive = jlimit (0.0f, 1.0f, smoothedLevel * 8.0f);

    if (processor.uiF0.load() > 0.0f && drive > 0.02f)
        for (int k = 0; k < 1 + (int) (drive * 2.0f); ++k)
            spawn ({ (float) getWidth() / 2.0f, (float) getHeight() - 34.0f }, drive * 0.6f, Colour (0xffe6ddd4));

    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        const float g = processor.uiVoiceGain[v].load();
        if (g > 0.01f && drive > 0.02f)
            for (int k = 0; k < 1 + (int) (drive * g * 3.0f); ++k)
                spawn (bubblePos (v), drive * g * 0.9f, ui::kVoice[v]);
    }

    if (particles.size() < 70 && rng.nextFloat() < 0.5f)
    {
        const float rMax = jmin ((float) getHeight() - 78.0f, (float) getWidth() / 2.0f - 46.0f);
        const float a = (rng.nextFloat() - 0.5f) * 2.3f;
        const float r = rng.nextFloat() * rMax;
        spawn ({ (float) getWidth() / 2.0f + r * std::sin (a), (float) getHeight() - 34.0f - r * std::cos (a) },
               0.05f, Colour (0xff3d4460));
        particles.back().vy *= 0.35f;
    }

    for (auto& pt : particles)
    {
        pt.age += dt;
        pt.vx += std::sin (time * 0.9f + pt.y * 0.02f) * 6.0f * dt;
        pt.vy += std::cos (time * 0.7f + pt.x * 0.015f) * 4.0f * dt;
        pt.x += pt.vx * dt;
        pt.y += pt.vy * dt;
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(),
                                     [this] (const Particle& pt)
                                     { return pt.age >= pt.life || pt.y < -8.0f
                                              || pt.x < -8.0f || pt.x > (float) getWidth() + 8.0f; }),
                     particles.end());
    repaint();
}

void StageView::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0a0c10));
    g.fillRoundedRectangle (r, 10.0f);

    const float cx = (float) getWidth() / 2.0f, cy = (float) getHeight() - 34.0f;
    const float rMax = jmin ((float) getHeight() - 78.0f, (float) getWidth() / 2.0f - 46.0f);

    // Concentric guide arcs (+12, 0, -12 semitone rings) and centre spoke.
    g.setColour (Colour (0xff232838).withAlpha (0.7f));
    for (float s : { -12.0f, 0.0f, 12.0f })
    {
        const float rr = (0.22f + 0.72f * (s + 14.0f) / 28.0f) * rMax;
        Path arc;
        arc.addCentredArc (cx, cy, rr, rr, 0, -1.25f, 1.25f, true);
        g.strokePath (arc, PathStrokeType (1.0f));
    }
    g.drawLine (cx, cy - rMax, cx, cy, 0.6f);
    g.setColour (ui::kDim.withAlpha (0.55f));
    g.setFont (Font (FontOptions (9.0f)));
    for (float s : { -12.0f, 0.0f, 12.0f })
    {
        const float rr = (0.22f + 0.72f * (s + 14.0f) / 28.0f) * rMax;
        g.drawText (String ((int) s), Rectangle<float> (26.0f, 12.0f)
                                          .withCentre ({ cx - rr * std::sin (1.32f), cy - rr * std::cos (1.32f) }),
                    Justification::centredRight);
    }

    // Faint breathing glow.
    if (smoothedLevel > 0.002f)
    {
        const float glow = jlimit (0.0f, 0.12f, smoothedLevel * 1.2f);
        ColourGradient grad (Colour (0xff2a3350).withAlpha (glow), cx, cy,
                             Colours::transparentBlack, cx, cy - rMax, true);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 10.0f);
    }

    // Glitter.
    for (const auto& pt : particles)
    {
        const float t = pt.age / pt.life;
        float a = pt.baseAlpha * std::sin (3.14159f * jlimit (0.0f, 1.0f, t));
        a *= 0.62f + 0.38f * std::sin (time * 9.0f + pt.twinkle); // twinkle
        if (a <= 0.01f)
            continue;
        g.setColour (pt.colour.withAlpha (a * 0.18f));
        g.fillEllipse (Rectangle<float> (pt.size * 4.2f, pt.size * 4.2f).withCentre ({ pt.x, pt.y }));
        g.setColour (pt.colour.withAlpha (a));
        g.fillEllipse (Rectangle<float> (pt.size * 1.4f, pt.size * 1.4f).withCentre ({ pt.x, pt.y }));
    }

    // Voice bubbles.
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        const auto id = String (v + 1);
        const int mode = (int) processor.apvts.getRawParameterValue ("v" + id + "Mode")->load();
        if (mode == 0)
            continue;
        const float gain = processor.apvts.getRawParameterValue ("v" + id + "Gain")->load();
        const bool muted = processor.apvts.getRawParameterValue ("v" + id + "Mute")->load() > 0.5f;
        const bool soloed = processor.apvts.getRawParameterValue ("v" + id + "Solo")->load() > 0.5f;
        const auto pos = bubblePos (v);
        const float d = 24.0f + gain * 16.0f;
        const auto c = ui::kVoice[v];

        g.setColour (c.withAlpha (muted ? 0.06f : 0.22f));
        g.fillEllipse (Rectangle<float> (d * 1.6f, d * 1.6f).withCentre (pos));
        g.setColour (muted ? c.withAlpha (0.25f) : c);
        g.fillEllipse (Rectangle<float> (d, d).withCentre (pos));
        if (soloed)
        {
            g.setColour (Colours::white.withAlpha (0.85f));
            g.drawEllipse (Rectangle<float> (d + 8.0f, d + 8.0f).withCentre (pos), 1.4f);
        }
        if (v == selected)
        {
            g.setColour (Colours::white);
            g.drawEllipse (Rectangle<float> (d + 3.0f, d + 3.0f).withCentre (pos), 1.8f);
        }
        g.setColour (Colour (0xff14161c));
        g.setFont (Font (FontOptions (10.0f, Font::bold)));
        g.drawText (ui::voiceLabel (processor.apvts, v),
                    Rectangle<float> (d + 20.0f, 12.0f).withCentre (pos), Justification::centred);
    }

    // Lead bubble.
    {
        const float pulse = 30.0f + jlimit (0.0f, 1.0f, smoothedLevel * 8.0f) * 8.0f;
        g.setColour (ui::kLead.withAlpha (0.25f));
        g.fillEllipse (Rectangle<float> (pulse * 1.7f, pulse * 1.7f).withCentre ({ cx, cy }));
        g.setColour (ui::kLead);
        g.fillEllipse (Rectangle<float> (pulse, pulse).withCentre ({ cx, cy }));
        String label = "LEAD";
        if (const float f0 = processor.uiF0.load(); f0 > 0.0f)
        {
            static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            const int m = (int) std::lround (69.0 + 12.0 * std::log2 (f0 / 440.0));
            label = String (pcs[((m % 12) + 12) % 12]) + String (m / 12 - 1);
        }
        g.setColour (Colour (0xff14161c));
        g.setFont (Font (FontOptions (11.0f, Font::bold)));
        g.drawText (label, Rectangle<float> (52.0f, 14.0f).withCentre ({ cx, cy }), Justification::centred);
    }

    g.setColour (Colour (0xff222735));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
}

void StageView::mouseDown (const MouseEvent& e)
{
    dragging = -1;
    for (int v = ChoraleProcessor::kNumVoices - 1; v >= 0; --v)
    {
        const auto id = String (v + 1);
        if ((int) processor.apvts.getRawParameterValue ("v" + id + "Mode")->load() == 0)
            continue;
        if (bubblePos (v).getDistanceFrom (e.position) < 24.0f)
        {
            dragging = v;
            selected = v;
            dragStartPan = processor.apvts.getRawParameterValue ("v" + id + "Pan")->load();
            dragStartGain = processor.apvts.getRawParameterValue ("v" + id + "Gain")->load();
            dragStartPos = e.position;
            if (auto* p = processor.apvts.getParameter ("v" + id + "Pan"))
                p->beginChangeGesture();
            if (auto* p = processor.apvts.getParameter ("v" + id + "Gain"))
                p->beginChangeGesture();
            onSelect (v);
            return;
        }
    }
}

void StageView::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0)
        return;
    const auto id = String (dragging + 1);
    const float pan = jlimit (-1.0f, 1.0f, dragStartPan + (e.position.x - dragStartPos.x) / 130.0f);
    const float gain = jlimit (0.0f, 1.0f, dragStartGain - (e.position.y - dragStartPos.y) / 150.0f);
    if (auto* p = processor.apvts.getParameter ("v" + id + "Pan"))
        p->setValueNotifyingHost (p->convertTo0to1 (pan));
    if (auto* p = processor.apvts.getParameter ("v" + id + "Gain"))
        p->setValueNotifyingHost (p->convertTo0to1 (gain));
}

void StageView::mouseUp (const MouseEvent&)
{
    if (dragging < 0)
        return;
    const auto id = String (dragging + 1);
    if (auto* p = processor.apvts.getParameter ("v" + id + "Pan"))
        p->endChangeGesture();
    if (auto* p = processor.apvts.getParameter ("v" + id + "Gain"))
        p->endChangeGesture();
    dragging = -1;
}

//==============================================================================
VoiceDetailPanel::VoiceDetailPanel (ChoraleProcessor& p) : processor (p)
{
    title.setFont (Font (FontOptions (13.0f, Font::bold)));
    addAndMakeVisible (title);
    addAndMakeVisible (mode);
    addAndMakeVisible (degree);
    addAndMakeVisible (note);

    for (auto* s : { &detune, &pan, &level })
    {
        s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
    }
    for (auto& [l, text] : std::initializer_list<std::pair<Label*, const char*>> {
             { &detuneLbl, "DETUNE" }, { &panLbl, "PAN" }, { &levelLbl, "LEVEL" } })
    {
        l->setText (text, dontSendNotification);
        l->setFont (Font (FontOptions (9.0f)));
        l->setColour (Label::textColourId, ui::kDim);
        l->setJustificationType (Justification::centred);
        addAndMakeVisible (*l);
    }

    for (auto* b : { &solo, &mute })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    setVoice (0);
}

void VoiceDetailPanel::setVoice (int v)
{
    voice = jlimit (0, ChoraleProcessor::kNumVoices - 1, v);
    const auto id = String (voice + 1);
    const auto accent = ui::kVoice[voice];

    title.setText ("VOICE " + id, dontSendNotification);
    title.setColour (Label::textColourId, accent);
    for (auto* s : { &detune, &pan, &level })
        s->setColour (Slider::rotarySliderFillColourId, accent);
    solo.setColour (TextButton::buttonOnColourId, Colours::white);
    mute.setColour (TextButton::buttonOnColourId, accent);

    // Rebind everything to this voice's parameters.
    modeAtt.reset(); degreeAtt.reset(); noteAtt.reset();
    detuneAtt.reset(); panAtt.reset(); levelAtt.reset();
    soloAtt.reset(); muteAtt.reset();

    auto fillCombo = [&] (ComboBox& c, const String& paramId)
    {
        c.clear (dontSendNotification);
        if (auto* p = dynamic_cast<AudioParameterChoice*> (processor.apvts.getParameter (paramId)))
            c.addItemList (p->choices, 1);
    };
    fillCombo (mode, "v" + id + "Mode");
    fillCombo (degree, "v" + id + "Degree");
    fillCombo (note, "v" + id + "Note");

    modeAtt = std::make_unique<ComboAtt> (processor.apvts, "v" + id + "Mode", mode);
    degreeAtt = std::make_unique<ComboAtt> (processor.apvts, "v" + id + "Degree", degree);
    noteAtt = std::make_unique<ComboAtt> (processor.apvts, "v" + id + "Note", note);
    detuneAtt = std::make_unique<SliderAtt> (processor.apvts, "v" + id + "Detune", detune);
    panAtt = std::make_unique<SliderAtt> (processor.apvts, "v" + id + "Pan", pan);
    levelAtt = std::make_unique<SliderAtt> (processor.apvts, "v" + id + "Gain", level);
    soloAtt = std::make_unique<ButtonAtt> (processor.apvts, "v" + id + "Solo", solo);
    muteAtt = std::make_unique<ButtonAtt> (processor.apvts, "v" + id + "Mute", mute);

    refresh();
    repaint();
}

void VoiceDetailPanel::refresh()
{
    const int m = (int) processor.apvts
                      .getRawParameterValue ("v" + String (voice + 1) + "Mode")->load();
    degree.setVisible (m == 1 || m == 0 || m == 3);
    note.setVisible (m == 2);
    const float alpha = m == 0 ? 0.4f : 1.0f;
    for (auto* c : getChildren())
        if (c != &mode && c != &title)
            c->setAlpha (alpha);
}

void VoiceDetailPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::kPanel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (ui::kVoice[voice].withAlpha (0.9f));
    g.fillRoundedRectangle (r.removeFromTop (3.0f).reduced (12.0f, 0.0f), 1.5f);
}

void VoiceDetailPanel::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    r.removeFromTop (4);
    auto titleRow = r.removeFromTop (22);
    solo.setBounds (titleRow.removeFromRight (26));
    titleRow.removeFromRight (6);
    mute.setBounds (titleRow.removeFromRight (26));
    title.setBounds (titleRow);
    r.removeFromTop (8);
    mode.setBounds (r.removeFromTop (26));
    r.removeFromTop (8);
    auto sel = r.removeFromTop (26);
    degree.setBounds (sel);
    note.setBounds (sel);
    r.removeFromTop (14);

    auto knobs = r.removeFromTop (74);
    const int kw = knobs.getWidth() / 3;
    auto k1 = knobs.removeFromLeft (kw), k2 = knobs.removeFromLeft (kw);
    detune.setBounds (k1.removeFromTop (58));
    detuneLbl.setBounds (k1);
    pan.setBounds (k2.removeFromTop (58));
    panLbl.setBounds (k2);
    level.setBounds (knobs.removeFromTop (58));
    levelLbl.setBounds (knobs);
}

//==============================================================================
void KeyboardStrip::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0a0c10));
    g.fillRoundedRectangle (r, 8.0f);

    constexpr int loNote = 36, hiNote = 84; // C2..B5
    constexpr int numWhite = 28;
    const float ww = (r.getWidth() - 12.0f) / numWhite;
    const float x0 = r.getX() + 6.0f, y0 = r.getY() + 5.0f, kh = r.getHeight() - 10.0f;

    const int leadNote = processor.uiF0.load() > 0.0f
                             ? (int) std::lround (69.0 + 12.0 * std::log2 (processor.uiF0.load() / 440.0))
                             : -1;
    int voiceNote[ChoraleProcessor::kNumVoices];
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        const float hz = processor.uiVoiceHz[v].load();
        voiceNote[v] = hz > 0.0f ? (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0)) : -1;
    }

    static const bool isBlack[12] = { false, true, false, true, false, false,
                                      true, false, true, false, true, false };
    auto keyRect = [&] (int m) -> Rectangle<float>
    {
        int whites = 0;
        for (int n = loNote; n < m; ++n)
            if (! isBlack[n % 12])
                ++whites;
        if (! isBlack[m % 12])
            return { x0 + (float) whites * ww, y0, ww - 1.0f, kh };
        return { x0 + (float) whites * ww - ww * 0.3f, y0, ww * 0.6f, kh * 0.58f };
    };

    auto keyColour = [&] (int m, Colour base) -> Colour
    {
        if (m == leadNote)
            return ui::kLead;
        for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
            if (voiceNote[v] == m)
                return ui::kVoice[v];
        return base;
    };

    for (int m = loNote; m < hiNote; ++m)
        if (! isBlack[m % 12])
        {
            g.setColour (keyColour (m, Colour (0xffd9dce6)));
            g.fillRoundedRectangle (keyRect (m), 2.0f);
        }
    for (int m = loNote; m < hiNote; ++m)
        if (isBlack[m % 12])
        {
            g.setColour (keyColour (m, Colour (0xff181b22)));
            g.fillRoundedRectangle (keyRect (m), 1.5f);
        }
}

//==============================================================================
ChoraleEditor::ChoraleEditor (ChoraleProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      chips (p, [this] (int v) { selectVoice (v); }),
      stage (p, [this] (int v) { selectVoice (v); }),
      detail (p),
      keyboard (p)
{
    setLookAndFeel (&lnf);

    titleLbl.setText ("CHORALE", dontSendNotification);
    titleLbl.setFont (Font (FontOptions (20.0f, Font::bold)));
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
        if (auto* par = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
            c.addItemList (par->choices, 1);
        addAndMakeVisible (c);
        att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvts, paramId, c);
    };
    initHeaderCombo (keyRoot, "keyRoot", keyAtt);
    initHeaderCombo (scale, "scale", scaleAtt);
    initHeaderCombo (correct, "correct", correctAtt);

    for (auto& [l, text] : std::initializer_list<std::pair<Label*, const char*>> {
             { &keyLbl, "KEY" }, { &scaleLbl, "SCALE" }, { &correctLbl, "CORRECT" }, { &mixLbl, "MIX" } })
    {
        l->setText (text, dontSendNotification);
        l->setFont (Font (FontOptions (9.0f)));
        l->setColour (Label::textColourId, ui::kDim);
        l->setJustificationType (Justification::centred);
        addAndMakeVisible (*l);
    }

    auto initKnob = [&] (Slider& s, const char* paramId, auto& att, Colour accent)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setColour (Slider::rotarySliderFillColourId, accent);
        addAndMakeVisible (s);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, paramId, s);
    };
    initKnob (mix, "dryWet", mixAtt, Colour (0xffb8c2dd));
    initKnob (humanize, "humanize", humanizeAtt, Colour (0xff7ee08a));
    initKnob (tone, "tone", toneAtt, Colour (0xffffc95c));
    initKnob (width, "width", widthAtt, Colour (0xff5aa9ff));
    initKnob (echoTime, "echoTime", echoTimeAtt, Colour (0xffc79bff));
    initKnob (echoFb, "echoFb", echoFbAtt, Colour (0xffc79bff));
    initKnob (echoMix, "echoMix", echoMixAtt, Colour (0xffc79bff));

    const char* fxNames[6] = { "HUMANIZE", "TONE", "WIDTH", "ECHO", "FEEDBACK", "ECHO MIX" };
    for (int i = 0; i < 6; ++i)
    {
        fxLbls[i].setText (fxNames[i], dontSendNotification);
        fxLbls[i].setFont (Font (FontOptions (9.0f)));
        fxLbls[i].setColour (Label::textColourId, ui::kDim);
        fxLbls[i].setJustificationType (Justification::centred);
        addAndMakeVisible (fxLbls[i]);
    }

    autoKeyLbl.setFont (Font (FontOptions (12.5f, Font::bold)));
    autoKeyLbl.setColour (Label::textColourId, Colour (0xff7ee08a));
    addAndMakeVisible (autoKeyLbl);

    pitchLbl.setFont (Font (FontOptions (11.5f)));
    pitchLbl.setColour (Label::textColourId, ui::kDim);
    addAndMakeVisible (pitchLbl);

    latencyLbl.setFont (Font (FontOptions (10.5f)));
    latencyLbl.setColour (Label::textColourId, Colour (0xff4d5468));
    latencyLbl.setJustificationType (Justification::centredRight);
    latencyLbl.setText ("46 ms lookahead", dontSendNotification);
    addAndMakeVisible (latencyLbl);

    addAndMakeVisible (chips);
    addAndMakeVisible (stage);
    addAndMakeVisible (detail);
    addAndMakeVisible (keyboard);

    chips.setSelected (1);
    stage.setSelected (0);

    setSize (1120, 700);
    startTimerHz (15);
}

ChoraleEditor::~ChoraleEditor()
{
    setLookAndFeel (nullptr);
}

void ChoraleEditor::selectVoice (int v)
{
    if (v < 0) // lead chip: nothing to edit yet
        return;
    detail.setVoice (v);
    stage.setSelected (v);
    chips.setSelected (v + 1);
}

void ChoraleEditor::applyPreset (int i)
{
    if (i < 0 || i >= presets::kNumPresets)
        return;
    const auto& pre = presets::kPresets[i];

    const bool autoScale = (int) proc.apvts.getRawParameterValue ("scale")->load() == 0;
    const int rootPc = autoScale ? proc.uiRoot.load()
                                 : (int) proc.apvts.getRawParameterValue ("keyRoot")->load();
    const int scaleIdx = (int) proc.apvts.getRawParameterValue ("scale")->load();
    const bool minorish = autoScale ? proc.uiMinor.load()
                                    : (scaleIdx == 2 || scaleIdx == 3 || scaleIdx == 4 || scaleIdx == 7);

    auto set = [&] (const String& id, float value)
    {
        if (auto* par = proc.apvts.getParameter (id))
        {
            par->beginChangeGesture();
            par->setValueNotifyingHost (par->convertTo0to1 (value));
            par->endChangeGesture();
        }
    };

    // Deliberately does NOT touch dryWet: mix is a session setting, not part
    // of a harmony shape (stomping it read as a "mix reset" bug).
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        const auto& vs = pre.v[v];
        const auto id = String (v + 1);
        set ("v" + id + "Mode", (float) vs.mode);
        set ("v" + id + "Degree", (float) vs.degree);
        set ("v" + id + "Note", (float) (presets::resolveNote (vs.note, rootPc, minorish) - 36));
        set ("v" + id + "Gain", vs.gain);
        set ("v" + id + "Pan", vs.pan);
        set ("v" + id + "Detune", vs.detune);
        set ("v" + id + "Solo", 0.0f);
        set ("v" + id + "Mute", 0.0f);
    }
    detail.setVoice (0);
}

void ChoraleEditor::timerCallback()
{
    static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const float f0 = proc.uiF0.load();
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

    const bool autoScale = (int) proc.apvts.getRawParameterValue ("scale")->load() == 0;
    autoKeyLbl.setText (autoScale ? String (pcs[proc.uiRoot.load() % 12])
                                        + (proc.uiMinor.load() ? " minor" : " major")
                                  : String(),
                        dontSendNotification);

    detail.refresh();
    chips.repaint();
    keyboard.repaint();
}

void ChoraleEditor::paint (Graphics& g)
{
    g.fillAll (ui::kBg);
    g.setColour (ui::kPanel.brighter (0.03f));
    g.fillRect (getLocalBounds().removeFromTop (56));
    g.setColour (Colour (0xff232838));
    g.drawHorizontalLine (56, 0.0f, (float) getWidth());
}

void ChoraleEditor::resized()
{
    auto r = getLocalBounds();

    auto header = r.removeFromTop (56).reduced (14, 0);
    titleLbl.setBounds (header.removeFromLeft (110));
    header.removeFromLeft (8);
    preset.setBounds (header.removeFromLeft (190).withSizeKeepingCentre (190, 26));
    header.removeFromLeft (16);

    auto labelledCombo = [&] (Label& l, ComboBox& c, int w)
    {
        auto area = header.removeFromLeft (w);
        l.setBounds (area.removeFromTop (15).translated (0, 6));
        c.setBounds (area.withSizeKeepingCentre (w, 25).translated (0, 1));
        header.removeFromLeft (8);
    };
    labelledCombo (keyLbl, keyRoot, 62);
    labelledCombo (scaleLbl, scale, 104);
    labelledCombo (correctLbl, correct, 88);
    header.removeFromLeft (4);
    autoKeyLbl.setBounds (header.removeFromLeft (86));

    auto mixArea = header.removeFromRight (52);
    mix.setBounds (mixArea.removeFromTop (43).translated (0, 3));
    mixLbl.setBounds (mixArea);

    chips.setBounds (r.removeFromTop (38));

    auto footer = r.removeFromBottom (26).reduced (14, 2);
    pitchLbl.setBounds (footer.removeFromLeft (160));
    latencyLbl.setBounds (footer);

    // FX bar.
    auto fx = r.removeFromBottom (78).reduced (14, 4);
    Slider* knobs[6] = { &humanize, &tone, &width, &echoTime, &echoFb, &echoMix };
    const int kw = jmin (110, fx.getWidth() / 6);
    const int pad = (fx.getWidth() - kw * 6) / 2;
    fx.removeFromLeft (pad);
    for (int i = 0; i < 6; ++i)
    {
        auto cell = fx.removeFromLeft (kw);
        knobs[i]->setBounds (cell.removeFromTop (54).withSizeKeepingCentre (54, 54));
        fxLbls[i].setBounds (cell);
    }

    keyboard.setBounds (r.removeFromBottom (52).reduced (14, 4));

    r.reduce (14, 6);
    detail.setBounds (r.removeFromLeft (236));
    r.removeFromLeft (10);
    stage.setBounds (r);
}

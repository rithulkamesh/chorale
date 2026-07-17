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

constexpr float kMinGainDb = -60.0f;
constexpr float kMaxGainDb = 6.0f;

float gainToDb (float gain)
{
    if (gain <= 0.0001f)
        return kMinGainDb;
    return jmax (kMinGainDb, Decibels::gainToDecibels (gain));
}

float dbToGain (float db)
{
    if (db <= kMinGainDb)
        return 0.0f;
    return Decibels::decibelsToGain (db);
}

String gainDbString (float db)
{
    if (db <= kMinGainDb + 0.05f)
        return "-inf";
    return String (db, 1) + " dB";
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

void ChoraleLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h,
                                           float sliderPos, float minSliderPos, float maxSliderPos,
                                           Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minSliderPos, maxSliderPos, style, s);
        return;
    }

    auto track = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f, 4.0f);
    if (dynamic_cast<GainFader*> (s.getParentComponent()) != nullptr)
        track.removeFromLeft (10.0f);

    const float tw = jmin (10.0f, track.getWidth() * 0.28f);
    const auto groove = track.withSizeKeepingCentre (tw, track.getHeight());

    g.setColour (Colour (0xff151920));
    g.fillRoundedRectangle (groove, 2.0f);
    g.setColour (Colour (0xff2a3038));
    g.drawRoundedRectangle (groove, 2.0f, 0.8f);

    const float minV = (float) s.getMinimum();
    const float maxV = (float) s.getMaximum();
    g.setColour (ui::kDim.withAlpha (0.55f));
    g.setFont (Font (FontOptions (7.5f)));
    for (float db : { 0.0f, -12.0f, -24.0f, -48.0f })
    {
        const float t = (db - minV) / (maxV - minV);
        const float ty = groove.getBottom() - t * groove.getHeight();
        g.drawHorizontalLine ((int) ty, groove.getX(), groove.getRight());
        const int dbi = (int) db;
        if (dbi == 0 || dbi == -24)
            g.drawText (String (dbi),
                        Rectangle<float> (groove.getX() - 14.0f, ty - 5.0f, 12.0f, 10.0f),
                        Justification::centredRight);
    }

    g.setColour (Colours::white.withAlpha (0.95f));
    g.fillRoundedRectangle (Rectangle<float> (groove.getX() - 1.0f, sliderPos - 4.0f,
                                              groove.getWidth() + 2.0f, 8.0f),
                            2.0f);
    g.setColour (Colour (0xff000000).withAlpha (0.25f));
    g.drawRoundedRectangle (Rectangle<float> (groove.getX() - 1.0f, sliderPos - 4.0f,
                                              groove.getWidth() + 2.0f, 8.0f),
                            2.0f, 0.8f);
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
    const bool chipBtn = b.getButtonText() == "STAGE" || b.getButtonText() == "MIXER";
    const float radius = chipBtn ? 12.0f : 5.0f;

    Colour fill = ui::kPanelHi;
    if (b.getToggleState())
        fill = b.findColour (TextButton::buttonOnColourId);
    else if (highlighted)
        fill = ui::kPanelHi.brighter (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    g.setColour (Colour (0xff2c3242));
    g.drawRoundedRectangle (r, radius, 1.0f);
    if (chipBtn && b.getToggleState())
    {
        g.setColour (Colours::white);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.4f);
    }
}

Font ChoraleLookAndFeel::getComboBoxFont (ComboBox&) { return Font (FontOptions (13.0f)); }
Font ChoraleLookAndFeel::getPopupMenuFont() { return Font (FontOptions (13.0f)); }
Font ChoraleLookAndFeel::getTextButtonFont (TextButton& b, int)
{
    if (b.getButtonText() == "STAGE" || b.getButtonText() == "MIXER")
        return Font (FontOptions (11.0f, Font::bold));
    return Font (FontOptions (12.0f, Font::bold));
}

//==============================================================================
GainFader::GainFader()
{
    slider.setSliderStyle (Slider::LinearVertical);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRange (ui::kMinGainDb, ui::kMaxGainDb, 0.1);
    slider.setValue (0.0);
    addAndMakeVisible (slider);

    valueLbl.setJustificationType (Justification::centred);
    valueLbl.setFont (Font (FontOptions (9.0f)));
    valueLbl.setColour (Label::textColourId, ui::kDim);
    addAndMakeVisible (valueLbl);

    slider.onValueChange = [this]
    {
        pushToParam();
        updateLabel();
    };
    slider.onDragStart = [this]
    {
        if (processor != nullptr && paramId.isNotEmpty())
            if (auto* p = processor->apvts.getParameter (paramId))
                p->beginChangeGesture();
    };
    slider.onDragEnd = [this]
    {
        if (processor != nullptr && paramId.isNotEmpty())
            if (auto* p = processor->apvts.getParameter (paramId))
                p->endChangeGesture();
    };

    startTimerHz (30);
}

void GainFader::bind (ChoraleProcessor& p, const String& id)
{
    processor = &p;
    paramId = id;
    syncFromParam();
}

void GainFader::setMeterLevel (float level)
{
    meterLevel = level;
    const float prev = smoothedMeter;
    smoothedMeter += 0.35f * (level - smoothedMeter);
    if (std::abs (smoothedMeter - prev) > 0.002f)
        repaint();
}

void GainFader::syncFromParam()
{
    if (processor == nullptr || paramId.isEmpty())
        return;
    const float gain = processor->apvts.getRawParameterValue (paramId)->load();
    slider.setValue (ui::gainToDb (gain), dontSendNotification);
    updateLabel();
}

void GainFader::pushToParam()
{
    if (processor == nullptr || paramId.isEmpty())
        return;
    const float db = (float) slider.getValue();
    const float gain = jmin (1.0f, ui::dbToGain (db));
    if (auto* p = processor->apvts.getParameter (paramId))
    {
        const float norm = p->convertTo0to1 (gain);
        if (std::abs (p->getValue() - norm) > 0.0005f)
            p->setValueNotifyingHost (norm);
    }
}

void GainFader::updateLabel()
{
    valueLbl.setText (ui::gainDbString ((float) slider.getValue()), dontSendNotification);
}

void GainFader::timerCallback()
{
    if (processor == nullptr || paramId.isEmpty())
        return;
    const float gain = processor->apvts.getRawParameterValue (paramId)->load();
    const float db = ui::gainToDb (gain);
    if (std::abs ((float) slider.getValue() - db) > 0.15f)
        slider.setValue (db, dontSendNotification);
    updateLabel();
}

void GainFader::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    area.removeFromBottom (14.0f);
    auto meter = area.removeFromLeft (9.0f).reduced (0.0f, 4.0f);

    g.setColour (Colour (0xff10141c));
    g.fillRoundedRectangle (meter, 1.5f);

    const float meterNorm = jlimit (0.0f, 1.0f,
                                    (ui::gainToDb (smoothedMeter) - ui::kMinGainDb)
                                        / (ui::kMaxGainDb - ui::kMinGainDb));
    auto fill = meter;
    fill.setHeight (meter.getHeight() * meterNorm);
    fill.setY (meter.getBottom() - fill.getHeight());
    g.setColour (Colour (0xff58a8e8).withAlpha (meterNorm > 0.02f ? 0.92f : 0.18f));
    g.fillRoundedRectangle (fill, 1.5f);
}

void GainFader::resized()
{
    auto r = getLocalBounds();
    valueLbl.setBounds (r.removeFromBottom (14));
    slider.setBounds (r);
}

void GainFader::mouseDown (const MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return;
    if (processor == nullptr || paramId.isEmpty())
        return;

    auto* dialog = new AlertWindow ("Gain", "Enter level in dB (-inf to +6)", AlertWindow::QuestionIcon);
    dialog->addTextEditor ("db", ui::gainDbString ((float) slider.getValue()), "dB");
    dialog->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    const auto id = paramId;
    dialog->enterModalState (true, ModalCallbackFunction::create (
                                        [this, dialog, id] (int result)
                                        {
                                            if (result == 1)
                                            {
                                                auto text = dialog->getTextEditorContents ("db").trim();
                                                float db = ui::kMinGainDb;
                                                if (! (text.equalsIgnoreCase ("-inf")
                                                       || text.equalsIgnoreCase ("mute")))
                                                    db = jlimit (ui::kMinGainDb, ui::kMaxGainDb,
                                                                 (float) text.getDoubleValue());
                                                slider.setValue (db, sendNotificationSync);
                                            }
                                            delete dialog;
                                        }));
}

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
    startTimerHz (30);
}

void StageView::stageGeometry (float& cx, float& cy, float& rMax, float& rMin) const
{
    cx = (float) getWidth() / 2.0f;
    cy = (float) getHeight() - 34.0f;
    rMax = jmin ((float) getHeight() - 78.0f, (float) getWidth() / 2.0f - 46.0f);
    rMin = 0.12f * rMax;
}

void StageView::panGainFromPoint (Point<float> pt, float& pan, float& gain) const
{
    float cx, cy, rMax, rMin;
    stageGeometry (cx, cy, rMax, rMin);

    const float dx = pt.x - cx;
    const float dy = cy - pt.y;
    const float r = std::hypot (dx, dy);
    const float a = std::atan2 (dx, dy);

    gain = jlimit (0.0f, 1.0f, (r - rMin) / (0.88f * rMax));
    pan = jlimit (-1.0f, 1.0f, a / 1.15f);
}

float StageView::bubbleHitRadius (int v) const
{
    const auto id = String (v + 1);
    if ((int) processor.apvts.getRawParameterValue ("v" + id + "Mode")->load() == 0)
        return 0.0f;
    return 28.0f;
}

Point<float> StageView::bubblePos (int v) const
{
    const auto id = String (v + 1);
    const float pan = processor.apvts.getRawParameterValue ("v" + id + "Pan")->load();
    const float gain = processor.apvts.getRawParameterValue ("v" + id + "Gain")->load();
    float cx, cy, rMax, rMin;
    stageGeometry (cx, cy, rMax, rMin);
    const float r = rMin + gain * 0.88f * rMax;
    const float a = pan * 1.15f;
    return { cx + r * std::sin (a), cy - r * std::cos (a) };
}

void StageView::timerCallback()
{
    smoothedLevel += 0.18f * (processor.uiLevel.load() - smoothedLevel);
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
        voiceLevel[v] += 0.22f * (processor.uiVoiceGain[v].load() - voiceLevel[v]);
    repaint();
}

void StageView::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0a0c10));
    g.fillRoundedRectangle (r, 10.0f);

    float cx, cy, rMax, rMin;
    stageGeometry (cx, cy, rMax, rMin);

    {
        const float glow = jlimit (0.0f, 1.0f, smoothedLevel * 10.0f);
        ColourGradient wash (ui::kLead.withAlpha (0.10f + glow * 0.14f), cx, cy,
                             Colours::transparentBlack, cx, cy - rMax * 1.05f, true);
        g.setGradientFill (wash);
        g.fillRoundedRectangle (r, 10.0f);
    }

    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        const auto id = String (v + 1);
        if ((int) processor.apvts.getRawParameterValue ("v" + id + "Mode")->load() == 0)
            continue;
        const float lvl = voiceLevel[v];
        if (lvl < 0.01f)
            continue;
        const auto pos = bubblePos (v);
        const float rad = 40.0f + lvl * 70.0f;
        ColourGradient bloom (ui::kVoice[v].withAlpha (0.04f + lvl * 0.10f), pos.x, pos.y,
                              Colours::transparentBlack, pos.x, pos.y - rad, true);
        g.setGradientFill (bloom);
        g.fillEllipse (pos.x - rad, pos.y - rad, rad * 2.0f, rad * 2.0f);
    }

    g.setColour (Colour (0xff232838).withAlpha (0.55f));
    for (float gv : { 0.33f, 0.66f, 1.0f })
    {
        const float rr = rMin + gv * 0.88f * rMax;
        Path arc;
        arc.addCentredArc (cx, cy, rr, rr, 0, -1.25f, 1.25f, true);
        g.strokePath (arc, PathStrokeType (0.8f));
    }
    g.drawLine (cx, cy - rMax, cx, cy, 0.5f);

    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        const auto id = String (v + 1);
        const int mode = (int) processor.apvts.getRawParameterValue ("v" + id + "Mode")->load();
        if (mode == 0)
            continue;
        const float gain = processor.apvts.getRawParameterValue ("v" + id + "Gain")->load();
        const bool muted = processor.apvts.getRawParameterValue ("v" + id + "Mute")->load() > 0.5f;
        const auto pos = bubblePos (v);
        const float lvl = voiceLevel[v];
        const float d = 22.0f + lvl * 10.0f + gain * 4.0f;
        const auto c = ui::kVoice[v];

        g.setColour (muted ? c.withAlpha (0.2f) : c.withAlpha (0.75f));
        g.fillEllipse (Rectangle<float> (d, d).withCentre (pos));
        if (v == selected)
        {
            g.setColour (Colours::white.withAlpha (0.85f));
            g.drawEllipse (Rectangle<float> (d + 4.0f, d + 4.0f).withCentre (pos), 1.2f);
        }
        g.setColour (Colour (0xff14161c));
        g.setFont (Font (FontOptions (9.5f, Font::bold)));
        g.drawText (ui::voiceLabel (processor.apvts, v),
                    Rectangle<float> (d + 16.0f, 11.0f).withCentre (pos), Justification::centred);
    }

    {
        const float pulse = 26.0f + jlimit (0.0f, 1.0f, smoothedLevel * 10.0f) * 6.0f;
        g.setColour (ui::kLead.withAlpha (0.85f));
        g.fillEllipse (Rectangle<float> (pulse, pulse).withCentre ({ cx, cy }));
        String label = "LEAD";
        if (const float f0 = processor.uiF0.load(); f0 > 0.0f)
        {
            static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            const int m = (int) std::lround (69.0 + 12.0 * std::log2 (f0 / 440.0));
            label = String (pcs[((m % 12) + 12) % 12]) + String (m / 12 - 1);
        }
        g.setColour (Colour (0xff14161c));
        g.setFont (Font (FontOptions (10.5f, Font::bold)));
        g.drawText (label, Rectangle<float> (48.0f, 13.0f).withCentre ({ cx, cy }), Justification::centred);
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
        if (bubblePos (v).getDistanceFrom (e.position) < bubbleHitRadius (v))
        {
            dragging = v;
            selected = v;
            if (auto* p = processor.apvts.getParameter ("v" + id + "Pan"))
                p->beginChangeGesture();
            if (auto* p = processor.apvts.getParameter ("v" + id + "Gain"))
                p->beginChangeGesture();
            onSelect (v);
            mouseDrag (e);
            return;
        }
    }
}

void StageView::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0)
        return;
    const auto id = String (dragging + 1);
    float pan, gain;
    panGainFromPoint (e.position, pan, gain);
    if (auto* p = processor.apvts.getParameter ("v" + id + "Pan"))
        p->setValueNotifyingHost (p->convertTo0to1 (pan));
    if (auto* p = processor.apvts.getParameter ("v" + id + "Gain"))
        p->setValueNotifyingHost (p->convertTo0to1 (gain));
    repaint();
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
MixerView::MixerView (ChoraleProcessor& p, std::function<void (int)> cb)
    : processor (p), onSelect (std::move (cb))
{
    auto fillCombo = [&] (ComboBox& c, const String& paramId)
    {
        if (auto* par = dynamic_cast<AudioParameterChoice*> (p.apvts.getParameter (paramId)))
            c.addItemList (par->choices, 1);
    };

    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        auto& ch = channels[v];
        const auto id = String (v + 1);
        const auto accent = ui::kVoice[v];

        ch.title.setText ("VOICE " + id, dontSendNotification);
        ch.title.setJustificationType (Justification::centred);
        ch.title.setFont (Font (FontOptions (10.0f, Font::bold)));
        ch.title.setColour (Label::textColourId, accent);
        addAndMakeVisible (ch.title);

        addAndMakeVisible (ch.mode);
        addAndMakeVisible (ch.degree);
        addAndMakeVisible (ch.note);
        fillCombo (ch.mode, "v" + id + "Mode");
        fillCombo (ch.degree, "v" + id + "Degree");
        fillCombo (ch.note, "v" + id + "Note");

        for (auto* s : { &ch.detune, &ch.pan })
        {
            s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
            s->setColour (Slider::rotarySliderFillColourId, accent);
            addAndMakeVisible (*s);
        }
        for (auto& [l, text] : std::initializer_list<std::pair<Label*, const char*>> {
                 { &ch.detuneLbl, "DETUNE" }, { &ch.panLbl, "PAN" } })
        {
            l->setText (text, dontSendNotification);
            l->setFont (Font (FontOptions (8.5f)));
            l->setColour (Label::textColourId, ui::kDim);
            l->setJustificationType (Justification::centred);
            addAndMakeVisible (*l);
        }

        ch.fader.bind (p, "v" + id + "Gain");
        addAndMakeVisible (ch.fader);

        ch.modeAtt = std::make_unique<Channel::ComboAtt> (p.apvts, "v" + id + "Mode", ch.mode);
        ch.degreeAtt = std::make_unique<Channel::ComboAtt> (p.apvts, "v" + id + "Degree", ch.degree);
        ch.noteAtt = std::make_unique<Channel::ComboAtt> (p.apvts, "v" + id + "Note", ch.note);
        ch.detuneAtt = std::make_unique<Channel::SliderAtt> (p.apvts, "v" + id + "Detune", ch.detune);
        ch.panAtt = std::make_unique<Channel::SliderAtt> (p.apvts, "v" + id + "Pan", ch.pan);

        ch.mode.onChange = [this, v] { refreshChannel (v); };
        refreshChannel (v);
    }
    startTimerHz (30);
}

void MixerView::refreshChannel (int v)
{
    auto& ch = channels[v];
    const int m = (int) processor.apvts.getRawParameterValue ("v" + String (v + 1) + "Mode")->load();
    ch.degree.setVisible (m == 1 || m == 0 || m == 3);
    ch.note.setVisible (m == 2);
    const float alpha = m == 0 ? 0.45f : 1.0f;
    ch.detune.setAlpha (alpha);
    ch.pan.setAlpha (alpha);
    ch.detuneLbl.setAlpha (alpha);
    ch.panLbl.setAlpha (alpha);
    ch.fader.setAlpha (alpha);
}

void MixerView::setSelected (int v)
{
    selected = jlimit (0, ChoraleProcessor::kNumVoices - 1, v);
    repaint();
}

void MixerView::paint (Graphics& g)
{
    g.setColour (Colour (0xff0a0c10));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    const int colW = getWidth() / ChoraleProcessor::kNumVoices;
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        auto col = getLocalBounds().withWidth (colW).translated (v * colW, 0).toFloat().reduced (2.0f, 8.0f);
        g.setColour (ui::kVoice[v].withAlpha (v == selected ? 0.55f : 0.85f));
        g.fillRoundedRectangle (col.getX() + 8.0f, col.getY(), col.getWidth() - 16.0f, 2.5f, 1.0f);
    }

    g.setColour (Colour (0xff1e222c));
    for (int v = 1; v < ChoraleProcessor::kNumVoices; ++v)
        g.drawVerticalLine (v * colW, 10.0f, (float) getHeight() - 10.0f);

    g.setColour (Colour (0xff222735));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
}

void MixerView::resized()
{
    const int colW = getWidth() / ChoraleProcessor::kNumVoices;
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        auto col = getLocalBounds().withWidth (colW).translated (v * colW, 0).reduced (5, 10);
        col.removeFromTop (4);
        channels[v].title.setBounds (col.removeFromTop (14));
        col.removeFromTop (4);
        channels[v].mode.setBounds (col.removeFromTop (22));
        col.removeFromTop (3);
        auto pitchRow = col.removeFromTop (22);
        channels[v].degree.setBounds (pitchRow);
        channels[v].note.setBounds (pitchRow);
        col.removeFromTop (6);

        auto knobs = col.removeFromTop (58);
        const int kw = knobs.getWidth() / 2;
        auto k1 = knobs.removeFromLeft (kw), k2 = knobs.removeFromLeft (kw);
        channels[v].detune.setBounds (k1.removeFromTop (44));
        channels[v].detuneLbl.setBounds (k1);
        channels[v].pan.setBounds (k2.removeFromTop (44));
        channels[v].panLbl.setBounds (k2);

        col.removeFromTop (4);
        channels[v].fader.setBounds (col);
    }
}

void MixerView::mouseDown (const MouseEvent& e)
{
    if (e.originalComponent != this)
        return;
    const int colW = getWidth() / ChoraleProcessor::kNumVoices;
    const int v = e.x / colW;
    if (v >= 0 && v < ChoraleProcessor::kNumVoices)
    {
        selected = v;
        onSelect (v);
        repaint();
    }
}

void MixerView::timerCallback()
{
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        refreshChannel (v);
        channels[v].fader.setMeterLevel (processor.uiVoiceGain[v].load());
    }
}

//==============================================================================
VoiceDetailPanel::VoiceDetailPanel (ChoraleProcessor& p) : processor (p)
{
    title.setFont (Font (FontOptions (13.0f, Font::bold)));
    addAndMakeVisible (title);
    addAndMakeVisible (mode);
    addAndMakeVisible (degree);
    addAndMakeVisible (note);

    for (auto* s : { &detune, &pan })
    {
        s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
    }
    addAndMakeVisible (level);
    for (auto& [l, text] : std::initializer_list<std::pair<Label*, const char*>> {
             { &detuneLbl, "DETUNE" }, { &panLbl, "PAN" }, { &levelLbl, "GAIN" } })
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
    for (auto* s : { &detune, &pan })
        s->setColour (Slider::rotarySliderFillColourId, accent);
    solo.setColour (TextButton::buttonOnColourId, Colours::white);
    mute.setColour (TextButton::buttonOnColourId, accent);

    // Rebind everything to this voice's parameters.
    modeAtt.reset(); degreeAtt.reset(); noteAtt.reset();
    detuneAtt.reset(); panAtt.reset();
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
    level.bind (processor, "v" + id + "Gain");
    soloAtt = std::make_unique<ButtonAtt> (processor.apvts, "v" + id + "Solo", solo);
    muteAtt = std::make_unique<ButtonAtt> (processor.apvts, "v" + id + "Mute", mute);

    refresh();
    repaint();
}

void VoiceDetailPanel::tick()
{
    level.setMeterLevel (processor.uiVoiceGain[voice].load());
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
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromTop (2);
    auto titleRow = r.removeFromTop (22);
    solo.setBounds (titleRow.removeFromRight (24));
    titleRow.removeFromRight (4);
    mute.setBounds (titleRow.removeFromRight (24));
    title.setBounds (titleRow);
    r.removeFromTop (6);
    mode.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);
    auto sel = r.removeFromTop (24);
    degree.setBounds (sel);
    note.setBounds (sel);
    r.removeFromTop (8);

    auto knobs = r.removeFromTop (68);
    const int kw = knobs.getWidth() / 2;
    auto k1 = knobs.removeFromLeft (kw), k2 = knobs.removeFromLeft (kw);
    detune.setBounds (k1.removeFromTop (52));
    detuneLbl.setBounds (k1);
    pan.setBounds (k2.removeFromTop (52));
    panLbl.setBounds (k2);

    r.removeFromTop (6);
    levelLbl.setBounds (r.removeFromTop (11));
    level.setBounds (r);
}

//==============================================================================
ChoraleEditor::ChoraleEditor (ChoraleProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      chips (p, [this] (int v) { selectVoice (v); }),
      stage (p, [this] (int v) { selectVoice (v); }),
      mixer (p, [this] (int v) { selectVoice (v); }),
      detail (p)
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

    autoKeyLbl.setFont (Font (FontOptions (24.0f, Font::bold)));
    autoKeyLbl.setColour (Label::textColourId, Colour (0xff8ef5a8));
    autoKeyLbl.setJustificationType (Justification::centred);
    autoKeyLbl.setInterceptsMouseClicks (false, false);
    addChildComponent (autoKeyLbl);

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
    addAndMakeVisible (mixer);
    addAndMakeVisible (detail);

    for (auto* b : { &stageBtn, &mixerBtn })
    {
        b->setClickingTogglesState (true);
        b->setColour (TextButton::buttonColourId, ui::kPanelHi);
        b->setColour (TextButton::buttonOnColourId, ui::kText);
        b->setColour (TextButton::textColourOnId, Colour (0xff14161c));
        b->setColour (TextButton::textColourOffId, ui::kDim);
        addAndMakeVisible (*b);
    }
    stageBtn.onClick = [this] { setMainView (false); };
    mixerBtn.onClick = [this] { setMainView (true); };

    chips.setSelected (1);
    stage.setSelected (0);
    mixer.setSelected (0);
    setMainView (false);

    setSize (1120, 648);
    startTimerHz (15);
}

ChoraleEditor::~ChoraleEditor()
{
    setLookAndFeel (nullptr);
}

void ChoraleEditor::selectVoice (int v)
{
    if (v < 0)
        return;
    detail.setVoice (v);
    stage.setSelected (v);
    mixer.setSelected (v);
    chips.setSelected (v + 1);
}

void ChoraleEditor::setMainView (bool mixerView)
{
    showMixer = mixerView;
    stage.setVisible (! mixerView);
    mixer.setVisible (mixerView);
    detail.setVisible (! mixerView);
    chips.setVisible (! mixerView);
    stageBtn.setToggleState (! mixerView, dontSendNotification);
    mixerBtn.setToggleState (mixerView, dontSendNotification);
    resized();
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
    if (autoScale)
    {
        autoKeyLbl.setText (String (pcs[proc.uiRoot.load() % 12])
                                + (proc.uiMinor.load() ? " minor" : " major"),
                            dontSendNotification);
        autoKeyLbl.setColour (Label::textColourId, Colour (0xff8ef5a8));
    }
    else
    {
        const int rootPc = (int) proc.apvts.getRawParameterValue ("keyRoot")->load();
        const int scaleIdx = (int) proc.apvts.getRawParameterValue ("scale")->load();
        String scaleName = "Major";
        if (auto* par = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter ("scale")))
            scaleName = par->choices[scaleIdx];
        autoKeyLbl.setText (String (pcs[rootPc % 12]) + "  " + scaleName, dontSendNotification);
        autoKeyLbl.setColour (Label::textColourId, ui::kText);
    }

    detail.refresh();
    detail.tick();
    chips.repaint();
}

void ChoraleEditor::paint (Graphics& g)
{
    g.fillAll (ui::kBg);
    g.setColour (ui::kPanel.brighter (0.03f));
    g.fillRect (0, 0, getWidth(), 56);
    g.setColour (Colour (0xff232838));
    g.drawHorizontalLine (56, 0.0f, (float) getWidth());
}

void ChoraleEditor::paintOverChildren (Graphics& g)
{
    if (autoKeyLbl.getText().isEmpty())
        return;

    const auto r = autoKeyLbl.getBounds().toFloat().expanded (24.0f, 8.0f);
    const bool detected = autoKeyLbl.findColour (Label::textColourId) == Colour (0xff8ef5a8);
    if (detected)
    {
        ColourGradient glow (Colour (0xff8ef5a8).withAlpha (0.14f), r.getCentreX(), r.getCentreY(),
                             Colours::transparentBlack, r.getCentreX(), r.getBottom() + 10.0f, true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (r, 10.0f);
    }

    g.setFont (autoKeyLbl.getFont());
    g.setColour (autoKeyLbl.findColour (Label::textColourId));
    g.drawText (autoKeyLbl.getText(), autoKeyLbl.getBounds(), Justification::centred);
}

void ChoraleEditor::layoutHeader()
{
    const int w = getWidth();
    const int pad = 14;

    titleLbl.setBounds (pad, 18, 100, 24);
    preset.setBounds (pad + 108, 15, 190, 26);
    stageBtn.setBounds (pad + 306, 16, 54, 24);
    mixerBtn.setBounds (pad + 364, 16, 54, 24);

    autoKeyLbl.setBounds (w / 2 - 160, 12, 320, 32);
    autoKeyLbl.setVisible (true);

    const int rightW = 348;
    const int rx = w - pad - rightW;
    keyLbl.setBounds (rx, 6, 62, 14);
    keyRoot.setBounds (rx, 20, 62, 25);
    scaleLbl.setBounds (rx + 68, 6, 104, 14);
    scale.setBounds (rx + 68, 20, 104, 25);
    correctLbl.setBounds (rx + 178, 6, 88, 14);
    correct.setBounds (rx + 178, 20, 88, 25);
    const int mixSz = 32;
    const int mixX = w - pad - mixSz;
    mixLbl.setBounds (mixX - 2, 6, mixSz + 4, 12);
    mix.setBounds (mixX, 20, mixSz, mixSz);
}

void ChoraleEditor::resized()
{
    layoutHeader();

    auto r = getLocalBounds();
    r.removeFromTop (56);

    if (! showMixer)
    {
        auto chipRow = r.removeFromTop (38);
        chips.setBounds (chipRow.reduced (14, 0));
    }
    else
        r.removeFromTop (4);

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

    r.reduce (14, 6);
    if (! showMixer)
    {
        detail.setBounds (r.removeFromLeft (220));
        r.removeFromLeft (8);
    }
    stage.setBounds (r);
    mixer.setBounds (r);
}

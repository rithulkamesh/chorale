#include "StageView.h"

using namespace juce;

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
    return 30.0f;
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
    g.setColour (ui::kBg.darker (0.2f));
    g.fillRoundedRectangle (r, 10.0f);
    g.reduceClipRegion (getLocalBounds().reduced (1));

    float cx, cy, rMax, rMin;
    stageGeometry (cx, cy, rMax, rMin);

    // --- Design-system radar background ---------------------------------
    // Full concentric rings around the lead position.
    g.setColour (ui::kBorder.withAlpha (0.55f));
    for (int i = 1; i <= 7; ++i)
    {
        const float rr = rMin + (float) i / 7.0f * 0.98f * rMax;
        g.drawEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 0.6f);
    }
    g.drawLine (cx, cy - rMax, cx, cy, 0.5f);

    // Staff lines fading in from the left and right edges.
    {
        const float staffY = r.getCentreY() - 16.0f;
        const float span = r.getWidth() * 0.22f;
        for (int line = 0; line < 5; ++line)
        {
            const float ly = staffY + (float) line * 8.0f;
            ColourGradient lg (ui::kDim.withAlpha (0.22f), r.getX() + 16.0f, ly,
                               Colours::transparentBlack, r.getX() + 16.0f + span, ly, false);
            g.setGradientFill (lg);
            g.fillRect (Rectangle<float> (r.getX() + 16.0f, ly, span, 1.0f));
            ColourGradient rg (ui::kDim.withAlpha (0.22f), r.getRight() - 16.0f, ly,
                               Colours::transparentBlack, r.getRight() - 16.0f - span, ly, false);
            g.setGradientFill (rg);
            g.fillRect (Rectangle<float> (r.getRight() - 16.0f - span, ly, span, 1.0f));
        }
    }

    // Scattered dots (deterministic star field).
    {
        Random rng (7); // fixed seed: same sky every frame
        for (int i = 0; i < 16; ++i)
        {
            const float px = r.getX() + rng.nextFloat() * r.getWidth();
            const float py = r.getY() + rng.nextFloat() * r.getHeight() * 0.85f;
            const float sz = 1.0f + rng.nextFloat() * 1.5f;
            g.setColour (ui::kDim.withAlpha (0.10f + rng.nextFloat() * 0.22f));
            g.fillEllipse (px, py, sz, sz);
        }
    }
    // ---------------------------------------------------------------------

    {
        const float glow = jlimit (0.0f, 1.0f, smoothedLevel * 10.0f);
        ColourGradient wash (ui::kText.withAlpha (0.05f + glow * 0.07f), cx, cy,
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
        ColourGradient bloom (ui::kVoice[v].withAlpha (0.03f + lvl * 0.08f), pos.x, pos.y,
                              Colours::transparentBlack, pos.x, pos.y - rad, true);
        g.setGradientFill (bloom);
        g.fillEllipse (pos.x - rad, pos.y - rad, rad * 2.0f, rad * 2.0f);
    }

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
        // Base diameter fits a 4-char label ("MIDI", "OCT^") comfortably.
        const float d = 34.0f + lvl * 8.0f + gain * 4.0f;
        const auto c = ui::kVoice[v];

        g.setColour (muted ? c.withAlpha (0.2f) : c.withAlpha (0.9f));
        g.fillEllipse (Rectangle<float> (d, d).withCentre (pos));
        if (v == selected)
        {
            g.setColour (ui::kText.withAlpha (0.85f));
            g.drawEllipse (Rectangle<float> (d + 4.0f, d + 4.0f).withCentre (pos), 1.2f);
        }
        g.setColour (ui::textOn (c).withAlpha (muted ? 0.5f : 1.0f));
        g.setFont (ui::sans (9.0f, true));
        g.drawText (ui::voiceLabel (processor.apvts, v),
                    Rectangle<float> (d, 11.0f).withCentre (pos), Justification::centred);
    }

    {
        const float pulse = 34.0f + jlimit (0.0f, 1.0f, smoothedLevel * 10.0f) * 6.0f;
        g.setColour (ui::kLead.withAlpha (0.92f));
        g.fillEllipse (Rectangle<float> (pulse, pulse).withCentre ({ cx, cy }));
        String label = "LEAD";
        if (const float f0 = processor.uiF0.load(); f0 > 0.0f)
        {
            static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            const int m = (int) std::lround (69.0 + 12.0 * std::log2 (f0 / 440.0));
            label = String (pcs[((m % 12) + 12) % 12]) + String (m / 12 - 1);
        }
        g.setColour (ui::kBg);
        g.setFont (ui::sans (10.5f, true));
        g.drawText (label, Rectangle<float> (48.0f, 13.0f).withCentre ({ cx, cy }), Justification::centred);
    }

    g.setColour (ui::kBorder);
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

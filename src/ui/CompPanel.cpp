#include "CompPanel.h"

using namespace juce;

namespace
{
constexpr float kMinDb = -60.0f, kMaxDb = 0.0f;
constexpr float kGrMeterW = 14.0f;
} // namespace

CompPanel::CompPanel (ChoraleProcessor& p, Colour c) : proc (p), accent (c)
{
    startTimerHz (30);
}

void CompPanel::setTarget (const String& pre, int scopeCh, Colour c)
{
    prefix = pre;
    scopeChannel = scopeCh;
    accent = c;
    repaint();
}

RangedAudioParameter* CompPanel::param (bool r) const
{
    return proc.apvts.getParameter (prefix + (r ? "R" : "T"));
}

float CompPanel::thresh() const
{
    if (auto* p = param (false))
        return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

float CompPanel::ratio() const
{
    if (auto* p = param (true))
        return p->convertFrom0to1 (p->getValue());
    return 2.0f;
}

// Static transfer curve incl. the auto makeup the audio path applies.
float CompPanel::outDbFor (float inDb) const
{
    const float t = thresh();
    const float inv = 1.0f - 1.0f / jmax (1.01f, ratio());
    const float makeup = -t * inv * 0.4f;
    if (inDb <= t)
        return inDb + makeup;
    return t + (inDb - t) / jmax (1.01f, ratio()) + makeup;
}

Rectangle<float> CompPanel::plotArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f).withTrimmedRight (kGrMeterW + 6.0f);
}

float CompPanel::dbToX (float dB) const
{
    const auto r = plotArea();
    return r.getX() + (dB - kMinDb) / (kMaxDb - kMinDb) * r.getWidth();
}

float CompPanel::dbToY (float dB) const
{
    const auto r = plotArea();
    return r.getBottom() - (dB - kMinDb) / (kMaxDb - kMinDb) * r.getHeight();
}

void CompPanel::timerCallback()
{
    proc.readScope (scopeChannel, scope, 512);
    double e = 0;
    for (float v : scope)
        e += v * v;
    const float rmsDb = 10.0f * std::log10 (jmax (1e-9, e / 512.0));
    levelDb += 0.3f * (jlimit (kMinDb, kMaxDb, rmsDb) - levelDb);
    const float gr = -proc.compGrDb (scopeChannel); // positive dB of reduction
    grSmooth += 0.4f * (gr - grSmooth);
    repaint();
}

void CompPanel::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.setColour (ui::kBg.darker (0.2f));
    g.fillRoundedRectangle (full, 8.0f);

    const auto r = plotArea();

    // Grid + unity diagonal.
    g.setColour (ui::kBorder.withAlpha (0.5f));
    for (float dB : { -48.0f, -36.0f, -24.0f, -12.0f })
    {
        g.drawVerticalLine ((int) dbToX (dB), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) dbToY (dB), r.getX(), r.getRight());
    }
    g.setColour (ui::kDim.withAlpha (0.35f));
    g.drawLine (dbToX (kMinDb), dbToY (kMinDb), dbToX (kMaxDb), dbToY (kMaxDb), 1.0f);

    g.setFont (ui::mono (7.5f));
    g.setColour (ui::kDim.withAlpha (0.6f));
    g.drawText ("-24", (int) dbToX (-24.0f) + 2, (int) r.getBottom() - 11, 24, 10, Justification::left);
    g.drawText ("0", (int) r.getRight() - 12, (int) r.getBottom() - 11, 10, 10, Justification::left);

    // Transfer curve.
    Path curve;
    for (int i = 0; i <= 64; ++i)
    {
        const float inDb = kMinDb + (kMaxDb - kMinDb) * (float) i / 64.0f;
        const float x = dbToX (inDb);
        const float y = dbToY (jlimit (kMinDb, 6.0f, outDbFor (inDb)));
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }
    g.setColour (accent);
    g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // Threshold node on the knee.
    {
        const float t = thresh();
        const auto knee = Point<float> (dbToX (t), dbToY (jlimit (kMinDb, 6.0f, outDbFor (t))));
        g.setColour (dragging ? ui::kText : accent);
        g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (knee));
        g.setColour (ui::kBg);
        g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre (knee));
    }

    // Live input level riding the curve.
    if (levelDb > kMinDb + 1.0f)
    {
        const auto dot = Point<float> (dbToX (levelDb),
                                       dbToY (jlimit (kMinDb, 6.0f, outDbFor (levelDb))));
        g.setColour (ui::kText.withAlpha (0.9f));
        g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre (dot));
    }

    // Gain-reduction meter (top-down, 0..-24 dB).
    {
        auto meter = full.reduced (8.0f);
        meter = meter.removeFromRight (kGrMeterW).withTrimmedTop (2.0f).withTrimmedBottom (2.0f);
        const auto lane = meter.withSizeKeepingCentre (3.0f, meter.getHeight());
        g.setColour (ui::kPanelHi);
        g.fillRoundedRectangle (lane, 1.5f);
        const float norm = jlimit (0.0f, 1.0f, grSmooth / 24.0f);
        auto fill = lane.withHeight (lane.getHeight() * norm);
        g.setColour (ui::kText.withAlpha (norm > 0.01f ? 0.9f : 0.2f));
        g.fillRoundedRectangle (fill, 1.5f);
        g.setColour (ui::kDim.withAlpha (0.6f));
        g.setFont (ui::mono (7.0f));
        g.drawText ("GR", Rectangle<float> (meter.getX() - 4.0f, lane.getBottom() - 2.0f, 22.0f, 10.0f),
                    Justification::centred);
    }

    g.setColour (ui::kBorder);
    g.drawRoundedRectangle (full.reduced (0.5f), 8.0f, 1.0f);
}

void CompPanel::mouseDown (const MouseEvent& e)
{
    dragging = true;
    if (auto* p = param (false))
        p->beginChangeGesture();
    mouseDrag (e);
}

void CompPanel::mouseDrag (const MouseEvent& e)
{
    if (! dragging)
        return;
    const auto r = plotArea();
    const float inDb = kMinDb + (e.position.x - r.getX()) / r.getWidth() * (kMaxDb - kMinDb);
    if (auto* p = param (false))
        p->setValueNotifyingHost (p->convertTo0to1 (jlimit (-40.0f, 0.0f, inDb)));
    repaint();
}

void CompPanel::mouseUp (const MouseEvent&)
{
    if (dragging)
        if (auto* p = param (false))
            p->endChangeGesture();
    dragging = false;
    repaint();
}

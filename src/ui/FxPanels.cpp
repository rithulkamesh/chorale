#include "FxPanels.h"
#include "../dsp/VoiceFx.h"

using namespace juce;

namespace
{
float scopeRms (ChoraleProcessor& proc, int ch, float* buf)
{
    proc.readScope (ch, buf, 512);
    double e = 0;
    for (int i = 0; i < 512; ++i)
        e += buf[i] * buf[i];
    return (float) std::sqrt (e / 512.0);
}

void panelFrame (Graphics& g, Component& c)
{
    g.setColour (ui::kBg.darker (0.2f));
    g.fillRoundedRectangle (c.getLocalBounds().toFloat(), 8.0f);
}

void panelBorder (Graphics& g, Component& c)
{
    g.setColour (ui::kBorder);
    g.drawRoundedRectangle (c.getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
}
} // namespace

//==============================================================================
SatPanel::SatPanel (ChoraleProcessor& p, Colour c) : proc (p), accent (c)
{
    startTimerHz (30);
}

void SatPanel::setTarget (const String& pre, int ch, Colour c)
{
    prefix = pre;
    scopeChannel = ch;
    accent = c;
    repaint();
}

void SatPanel::timerCallback()
{
    level += 0.3f * (scopeRms (proc, scopeChannel, scope) - level);
    repaint();
}

void SatPanel::paint (Graphics& g)
{
    panelFrame (g, *this);
    const auto r = getLocalBounds().toFloat().reduced (10.0f);

    float drive = 0.3f, mix = 1.0f;
    if (auto* p = proc.apvts.getRawParameterValue (prefix + "Drive"))
        drive = p->load();
    if (auto* p = proc.apvts.getRawParameterValue (prefix + "Mix"))
        mix = p->load();

    // Axes.
    g.setColour (ui::kBorder.withAlpha (0.6f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    g.drawVerticalLine ((int) r.getCentreX(), r.getY(), r.getBottom());
    // Unity reference.
    g.setColour (ui::kDim.withAlpha (0.3f));
    g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.0f);

    // Transfer curve, the exact function the audio path applies (incl. mix).
    Path curve;
    const int n = 64;
    for (int i = 0; i <= n; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) n;
        const float y = jlimit (-1.0f, 1.0f, Saturator::process (x, drive, mix));
        const float px = r.getX() + (x + 1.0f) * 0.5f * r.getWidth();
        const float py = r.getCentreY() - y * 0.5f * r.getHeight();
        if (i == 0)
            curve.startNewSubPath (px, py);
        else
            curve.lineTo (px, py);
    }
    g.setColour (accent);
    g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // Live input level marker on the curve (positive side).
    if (level > 0.001f)
    {
        const float x = jlimit (0.0f, 1.0f, level * 1.4f);
        const float y = jlimit (-1.0f, 1.0f, Saturator::process (x, drive, mix));
        const auto dot = Point<float> (r.getX() + (x + 1.0f) * 0.5f * r.getWidth(),
                                       r.getCentreY() - y * 0.5f * r.getHeight());
        g.setColour (ui::kText.withAlpha (0.9f));
        g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre (dot));
    }

    panelBorder (g, *this);
}

//==============================================================================
EchoPanel::EchoPanel (ChoraleProcessor& p, Colour c) : proc (p), accent (c)
{
    startTimerHz (30);
}

void EchoPanel::setTarget (const String& pre, Colour c)
{
    prefix = pre;
    accent = c;
    repaint();
}

void EchoPanel::timerCallback()
{
    float mix = 1.0f;
    if (auto* p = proc.apvts.getRawParameterValue (prefix + "Mix"))
        mix = p->load();
    level += 0.3f * (scopeRms (proc, ChoraleProcessor::kNumVoices, scope) * mix - level);
    repaint();
}

void EchoPanel::paint (Graphics& g)
{
    panelFrame (g, *this);
    const auto r = getLocalBounds().toFloat().reduced (12.0f, 10.0f);

    const float timeMs = jmax (1.0f, proc.apvts.getRawParameterValue (prefix + "Time")->load());
    const float fb = proc.apvts.getRawParameterValue (prefix + "Fb")->load();
    float mix = 0.0f;
    if (auto* p = proc.apvts.getRawParameterValue (prefix + "Mix"))
        mix = p->load();

    const float span = jmax (1200.0f, timeMs * 5.0f); // ms across the panel
    const float midY = r.getCentreY();
    g.setColour (ui::kBorder.withAlpha (0.6f));
    g.drawHorizontalLine ((int) midY, r.getX(), r.getRight());

    // Source impulse + ping-pong repeats decaying by feedback. Live level
    // scales everything, so the display breathes with the mix.
    const float base = jlimit (0.12f, 1.0f, 0.25f + level * 6.0f) * (mix > 0.001f ? 1.0f : 0.35f);
    auto bar = [&] (float ms, float amp, bool up)
    {
        const float x = r.getX() + ms / span * r.getWidth();
        if (x > r.getRight())
            return;
        const float h = amp * r.getHeight() * 0.46f;
        g.fillRoundedRectangle (Rectangle<float> (x - 1.5f, up ? midY - h : midY,
                                                  3.0f, h),
                                1.5f);
    };
    g.setColour (accent);
    bar (0.0f, base, true);
    float amp = base;
    for (int k = 1; k <= 8; ++k)
    {
        amp *= jmax (0.05f, fb);
        if (amp < 0.02f)
            break;
        g.setColour (accent.withAlpha (jlimit (0.15f, 1.0f, amp / base)));
        bar (timeMs * (float) k, amp, k % 2 == 0); // alternate sides = ping-pong
    }

    if (timeMs <= 1.0f)
    {
        g.setColour (ui::kDim.withAlpha (0.7f));
        g.setFont (ui::sans (9.5f));
        g.drawText ("Echo time is 0 — turn up TIME",
                    getLocalBounds(), Justification::centred);
    }

    panelBorder (g, *this);
}

//==============================================================================
VerbPanel::VerbPanel (ChoraleProcessor& p, Colour c) : proc (p), accent (c)
{
    startTimerHz (30);
}

void VerbPanel::setTarget (const String& pre, Colour c)
{
    prefix = pre;
    accent = c;
    repaint();
}

void VerbPanel::timerCallback()
{
    float mix = 1.0f;
    if (auto* p = proc.apvts.getRawParameterValue (prefix + "Mix"))
        mix = p->load();
    level += 0.3f * (scopeRms (proc, ChoraleProcessor::kNumVoices, scope) * mix - level);
    repaint();
}

void VerbPanel::paint (Graphics& g)
{
    panelFrame (g, *this);
    const auto r = getLocalBounds().toFloat().reduced (12.0f, 10.0f);

    const float size = proc.apvts.getRawParameterValue (prefix + "Size")->load();
    // Rough T60 for the freeverb tuning: bigger room, longer tail.
    const float t60 = 0.4f + size * 3.6f; // seconds
    const float span = 4.0f;              // seconds across the panel

    const float base = jlimit (0.15f, 1.0f, 0.3f + level * 6.0f);

    // Decay envelope, filled.
    Path env;
    env.startNewSubPath (r.getX(), r.getBottom() - base * r.getHeight());
    const int n = 48;
    for (int i = 1; i <= n; ++i)
    {
        const float t = span * (float) i / (float) n;
        const float amp = base * std::pow (10.0f, -3.0f * t / t60); // -60 dB at t60
        env.lineTo (r.getX() + t / span * r.getWidth(), r.getBottom() - amp * r.getHeight());
    }
    Path fill = env;
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (env, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // T60 marker.
    if (t60 < span)
    {
        const float x = r.getX() + t60 / span * r.getWidth();
        g.setColour (ui::kDim.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom());
    }
    g.setColour (ui::kDim.withAlpha (0.7f));
    g.setFont (ui::mono (7.5f));
    g.drawText (String (t60, 1) + " s tail",
                Rectangle<float> (r.getRight() - 60.0f, r.getY(), 60.0f, 12.0f),
                Justification::centredRight);

    panelBorder (g, *this);
}

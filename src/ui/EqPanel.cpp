#include "EqPanel.h"
#include "../dsp/VoiceFx.h"

using namespace juce;

namespace
{
constexpr float kMinHz = 25.0f, kMaxHz = 20000.0f;
constexpr float kMaxDb = 15.0f;    // EQ gain scale (curve + nodes)
constexpr float kSpecTop = 6.0f;   // spectrum dB at panel top
constexpr float kSpecBot = -66.0f; // spectrum dB at panel bottom
} // namespace

EqPanel::EqPanel (ChoraleProcessor& p, Colour c) : proc (p), accent (c)
{
    for (int i = 0; i < kFftSize; ++i)
        window[i] = 0.5f * (1.0f - std::cos (2.0f * MathConstants<float>::pi
                                             * (float) i / (float) (kFftSize - 1)));
    startTimerHz (30);
}

void EqPanel::setTarget (const String& pre, int scopeCh, Colour c)
{
    prefix = pre;
    scopeChannel = scopeCh;
    accent = c;
    std::fill (std::begin (smoothedDb), std::end (smoothedDb), kSpecBot);
    repaint();
}

RangedAudioParameter* EqPanel::param (int band, bool gain) const
{
    return proc.apvts.getParameter (prefix + String (band + 1) + (gain ? "G" : "F"));
}

float EqPanel::bandFreq (int band) const
{
    if (auto* p = param (band, false))
        return p->convertFrom0to1 (p->getValue());
    return 1000.0f;
}

float EqPanel::bandGain (int band) const
{
    if (auto* p = param (band, true))
        return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

float EqPanel::freqToX (float hz) const
{
    const float t = std::log (jlimit (kMinHz, kMaxHz, hz) / kMinHz) / std::log (kMaxHz / kMinHz);
    return 8.0f + t * ((float) getWidth() - 16.0f);
}

float EqPanel::xToFreq (float x) const
{
    const float t = jlimit (0.0f, 1.0f, (x - 8.0f) / ((float) getWidth() - 16.0f));
    return kMinHz * std::pow (kMaxHz / kMinHz, t);
}

float EqPanel::gainToY (float dB) const
{
    return (float) getHeight() / 2.0f - dB / kMaxDb * ((float) getHeight() / 2.0f - 8.0f);
}

float EqPanel::yToGain (float y) const
{
    return jlimit (-12.0f, 12.0f,
                   ((float) getHeight() / 2.0f - y) / ((float) getHeight() / 2.0f - 8.0f) * kMaxDb);
}

Point<float> EqPanel::nodePos (int band) const
{
    return { freqToX (bandFreq (band)), gainToY (bandGain (band)) };
}

void EqPanel::timerCallback()
{
    // Pull the scope, FFT it, smooth with fast attack / slow decay.
    proc.readScope (scopeChannel, fftData, kFftSize);
    for (int i = 0; i < kFftSize; ++i)
        fftData[i] *= window[i];
    std::fill (fftData + kFftSize, fftData + kFftSize * 2, 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData);
    for (int i = 0; i < kFftSize / 2; ++i)
    {
        const float mag = fftData[i] * 4.0f / (float) kFftSize;
        const float db = 20.0f * std::log10 (jmax (mag, 1e-7f));
        smoothedDb[i] = db > smoothedDb[i] ? db : smoothedDb[i] * 0.92f + db * 0.08f;
    }
    repaint();
}

void EqPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::kBg.darker (0.2f));
    g.fillRoundedRectangle (r, 8.0f);

    // Grid.
    g.setColour (ui::kBorder.withAlpha (0.5f));
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) freqToX (hz), 6.0f, (float) getHeight() - 6.0f);
    for (float dB : { -12.0f, -6.0f, 6.0f, 12.0f })
        g.drawHorizontalLine ((int) gainToY (dB), 8.0f, (float) getWidth() - 8.0f);
    g.setColour (ui::kBorder);
    g.drawHorizontalLine ((int) gainToY (0.0f), 6.0f, (float) getWidth() - 6.0f);

    g.setFont (ui::mono (7.5f));
    g.setColour (ui::kDim.withAlpha (0.6f));
    g.drawText ("100", (int) freqToX (100.0f) + 3, getHeight() - 14, 30, 10, Justification::left);
    g.drawText ("1k", (int) freqToX (1000.0f) + 3, getHeight() - 14, 30, 10, Justification::left);
    g.drawText ("10k", (int) freqToX (10000.0f) + 3, getHeight() - 14, 30, 10, Justification::left);

    // Live spectrum behind everything.
    {
        const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        Path spec;
        bool started = false;
        const int n = jmax (48, getWidth() / 3);
        for (int i = 0; i <= n; ++i)
        {
            const float x = 8.0f + (float) i / (float) n * ((float) getWidth() - 16.0f);
            const float hz = xToFreq (x);
            const float bin = hz / (float) sr * (float) kFftSize;
            const int b0 = jlimit (1, kFftSize / 2 - 2, (int) bin);
            const float frac = jlimit (0.0f, 1.0f, bin - (float) b0);
            const float db = smoothedDb[b0] * (1 - frac) + smoothedDb[b0 + 1] * frac;
            const float t = jlimit (0.0f, 1.0f, (kSpecTop - db) / (kSpecTop - kSpecBot));
            const float y = 6.0f + t * ((float) getHeight() - 12.0f);
            if (! started)
            {
                spec.startNewSubPath (x, y);
                started = true;
            }
            else
                spec.lineTo (x, y);
        }
        Path fill = spec;
        fill.lineTo ((float) getWidth() - 8.0f, (float) getHeight() - 6.0f);
        fill.lineTo (8.0f, (float) getHeight() - 6.0f);
        fill.closeSubPath();
        g.setColour (ui::kDim.withAlpha (0.10f));
        g.fillPath (fill);
        g.setColour (ui::kDim.withAlpha (0.45f));
        g.strokePath (spec, PathStrokeType (1.0f));
    }

    // Combined response curve, same biquads as the audio path.
    const float sr = proc.getSampleRate() > 0 ? (float) proc.getSampleRate() : 48000.0f;
    Biquad bands[kBands];
    bands[0].setLowShelf (sr, bandFreq (0), bandGain (0));
    for (int b = 1; b < kBands - 1; ++b)
        bands[b].setPeak (sr, bandFreq (b), bandGain (b), 1.0f);
    bands[kBands - 1].setHighShelf (sr, bandFreq (kBands - 1), bandGain (kBands - 1));

    Path curve;
    const int n = jmax (32, getWidth() / 4);
    for (int i = 0; i <= n; ++i)
    {
        const float x = 8.0f + (float) i / (float) n * ((float) getWidth() - 16.0f);
        const float hz = xToFreq (x);
        float dB = 0.0f;
        for (const auto& b : bands)
            dB += b.magDb (sr, hz);
        const float y = gainToY (dB);
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }
    Path fill = curve;
    fill.lineTo ((float) getWidth() - 8.0f, gainToY (0.0f));
    fill.lineTo (8.0f, gainToY (0.0f));
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    for (int b = 0; b < kBands; ++b)
    {
        const auto pos = nodePos (b);
        const bool engaged = std::abs (bandGain (b)) > 0.05f;
        g.setColour (b == dragBand ? ui::kText : accent.withAlpha (engaged ? 1.0f : 0.55f));
        g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (pos));
        g.setColour (ui::kBg);
        g.fillEllipse (Rectangle<float> (3.5f, 3.5f).withCentre (pos));
    }

    g.setColour (ui::kBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

void EqPanel::mouseDown (const MouseEvent& e)
{
    dragBand = -1;
    float best = 15.0f;
    for (int b = 0; b < kBands; ++b)
    {
        const float d = nodePos (b).getDistanceFrom (e.position);
        if (d < best)
        {
            best = d;
            dragBand = b;
        }
    }
    if (dragBand >= 0)
    {
        if (auto* p = param (dragBand, false)) p->beginChangeGesture();
        if (auto* p = param (dragBand, true)) p->beginChangeGesture();
        mouseDrag (e);
    }
}

void EqPanel::mouseDrag (const MouseEvent& e)
{
    if (dragBand < 0)
        return;
    if (auto* p = param (dragBand, false))
        p->setValueNotifyingHost (p->convertTo0to1 (xToFreq (e.position.x)));
    if (auto* p = param (dragBand, true))
        p->setValueNotifyingHost (p->convertTo0to1 (yToGain (e.position.y)));
    repaint();
}

void EqPanel::mouseUp (const MouseEvent&)
{
    if (dragBand < 0)
        return;
    if (auto* p = param (dragBand, false)) p->endChangeGesture();
    if (auto* p = param (dragBand, true)) p->endChangeGesture();
    dragBand = -1;
    repaint();
}

void EqPanel::mouseDoubleClick (const MouseEvent& e)
{
    for (int b = 0; b < kBands; ++b)
        if (nodePos (b).getDistanceFrom (e.position) < 12.0f)
            if (auto* p = param (b, true))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
                p->endChangeGesture();
            }
}

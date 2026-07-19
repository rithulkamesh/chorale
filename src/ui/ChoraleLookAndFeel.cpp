#include "ChoraleLookAndFeel.h"
#include "GainFader.h"

using namespace juce;

ChoraleLookAndFeel::ChoraleLookAndFeel()
{
    setDefaultSansSerifTypeface (ui::plexSans());
    setColour (Slider::rotarySliderFillColourId, ui::kAccent);
    setColour (Label::textColourId, ui::kText);
    setColour (PopupMenu::backgroundColourId, ui::kPanelHi);
    setColour (PopupMenu::textColourId, ui::kText);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff2e3138));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (PopupMenu::headerTextColourId, ui::kDim);
    setColour (ComboBox::textColourId, ui::kText);
    setColour (TextButton::textColourOffId, ui::kDim);
    setColour (TextButton::textColourOnId, ui::kBg);
}

// The knob spec: dark body with a subtle rim, short tick at 12 o'clock,
// 1px inactive track arc, 2px active arc, ink dot riding the arc end.
void ChoraleLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                           float pos, float a0, float a1, Slider& s)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = a0 + pos * (a1 - a0);
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    const bool bipolar = s.getMinimum() < 0.0;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;

    // Small knobs (header MIX etc): arc + dot only — the full body doesn't
    // read at this size.
    if (radius < 17.0f)
    {
        Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0, a0, a1, true);
        g.setColour (ui::kTrack);
        g.strokePath (track, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
        if (std::abs (angle - from) > 0.01f)
        {
            Path fillArc;
            fillArc.addCentredArc (centre.x, centre.y, radius, radius, 0,
                                   jmin (from, angle), jmax (from, angle), true);
            g.setColour (accent);
            g.strokePath (fillArc, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        const auto tip = centre.getPointOnCircumference (radius, angle);
        g.setColour (ui::kText);
        g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (tip));
        return;
    }

    // Body + rim.
    const float bodyR = radius - 4.0f;
    {
        const auto body = Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (centre);
        ColourGradient grad (Colour (0xff202226), centre.x, centre.y - bodyR,
                             Colour (0xff141519), centre.x, centre.y + bodyR, false);
        g.setGradientFill (grad);
        g.fillEllipse (body);
        g.setColour (Colour (0xff33363c));
        g.drawEllipse (body, 1.0f);
    }

    // Pointer on the body, rotating with the value (follows the arc dot).
    {
        const auto pIn = centre.getPointOnCircumference (bodyR - 7.0f, angle);
        const auto pOut = centre.getPointOnCircumference (bodyR - 1.5f, angle);
        g.setColour (ui::kText.withAlpha (0.9f));
        g.drawLine ({ pIn, pOut }, 1.8f);
    }

    // Inactive track arc (1px).
    Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0, a0, a1, true);
    g.setColour (ui::kTrack);
    g.strokePath (track, PathStrokeType (1.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // Active arc (2px) from the value-0 position.
    if (std::abs (angle - from) > 0.01f)
    {
        Path fill;
        fill.addCentredArc (centre.x, centre.y, radius, radius, 0,
                            jmin (from, angle), jmax (from, angle), true);
        g.setColour (accent);
        g.strokePath (fill, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Dot follows the arc end.
    const auto tip = centre.getPointOnCircumference (radius, angle);
    g.setColour (ui::kBg);
    g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (tip));
    g.setColour (ui::kText);
    g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (tip));
}

// The fader spec: 1px track, 1px zero line, tall rounded pill cap.
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
        track.removeFromLeft (12.0f);

    const float cx = track.getCentreX();
    const float minV = (float) s.getMinimum();
    const float maxV = (float) s.getMaximum();
    auto yFor = [&] (float db)
    { return track.getBottom() - (db - minV) / (maxV - minV) * track.getHeight(); };

    // Track (1px) + zero line (1px, wider).
    g.setColour (ui::kTrack);
    g.fillRect (Rectangle<float> (cx - 0.5f, track.getY(), 1.0f, track.getHeight()));
    g.fillRect (Rectangle<float> (cx - 9.0f, yFor (0.0f) - 0.5f, 18.0f, 1.0f));

    // Ticks + sparse labels.
    g.setFont (ui::mono (7.5f));
    for (float db : { -12.0f, -24.0f, -36.0f, -48.0f })
    {
        const float ty = yFor (db);
        g.setColour (ui::kTrack.withAlpha (0.8f));
        g.drawHorizontalLine ((int) ty, cx - 4.0f, cx + 4.0f);
        const int dbi = (int) db;
        if (dbi == -24)
        {
            g.setColour (ui::kDim.withAlpha (0.55f));
            g.drawText (String (dbi), Rectangle<float> (cx - 22.0f, ty - 5.0f, 15.0f, 10.0f),
                        Justification::centredRight);
        }
    }
    g.setColour (ui::kDim.withAlpha (0.55f));
    g.drawText ("0", Rectangle<float> (cx - 22.0f, yFor (0.0f) - 5.0f, 15.0f, 10.0f),
                Justification::centredRight);

    // Cap: rounded pill with a centre dash.
    const auto cap = Rectangle<float> (cx - 8.0f, sliderPos - 13.0f, 16.0f, 26.0f);
    ColourGradient grad (Colour (0xff9298a0), cap.getX(), cap.getY(),
                         Colour (0xff6d737b), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 7.0f);
    g.setColour (ui::kBg.withAlpha (0.55f));
    g.drawRoundedRectangle (cap, 7.0f, 1.0f);
    g.setColour (ui::kText);
    g.fillRoundedRectangle (Rectangle<float> (cx - 4.5f, sliderPos - 1.0f, 9.0f, 2.0f), 1.0f);
}

void ChoraleLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int,
                                       ComboBox& box)
{
    const auto r = Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Colour (0xff17181c));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (box.hasKeyboardFocus (true) ? ui::kDim : Colour (0xff363b42));
    g.drawRoundedRectangle (r, 9.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h / 2.0f - 1.5f;
    arrow.addTriangle (ax, ay, ax + 7.0f, ay, ax + 3.5f, ay + 4.5f);
    g.setColour (ui::kDim);
    g.fillPath (arrow);
}

void ChoraleLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&,
                                               bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);

    // The preset button masquerades as a combo box.
    if (b.getComponentID() == "presetBtn")
    {
        g.setColour (Colour (0xff17181c));
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (highlighted ? ui::kDim : Colour (0xff363b42));
        g.drawRoundedRectangle (r, 9.0f, 1.0f);
        Path arrow;
        const float ax = r.getRight() - 16.0f, ay = r.getCentreY() - 1.5f;
        arrow.addTriangle (ax, ay, ax + 7.0f, ay, ax + 3.5f, ay + 4.5f);
        g.setColour (ui::kDim);
        g.fillPath (arrow);
        return;
    }

    const bool chipBtn = b.getButtonText() == "STAGE" || b.getButtonText() == "MIXER"
                         || b.getButtonText() == "FX";
    const float radius = chipBtn ? 12.0f : 4.0f;

    if (b.getToggleState())
    {
        g.setColour (b.findColour (TextButton::buttonOnColourId));
        g.fillRoundedRectangle (r, radius);
    }
    else if (highlighted)
    {
        g.setColour (ui::kPanelHi.brighter (0.15f));
        g.fillRoundedRectangle (r, radius);
    }
    g.setColour (b.getToggleState() ? ui::kText : ui::kBorder);
    g.drawRoundedRectangle (r, radius, 1.0f);
}

Font ChoraleLookAndFeel::getComboBoxFont (ComboBox&) { return ui::sans (12.5f); }
Font ChoraleLookAndFeel::getPopupMenuFont() { return ui::sans (13.0f); }
Font ChoraleLookAndFeel::getTextButtonFont (TextButton& b, int)
{
    if (b.getComponentID() == "presetBtn")
        return ui::sans (12.5f);
    if (b.getButtonText() == "STAGE" || b.getButtonText() == "MIXER"
        || b.getButtonText() == "FX")
        return ui::sans (11.0f, true);
    return ui::sans (12.0f, true);
}

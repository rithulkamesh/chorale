#include "NoteChips.h"

using namespace juce;

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

        if (i == selected)
        {
            g.setColour (ui::kText);
            g.fillRoundedRectangle (r, 12.0f);
            g.setColour (ui::kBg);
        }
        else
        {
            g.setColour (c.withAlpha (off ? 0.25f : 0.7f));
            g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
            g.setColour (off ? ui::kDim.withAlpha (0.5f)
                             : (isLead ? c : ui::voiceInk (v)));
        }
        g.setFont (ui::sans (11.0f, true));
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

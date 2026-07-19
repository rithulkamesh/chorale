#include "FxView.h"

using namespace juce;

//==============================================================================
ChainStrip::ChainStrip (ChoraleProcessor& p, std::function<void (int)> cb)
    : proc (p), onSelect (std::move (cb))
{
    startTimerHz (10);
}

void ChainStrip::setModules (Array<Module> m)
{
    modules = std::move (m);
    selected = jlimit (0, modules.size() - 1, selected);
    repaint();
}

void ChainStrip::setSelected (int index)
{
    selected = jlimit (0, modules.size() - 1, index);
    repaint();
}

Rectangle<float> ChainStrip::cardRect (int index) const
{
    const int n = jmax (1, modules.size());
    constexpr float arrowW = 16.0f;
    const float cardW = ((float) getWidth() - arrowW * (float) (n - 1)) / (float) n;
    return { (float) index * (cardW + arrowW), 2.0f, cardW, (float) getHeight() - 4.0f };
}

bool ChainStrip::moduleOn (int index) const
{
    const auto& id = modules[index].onParamId;
    if (id.isEmpty())
        return true;
    return proc.apvts.getRawParameterValue (id)->load() > 0.5f;
}

void ChainStrip::paint (Graphics& g)
{
    for (int i = 0; i < modules.size(); ++i)
    {
        const auto r = cardRect (i);
        const bool on = moduleOn (i);

        g.setColour (Colour (0xff17181c));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (i == selected ? ui::kText : Colour (0xff2c2f34));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, i == selected ? 1.2f : 1.0f);

        // Power dot (only for toggleable modules).
        float textX = r.getX() + 8.0f;
        if (modules[i].onParamId.isNotEmpty())
        {
            const auto dot = Rectangle<float> (8.0f, 8.0f)
                                 .withCentre ({ r.getX() + 13.0f, r.getCentreY() });
            g.setColour (on ? ui::kText : ui::kDim.withAlpha (0.35f));
            if (on)
                g.fillEllipse (dot);
            else
                g.drawEllipse (dot, 1.2f);
            textX = dot.getRight() + 6.0f;
        }

        g.setColour (on ? (i == selected ? ui::kText : ui::kDim.brighter (0.3f))
                        : ui::kDim.withAlpha (0.45f));
        g.setFont (ui::sans (10.0f, true));
        g.drawText (modules[i].name,
                    Rectangle<float> (textX, r.getY(), r.getRight() - textX - 4.0f, r.getHeight()),
                    Justification::centredLeft);

        if (i < modules.size() - 1)
        {
            g.setColour (ui::kDim.withAlpha (0.5f));
            g.setFont (ui::sans (11.0f));
            g.drawText (String::charToString (0x2192),
                        Rectangle<float> (r.getRight(), r.getY(), 16.0f, r.getHeight()),
                        Justification::centred);
        }
    }
}

void ChainStrip::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < modules.size(); ++i)
    {
        const auto r = cardRect (i);
        if (! r.contains (e.position))
            continue;
        // Left sliver with the dot = power toggle; anywhere else = select.
        if (modules[i].onParamId.isNotEmpty() && e.position.x < r.getX() + 22.0f)
        {
            if (auto* p = proc.apvts.getParameter (modules[i].onParamId))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
                p->endChangeGesture();
            }
        }
        selected = i;
        onSelect (i);
        repaint();
        return;
    }
}

//==============================================================================
FxView::FxView (ChoraleProcessor& p)
    : proc (p),
      voiceChain (p, [this] (int m) { showVoiceModule (m); }),
      masterChain (p, [this] (int m) { showMasterModule (m); }),
      voiceEq (p, ui::voiceInk (0)),
      masterEq (p, ui::kText)
{
    voiceTitle.setFont (ui::sans (13.0f, true));
    masterTitle.setFont (ui::sans (13.0f, true));
    masterTitle.setText ("MASTER", dontSendNotification);
    masterTitle.setColour (Label::textColourId, ui::kText);
    addAndMakeVisible (voiceTitle);
    addAndMakeVisible (masterTitle);

    for (auto* h : { &voiceHint, &masterHint })
    {
        h->setFont (ui::sans (9.5f));
        h->setColour (Label::textColourId, ui::kDim.withAlpha (0.7f));
        h->setJustificationType (Justification::centred);
        addAndMakeVisible (*h);
    }

    addAndMakeVisible (voiceChain);
    addAndMakeVisible (masterChain);
    {
        Array<ChainStrip::Module> mods;
        mods.add ({ "EQ", "mEqOn" });
        mods.add ({ "REVERB", "" });
        masterChain.setModules (std::move (mods));
    }

    addChildComponent (voiceEq);
    addChildComponent (masterEq);
    masterEq.setTarget ("mEq", ChoraleProcessor::kNumVoices, ui::kText);

    initKnob (compT, compTLbl, "THRESH");
    initKnob (compR, compRLbl, "RATIO");
    initKnob (sendEcho, sendEchoLbl, "ECHO SEND");
    initKnob (sendVerb, sendVerbLbl, "VERB SEND");
    initKnob (verbSize, verbSizeLbl, "SIZE");
    initKnob (verbMix, verbMixLbl, "MIX");

    verbSizeAtt = std::make_unique<SliderAtt> (proc.apvts, "verbSize", verbSize);
    verbMixAtt = std::make_unique<SliderAtt> (proc.apvts, "verbMix", verbMix);
    verbSize.setColour (Slider::rotarySliderFillColourId, ui::kAccent);
    verbMix.setColour (Slider::rotarySliderFillColourId, ui::kAccent);

    for (auto* b : { &solo, &mute })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }

    setVoice (0);
    showVoiceModule (0);
    showMasterModule (0);
    startTimerHz (10);
}

void FxView::initKnob (Slider& s, Label& l, const char* text)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addChildComponent (s);
    l.setText (text, dontSendNotification);
    l.setFont (ui::sans (9.0f));
    l.setColour (Label::textColourId, ui::kDim);
    l.setJustificationType (Justification::centred);
    addChildComponent (l);
}

void FxView::setVoice (int v)
{
    voice = jlimit (0, ChoraleProcessor::kNumVoices - 1, v);
    const auto id = String (voice + 1);
    const auto accent = ui::voiceInk (voice);

    voiceTitle.setText ("VOICE " + id + "  CHAIN", dontSendNotification);
    voiceTitle.setColour (Label::textColourId, accent);
    voiceEq.setTarget ("v" + id + "Eq", voice, accent);
    {
        Array<ChainStrip::Module> mods;
        mods.add ({ "EQ", "v" + id + "EqOn" });
        mods.add ({ "COMP", "v" + id + "CompOn" });
        mods.add ({ "ECHO", "" });
        mods.add ({ "VERB", "" });
        voiceChain.setModules (std::move (mods));
    }

    solo.setColour (TextButton::buttonOnColourId, ui::kText);
    mute.setColour (TextButton::buttonOnColourId, accent);

    compTAtt.reset();
    compRAtt.reset();
    sendEchoAtt.reset();
    sendVerbAtt.reset();
    soloAtt.reset();
    muteAtt.reset();
    compTAtt = std::make_unique<SliderAtt> (proc.apvts, "v" + id + "CompT", compT);
    compRAtt = std::make_unique<SliderAtt> (proc.apvts, "v" + id + "CompR", compR);
    sendEchoAtt = std::make_unique<SliderAtt> (proc.apvts, "v" + id + "SendEcho", sendEcho);
    sendVerbAtt = std::make_unique<SliderAtt> (proc.apvts, "v" + id + "SendVerb", sendVerb);
    soloAtt = std::make_unique<ButtonAtt> (proc.apvts, "v" + id + "Solo", solo);
    muteAtt = std::make_unique<ButtonAtt> (proc.apvts, "v" + id + "Mute", mute);
    for (auto* s : { &compT, &compR, &sendEcho, &sendVerb })
        s->setColour (Slider::rotarySliderFillColourId, accent);
    repaint();
}

void FxView::showVoiceModule (int m)
{
    voiceChain.setSelected (m);
    voiceEq.setVisible (m == 0);
    compT.setVisible (m == 1);
    compTLbl.setVisible (m == 1);
    compR.setVisible (m == 1);
    compRLbl.setVisible (m == 1);
    sendEcho.setVisible (m == 2);
    sendEchoLbl.setVisible (m == 2);
    sendVerb.setVisible (m == 3);
    sendVerbLbl.setVisible (m == 3);
    voiceHint.setText (m == 2 ? "Feeds the global echo (time/feedback in the bar below)"
                              : (m == 3 ? "Feeds the shared reverb (size/mix under MASTER)" : ""),
                       dontSendNotification);
    resized();
}

void FxView::showMasterModule (int m)
{
    masterChain.setSelected (m);
    masterEq.setVisible (m == 0);
    verbSize.setVisible (m == 1);
    verbSizeLbl.setVisible (m == 1);
    verbMix.setVisible (m == 1);
    verbMixLbl.setVisible (m == 1);
    masterHint.setText (m == 1 ? "Shared bus: voices feed it via VERB sends" : "",
                        dontSendNotification);
    resized();
}

void FxView::timerCallback()
{
    // Editors dim when their module is bypassed.
    const auto id = String (voice + 1);
    const bool eqOn = proc.apvts.getRawParameterValue ("v" + id + "EqOn")->load() > 0.5f;
    const bool compOn = proc.apvts.getRawParameterValue ("v" + id + "CompOn")->load() > 0.5f;
    const bool mEqOn = proc.apvts.getRawParameterValue ("mEqOn")->load() > 0.5f;
    voiceEq.setAlpha (eqOn ? 1.0f : 0.45f);
    masterEq.setAlpha (mEqOn ? 1.0f : 0.45f);
    for (auto* c : { (Component*) &compT, (Component*) &compR, (Component*) &compTLbl,
                     (Component*) &compRLbl })
        c->setAlpha (compOn ? 1.0f : 0.45f);
}

void FxView::paint (Graphics& g)
{
    g.setColour (ui::kPanel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (ui::kBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
    g.setColour (ui::kVoice[voice]);
    g.fillRoundedRectangle (Rectangle<float> (12.0f, 0.0f, (float) getWidth() * 0.62f - 24.0f, 2.0f), 1.0f);
}

void FxView::resized()
{
    auto r = getLocalBounds().reduced (12, 10);

    auto left = r.removeFromLeft ((int) ((float) getWidth() * 0.62f) - 18);
    r.removeFromLeft (12);
    auto right = r;

    // --- Voice side --------------------------------------------------------
    auto titleRow = left.removeFromTop (22);
    solo.setBounds (titleRow.removeFromRight (24));
    titleRow.removeFromRight (4);
    mute.setBounds (titleRow.removeFromRight (24));
    voiceTitle.setBounds (titleRow);
    left.removeFromTop (6);
    voiceChain.setBounds (left.removeFromTop (34));
    left.removeFromTop (6);
    voiceHint.setBounds (left.removeFromBottom (14));

    voiceEq.setBounds (left.reduced (0, 2));
    // Knob editors centre in the same area.
    {
        auto area = left;
        auto knobCell = [&] (Slider& s, Label& l, int cx)
        {
            s.setBounds (Rectangle<int> (64, 64).withCentre ({ cx, area.getCentreY() - 8 }));
            l.setBounds (Rectangle<int> (100, 14).withCentre ({ cx, area.getCentreY() + 34 }));
        };
        const int mid = area.getCentreX();
        knobCell (compT, compTLbl, mid - 70);
        knobCell (compR, compRLbl, mid + 70);
        knobCell (sendEcho, sendEchoLbl, mid);
        knobCell (sendVerb, sendVerbLbl, mid);
    }

    // --- Master side -------------------------------------------------------
    masterTitle.setBounds (right.removeFromTop (22));
    right.removeFromTop (6);
    masterChain.setBounds (right.removeFromTop (34));
    right.removeFromTop (6);
    masterHint.setBounds (right.removeFromBottom (14));

    masterEq.setBounds (right.reduced (0, 2));
    {
        auto area = right;
        const int mid = area.getCentreX();
        verbMix.setBounds (Rectangle<int> (64, 64).withCentre ({ mid - 55, area.getCentreY() - 8 }));
        verbMixLbl.setBounds (Rectangle<int> (90, 14).withCentre ({ mid - 55, area.getCentreY() + 34 }));
        verbSize.setBounds (Rectangle<int> (64, 64).withCentre ({ mid + 55, area.getCentreY() - 8 }));
        verbSizeLbl.setBounds (Rectangle<int> (90, 14).withCentre ({ mid + 55, area.getCentreY() + 34 }));
    }
}

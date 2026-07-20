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
    constexpr float arrowW = 14.0f;
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
                        Rectangle<float> (r.getRight(), r.getY(), 14.0f, r.getHeight()),
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
      canvas (p, [this] (int n)
              {
                  selectNode (n);
                  // Canvas-initiated voice clicks drive app-wide selection;
                  // fired here (not in selectNode) to avoid feedback loops.
                  if (graph::kindOf (n) == graph::NodeKind::Voice && onVoicePicked != nullptr)
                      onVoicePicked (n % 8);
              }),
      masterChain (p, [this] (int m) { showMasterModule (m); }),
      eqPanel (p, ui::voiceInk (0)),
      compPanel (p, ui::voiceInk (0)),
      satPanel (p, ui::voiceInk (0)),
      echoPanel (p, ui::voiceInk (0)),
      verbPanel (p, ui::voiceInk (0)),
      masterEq (p, ui::kText),
      masterComp (p, ui::kText),
      masterSat (p, ui::kText)
{
    nodeTitle.setFont (ui::sans (13.0f, true));
    masterTitle.setFont (ui::sans (13.0f, true));
    masterTitle.setText ("MASTER", dontSendNotification);
    masterTitle.setColour (Label::textColourId, ui::kText);
    addAndMakeVisible (nodeTitle);
    addAndMakeVisible (masterTitle);

    for (auto* h : { &nodeHint, &masterHint })
    {
        h->setFont (ui::sans (9.5f));
        h->setColour (Label::textColourId, ui::kDim.withAlpha (0.7f));
        h->setJustificationType (Justification::centred);
        addAndMakeVisible (*h);
    }

    addAndMakeVisible (canvas);
    addAndMakeVisible (masterChain);
    {
        Array<ChainStrip::Module> mods;
        mods.add ({ "EQ", "mEqOn" });
        mods.add ({ "COMP", "mCompOn" });
        mods.add ({ "SAT", "mSatOn" });
        masterChain.setModules (std::move (mods));
    }

    for (auto* c : std::initializer_list<Component*> { &eqPanel, &compPanel, &satPanel,
                                                       &echoPanel, &verbPanel, &masterEq,
                                                       &masterComp, &masterSat })
        addChildComponent (*c);
    masterEq.setTarget ("mEq", ChoraleProcessor::kNumVoices, ui::kText);
    masterComp.setTarget ("mComp", ChoraleProcessor::kNumVoices, ui::kText);
    masterSat.setTarget ("mSat", ChoraleProcessor::kNumVoices, ui::kText);

    initKnob (compT, compTLbl, "THRESH");
    initKnob (compR, compRLbl, "RATIO");
    initKnob (satDrive, satDriveLbl, "DRIVE");
    initKnob (satMix, satMixLbl, "MIX");
    initKnob (gainKnob, gainLbl, "GAIN");
    initKnob (echoTime, echoTimeLbl, "TIME");
    initKnob (echoFb, echoFbLbl, "FEEDBACK");
    initKnob (echoMix, echoMixLbl, "MIX");
    initKnob (verbSize, verbSizeLbl, "SIZE");
    initKnob (verbMix, verbMixLbl, "MIX");
    initKnob (mCompT, mCompTLbl, "THRESH");
    initKnob (mCompR, mCompRLbl, "RATIO");
    initKnob (mSatDrive, mSatDriveLbl, "DRIVE");
    initKnob (mSatMix, mSatMixLbl, "MIX");

    mCompTAtt = std::make_unique<SliderAtt> (proc.apvts, "mCompT", mCompT);
    mCompRAtt = std::make_unique<SliderAtt> (proc.apvts, "mCompR", mCompR);
    mSatDriveAtt = std::make_unique<SliderAtt> (proc.apvts, "mSatDrive", mSatDrive);
    mSatMixAtt = std::make_unique<SliderAtt> (proc.apvts, "mSatMix", mSatMix);
    for (auto* s : { &mCompT, &mCompR, &mSatDrive, &mSatMix })
        s->setColour (Slider::rotarySliderFillColourId, ui::kAccent);

    for (auto* b : { &solo, &mute })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }

    selectNode (graph::kVoice0);
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
    selectNode (graph::kVoice0 + jlimit (0, ChoraleProcessor::kNumVoices - 1, v));
    canvas.setSelected (selectedNode);
}

void FxView::selectNode (int nodeId)
{
    using namespace graph;
    selectedNode = nodeId;
    const int slot = nodeId % 8;
    const auto vId = String (slot + 1);
    const auto accent = ui::voiceInk (slot);
    const auto kind = kindOf (nodeId);

    // Visibility.
    const bool isVoice = kind == NodeKind::Voice;
    eqPanel.setVisible (kind == NodeKind::Eq);
    compPanel.setVisible (kind == NodeKind::Comp);
    compT.setVisible (kind == NodeKind::Comp);
    compTLbl.setVisible (kind == NodeKind::Comp);
    compR.setVisible (kind == NodeKind::Comp);
    compRLbl.setVisible (kind == NodeKind::Comp);
    satPanel.setVisible (kind == NodeKind::Sat);
    satDrive.setVisible (kind == NodeKind::Sat);
    satDriveLbl.setVisible (kind == NodeKind::Sat);
    satMix.setVisible (kind == NodeKind::Sat);
    satMixLbl.setVisible (kind == NodeKind::Sat);
    echoPanel.setVisible (kind == NodeKind::Echo);
    echoTime.setVisible (kind == NodeKind::Echo);
    echoTimeLbl.setVisible (kind == NodeKind::Echo);
    echoFb.setVisible (kind == NodeKind::Echo);
    echoFbLbl.setVisible (kind == NodeKind::Echo);
    echoMix.setVisible (kind == NodeKind::Echo);
    echoMixLbl.setVisible (kind == NodeKind::Echo);
    verbPanel.setVisible (kind == NodeKind::Verb);
    verbSize.setVisible (kind == NodeKind::Verb);
    verbSizeLbl.setVisible (kind == NodeKind::Verb);
    verbMix.setVisible (kind == NodeKind::Verb);
    verbMixLbl.setVisible (kind == NodeKind::Verb);
    solo.setVisible (isVoice);
    mute.setVisible (isVoice);
    gainKnob.setVisible (kind == NodeKind::Gain);
    gainLbl.setVisible (kind == NodeKind::Gain);

    // Bindings + titles.
    compTAtt.reset();
    compRAtt.reset();
    satDriveAtt.reset();
    satMixAtt.reset();
    echoTimeAtt.reset();
    echoFbAtt.reset();
    echoMixAtt.reset();
    verbSizeAtt.reset();
    verbMixAtt.reset();
    gainAtt.reset();
    soloAtt.reset();
    muteAtt.reset();

    switch (kind)
    {
        case NodeKind::Voice:
            nodeTitle.setText ("VOICE " + vId, dontSendNotification);
            soloAtt = std::make_unique<ButtonAtt> (proc.apvts, "v" + vId + "Solo", solo);
            muteAtt = std::make_unique<ButtonAtt> (proc.apvts, "v" + vId + "Mute", mute);
            nodeHint.setText ("Wire this voice through nodes into OUT; drag its out port to patch",
                              dontSendNotification);
            break;
        case NodeKind::Eq:
            nodeTitle.setText ("EQ " + vId, dontSendNotification);
            eqPanel.setTarget ("v" + vId + "Eq", slot, accent);
            nodeHint.setText ("", dontSendNotification);
            break;
        case NodeKind::Comp:
            nodeTitle.setText ("COMP " + vId, dontSendNotification);
            compPanel.setTarget ("v" + vId + "Comp", slot, accent);
            compTAtt = std::make_unique<SliderAtt> (proc.apvts, "v" + vId + "CompT", compT);
            compRAtt = std::make_unique<SliderAtt> (proc.apvts, "v" + vId + "CompR", compR);
            nodeHint.setText ("", dontSendNotification);
            break;
        case NodeKind::Sat:
            nodeTitle.setText ("SAT " + vId, dontSendNotification);
            satPanel.setTarget ("v" + vId + "Sat", slot, accent);
            satDriveAtt = std::make_unique<SliderAtt> (proc.apvts, "v" + vId + "SatDrive", satDrive);
            satMixAtt = std::make_unique<SliderAtt> (proc.apvts, "v" + vId + "SatMix", satMix);
            nodeHint.setText ("", dontSendNotification);
            break;
        case NodeKind::Gain:
        {
            const auto g = String (nodeId - kGain0 + 1);
            nodeTitle.setText ("GAIN " + g, dontSendNotification);
            gainAtt = std::make_unique<SliderAtt> (proc.apvts, "gain" + g + "Level", gainKnob);
            nodeHint.setText ("Leveled summer: wire several nodes in, one out", dontSendNotification);
            break;
        }
        case NodeKind::Echo:
        {
            const auto e = String (nodeId - kEcho0 + 1);
            nodeTitle.setText ("ECHO " + e, dontSendNotification);
            echoTimeAtt = std::make_unique<SliderAtt> (proc.apvts, "echo" + e + "Time", echoTime);
            echoFbAtt = std::make_unique<SliderAtt> (proc.apvts, "echo" + e + "Fb", echoFb);
            echoMixAtt = std::make_unique<SliderAtt> (proc.apvts, "echo" + e + "Mix", echoMix);
            echoPanel.setTarget ("echo" + e, ui::kAccent);
            nodeHint.setText ("Ping-pong delay: dry passes at unity, MIX sets the tail",
                              dontSendNotification);
            break;
        }
        case NodeKind::Verb:
        {
            const auto e = String (nodeId - kVerb0 + 1);
            nodeTitle.setText ("REVERB " + e, dontSendNotification);
            verbSizeAtt = std::make_unique<SliderAtt> (proc.apvts, "verb" + e + "Size", verbSize);
            verbMixAtt = std::make_unique<SliderAtt> (proc.apvts, "verb" + e + "Mix", verbMix);
            verbPanel.setTarget ("verb" + e, ui::kAccent);
            nodeHint.setText ("Reverb: dry passes at unity, MIX sets the tail",
                              dontSendNotification);
            break;
        }
        case NodeKind::Out:
            nodeTitle.setText ("OUT", dontSendNotification);
            nodeHint.setText ("Wet bus: tone / width in the bar, then mixed with the lead",
                              dontSendNotification);
            break;
    }
    nodeTitle.setColour (Label::textColourId,
                         kind == NodeKind::Voice || kind == NodeKind::Eq
                                 || kind == NodeKind::Comp || kind == NodeKind::Sat
                             ? accent
                             : ui::kText);
    for (auto* s : { &compT, &compR, &satDrive, &satMix })
        s->setColour (Slider::rotarySliderFillColourId, accent);
    for (auto* s : { &echoTime, &echoFb, &echoMix, &verbSize, &verbMix, &gainKnob })
        s->setColour (Slider::rotarySliderFillColourId, ui::kAccent);
    solo.setColour (TextButton::buttonOnColourId, ui::kText);
    mute.setColour (TextButton::buttonOnColourId, accent);
    canvas.setSelected (nodeId);
    resized();
    repaint();
}

void FxView::showMasterModule (int m)
{
    masterChain.setSelected (m);
    masterEq.setVisible (m == 0);
    masterComp.setVisible (m == 1);
    mCompT.setVisible (m == 1);
    mCompTLbl.setVisible (m == 1);
    mCompR.setVisible (m == 1);
    mCompRLbl.setVisible (m == 1);
    masterSat.setVisible (m == 2);
    mSatDrive.setVisible (m == 2);
    mSatDriveLbl.setVisible (m == 2);
    mSatMix.setVisible (m == 2);
    mSatMixLbl.setVisible (m == 2);
    masterHint.setText ("", dontSendNotification);
    resized();
}

void FxView::timerCallback()
{
    // Dim editors whose module is bypassed.
    const auto vId = String (selectedNode % 8 + 1);
    auto dim = [this] (const String& onParam, std::initializer_list<Component*> comps)
    {
        if (auto* raw = proc.apvts.getRawParameterValue (onParam))
        {
            const bool on = raw->load() > 0.5f;
            for (auto* c : comps)
                c->setAlpha (on ? 1.0f : 0.45f);
        }
    };
    dim ("v" + vId + "EqOn", { &eqPanel });
    dim ("v" + vId + "CompOn", { &compPanel, &compT, &compR, &compTLbl, &compRLbl });
    dim ("v" + vId + "SatOn", { &satPanel, &satDrive, &satMix, &satDriveLbl, &satMixLbl });
    dim ("mEqOn", { &masterEq });
    dim ("mCompOn", { &masterComp, &mCompT, &mCompR, &mCompTLbl, &mCompRLbl });
    dim ("mSatOn", { &masterSat, &mSatDrive, &mSatMix, &mSatDriveLbl, &mSatMixLbl });
}

void FxView::paint (Graphics& g)
{
    g.setColour (ui::kPanel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (ui::kBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
}

void FxView::resized()
{
    auto r = getLocalBounds().reduced (12, 8);

    canvas.setBounds (r.removeFromTop ((int) ((float) getHeight() * 0.52f)));
    r.removeFromTop (6);

    auto left = r.removeFromLeft ((int) ((float) getWidth() * 0.62f) - 18);
    r.removeFromLeft (12);
    auto right = r;

    // --- Selected node editor ---------------------------------------------
    auto titleRow = left.removeFromTop (20);
    solo.setBounds (titleRow.removeFromRight (24));
    titleRow.removeFromRight (4);
    mute.setBounds (titleRow.removeFromRight (24));
    nodeTitle.setBounds (titleRow);
    left.removeFromTop (4);
    nodeHint.setBounds (left.removeFromBottom (13));

    const auto area = left.reduced (0, 1);
    auto panelWithKnobs = [] (Rectangle<int> a, Component& panel,
                              Slider& k1, Label& l1, Slider& k2, Label& l2)
    {
        auto knobCol = a.removeFromRight (92);
        panel.setBounds (a);
        k1.setBounds (Rectangle<int> (48, 48).withCentre ({ knobCol.getCentreX(), knobCol.getCentreY() - 34 }));
        l1.setBounds (Rectangle<int> (88, 12).withCentre ({ knobCol.getCentreX(), knobCol.getCentreY() - 6 }));
        k2.setBounds (Rectangle<int> (48, 48).withCentre ({ knobCol.getCentreX(), knobCol.getCentreY() + 28 }));
        l2.setBounds (Rectangle<int> (88, 12).withCentre ({ knobCol.getCentreX(), knobCol.getCentreY() + 56 }));
    };

    eqPanel.setBounds (area);
    panelWithKnobs (area, compPanel, compT, compTLbl, compR, compRLbl);
    panelWithKnobs (area, satPanel, satDrive, satDriveLbl, satMix, satMixLbl);
    // ECHO node: taps sketch + TIME/FEEDBACK column + MIX column.
    {
        auto a = area;
        auto mixCol = a.removeFromRight (76);
        echoMix.setBounds (Rectangle<int> (48, 48).withCentre ({ mixCol.getCentreX(), mixCol.getCentreY() - 12 }));
        echoMixLbl.setBounds (Rectangle<int> (62, 12).withCentre ({ mixCol.getCentreX(), mixCol.getCentreY() + 18 }));
        panelWithKnobs (a, echoPanel, echoTime, echoTimeLbl, echoFb, echoFbLbl);
    }
    panelWithKnobs (area, verbPanel, verbSize, verbSizeLbl, verbMix, verbMixLbl);
    gainKnob.setBounds (Rectangle<int> (60, 60).withCentre ({ area.getCentreX(), area.getCentreY() - 8 }));
    gainLbl.setBounds (Rectangle<int> (90, 13).withCentre ({ area.getCentreX(), area.getCentreY() + 30 }));

    // --- Master side -------------------------------------------------------
    masterTitle.setBounds (right.removeFromTop (20));
    right.removeFromTop (4);
    masterChain.setBounds (right.removeFromTop (28));
    right.removeFromTop (4);
    masterHint.setBounds (right.removeFromBottom (13));

    const auto mArea = right.reduced (0, 1);
    auto panelWithKnobsM = [] (Rectangle<int> a, Component& panel,
                               Slider& k1, Label& l1, Slider& k2, Label& l2)
    {
        auto knobCol = a.removeFromRight (78);
        panel.setBounds (a);
        k1.setBounds (Rectangle<int> (44, 44).withCentre ({ knobCol.getCentreX(), knobCol.getCentreY() - 32 }));
        l1.setBounds (Rectangle<int> (76, 12).withCentre ({ knobCol.getCentreX(), knobCol.getCentreY() - 4 }));
        k2.setBounds (Rectangle<int> (44, 44).withCentre ({ knobCol.getCentreX(), knobCol.getCentreY() + 26 }));
        l2.setBounds (Rectangle<int> (76, 12).withCentre ({ knobCol.getCentreX(), knobCol.getCentreY() + 54 }));
    };
    masterEq.setBounds (mArea);
    panelWithKnobsM (mArea, masterComp, mCompT, mCompTLbl, mCompR, mCompRLbl);
    panelWithKnobsM (mArea, masterSat, mSatDrive, mSatDriveLbl, mSatMix, mSatMixLbl);
}

#include "VoiceDetailPanel.h"

using namespace juce;

VoiceDetailPanel::VoiceDetailPanel (ChoraleProcessor& p) : processor (p)
{
    title.setFont (ui::sans (13.0f, true));
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
        l->setFont (ui::sans (9.0f));
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
    title.setColour (Label::textColourId, ui::voiceInk (voice));
    for (auto* s : { &detune, &pan })
        s->setColour (Slider::rotarySliderFillColourId, accent);
    solo.setColour (TextButton::buttonOnColourId, ui::kText);
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
    g.setColour (ui::kVoice[voice]);
    g.fillRoundedRectangle (r.removeFromTop (2.0f).reduced (12.0f, 0.0f), 1.0f);
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

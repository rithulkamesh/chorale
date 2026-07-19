#include "MixerView.h"

using namespace juce;

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
        ch.title.setJustificationType (Justification::centredLeft);
        ch.title.setFont (ui::sans (10.0f, true));
        ch.title.setColour (Label::textColourId, ui::voiceInk (v));
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
            l->setFont (ui::sans (8.5f));
            l->setColour (Label::textColourId, ui::kDim);
            l->setJustificationType (Justification::centred);
            addAndMakeVisible (*l);
        }

        ch.fader.bind (p, "v" + id + "Gain");
        addAndMakeVisible (ch.fader);

        for (auto* b : { &ch.solo, &ch.mute })
        {
            b->setClickingTogglesState (true);
            addAndMakeVisible (*b);
        }
        ch.solo.setColour (TextButton::buttonOnColourId, ui::kText);
        ch.mute.setColour (TextButton::buttonOnColourId, ui::voiceInk (v));
        ch.soloAtt = std::make_unique<Channel::ButtonAtt> (p.apvts, "v" + id + "Solo", ch.solo);
        ch.muteAtt = std::make_unique<Channel::ButtonAtt> (p.apvts, "v" + id + "Mute", ch.mute);

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
    g.setColour (ui::kBg.darker (0.2f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    const int colW = getWidth() / ChoraleProcessor::kNumVoices;
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        auto col = getLocalBounds().withWidth (colW).translated (v * colW, 0).toFloat().reduced (2.0f, 8.0f);
        g.setColour (ui::kVoice[v].withAlpha (v == selected ? 1.0f : 0.55f));
        g.fillRoundedRectangle (col.getX() + 8.0f, col.getY(), col.getWidth() - 16.0f, 2.0f, 1.0f);
    }

    g.setColour (ui::kPanelHi);
    for (int v = 1; v < ChoraleProcessor::kNumVoices; ++v)
        g.drawVerticalLine (v * colW, 10.0f, (float) getHeight() - 10.0f);

    g.setColour (ui::kBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
}

void MixerView::resized()
{
    const int colW = getWidth() / ChoraleProcessor::kNumVoices;
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        auto col = getLocalBounds().withWidth (colW).translated (v * colW, 0).reduced (5, 10);
        col.removeFromTop (4);
        auto titleRow = col.removeFromTop (16);
        channels[v].solo.setBounds (titleRow.removeFromRight (18));
        titleRow.removeFromRight (3);
        channels[v].mute.setBounds (titleRow.removeFromRight (18));
        channels[v].title.setBounds (titleRow);
        col.removeFromTop (3);
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

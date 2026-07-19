#include "GainFader.h"

using namespace juce;

GainFader::GainFader()
{
    slider.setSliderStyle (Slider::LinearVertical);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRange (ui::kMinGainDb, ui::kMaxGainDb, 0.1);
    slider.setValue (0.0);
    addAndMakeVisible (slider);

    valueLbl.setJustificationType (Justification::centred);
    valueLbl.setFont (ui::mono (9.0f));
    valueLbl.setColour (Label::textColourId, ui::kDim);
    addAndMakeVisible (valueLbl);

    slider.onValueChange = [this]
    {
        pushToParam();
        updateLabel();
    };
    slider.onDragStart = [this]
    {
        if (processor != nullptr && paramId.isNotEmpty())
            if (auto* p = processor->apvts.getParameter (paramId))
                p->beginChangeGesture();
    };
    slider.onDragEnd = [this]
    {
        if (processor != nullptr && paramId.isNotEmpty())
            if (auto* p = processor->apvts.getParameter (paramId))
                p->endChangeGesture();
    };

    startTimerHz (30);
}

void GainFader::bind (ChoraleProcessor& p, const String& id)
{
    processor = &p;
    paramId = id;
    syncFromParam();
}

void GainFader::setMeterLevel (float level)
{
    meterLevel = level;
    const float prev = smoothedMeter;
    smoothedMeter += 0.35f * (level - smoothedMeter);
    if (std::abs (smoothedMeter - prev) > 0.002f)
        repaint();
}

void GainFader::syncFromParam()
{
    if (processor == nullptr || paramId.isEmpty())
        return;
    const float gain = processor->apvts.getRawParameterValue (paramId)->load();
    slider.setValue (ui::gainToDb (gain), dontSendNotification);
    updateLabel();
}

void GainFader::pushToParam()
{
    if (processor == nullptr || paramId.isEmpty())
        return;
    const float db = (float) slider.getValue();
    const float gain = jmin (1.0f, ui::dbToGain (db));
    if (auto* p = processor->apvts.getParameter (paramId))
    {
        const float norm = p->convertTo0to1 (gain);
        if (std::abs (p->getValue() - norm) > 0.0005f)
            p->setValueNotifyingHost (norm);
    }
}

void GainFader::updateLabel()
{
    valueLbl.setText (ui::gainDbString ((float) slider.getValue()), dontSendNotification);
}

void GainFader::timerCallback()
{
    if (processor == nullptr || paramId.isEmpty())
        return;
    const float gain = processor->apvts.getRawParameterValue (paramId)->load();
    const float db = ui::gainToDb (gain);
    if (std::abs ((float) slider.getValue() - db) > 0.15f)
        slider.setValue (db, dontSendNotification);
    updateLabel();
}

// Meter per spec: 1px dotted lane above the level, solid thin line below.
void GainFader::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    area.removeFromBottom (14.0f);
    auto meter = area.removeFromLeft (11.0f).reduced (0.0f, 4.0f);
    const float mx = meter.getCentreX();

    const float meterNorm = jlimit (0.0f, 1.0f,
                                    (ui::gainToDb (smoothedMeter) - ui::kMinGainDb)
                                        / (ui::kMaxGainDb - ui::kMinGainDb));
    const float levelY = meter.getBottom() - meterNorm * meter.getHeight();

    // Dotted lane above the current level.
    g.setColour (ui::kTrack);
    for (float yy = meter.getY(); yy < levelY; yy += 4.0f)
        g.fillRect (Rectangle<float> (mx - 0.5f, yy, 1.0f, 2.0f));

    // Tick marks.
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f })
    {
        const float t = (db - ui::kMinGainDb) / (ui::kMaxGainDb - ui::kMinGainDb);
        const float ty = meter.getBottom() - t * meter.getHeight();
        g.fillRect (Rectangle<float> (mx - 3.0f, ty - 0.5f, 6.0f, 1.0f));
    }

    // Solid line = level.
    if (meterNorm > 0.01f)
    {
        g.setColour (ui::kText.withAlpha (0.9f));
        g.fillRoundedRectangle (Rectangle<float> (mx - 1.0f, levelY, 2.0f,
                                                  meter.getBottom() - levelY),
                                1.0f);
    }
}

void GainFader::resized()
{
    auto r = getLocalBounds();
    valueLbl.setBounds (r.removeFromBottom (14));
    slider.setBounds (r);
}

void GainFader::mouseDown (const MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return;
    if (processor == nullptr || paramId.isEmpty())
        return;

    auto* dialog = new AlertWindow ("Gain", "Enter level in dB (-inf to +6)", AlertWindow::QuestionIcon);
    dialog->addTextEditor ("db", ui::gainDbString ((float) slider.getValue()), "dB");
    dialog->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    dialog->enterModalState (true, ModalCallbackFunction::create (
                                       [this, dialog] (int result)
                                       {
                                           if (result == 1)
                                           {
                                               auto text = dialog->getTextEditorContents ("db").trim();
                                               float db = ui::kMinGainDb;
                                               if (! (text.equalsIgnoreCase ("-inf")
                                                      || text.equalsIgnoreCase ("mute")))
                                                   db = jlimit (ui::kMinGainDb, ui::kMaxGainDb,
                                                                (float) text.getDoubleValue());
                                               slider.setValue (db, sendNotificationSync);
                                           }
                                           delete dialog;
                                       }));
}

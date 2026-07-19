#include "PluginEditor.h"
#include "Assets.h"
#include "Presets.h"
#include "UpdateCheck.h"

using namespace juce;

ChoraleEditor::ChoraleEditor (ChoraleProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      chips (p, [this] (int v) { selectVoice (v); }),
      stage (p, [this] (int v) { selectVoice (v); }),
      mixer (p, [this] (int v) { selectVoice (v); }),
      fx (p),
      detail (p)
{
    update::checkOnce();
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);
    addAndMakeVisible (content);

    logo = Drawable::createFromImageData (assets::kLogoSvg, strlen (assets::kLogoSvg));
    wordmark = Drawable::createFromImageData (assets::kWordmarkSvg, strlen (assets::kWordmarkSvg));

    presetBtn.setComponentID ("presetBtn"); // combo-style skin, see LookAndFeel
    presetBtn.onClick = [this] { showPresetMenu(); };
    content.addAndMakeVisible (presetBtn);

    auto initHeaderCombo = [&] (ComboBox& c, const char* paramId, auto& att)
    {
        if (auto* par = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
            c.addItemList (par->choices, 1);
        content.addAndMakeVisible (c);
        att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvts, paramId, c);
    };
    initHeaderCombo (keyRoot, "keyRoot", keyAtt);
    initHeaderCombo (scale, "scale", scaleAtt);
    initHeaderCombo (correct, "correct", correctAtt);
    initHeaderCombo (latMode, "latMode", latAtt);

    for (auto& [l, text] : std::initializer_list<std::pair<Label*, const char*>> {
             { &keyLbl, "KEY" }, { &scaleLbl, "SCALE" }, { &correctLbl, "CORRECT" }, { &mixLbl, "MIX" } })
    {
        l->setText (text, dontSendNotification);
        l->setFont (ui::sans (9.0f));
        l->setColour (Label::textColourId, ui::kDim);
        l->setJustificationType (Justification::centred);
        content.addAndMakeVisible (*l);
    }

    auto initKnob = [&] (Slider& s, const char* paramId, auto& att, Colour accent)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setColour (Slider::rotarySliderFillColourId, accent);
        content.addAndMakeVisible (s);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, paramId, s);
    };
    initKnob (mix, "dryWet", mixAtt, ui::kText);
    initKnob (humanize, "humanize", humanizeAtt, ui::kAccent);
    initKnob (tone, "tone", toneAtt, ui::kAccent);
    initKnob (width, "width", widthAtt, ui::kAccent);
    initKnob (echoTime, "echoTime", echoTimeAtt, ui::kAccent);
    initKnob (echoFb, "echoFb", echoFbAtt, ui::kAccent);
    initKnob (echoMix, "echoMix", echoMixAtt, ui::kAccent);

    const char* fxNames[6] = { "HUMANIZE", "TONE", "WIDTH", "ECHO", "FEEDBACK", "ECHO MIX" };
    for (int i = 0; i < 6; ++i)
    {
        fxLbls[i].setText (fxNames[i], dontSendNotification);
        fxLbls[i].setFont (ui::sans (9.0f));
        fxLbls[i].setColour (Label::textColourId, ui::kDim);
        fxLbls[i].setJustificationType (Justification::centred);
        content.addAndMakeVisible (fxLbls[i]);
    }

    autoKeyLbl.setFont (ui::sans (24.0f, true));
    autoKeyLbl.setColour (Label::textColourId, ui::kText);
    autoKeyLbl.setJustificationType (Justification::centred);
    autoKeyLbl.setInterceptsMouseClicks (false, false);
    content.addChildComponent (autoKeyLbl);

    pitchLbl.setFont (ui::mono (11.0f));
    pitchLbl.setColour (Label::textColourId, ui::kDim);
    content.addAndMakeVisible (pitchLbl);

    latencyLbl.setFont (ui::mono (10.0f));
    latencyLbl.setColour (Label::textColourId, Colour (0xff55595f));
    latencyLbl.setJustificationType (Justification::centredRight);
    content.addAndMakeVisible (latencyLbl);

    content.addAndMakeVisible (chips);
    content.addAndMakeVisible (stage);
    content.addAndMakeVisible (mixer);
    content.addAndMakeVisible (fx);
    content.addAndMakeVisible (detail);

    updateBtn.setFont (ui::sans (10.5f, true), false, Justification::centredRight);
    updateBtn.setColour (HyperlinkButton::textColourId, ui::kText);
    content.addChildComponent (updateBtn);

    for (auto* b : { &stageBtn, &mixerBtn, &fxBtn })
    {
        b->setClickingTogglesState (true);
        b->setColour (TextButton::buttonOnColourId, ui::kText);
        b->setColour (TextButton::textColourOnId, ui::kBg);
        b->setColour (TextButton::textColourOffId, ui::kDim);
        content.addAndMakeVisible (*b);
    }
    stageBtn.onClick = [this] { setMainView (0); };
    mixerBtn.onClick = [this] { setMainView (1); };
    fxBtn.onClick = [this] { setMainView (2); };

    undoBtn.setButtonText (String::charToString (0x21b6));
    redoBtn.setButtonText (String::charToString (0x21b7));
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    for (auto* b : { &undoBtn, &redoBtn })
        content.addAndMakeVisible (*b);

    // A/B: clicking the inactive slot swaps the whole parameter state.
    for (auto* b : { &aBtn, &bBtn })
    {
        b->setColour (TextButton::buttonOnColourId, ui::kText);
        b->setColour (TextButton::textColourOnId, ui::kBg);
        content.addAndMakeVisible (*b);
    }
    aBtn.onClick = [this] { if (proc.abActive.load() != 0) proc.toggleAB(); };
    bBtn.onClick = [this] { if (proc.abActive.load() != 1) proc.toggleAB(); };

    chips.setSelected (1);
    stage.setSelected (0);
    mixer.setSelected (0);
    setMainView (0);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) kBaseW / kBaseH);
        c->setSizeLimits (kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
    }
    const float s = jlimit (0.5f, 2.0f, proc.uiScale.load());
    setSize ((int) (kBaseW * s), (int) (kBaseH * s));
    startTimerHz (15);
}

ChoraleEditor::~ChoraleEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
File ChoraleEditor::userPresetDir()
{
    auto dir = File::getSpecialLocation (File::userApplicationDataDirectory);
#if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
#endif
    return dir.getChildFile ("Chorale").getChildFile ("Presets");
}

void ChoraleEditor::showPresetMenu()
{
    userFiles = userPresetDir().findChildFiles (File::findFiles, false, "*.xml");
    std::sort (userFiles.begin(), userFiles.end(),
               [] (const File& a, const File& b)
               { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });

    // Hand-balanced columns so a category never orphans at a column bottom.
    PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    const char* lastCat = "";
    for (int i = 0; i < presets::kNumPresets; ++i)
    {
        const String cat (presets::kPresets[i].category);
        if (cat != lastCat)
        {
            if (cat == "Choirs" || cat == "Pedals")
                menu.addColumnBreak();
            lastCat = presets::kPresets[i].category;
            menu.addSectionHeader (cat);
        }
        menu.addItem (i + 1, presets::kPresets[i].name);
    }

    if (! userFiles.isEmpty())
    {
        menu.addSectionHeader ("User");
        for (int i = 0; i < userFiles.size(); ++i)
            menu.addItem (1001 + i, userFiles[i].getFileNameWithoutExtension());
    }
    menu.addSeparator();
    menu.addItem (9001, "Save preset...");
    if (currentUserFile.existsAsFile())
        menu.addItem (9002, "Delete '" + currentUserFile.getFileNameWithoutExtension() + "'");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (presetBtn),
                        [this] (int id)
                        {
                            if (id <= 0)
                                return;
                            if (id <= presets::kNumPresets)
                            {
                                currentUserFile = File();
                                applyPreset (id - 1);
                                presetBtn.setButtonText (presets::kPresets[id - 1].name);
                            }
                            else if (id >= 1001 && id < 9000)
                            {
                                const int idx = id - 1001;
                                if (idx >= 0 && idx < userFiles.size())
                                    loadUserPreset (userFiles[idx]);
                            }
                            else if (id == 9001)
                                promptSaveUserPreset();
                            else if (id == 9002)
                                confirmDeleteUserPreset();
                        });
}

void ChoraleEditor::promptSaveUserPreset()
{
    auto* dialog = new AlertWindow ("Save preset", "Preset name:", AlertWindow::NoIcon);
    dialog->addTextEditor ("name", currentUserFile.existsAsFile()
                                       ? currentUserFile.getFileNameWithoutExtension()
                                       : String(),
                           "Name");
    dialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    dialog->enterModalState (true, ModalCallbackFunction::create (
                                       [this, dialog] (int result)
                                       {
                                           if (result == 1)
                                           {
                                               const auto name = File::createLegalFileName (
                                                   dialog->getTextEditorContents ("name").trim());
                                               if (name.isNotEmpty())
                                               {
                                                   auto dir = userPresetDir();
                                                   dir.createDirectory();
                                                   auto file = dir.getChildFile (name + ".xml");
                                                   if (auto xml = proc.apvts.copyState().createXml())
                                                       xml->writeTo (file);
                                                   currentUserFile = file;
                                                   presetBtn.setButtonText (name);
                                               }
                                           }
                                           delete dialog;
                                       }));
}

void ChoraleEditor::confirmDeleteUserPreset()
{
    if (! currentUserFile.existsAsFile())
        return;
    auto* dialog = new AlertWindow ("Delete preset",
                                    "Delete '" + currentUserFile.getFileNameWithoutExtension() + "'?",
                                    AlertWindow::WarningIcon);
    dialog->addButton ("Delete", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    dialog->enterModalState (true, ModalCallbackFunction::create (
                                       [this, dialog] (int result)
                                       {
                                           if (result == 1)
                                           {
                                               currentUserFile.deleteFile();
                                               currentUserFile = File();
                                               presetBtn.setButtonText ("Presets...");
                                           }
                                           delete dialog;
                                       }));
}

void ChoraleEditor::loadUserPreset (const File& f)
{
    if (auto xml = XmlDocument::parse (f))
    {
        proc.apvts.replaceState (ValueTree::fromXml (*xml));
        currentUserFile = f;
        presetBtn.setButtonText (f.getFileNameWithoutExtension());
        detail.setVoice (0);
    }
}

//==============================================================================
void ChoraleEditor::selectVoice (int v)
{
    if (v < 0)
        return;
    detail.setVoice (v);
    stage.setSelected (v);
    mixer.setSelected (v);
    fx.setVoice (v);
    chips.setSelected (v + 1);
}

void ChoraleEditor::setMainView (int mode)
{
    viewMode = mode;
    stage.setVisible (mode == 0);
    mixer.setVisible (mode == 1);
    fx.setVisible (mode == 2);
    detail.setVisible (mode == 0);
    chips.setVisible (mode != 1);
    stageBtn.setToggleState (mode == 0, dontSendNotification);
    mixerBtn.setToggleState (mode == 1, dontSendNotification);
    fxBtn.setToggleState (mode == 2, dontSendNotification);
    layoutContent();
}

void ChoraleEditor::applyPreset (int i)
{
    if (i < 0 || i >= presets::kNumPresets)
        return;
    const auto& pre = presets::kPresets[i];

    const bool autoScale = (int) proc.apvts.getRawParameterValue ("scale")->load() == 0;
    const int rootPc = autoScale ? proc.uiRoot.load()
                                 : (int) proc.apvts.getRawParameterValue ("keyRoot")->load();
    const int scaleIdx = (int) proc.apvts.getRawParameterValue ("scale")->load();
    const bool minorish = autoScale ? proc.uiMinor.load()
                                    : (scaleIdx == 2 || scaleIdx == 3 || scaleIdx == 4 || scaleIdx == 7);

    auto set = [&] (const String& id, float value)
    {
        if (auto* par = proc.apvts.getParameter (id))
        {
            par->beginChangeGesture();
            par->setValueNotifyingHost (par->convertTo0to1 (value));
            par->endChangeGesture();
        }
    };

    // Deliberately does NOT touch dryWet: mix is a session setting, not part
    // of a harmony shape (stomping it read as a "mix reset" bug).
    proc.undoManager.beginNewTransaction ("Apply preset");
    for (int v = 0; v < ChoraleProcessor::kNumVoices; ++v)
    {
        const auto& vs = pre.v[v];
        const auto id = String (v + 1);
        set ("v" + id + "Mode", (float) vs.mode);
        set ("v" + id + "Degree", (float) vs.degree);
        set ("v" + id + "Note", (float) (presets::resolveNote (vs.note, rootPc, minorish) - 36));
        set ("v" + id + "Gain", vs.gain);
        set ("v" + id + "Pan", vs.pan);
        set ("v" + id + "Detune", vs.detune);
        set ("v" + id + "Solo", 0.0f);
        set ("v" + id + "Mute", 0.0f);
    }
    detail.setVoice (0);
}

void ChoraleEditor::timerCallback()
{
    static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const float f0 = proc.uiF0.load();
    if (f0 > 0.0f)
    {
        const double midi = 69.0 + 12.0 * std::log2 (f0 / 440.0);
        const int n = (int) std::lround (midi);
        const int cents = (int) std::lround ((midi - n) * 100.0);
        pitchLbl.setText (String (pcs[((n % 12) + 12) % 12]) + String (n / 12 - 1) + "  "
                              + (cents >= 0 ? "+" : "") + String (cents) + "c",
                          dontSendNotification);
    }
    else
        pitchLbl.setText ("-", dontSendNotification);

    const bool autoScale = (int) proc.apvts.getRawParameterValue ("scale")->load() == 0;
    if (autoScale)
    {
        autoKeyLbl.setText (String (pcs[proc.uiRoot.load() % 12])
                                + (proc.uiMinor.load() ? " minor" : " major"),
                            dontSendNotification);
    }
    else
    {
        const int rootPc = (int) proc.apvts.getRawParameterValue ("keyRoot")->load();
        const int scaleIdx = (int) proc.apvts.getRawParameterValue ("scale")->load();
        String scaleName = "Major";
        if (auto* par = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter ("scale")))
            scaleName = par->choices[scaleIdx];
        autoKeyLbl.setText (String (pcs[rootPc % 12]) + "  " + scaleName, dontSendNotification);
    }

    if (const double srr = proc.getSampleRate(); srr > 0)
        latencyLbl.setText (String ((int) std::lround (proc.getLatencySamples() / srr * 1000.0))
                                + " ms lookahead",
                            dontSendNotification);

    if (update::state().available.load() && ! updateBtn.isVisible())
    {
        String tag;
        {
            const SpinLock::ScopedLockType sl (update::state().lock);
            tag = update::state().tag;
        }
        updateBtn.setButtonText (tag + " available");
        updateBtn.setURL (URL ("https://github.com/rithulkamesh/chorale/releases/latest"));
        updateBtn.setVisible (true);
    }

    undoBtn.setEnabled (proc.undoManager.canUndo());
    redoBtn.setEnabled (proc.undoManager.canRedo());
    aBtn.setToggleState (proc.abActive.load() == 0, dontSendNotification);
    bBtn.setToggleState (proc.abActive.load() == 1, dontSendNotification);

    // Group parameter edits into ~1 s undo steps.
    if (++undoTick >= 15)
    {
        undoTick = 0;
        proc.undoManager.beginNewTransaction();
    }

    detail.refresh();
    detail.tick();
    chips.repaint();
}

bool ChoraleEditor::keyPressed (const KeyPress& k)
{
    if (k == KeyPress ('z', ModifierKeys::commandModifier, 0))
    {
        proc.undoManager.undo();
        return true;
    }
    if (k == KeyPress ('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0))
    {
        proc.undoManager.redo();
        return true;
    }
    return false;
}

//==============================================================================
void ChoraleEditor::paint (Graphics& g)
{
    g.fillAll (ui::kBg);
}

void ChoraleEditor::paintContent (Graphics& g)
{
    g.fillAll (ui::kBg);
    g.setColour (ui::kPanel);
    g.fillRect (0, 0, kBaseW, 56);
    g.setColour (ui::kBorder);
    g.drawHorizontalLine (56, 0.0f, (float) kBaseW);

    if (logo != nullptr)
        logo->drawWithin (g, Rectangle<float> (12.0f, 11.0f, 34.0f, 34.0f),
                          RectanglePlacement::centred, 1.0f);
    if (wordmark != nullptr)
    {
        // Drawn twice with a half-pixel offset: the letterforms are hairline
        // at this size and read too thin single-pass.
        const auto box = Rectangle<float> (54.0f, 19.5f, 142.0f, 17.0f);
        const auto placement = RectanglePlacement::xLeft | RectanglePlacement::yMid
                               | RectanglePlacement::onlyReduceInSize;
        wordmark->drawWithin (g, box, placement, 1.0f);
        wordmark->drawWithin (g, box.translated (0.5f, 0.0f), placement, 1.0f);
    }
}

void ChoraleEditor::paintContentOver (Graphics& g)
{
    if (autoKeyLbl.getText().isEmpty())
        return;

    g.setFont (autoKeyLbl.getFont());
    g.setColour (autoKeyLbl.findColour (Label::textColourId));
    g.drawText (autoKeyLbl.getText(), autoKeyLbl.getBounds(), Justification::centred);
}

void ChoraleEditor::layoutHeader()
{
    const int w = kBaseW;
    const int pad = 14;

    presetBtn.setBounds (206, 15, 152, 26);
    stageBtn.setBounds (366, 16, 50, 24);
    mixerBtn.setBounds (420, 16, 50, 24);
    fxBtn.setBounds (474, 16, 34, 24);

    autoKeyLbl.setBounds (w / 2 - 160, 12, 320, 32);
    autoKeyLbl.setVisible (true);

    const int rightW = 348;
    const int rx = w - pad - rightW;
    keyLbl.setBounds (rx, 6, 62, 14);
    keyRoot.setBounds (rx, 20, 62, 25);
    scaleLbl.setBounds (rx + 68, 6, 104, 14);
    scale.setBounds (rx + 68, 20, 104, 25);
    correctLbl.setBounds (rx + 178, 6, 88, 14);
    correct.setBounds (rx + 178, 20, 88, 25);
    const int mixSz = 34;
    const int mixX = w - pad - mixSz;
    mixLbl.setBounds (mixX - 2, 5, mixSz + 4, 12);
    mix.setBounds (mixX, 18, mixSz, mixSz);
}

void ChoraleEditor::layoutContent()
{
    layoutHeader();

    auto r = Rectangle<int> (0, 0, kBaseW, kBaseH);
    r.removeFromTop (56);

    if (viewMode != 1)
    {
        auto chipRow = r.removeFromTop (38);
        chips.setBounds (chipRow.reduced (14, 0));
    }
    else
        r.removeFromTop (4);

    r.removeFromBottom (10); // breathing room below the footer
    auto footer = r.removeFromBottom (26).reduced (14, 2);
    pitchLbl.setBounds (footer.removeFromLeft (120));
    footer.removeFromLeft (10);
    undoBtn.setBounds (footer.removeFromLeft (26));
    footer.removeFromLeft (4);
    redoBtn.setBounds (footer.removeFromLeft (26));
    footer.removeFromLeft (12);
    aBtn.setBounds (footer.removeFromLeft (24));
    footer.removeFromLeft (4);
    bBtn.setBounds (footer.removeFromLeft (24));
    latencyLbl.setBounds (footer.removeFromRight (120));
    footer.removeFromRight (6);
    latMode.setBounds (footer.removeFromRight (84).withSizeKeepingCentre (84, 22));
    footer.removeFromRight (10);
    updateBtn.setBounds (footer.removeFromRight (130));

    // Global wet-bus knob bar.
    auto fxBar = r.removeFromBottom (78).reduced (14, 4);
    Slider* knobs[6] = { &humanize, &tone, &width, &echoTime, &echoFb, &echoMix };
    const int kw = jmin (110, fxBar.getWidth() / 6);
    const int pad = (fxBar.getWidth() - kw * 6) / 2;
    fxBar.removeFromLeft (pad);
    for (int i = 0; i < 6; ++i)
    {
        auto cell = fxBar.removeFromLeft (kw);
        knobs[i]->setBounds (cell.removeFromTop (54).withSizeKeepingCentre (54, 54));
        fxLbls[i].setBounds (cell);
    }

    r.reduce (14, 6);
    if (viewMode == 0)
    {
        detail.setBounds (r.removeFromLeft (220));
        r.removeFromLeft (8);
    }
    stage.setBounds (r);
    mixer.setBounds (r);
    fx.setBounds (r);
}

void ChoraleEditor::resized()
{
    const float s = jmax (0.25f, (float) getWidth() / (float) kBaseW);
    content.setTransform (AffineTransform::scale (s));
    content.setBounds (0, 0, kBaseW, kBaseH);
    layoutContent();
    proc.uiScale.store (s);
}

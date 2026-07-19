#pragma once

#include "PluginProcessor.h"
#include "ui/ChoraleLookAndFeel.h"
#include "ui/FxView.h"
#include "ui/MixerView.h"
#include "ui/NoteChips.h"
#include "ui/StageView.h"
#include "ui/Theme.h"
#include "ui/VoiceDetailPanel.h"

class ChoraleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kBaseW = 1120, kBaseH = 648;

    explicit ChoraleEditor (ChoraleProcessor&);
    ~ChoraleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    // Everything lives on a fixed-size logical canvas that gets scale-
    // transformed, so the whole UI resizes without per-widget layout math.
    struct Content : juce::Component
    {
        explicit Content (ChoraleEditor& e) : ed (e) {}
        void paint (juce::Graphics& g) override { ed.paintContent (g); }
        void paintOverChildren (juce::Graphics& g) override { ed.paintContentOver (g); }
        ChoraleEditor& ed;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void paintContentOver (juce::Graphics&);
    void layoutContent();
    void layoutHeader();
    void applyPreset (int presetIndex);
    void selectVoice (int v);
    void setMainView (int mode); // 0 = stage, 1 = mixer, 2 = fx

    // User presets: plain XML state snapshots in the user app-data folder.
    static juce::File userPresetDir();
    void showPresetMenu();
    void promptSaveUserPreset();
    void confirmDeleteUserPreset();
    void loadUserPreset (const juce::File&);

    ChoraleProcessor& proc;
    ChoraleLookAndFeel lnf;
    Content content { *this };

    std::unique_ptr<juce::Drawable> logo, wordmark;

    juce::TextButton presetBtn { "Presets..." };
    juce::ComboBox keyRoot, scale, correct, latMode;
    juce::Slider mix, humanize, tone, width, echoTime, echoFb, echoMix;
    juce::Label keyLbl, scaleLbl, correctLbl, mixLbl, autoKeyLbl, pitchLbl, latencyLbl;
    juce::Label fxLbls[6];
    juce::TextButton stageBtn { "STAGE" }, mixerBtn { "MIXER" }, fxBtn { "FX" };
    juce::TextButton undoBtn, redoBtn, aBtn { "A" }, bBtn { "B" };
    juce::HyperlinkButton updateBtn;

    NoteChips chips;
    StageView stage;
    MixerView mixer;
    FxView fx;
    VoiceDetailPanel detail;

    int viewMode = 0;
    int undoTick = 0;
    juce::Array<juce::File> userFiles;
    juce::File currentUserFile;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        keyAtt, scaleAtt, correctAtt, latAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        mixAtt, humanizeAtt, toneAtt, widthAtt, echoTimeAtt, echoFbAtt, echoMixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoraleEditor)
};

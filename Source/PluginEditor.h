#pragma once
#include "EditorParts.h"

class AutoladdEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int baseW = 780, baseH = 520;

    explicit AutoladdEditor (AutoladdProcessor&);
    ~AutoladdEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override {}

    void showCustomize (bool shouldShow);

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override;
    void applyUi();
    void layoutControls();

    AutoladdProcessor& proc;
    Win7LookAndFeel laf;
    autoladdui::Backdrop content { laf };
    juce::TooltipWindow tooltipWindow { this, 700 };

    autoladdui::CaptionButton gearButton { laf };
    juce::GroupComponent gTune, gRetune, gVoice, gDetect, gOutput, gKey, gPitch;

    autoladdui::Knob kTune, kSpeed, kRobot, kFormant, kTranspose, kFine, kSens, kGate, kRefA, kMix, kGain;

    juce::Label presetLabel, hintLabel, keyLabel, scaleLabel;
    juce::ComboBox presetBox, keyBox, scaleBox;
    juce::ToggleButton bypassButton { "Bypass" }, midiButton { "MIDI note target" };
    juce::TextButton allButton { "All" }, noneButton { "None" }, invertButton { "Invert" };

    autoladdui::KeyboardView keyboard { laf };
    autoladdui::PitchGadget gadget { laf };
    autoladdui::CustomizePanel customize { laf };

    uint16_t lastNonCustomMask = 0x0FFF;
    bool initialised = false;

    std::unique_ptr<APVTS::ComboBoxAttachment> keyAttachment, scaleAttachment;
    std::unique_ptr<APVTS::ButtonAttachment> bypassAttachment, midiAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoladdEditor)
};

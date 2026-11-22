#include "PluginEditor.h"

using namespace juce;
using namespace autoladdui;

namespace
{
Rectangle<int> innerArea (Rectangle<int> group)
{
    return group.withTrimmedTop (26).reduced (8, 0).withTrimmedBottom (6);
}

void placeRow (std::initializer_list<Knob*> knobs, Rectangle<int> group)
{
    const auto a = innerArea (group);
    const int cw = a.getWidth() / (int) knobs.size();
    int i = 0;
    for (auto* k : knobs)
        k->place ({ a.getX() + i++ * cw, a.getY(), cw, a.getHeight() });
}

void styleLabel (Label& l, const String& text, Justification j = Justification::centredLeft, bool bold = false)
{
    l.setText (text, dontSendNotification);
    l.setJustificationType (j);
    l.setFont (win7Font (12.5f, bold));
    l.setInterceptsMouseClicks (false, false);
}
} // namespace

AutoladdEditor::AutoladdEditor (AutoladdProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    addAndMakeVisible (content);
    content.setLookAndFeel (&laf);

    // ---- group boxes -----------------------------------------------------------------------
    gTune.setText ("AUTOTUNE");
    gRetune.setText ("Retune");
    gVoice.setText ("Voice");
    gDetect.setText ("Detection");
    gOutput.setText ("Output");
    gKey.setText ("Key & Scale");
    gPitch.setText ("Pitch");
    for (auto* g : { &gTune, &gRetune, &gVoice, &gDetect, &gOutput, &gKey, &gPitch })
        content.addAndMakeVisible (*g);

    // ---- knobs -----------------------------------------------------------------------------
    auto& a = proc.apvts;
    kTune.init (content, a, ids::autotune, "Autotune level", 100);
    kSpeed.init (content, a, ids::speed, "Speed");
    kRobot.init (content, a, ids::robot, "Robot");
    kFormant.init (content, a, ids::formant, "Formant");
    kTranspose.init (content, a, ids::transpose, "Transpose");
    kFine.init (content, a, ids::fine, "Fine");
    kSens.init (content, a, ids::sensitivity, "Sensitivity");
    kGate.init (content, a, ids::gate, "Gate");
    kRefA.init (content, a, ids::refA, "Ref. A");
    kMix.init (content, a, ids::mix, "Mix");
    kGain.init (content, a, ids::gain, "Gain");
    kTune.label.setFont (win7Font (14.0f, true));
    kTune.slider.setTooltip ("How strongly the voice is pulled onto the notes. 100% = full robotic lock.");

    // ---- command bar -----------------------------------------------------------------------
    styleLabel (presetLabel, "Preset");
    presetBox.addItemList (AutoladdProcessor::getPresetNames(), 1);
    presetBox.setTextWhenNothingSelected ("Choose a preset...");
    presetBox.onChange = [this] {
        if (presetBox.getSelectedId() > 0)
        {
            proc.applyPreset (presetBox.getSelectedId() - 1);
            presetBox.setSelectedId (0, dontSendNotification);
        }
    };
    bypassAttachment = std::make_unique<APVTS::ButtonAttachment> (a, ids::bypass, bypassButton);
    midiAttachment = std::make_unique<APVTS::ButtonAttachment> (a, ids::midiTarget, midiButton);
    midiButton.setTooltip ("Hold MIDI notes to tune the voice to exactly those notes");
    styleLabel (hintLabel, "Double-click a knob to reset it", Justification::centredRight);
    hintLabel.setColour (Label::textColourId, Colour (0xff6a7480));
    for (auto* c : std::initializer_list<Component*> { &presetLabel, &presetBox, &bypassButton, &midiButton, &hintLabel })
        content.addAndMakeVisible (*c);

    // ---- key & scale -----------------------------------------------------------------------
    styleLabel (keyLabel, "Key");
    styleLabel (scaleLabel, "Scale");
    for (auto* n : autoladd::kNoteNames) keyBox.addItem (n, keyBox.getNumItems() + 1);
    for (auto& s : autoladd::kScales) scaleBox.addItem (s.name, scaleBox.getNumItems() + 1);
    keyAttachment = std::make_unique<APVTS::ComboBoxAttachment> (a, ids::key, keyBox);
    scaleAttachment = std::make_unique<APVTS::ComboBoxAttachment> (a, ids::scale, scaleBox);
    scaleBox.onChange = [this] {
        // Switching to "Custom" keeps the notes you were just hearing, so you can start editing from them.
        if (scaleBox.getSelectedItemIndex() == autoladd::kCustomScaleIndex)
            proc.setNoteToggles (lastNonCustomMask);
    };
    allButton.onClick = [this] { proc.setCustomMask (0x0FFF); };
    noneButton.onClick = [this] { proc.setCustomMask (0); };
    invertButton.onClick = [this] { proc.setCustomMask ((uint16_t) (~proc.getEffectiveMask() & 0x0FFF)); };
    allButton.setTooltip ("Enable every note");
    noneButton.setTooltip ("Disable every note (no pitch correction)");
    invertButton.setTooltip ("Swap enabled and disabled notes");
    for (auto* c : std::initializer_list<Component*> { &keyLabel, &scaleLabel, &keyBox, &scaleBox, &allButton, &noneButton,
                                                       &invertButton, &keyboard, &gadget })
        content.addAndMakeVisible (*c);

    keyboard.getMask = [this] { return proc.getEffectiveMask(); };
    keyboard.onMask = [this] (uint16_t m) { proc.setCustomMask (m); };
    keyboard.setTooltip ("Click or drag to switch notes on/off. Enabled notes are the only ones the voice can land on.");

    // ---- title-bar button + customize dialog -----------------------------------------------
    content.addAndMakeVisible (gearButton);
    gearButton.onClick = [this] { showCustomize (true); };
    content.addChildComponent (customize);
    customize.onClose = [this] { showCustomize (false); };
    customize.onChanged = [this] {
        auto s = proc.getUiSettings();
        customize.store (s);
        proc.setUiSettings (s);
        applyUi();
        setSize (roundToInt ((float) baseW * s.scale), roundToInt ((float) baseH * s.scale));
    };

    layoutControls();
    applyUi();

    setResizable (true, true);
    setResizeLimits (baseW * 6 / 10, baseH * 6 / 10, baseW * 22 / 10, baseH * 22 / 10);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    const float scale = jlimit (0.6f, 2.2f, proc.getUiSettings().scale);
    setSize (roundToInt ((float) baseW * scale), roundToInt ((float) baseH * scale));

    initialised = true;
    startTimerHz (30);
}

AutoladdEditor::~AutoladdEditor()
{
    stopTimer();
    content.setLookAndFeel (nullptr);
}

void AutoladdEditor::layoutControls()
{
    content.setBounds (0, 0, baseW, baseH);
    customize.setBounds (0, 0, baseW, baseH);

    gearButton.setBounds (baseW - 76, 0, 58, 21);

    // command bar
    presetLabel.setBounds (20, 44, 50, 24);
    presetBox.setBounds (72, 44, 172, 24);
    bypassButton.setBounds (262, 44, 84, 24);
    midiButton.setBounds (352, 44, 150, 24);
    hintLabel.setBounds (512, 44, 248, 24);

    // groups
    gTune.setBounds (20, 82, 236, 272);
    gRetune.setBounds (268, 82, 240, 130);
    gVoice.setBounds (520, 82, 240, 130);
    gDetect.setBounds (268, 224, 240, 130);
    gOutput.setBounds (520, 224, 240, 130);
    gKey.setBounds (20, 366, 516, 138);
    gPitch.setBounds (548, 366, 212, 138);

    kTune.place (innerArea (gTune.getBounds()).reduced (4, 0));
    placeRow ({ &kSpeed, &kRobot }, gRetune.getBounds());
    placeRow ({ &kFormant, &kTranspose, &kFine }, gVoice.getBounds());
    placeRow ({ &kSens, &kGate, &kRefA }, gDetect.getBounds());
    placeRow ({ &kMix, &kGain }, gOutput.getBounds());

    // key & scale
    keyLabel.setBounds (30, 396, 40, 24);
    keyBox.setBounds (72, 396, 146, 24);
    scaleLabel.setBounds (30, 428, 40, 24);
    scaleBox.setBounds (72, 428, 146, 24);
    allButton.setBounds (30, 462, 56, 26);
    noneButton.setBounds (92, 462, 56, 26);
    invertButton.setBounds (154, 462, 64, 26);
    keyboard.setBounds (232, 394, 294, 100);

    gadget.setBounds (558, 392, 192, 104);
}

void AutoladdEditor::applyUi()
{
    const auto s = proc.getUiSettings();
    const auto& td = kThemes[jlimit (0, kNumThemes - 1, s.theme)];
    const float hue = s.theme == kCustomTheme ? s.hue : td.hue;
    laf.setTheme (Win7Theme::make (hue, td.sat, jlimit (0.0f, 1.0f, s.glass)));
    keyboard.showNames = s.showNames;
    customize.load (s);
    content.sendLookAndFeelChange();
    content.repaint();
}

void AutoladdEditor::showCustomize (bool shouldShow)
{
    if (shouldShow)
    {
        customize.load (proc.getUiSettings());
        customize.toFront (false);
    }
    customize.setVisible (shouldShow);
}

void AutoladdEditor::resized()
{
    if (getWidth() <= 0)
        return; // called while the window is still being created

    const float sc = (float) getWidth() / (float) baseW;
    content.setTransform (AffineTransform::scale (sc));
    content.setBounds (0, 0, baseW, baseH);

    auto s = proc.getUiSettings();
    if (initialised && std::abs (s.scale - sc) > 0.002f)
    {
        s.scale = sc;
        proc.setUiSettings (s);
    }
}

void AutoladdEditor::timerCallback()
{
    const auto d = proc.getPitchDisplay();
    gadget.update (d);
    keyboard.outPitchClass = d.voiced ? ((roundToInt (d.noteOut) % 12) + 12) % 12 : -1;
    keyboard.repaint();

    if (scaleBox.getSelectedItemIndex() != autoladd::kCustomScaleIndex)
        lastNonCustomMask = proc.getEffectiveMask();
}

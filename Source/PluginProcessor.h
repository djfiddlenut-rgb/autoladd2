#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp/AutoTuneEngine.h"
#include "Dsp/Scales.h"

namespace ids
{
inline constexpr const char* autotune    = "autotune";
inline constexpr const char* speed       = "speed";
inline constexpr const char* robot       = "robot";
inline constexpr const char* formant     = "formant";
inline constexpr const char* transpose   = "transpose";
inline constexpr const char* fine        = "fine";
inline constexpr const char* mix         = "mix";
inline constexpr const char* gain        = "gain";
inline constexpr const char* sensitivity = "sensitivity";
inline constexpr const char* gate        = "gate";
inline constexpr const char* refA        = "refa";
inline constexpr const char* key         = "key";
inline constexpr const char* scale       = "scale";
inline constexpr const char* bypass      = "bypass";
inline constexpr const char* midiTarget  = "miditarget";
inline juce::String note (int i) { return "note" + juce::String (i); }
} // namespace ids

/** Look & feel settings that live in the plugin state (they are not automatable parameters). */
struct UiSettings
{
    int   theme     = 0;      // index into the theme table of the editor
    float hue       = 0.585f; // used when the theme is "Custom"
    float glass     = 0.6f;   // 0..1 glass transparency
    float scale     = 1.0f;   // window size relative to the default size
    bool  showNames = true;   // note names on the keyboard
};

struct PitchDisplay
{
    float noteIn = 0.0f, noteOut = 0.0f;
    bool voiced = false;
};

class AutoladdProcessor : public juce::AudioProcessor
{
public:
    AutoladdProcessor();
    ~AutoladdProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "autoladd"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- helpers for the editor ------------------------------------------------------------
    /** Pitch classes (bit 0 = C) the autotune is currently snapping to. Safe from any thread. */
    uint16_t getEffectiveMask() const;
    /** Writes the 12 custom-scale note toggles (does not touch the scale choice). */
    void setNoteToggles (uint16_t mask);
    /** Switches to the "Custom" scale with exactly these notes enabled. */
    void setCustomMask (uint16_t mask);

    PitchDisplay getPitchDisplay() const;

    static const juce::StringArray& getPresetNames();
    void applyPreset (int index);

    UiSettings getUiSettings() const;
    void setUiSettings (const UiSettings&);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float plainValue);

    autoladd::AutoTuneEngine engine;
    int engineChannels = 2;

    std::atomic<float>* pAutotune = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pRobot = nullptr;
    std::atomic<float>* pFormant = nullptr;
    std::atomic<float>* pTranspose = nullptr;
    std::atomic<float>* pFine = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pGain = nullptr;
    std::atomic<float>* pSens = nullptr;
    std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pRefA = nullptr;
    std::atomic<float>* pKey = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pMidi = nullptr;
    std::atomic<float>* pNote[12] {};

    int heldCount[12] {};
    std::atomic<uint16_t> heldMask { 0 };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmooth, gainSmooth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoladdProcessor)
};

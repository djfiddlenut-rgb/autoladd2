# autoladd

An extremely hard autotune VST3 for FL Studio, with a Windows 7 Aero interface.

## Getting the plugin (Windows)

**Easiest - no tools needed:** put this folder in a GitHub repository and push it. The
*Build autoladd (Windows VST3)* workflow runs automatically (Actions tab). Download the
`autoladd-vst3-windows` artifact, unzip it, and run `install-windows.bat` (as administrator).

**Or build locally:** install Visual Studio 2022 (Desktop development with C++), CMake and Git, then run
`Scripts\build-windows.bat`. The result is `build\autoladd_artefacts\Release\VST3\autoladd.vst3`.

Then in FL Studio: *Options -> Manage plugins -> Find plugins*. Add **autoladd** to a mixer insert
(it is an effect, not an instrument). FL Studio 20.8 or newer is needed for VST3.

## Using it

| Control | What it does |
|---|---|
| **Autotune level** (big knob) | 0% = your natural pitch, 100% = locked hard onto the notes |
| **Speed** | How fast it snaps. *Instant* = the classic hard, robotic jump |
| **Robot** | Freezes the vocal cycle for a metallic buzz. 100% = full robot |
| **Key / Scale** | Pick a key and one of 19 scales, or choose *Custom* |
| **Piano keyboard** | Click or drag keys to enable/disable individual notes - any edit switches to *Custom*. Orange dot = the note it is currently tuned to |
| **MIDI note target** | Hold MIDI notes and the voice is tuned to exactly those notes |
| **Formant / Transpose / Fine** | Change the character or pitch of the voice (robots, aliens, deep voices) |
| **Sensitivity / Gate / Ref. A** | Pitch-detection tuning and the A=440 reference |
| **Mix / Gain** | Dry/wet and output level |
| **Presets** | Natural Tune, Pop Tune, Hard Tune, Robot Voice, Extreme Robot, Monotone Robot, Deep Robot, Alien |
| **Gear button** (title bar) | Color scheme (Aero Blue, Graphite, Emerald, Rose, Sunset, Violet, custom hue), glass transparency, window size, note names |

The window is also resizable by dragging the bottom-right corner. Double-click any knob to reset it.

Tips for the most extreme sound: Autotune 100%, Speed *Instant*, Robot 100%, and a narrow scale
(or the **Monotone Robot** preset, which locks everything to a single note).

Works best on one voice at a time (it is a monophonic tuner). The plugin reports about 39 ms of latency
to FL Studio, which compensates for it automatically.

## Layout

- `Source/Dsp` - pitch tracker (YIN) and the pitch-synchronous resynthesis engine (no JUCE dependency)
- `Source/PluginProcessor.*` - parameters, MIDI, audio
- `Source/PluginEditor.*`, `EditorParts.h`, `Win7LookAndFeel.h` - the interface
- `Tests/` - offline test harness (`-DAUTOLADD_BUILD_TESTS=ON`)

Built with [JUCE](https://juce.com) 8 (AGPLv3 / commercial licence). If you distribute the plugin to others, check JUCE's licence terms.

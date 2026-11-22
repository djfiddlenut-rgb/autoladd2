#pragma once
#include <cstdint>
#include <initializer_list>

namespace autoladd
{
// Scale masks are relative to the tonic: bit i set => interval of i semitones is allowed.
constexpr uint16_t intervalsToMask (std::initializer_list<int> intervals)
{
    uint16_t m = 0;
    for (int i : intervals)
        m = (uint16_t) (m | (1u << i));
    return m;
}

struct ScaleDef
{
    const char* name;
    uint16_t mask;
};

// The LAST entry must stay "Custom" (it means: use the 12 note toggles instead).
inline constexpr ScaleDef kScales[] = {
    { "Chromatic",          0x0FFF },
    { "Major",              intervalsToMask ({ 0, 2, 4, 5, 7, 9, 11 }) },
    { "Natural Minor",      intervalsToMask ({ 0, 2, 3, 5, 7, 8, 10 }) },
    { "Harmonic Minor",     intervalsToMask ({ 0, 2, 3, 5, 7, 8, 11 }) },
    { "Melodic Minor",      intervalsToMask ({ 0, 2, 3, 5, 7, 9, 11 }) },
    { "Major Pentatonic",   intervalsToMask ({ 0, 2, 4, 7, 9 }) },
    { "Minor Pentatonic",   intervalsToMask ({ 0, 3, 5, 7, 10 }) },
    { "Blues",              intervalsToMask ({ 0, 3, 5, 6, 7, 10 }) },
    { "Dorian",             intervalsToMask ({ 0, 2, 3, 5, 7, 9, 10 }) },
    { "Phrygian",           intervalsToMask ({ 0, 1, 3, 5, 7, 8, 10 }) },
    { "Lydian",             intervalsToMask ({ 0, 2, 4, 6, 7, 9, 11 }) },
    { "Mixolydian",         intervalsToMask ({ 0, 2, 4, 5, 7, 9, 10 }) },
    { "Locrian",            intervalsToMask ({ 0, 1, 3, 5, 6, 8, 10 }) },
    { "Whole Tone",         intervalsToMask ({ 0, 2, 4, 6, 8, 10 }) },
    { "Diminished",         intervalsToMask ({ 0, 2, 3, 5, 6, 8, 9, 11 }) },
    { "Phrygian Dominant",  intervalsToMask ({ 0, 1, 4, 5, 7, 8, 10 }) },
    { "Major Chord",        intervalsToMask ({ 0, 4, 7 }) },
    { "Minor Chord",        intervalsToMask ({ 0, 3, 7 }) },
    { "Root + Fifth",       intervalsToMask ({ 0, 7 }) },
    { "Custom",             0x0FFF },
};

inline constexpr int kNumScales = (int) (sizeof (kScales) / sizeof (kScales[0]));
inline constexpr int kCustomScaleIndex = kNumScales - 1;

inline constexpr const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// Rotate a tonic-relative mask so bit 0 means C (absolute pitch class).
constexpr uint16_t rotateMaskToKey (uint16_t relMask, int key)
{
    key = ((key % 12) + 12) % 12;
    return (uint16_t) ((((unsigned) relMask << key) | ((unsigned) relMask >> (12 - key))) & 0x0FFFu);
}

constexpr uint16_t absoluteMaskFor (int scaleIndex, int key)
{
    return rotateMaskToKey (kScales[scaleIndex].mask, key);
}
} // namespace autoladd

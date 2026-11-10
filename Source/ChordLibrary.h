#pragma once

#include <array>
#include <string>
#include <vector>

namespace SixthChords {
constexpr int bankSize = 12;
constexpr int maxChordNotes = 6;
constexpr int voicingCount = 6;

struct Chord {
    const char* name;
    std::array<int, maxChordNotes> notes;
    int size;
};

struct Bank {
    const char* name;
    const char* category;
    const char* description;
    std::array<Chord, bankSize> chords;
};

struct Notes {
    std::array<int, maxChordNotes> notes {};
    int size = 0;
};

// Original, style-inspired palettes, not transcriptions of named artists' works.
// Call banks() during processor construction, before entering the audio callback.
const std::vector<Bank>& banks();
const char* voicingName(int voicing) noexcept;
// GUI-only label formatting; unlike resolve(), this helper may allocate.
std::string chordName(int bank, int slot, int key);

// key is a chromatic transposition in semitones (0..11); octaveShift is octaves.
// resolve() allocates no memory. Invalid bank/slot indices return an empty chord.
Notes resolve(int bank, int slot, int key, int voicing, int octaveShift = 0);
}

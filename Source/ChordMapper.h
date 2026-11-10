#pragma once

#include "ChordLibrary.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>

namespace SixthChords {

struct Mapping {
    bool enabled = false;
    int bank = 0;
    int key = 0;
    int voicing = 0;
    int registerShift = 0;
};

// Owns note lifetimes independently of the currently selected chord palette.
// All methods/getters are audio-thread methods. Publish GUI state separately
// through the processor's atomics rather than reading these fields concurrently.
class Mapper {
public:
    // Call before processing. The processor must also stop its existing voices
    // if prepare() is called while notes are sounding.
    void prepare();

    // input and output must be different buffers. output is cleared first;
    // reserve its expected MIDI capacity before entering the audio callback.
    // Other MIDI messages keep their original bytes and sample positions.
    void process(const juce::MidiBuffer& input, juce::MidiBuffer& output,
                 const Mapping& mapping);

    // Appends one note-off per owned output pitch/channel, then clears state.
    // This does not clear output or release an external sustain pedal.
    void reset(juce::MidiBuffer& output, int samplePosition = 0);

    int lastSlot() const noexcept { return lastPlayedSlot; }
    int lastTrigger() const noexcept { return lastPlayedTrigger; }
    const Mapping& lastMapping() const noexcept { return playedMapping; }
    Notes lastNotes() const noexcept { return playedNotes; }
    int activeSlots() const noexcept;

private:
    struct Trigger {
        Notes generated;
        bool active = false;
        int chordSlot = -1;
    };

    void clearTracking() noexcept;
    void releaseTrigger(int channelIndex, int inputNote, juce::MidiBuffer& output,
                        int samplePosition, juce::uint8 velocity);
    void clearChannel(int channelIndex, juce::MidiBuffer& output, int samplePosition);
    void pressTrigger(int channelIndex, int inputNote, juce::uint8 velocity,
                      const Mapping&, juce::MidiBuffer& output, int samplePosition);

    std::array<std::array<Trigger, 128>, 16> triggers {};
    std::array<std::array<std::uint16_t, 128>, 16> owners {};
    std::array<std::uint16_t, bankSize> slotOwners {};
    int lastPlayedSlot = -1;
    int lastPlayedTrigger = -1;
    Mapping playedMapping;
    Notes playedNotes;
    bool modeKnown = false;
    bool previousEnabled = false;
};

}

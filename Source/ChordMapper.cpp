#include "ChordMapper.h"

namespace SixthChords {

void Mapper::clearTracking() noexcept
{
    for(auto& channel : triggers)
        for(auto& trigger : channel)
            trigger = Trigger {};
    for(auto& channel : owners)
        channel.fill(0);
    slotOwners.fill(0);
    lastPlayedSlot = -1;
    lastPlayedTrigger = -1;
    playedMapping = Mapping {};
    playedNotes = Notes {};
    modeKnown = false;
    previousEnabled = false;
}

void Mapper::prepare()
{
    // Static musical data is initialized here, never for the first time in
    // process(). Processor construction also primes it for parameter choices.
    (void) banks();
    clearTracking();
}

int Mapper::activeSlots() const noexcept
{
    int mask = 0;
    for(int slot = 0; slot < bankSize; ++slot)
        if(slotOwners[static_cast<size_t>(slot)] != 0)
            mask |= 1 << slot;
    return mask;
}

void Mapper::releaseTrigger(int channelIndex, int inputNote,
                            juce::MidiBuffer& output, int samplePosition,
                            juce::uint8 velocity)
{
    auto& trigger = triggers[static_cast<size_t>(channelIndex)][static_cast<size_t>(inputNote)];
    if(!trigger.active)
        return;

    auto& channelOwners = owners[static_cast<size_t>(channelIndex)];
    for(int i = 0; i < trigger.generated.size; ++i)
    {
        const int note = trigger.generated.notes[static_cast<size_t>(i)];
        auto& count = channelOwners[static_cast<size_t>(note)];
        jassert(count != 0);
        if(count != 0 && --count == 0)
            output.addEvent(juce::MidiMessage::noteOff(channelIndex + 1, note, velocity),
                            samplePosition);
    }

    if(trigger.chordSlot >= 0)
    {
        auto& count = slotOwners[static_cast<size_t>(trigger.chordSlot)];
        jassert(count != 0);
        if(count != 0)
            --count;
    }
    trigger = Trigger {};
}

void Mapper::clearChannel(int channelIndex, juce::MidiBuffer& output,
                          int samplePosition)
{
    for(int note = 0; note < 128; ++note)
        releaseTrigger(channelIndex, note, output, samplePosition, 0);
}

void Mapper::reset(juce::MidiBuffer& output, int samplePosition)
{
    for(int channel = 0; channel < 16; ++channel)
        clearChannel(channel, output, samplePosition);
    clearTracking();
}

void Mapper::pressTrigger(int channelIndex, int inputNote, juce::uint8 velocity,
                          const Mapping& mapping, juce::MidiBuffer& output,
                          int samplePosition)
{
    // A repeated key-down replaces that key's previous mapping. In particular,
    // a bank change followed by the same key must not orphan the earlier chord.
    releaseTrigger(channelIndex, inputNote, output, samplePosition, 0);

    Notes generated;
    if(mapping.enabled)
        generated = resolve(mapping.bank, inputNote % bankSize, mapping.key,
                            mapping.voicing, inputNote / 12 - 4 + mapping.registerShift);
    else
    {
        generated.notes[0] = inputNote;
        generated.size = 1;
    }

    auto& trigger = triggers[static_cast<size_t>(channelIndex)][static_cast<size_t>(inputNote)];
    // Defensively retain only distinct, valid notes even if a malformed library
    // entry is encountered. No reference can then be added twice by one key.
    for(int i = 0; i < juce::jlimit(0, maxChordNotes, generated.size); ++i)
    {
        const int note = generated.notes[static_cast<size_t>(i)];
        if(!juce::isPositiveAndBelow(note, 128))
            continue;
        bool duplicate = false;
        for(int existing = 0; existing < trigger.generated.size; ++existing)
            duplicate = duplicate || trigger.generated.notes[static_cast<size_t>(existing)] == note;
        if(!duplicate)
            trigger.generated.notes[static_cast<size_t>(trigger.generated.size++)] = note;
    }
    if(trigger.generated.size == 0)
        return;

    trigger.active = true;
    if(mapping.enabled)
    {
        trigger.chordSlot = inputNote % bankSize;
        ++slotOwners[static_cast<size_t>(trigger.chordSlot)];
        lastPlayedSlot = trigger.chordSlot;
        lastPlayedTrigger = inputNote;
        playedMapping = mapping;
        playedNotes = trigger.generated;
    }

    auto& channelOwners = owners[static_cast<size_t>(channelIndex)];
    for(int i = 0; i < trigger.generated.size; ++i)
    {
        const int note = trigger.generated.notes[static_cast<size_t>(i)];
        auto& count = channelOwners[static_cast<size_t>(note)];
        // Shared common tones are sustained rather than retriggered. A tone
        // ends only when the final key that owns it is released, per channel.
        if(count++ == 0)
            output.addEvent(juce::MidiMessage::noteOn(channelIndex + 1, note, velocity),
                            samplePosition);
    }
}

void Mapper::process(const juce::MidiBuffer& input, juce::MidiBuffer& output,
                     const Mapping& mapping)
{
    jassert(&input != &output);
    if(&input == &output)
        return;

    output.clear();
    // Mode switching must release notes played in the previous mode. Tracking
    // single-note bypass input makes both directions safe, including held keys.
    if(modeKnown && previousEnabled != mapping.enabled)
        reset(output, 0);
    modeKnown = true;
    previousEnabled = mapping.enabled;

    for(const auto metadata : input)
    {
        const auto* bytes = metadata.data;
        const int size = metadata.numBytes;
        if(size <= 0)
            continue;
        const int status = bytes[0] & 0xf0;
        const int channel = bytes[0] & 0x0f;

        if(size >= 3 && (status == 0x90 || status == 0x80))
        {
            const int note = bytes[1] & 0x7f;
            const auto velocity = static_cast<juce::uint8>(bytes[2] & 0x7f);
            if(status == 0x90 && velocity != 0)
                pressTrigger(channel, note, velocity, mapping, output, metadata.samplePosition);
            else
            {
                const bool owned = triggers[static_cast<size_t>(channel)][static_cast<size_t>(note)].active;
                releaseTrigger(channel, note, output, metadata.samplePosition,
                               status == 0x80 ? velocity : 0);
                // In chord mode an unmatched input off must never terminate a
                // common tone currently owned by some other mapped trigger.
                if(!owned && !mapping.enabled)
                    output.addEvent(bytes, size, metadata.samplePosition);
            }
        }
        else
        {
            if(size >= 3 && status == 0xb0 && (bytes[1] == 120 || bytes[1] == 123))
                clearChannel(channel, output, metadata.samplePosition);
            // Forward raw data, avoiding an unnecessary MidiMessage/SysEx copy.
            output.addEvent(bytes, size, metadata.samplePosition);
        }
    }
}

}

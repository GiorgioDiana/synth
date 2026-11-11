#include "ChordTests.h"
#include "ChordLibrary.h"
#include "ChordMapper.h"
#include "PluginProcessor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using SixthChords::Mapper;
using SixthChords::Mapping;
using SixthChords::Notes;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error("Chord mode: " + message);
}

std::vector<int> pitches(const Notes& notes)
{
    return {notes.notes.begin(), notes.notes.begin() + notes.size};
}

std::vector<int> pitchClasses(const Notes& notes, int transpose = 0)
{
    std::vector<int> result;
    for (int note : pitches(notes))
        result.push_back((note + transpose) % 12);
    std::sort(result.begin(), result.end());
    return result;
}

void validateNotes(const Notes& notes, int expectedSize, const std::string& context)
{
    require(notes.size == expectedSize, context + " changed the number of voices");
    require(notes.size >= 3 && notes.size <= SixthChords::maxChordNotes,
            context + " has an unsupported voice count");
    for (int i = 0; i < notes.size; ++i)
    {
        require(notes.notes[static_cast<size_t>(i)] >= 0 && notes.notes[static_cast<size_t>(i)] <= 127,
                context + " emitted a note outside MIDI range");
        if (i != 0)
            require(notes.notes[static_cast<size_t>(i - 1)] < notes.notes[static_cast<size_t>(i)],
                    context + " contains duplicate or unsorted pitches");
    }
}

void testLibrary()
{
    const auto& banks = SixthChords::banks();
    require(banks.size() >= 48 && SixthChords::bankSize == 49,
            "the requested 49-key chord-bank library is missing");
    require(SixthChords::firstTrigger == 24 && SixthChords::lastTrigger == 72,
            "the published C1-C5 performance range changed");
    std::set<std::string> bankNames, palettes;
    bool alternateVoicing = false, variedChordQualities = false;
    bool majorTonic = false, minorTonic = false, dominantSeventh = false;
    size_t chordCount = 0;
    for (size_t bankIndex = 0; bankIndex < banks.size(); ++bankIndex)
    {
        const auto& bank = banks[bankIndex];
        const auto nonempty = [](const char* value) { return value != nullptr && *value != '\0'; };
        require(nonempty(bank.name) && nonempty(bank.category) && nonempty(bank.description),
                "a bank is missing its name, category or musical description");
        require(bankNames.insert(bank.name).second, "duplicate bank name: " + std::string(bank.name));
        std::ostringstream palette;
        std::set<std::string> intervalShapes, writtenVoicings;
        bool differentOctaveHarmony = false;
        for (int slot = 0; slot < SixthChords::bankSize; ++slot)
        {
            ++chordCount;
            const auto& chord = bank.chords[static_cast<size_t>(slot)];
            require(nonempty(chord.name), std::string(bank.name) + " has an unnamed chord");
            const auto context = std::string(bank.name) + " / MIDI " + std::to_string(24+slot) + " / " + chord.name;
            const auto written = SixthChords::resolve(static_cast<int>(bankIndex), slot, 0, 0);
            validateNotes(written, chord.size, context);
            auto expected = std::vector<int>(chord.notes.begin(), chord.notes.begin() + chord.size);
            std::sort(expected.begin(), expected.end());
            require(pitches(written) == expected, context + " does not preserve the written voicing");
            const auto classes = pitchClasses(written);
            const auto contains = [&](int note) { return std::find(classes.begin(), classes.end(), note) != classes.end(); };
            majorTonic = majorTonic || (contains(0) && contains(4) && contains(7));
            minorTonic = minorTonic || (contains(0) && contains(3) && contains(7));
            for (int root = 0; root < 12; ++root)
                dominantSeventh = dominantSeventh || (contains(root) && contains((root+4)%12)
                                                     && contains((root+7)%12) && contains((root+10)%12));
            if (slot + 12 < SixthChords::bankSize)
                differentOctaveHarmony = differentOctaveHarmony
                    || classes != pitchClasses(SixthChords::resolve(static_cast<int>(bankIndex), slot+12, 0, 0));
            std::ostringstream shape;
            for (int note : expected)
            {
                palette << note << ',';
                shape << (note - expected.front()) % 12 << ',';
            }
            std::ostringstream voicingKey;
            for (int note:expected) voicingKey<<note<<',';
            require(writtenVoicings.insert(voicingKey.str()).second,
                    context + " duplicates another written voicing in its 49-key bank");
            palette << ';';
            intervalShapes.insert(shape.str());
            // Exercise the complete published key/voicing space and extreme registers,
            // without a combinatorial loop over every possible integer octave value.
            for (int key = 0; key < 12; ++key)
                for (int voicing = 0; voicing < SixthChords::voicingCount; ++voicing)
                    for (int octave : {-6, -2, 0, 8})
                    {
                        const auto voiced = SixthChords::resolve(static_cast<int>(bankIndex), slot, key, voicing, octave);
                        validateNotes(voiced, chord.size, context);
                        require(pitchClasses(voiced) == pitchClasses(written, key),
                                context + " changes chord tones while voicing or transposing");
                        if (key == 0 && octave == 0 && pitches(voiced) != pitches(written))
                            alternateVoicing = true;
                    }
            const auto octaveUp = SixthChords::resolve(static_cast<int>(bankIndex), slot, 0, 0, 1);
            if (written.notes[static_cast<size_t>(written.size - 1)] + 12 <= 127)
                for (int i = 0; i < written.size; ++i)
                    require(octaveUp.notes[static_cast<size_t>(i)] == written.notes[static_cast<size_t>(i)] + 12,
                            context + " ignores the selected register");
        }
        require(palettes.insert(palette.str()).second,
                std::string(bank.name) + " duplicates an entire existing written palette");
        require(differentOctaveHarmony, std::string(bank.name) + " merely repeats 12 pitch-class palettes across octaves");
        variedChordQualities = variedChordQualities || intervalShapes.size() > 1;
        // The original v0.3 twelve-slot palette is intentionally embedded at C3-B3.
        for (int legacy = 0; legacy < SixthChords::legacyBankSize; ++legacy)
        {
            require(pitches(SixthChords::resolveLegacy(static_cast<int>(bankIndex),legacy,0,0))
                    == pitches(SixthChords::resolve(static_cast<int>(bankIndex),24+legacy,0,0)),
                    std::string(bank.name) + " changed its recorded legacy twelve-key palette");
            require(SixthChords::legacyChordName(static_cast<int>(bankIndex),legacy,0)
                    == SixthChords::chordName(static_cast<int>(bankIndex),24+legacy,0),
                    std::string(bank.name) + " changed its legacy chord labels");
        }
    }
    require(alternateVoicing, "the voicing selector has no effect on the library");
    require(variedChordQualities && majorTonic && minorTonic && dominantSeventh,
            "the library lacks basic tonic/dominant harmonic choices");
    for (int voicing = 0; voicing < SixthChords::voicingCount; ++voicing)
        require(SixthChords::voicingName(voicing) != nullptr && *SixthChords::voicingName(voicing) != '\0',
                "a voicing choice is unnamed");
    require(SixthChords::resolve(-1,0,0,0).size == 0
            && SixthChords::resolve(static_cast<int>(banks.size()),0,0,0).size == 0
            && SixthChords::resolve(0,-1,0,0).size == 0
            && SixthChords::resolve(0,SixthChords::bankSize,0,0).size == 0
            && SixthChords::resolveLegacy(0,12,0,0).size == 0,
            "invalid library indices did not return an empty chord");
    std::cout << "PASS chord library: " << banks.size() << " distinct 49-key banks, " << chordCount
              << " named slots, full voicing/range coverage and preserved legacy palettes\n";
}

struct Event {
    juce::MidiMessage message;
    int position;
};
std::vector<Event> events(const juce::MidiBuffer& buffer)
{
    std::vector<Event> result;
    for (const auto metadata : buffer)
        result.push_back({metadata.getMessage(), metadata.samplePosition});
    return result;
}
juce::MidiBuffer process(Mapper& mapper, const Mapping& mapping,
                         std::initializer_list<Event> input = {})
{
    juce::MidiBuffer incoming, outgoing;
    outgoing.ensureSize(4096);
    for (const auto& event : input)
        incoming.addEvent(event.message, event.position);
    mapper.process(incoming, outgoing, mapping);
    return outgoing;
}
Event on(int note, int position = 0, int channel = 1, int velocity = 93)
{
    return {juce::MidiMessage::noteOn(channel, note, static_cast<juce::uint8>(velocity)), position};
}
Event off(int note, int position = 0, int channel = 1)
{
    return {juce::MidiMessage::noteOff(channel, note, static_cast<juce::uint8>(31)), position};
}
Mapping enabledMapping(int bank = 0)
{
    Mapping mapping;
    mapping.enabled = true;
    mapping.bank = bank;
    return mapping;
}
void expectNotes(const juce::MidiBuffer& buffer, std::vector<int> expected,
                 bool noteOn, int channel, int position, int velocity = -1)
{
    std::vector<int> actual;
    for (const auto& event : events(buffer))
    {
        require(noteOn ? event.message.isNoteOn() : event.message.isNoteOff(),
                "unexpected MIDI message while checking generated notes");
        require(event.message.getChannel() == channel && event.position == position,
                "generated note changed its channel or sample timestamp");
        if (velocity >= 0)
            require(event.message.getVelocity() == velocity, "generated note changed the trigger velocity");
        actual.push_back(event.message.getNoteNumber());
    }
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    require(actual == expected, "generated note set does not match the selected palette/voicing");
}
std::vector<int> subtract(const std::vector<int>& a, const std::vector<int>& b)
{
    std::vector<int> result;
    for (int note : a)
        if (std::find(b.begin(), b.end(), note) == b.end())
            result.push_back(note);
    return result;
}

std::uint64_t slotMask(int slot) { return std::uint64_t{1} << slot; }

void testKeyboardMapping()
{
    Mapper mapper;
    mapper.prepare();
    auto mapping = enabledMapping();
    unsigned previousGeneration = mapper.generation();
    for (int slot = 0; slot < SixthChords::bankSize; ++slot)
    {
        const int trigger = SixthChords::firstTrigger + slot;
        const auto written = SixthChords::resolve(mapping.bank,slot,0,0);
        expectNotes(process(mapper,mapping,{on(trigger,17,3,81)}),pitches(written),true,3,17,81);
        require(mapper.lastSlot() == slot && mapper.lastTrigger() == trigger
                && mapper.activeSlots() == slotMask(slot), "49-key active/last chord state is incorrect");
        require(mapper.generation() == previousGeneration+1, "mapped note-on did not advance visual feedback generation");
        previousGeneration = mapper.generation();
        expectNotes(process(mapper,mapping,{off(trigger,61,3)}),pitches(written),false,3,61,31);
        require(mapper.activeSlots() == 0, "released key remains marked active");
    }
    mapping.key = 7; mapping.voicing = 3; mapping.registerShift = -1;
    for (int trigger : {24,36,48,60,72})
    {
        const auto expected = SixthChords::resolve(mapping.bank,trigger-24,mapping.key,
                                                  mapping.voicing,mapping.registerShift);
        expectNotes(process(mapper,mapping,{on(trigger,9,16)}),pitches(expected),true,16,9,93);
        expectNotes(process(mapper,mapping,{off(trigger,127,16)}),pitches(expected),false,16,127,31);
    }
    const auto beforeMelody = mapper.generation();
    for (int trigger : {0,23,73,127})
    {
        expectNotes(process(mapper,mapping,{on(trigger,5,16)}),{trigger},true,16,5,93);
        expectNotes(process(mapper,mapping,{off(trigger,87,16)}),{trigger},false,16,87,31);
        require(mapper.activeSlots() == 0 && mapper.generation() == beforeMelody,
                "outside-range melody notes were treated as chord triggers");
    }
    mapping.layout = 1;
    for (int trigger : {0,36,48,60,72,127})
    {
        const auto expected = SixthChords::resolveLegacy(mapping.bank,trigger%12,mapping.key,
                                                        mapping.voicing,trigger/12-4+mapping.registerShift);
        expectNotes(process(mapper,mapping,{on(trigger,9,16)}),pitches(expected),true,16,9,93);
        expectNotes(process(mapper,mapping,{off(trigger,127,16)}),pitches(expected),false,16,127,31);
    }
    std::cout << "PASS 49-key C1-C5 mapping, full-range melodies, legacy octave maps and trigger feedback\n";
}

void testOwnership()
{
    int selectedBank = -1, slotA = -1, slotB = -1;
    for (size_t bank = 0; bank < SixthChords::banks().size() && selectedBank < 0; ++bank)
        for (int a = 0; a < SixthChords::bankSize && selectedBank < 0; ++a)
            for (int b = a+1; b < SixthChords::bankSize; ++b)
            {
                const auto notesA = pitches(SixthChords::resolve(static_cast<int>(bank),a,0,0));
                const auto notesB = pitches(SixthChords::resolve(static_cast<int>(bank),b,0,0));
                if (subtract(notesA,notesB).size() < notesA.size()
                    && !subtract(notesA,notesB).empty() && !subtract(notesB,notesA).empty())
                {
                    selectedBank = static_cast<int>(bank); slotA = a; slotB = b; break;
                }
            }
    require(selectedBank >= 0, "no pair of palette chords shares a common tone");
    auto mapping = enabledMapping(selectedBank);
    const auto a = pitches(SixthChords::resolve(selectedBank,slotA,0,0));
    const auto b = pitches(SixthChords::resolve(selectedBank,slotB,0,0));
    const int triggerA = 24+slotA, triggerB = 24+slotB;
    Mapper mapper;
    mapper.prepare();
    expectNotes(process(mapper,mapping,{on(triggerA)}),a,true,1,0,93);
    expectNotes(process(mapper,mapping,{on(triggerB,5)}),subtract(b,a),true,1,5,93);
    require(mapper.activeSlots() == (slotMask(slotA)|slotMask(slotB)), "overlapping chords lost their held slots");
    expectNotes(process(mapper,mapping,{off(triggerA,14)}),subtract(a,b),false,1,14,31);
    expectNotes(process(mapper,mapping,{off(triggerB,31)}),b,false,1,31,31);
    require(mapper.activeSlots() == 0, "common-tone ownership left a stuck chord");

    expectNotes(process(mapper,mapping,{on(triggerA)}),a,true,1,0,93);
    mapping.bank = (mapping.bank+1)%static_cast<int>(SixthChords::banks().size());
    mapping.key = 5; mapping.voicing = 4; mapping.registerShift = 1;
    require(events(process(mapper,mapping)).empty(), "bank changes unexpectedly rewrote held notes");
    expectNotes(process(mapper,mapping,{off(triggerA,47)}),a,false,1,47,31);
    const auto changed = pitches(SixthChords::resolve(mapping.bank,slotA,mapping.key,mapping.voicing,mapping.registerShift));
    expectNotes(process(mapper,mapping,{on(triggerA,7)}),changed,true,1,7,93);
    const auto generation = mapper.generation();
    const auto repeated = events(process(mapper,mapping,{on(triggerA,13)}));
    require(mapper.generation() == generation+1, "repeating the same key did not refresh chord visuals");
    require(repeated.size() == changed.size()*2, "repeated key did not replace its previous ownership");
    for (int note : changed)
    {
        int offIndex = -1, onIndex = -1;
        for (size_t i = 0; i < repeated.size(); ++i)
            if (repeated[i].message.getNoteNumber() == note)
            {
                require(repeated[i].position == 13, "retrigger changed MIDI timing");
                if (repeated[i].message.isNoteOff()) offIndex = static_cast<int>(i);
                if (repeated[i].message.isNoteOn()) onIndex = static_cast<int>(i);
            }
        require(offIndex >= 0 && onIndex > offIndex, "retrigger emitted note-on before releasing its old voice");
    }
    expectNotes(process(mapper,mapping,{off(triggerA,19)}),changed,false,1,19,31);

    // A stale raw note-off above C5 must not cut a pitch owned by a mapped chord.
    mapping.registerShift = 2;
    int highSlot = -1, highNote = -1;
    for (int slot = 0; slot < SixthChords::bankSize && highSlot < 0; ++slot)
    {
        const auto chord = SixthChords::resolve(mapping.bank,slot,mapping.key,mapping.voicing,mapping.registerShift);
        for (int note : pitches(chord))
            if (note > SixthChords::lastTrigger) { highSlot = slot; highNote = note; break; }
    }
    require(highSlot >= 0, "high-register chord coverage is missing");
    const auto highChord = pitches(SixthChords::resolve(mapping.bank,highSlot,mapping.key,mapping.voicing,mapping.registerShift));
    expectNotes(process(mapper,mapping,{on(24+highSlot)}),highChord,true,1,0,93);
    require(events(process(mapper,mapping,{off(highNote,5)})).empty()
            && mapper.activeSlots() == slotMask(highSlot), "unmatched outside-range off cut a mapped high chord tone");
    expectNotes(process(mapper,mapping,{off(24+highSlot,15)}),highChord,false,1,15,31);
    std::cout << "PASS shared-note ownership, held bank changes, retrigger generations and stale melody offs\n";
}

void testModesAndControllers()
{
    Mapper mapper;
    mapper.prepare();
    Mapping mapping;
    expectNotes(process(mapper,mapping,{on(60,4)}),{60},true,1,4,93);
    mapping.enabled = true;
    expectNotes(process(mapper,mapping),{60},false,1,0,0);
    require(events(process(mapper,mapping,{off(60,15)})).empty(), "stale raw note-off cut a chord after enabling mode");
    const auto chord = pitches(SixthChords::resolve(0,0,0,0));
    expectNotes(process(mapper,mapping,{on(24,7)}),chord,true,1,7,93);
    mapping.enabled = false;
    expectNotes(process(mapper,mapping),chord,false,1,0,0);
    require(mapper.activeSlots() == 0, "bypassing chord mode did not clear held chord display");

    mapping.enabled = true;
    process(mapper,mapping,{on(24,0,1),on(24,1,2)});
    expectNotes(process(mapper,mapping,{off(24,5,1)}),chord,false,1,5,31);
    require(mapper.activeSlots() == slotMask(0), "one channel released another channel's chord");
    const auto allOff = events(process(mapper,mapping,{{juce::MidiMessage::allNotesOff(2),9}}));
    require(allOff.size() == chord.size()+1, "all-notes-off failed to flush channel ownership");
    require(allOff.back().message.isControllerOfType(123) && allOff.back().position == 9,
            "all-notes-off controller was lost or retimed");
    for (size_t i = 0; i < chord.size(); ++i)
        require(allOff[i].message.isNoteOff() && allOff[i].message.getChannel() == 2 && allOff[i].position == 9,
                "all-notes-off affected the wrong channel or sample time");
    require(mapper.activeSlots() == 0, "all-notes-off left active chord ownership");
    process(mapper,mapping,{on(24,0,4)});
    const auto soundOff = events(process(mapper,mapping,{{juce::MidiMessage::allSoundOff(4),12}}));
    require(soundOff.size() == chord.size()+1 && soundOff.back().message.isControllerOfType(120),
            "all-sound-off did not flush chord ownership and pass through");

    const auto sustain = juce::MidiMessage::controllerEvent(7,64,127);
    const auto pitch = juce::MidiMessage::pitchWheel(7,10000);
    const auto mod = juce::MidiMessage::controllerEvent(7,1,85);
    const auto sustainOff = juce::MidiMessage::controllerEvent(7,64,0);
    const auto forwarded = events(process(mapper,mapping,{{sustain,3},{pitch,11},{mod,17},{sustainOff,29}}));
    const std::array<juce::MidiMessage,4> expected {sustain,pitch,mod,sustainOff};
    require(forwarded.size() == expected.size(), "controller passthrough added or removed events");
    for (size_t i = 0; i < expected.size(); ++i)
    {
        require(forwarded[i].message.getRawDataSize() == expected[i].getRawDataSize()
                && std::equal(expected[i].getRawData(),expected[i].getRawData()+expected[i].getRawDataSize(),
                              forwarded[i].message.getRawData()), "controller bytes were altered");
        require(forwarded[i].position == std::array<int,4>{3,11,17,29}[i], "controller timestamp changed");
    }
    process(mapper,mapping,{on(24,0,1),on(25,2,2)});
    juce::MidiBuffer resetOutput;
    mapper.reset(resetOutput,23);
    require(!events(resetOutput).empty() && mapper.activeSlots() == 0
            && mapper.lastSlot() == -1 && mapper.lastTrigger() == -1, "panic reset did not clear mapper state");
    for (const auto& event : events(resetOutput))
        require(event.message.isNoteOff() && event.position == 23, "panic reset lost note-off timing");
    require(events(process(mapper,mapping,{off(24),off(25,1,2)})).empty(), "stale note-offs survived mapper panic");

    expectNotes(process(mapper,mapping,{on(24)}),chord,true,1,0,93);
    mapping.layout = 1;
    expectNotes(process(mapper,mapping),chord,false,1,0,0);
    require(mapper.activeSlots() == 0, "layout switching did not flush the old trigger map");
    const auto legacyChord = pitches(SixthChords::resolveLegacy(0,0,0,0));
    expectNotes(process(mapper,mapping,{on(48,9)}),legacyChord,true,1,9,93);
    expectNotes(process(mapper,mapping,{off(48,21)}),legacyChord,false,1,21,31);
    std::cout << "PASS mode/layout changes, MIDI channel isolation, controller passthrough and panic\n";
}

void setParameter(SixthProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter(id);
    require(parameter != nullptr, std::string("missing chord parameter ") + id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
std::vector<int> soundingPitches(const SixthProcessor& processor)
{
    std::vector<int> result;
    for (int note=0;note<128;++note)
        if (processor.isSoundingNote(note)) result.push_back(note);
    return result;
}

void testSoundingFeedback()
{
    int bankIndex=-1,slotA=-1,slotB=-1;
    for (size_t bank=0;bank<SixthChords::banks().size() && bankIndex<0;++bank)
        for (int a=0;a<SixthChords::bankSize && bankIndex<0;++a)
            for (int b=a+1;b<SixthChords::bankSize;++b) {
                const auto first=pitches(SixthChords::resolve(static_cast<int>(bank),a,0,0));
                const auto second=pitches(SixthChords::resolve(static_cast<int>(bank),b,0,0));
                if (!subtract(first,second).empty() && !subtract(second,first).empty()
                    && subtract(first,second).size()<first.size()) {
                    bankIndex=static_cast<int>(bank);slotA=a;slotB=b;break;
                }
            }
    require(bankIndex>=0,"sounding-feedback test needs overlapping chord tones");
    const auto notesA=pitches(SixthChords::resolve(bankIndex,slotA,0,0));
    const auto notesB=pitches(SixthChords::resolve(bankIndex,slotB,0,0));
    const int triggerA=24+slotA,triggerB=24+slotB;
    SixthProcessor processor;
    setParameter(processor,"chordMode",1);setParameter(processor,"chordBank",static_cast<float>(bankIndex));
    setParameter(processor,"chordLayout",0);setParameter(processor,"attack",0.001f);
    setParameter(processor,"sustain",1);setParameter(processor,"release",0.02f);
    setParameter(processor,"chorus",0);setParameter(processor,"noise",0);setParameter(processor,"drift",0);
    processor.prepareToPlay(48000,256);
    const auto tick=[&](std::initializer_list<Event> input={}) {
        juce::MidiBuffer midi;
        for (const auto& event:input) midi.addEvent(event.message,event.position);
        juce::AudioBuffer<float> audio(2,256);
        processor.processBlock(audio,midi);
        for (int channel=0;channel<2;++channel)
            for (int sample=0;sample<audio.getNumSamples();++sample)
                require(std::isfinite(audio.getSample(channel,sample)),"sounding feedback accompanied non-finite audio");
    };
    tick({on(triggerA)});
    require(soundingPitches(processor)==notesA,"sounding-note feedback shows input keys instead of generated chord pitches");
    if (std::find(notesA.begin(),notesA.end(),triggerA)==notesA.end())
        require(!processor.isSoundingNote(triggerA),"a silent raw trigger key is marked as sounding");
    require(processor.chordGeneration.load()==1,"hardware trigger did not publish one feedback generation");
    tick({on(triggerA)});
    require(processor.chordGeneration.load()==2,"repeated hardware key did not refresh the same chord display");
    tick({on(triggerB)});
    auto combined=notesA;combined.insert(combined.end(),notesB.begin(),notesB.end());
    std::sort(combined.begin(),combined.end());combined.erase(std::unique(combined.begin(),combined.end()),combined.end());
    require(soundingPitches(processor)==combined,"overlapping chord voices are missing from sounding-key feedback");
    tick({off(triggerA)});
    for (int block=0;block<20;++block) tick();
    require(soundingPitches(processor)==notesB,"one released trigger cleared shared or still-held sounding chord tones");
    tick({off(triggerB)});
    for (int block=0;block<20;++block) tick();
    require(soundingPitches(processor).empty(),"released envelopes remain marked sounding after their tails end");

    tick({{juce::MidiMessage::controllerEvent(1,64,127),0},on(triggerA,1)});
    tick({off(triggerA)});
    for (int block=0;block<100;++block) tick();
    require(processor.activeChordSlots.load()==0 && soundingPitches(processor)==notesA,
            "sustain feedback follows physical key state instead of actual sounding voices");
    tick({{juce::MidiMessage::controllerEvent(1,64,0),0}});
    require(!soundingPitches(processor).empty(),"release tails disappeared immediately from sounding-key feedback");
    for (int block=0;block<20;++block) tick();
    require(soundingPitches(processor).empty(),"pedal-up release left stale sounding-key feedback");

    const auto beforeMelody=processor.chordGeneration.load();
    tick({on(23)});
    require(processor.isSoundingNote(23) && processor.activeChordSlots.load()==0
            && processor.chordGeneration.load()==beforeMelody,"outside-range melody feedback was converted to a chord");
    tick({off(23)});
    for (int block=0;block<20;++block) tick();
    tick({{juce::MidiMessage::controllerEvent(1,64,127),0},on(triggerA,1)});
    processor.requestPanic();tick();
    require(processor.activeChordSlots.load()==0 && soundingPitches(processor).empty(),
            "panic did not clear actual sounding-note feedback");
    require(!processor.isSoundingNote(-1) && !processor.isSoundingNote(128),"out-of-range sounding-note query is not safe");
    std::cout<<"PASS hardware/retrigger feedback, overlapping voices, sustain/release tails, melody and panic\n";
}

template <typename Predicate>
juce::Component* findComponent(juce::Component& root, const Predicate& predicate)
{
    if (predicate(root))
        return &root;
    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* found = findComponent(*root.getChildComponent(index), predicate))
            return found;
    return nullptr;
}

juce::TextButton* findViewButton(juce::Component& root, const juce::String& caption)
{
    return dynamic_cast<juce::TextButton*>(findComponent(root, [&](juce::Component& component)
    {
        const auto* button = dynamic_cast<const juce::TextButton*>(&component);
        return button != nullptr && button->getButtonText() == caption;
    }));
}

juce::MouseEvent leftClickEvent(juce::Component& component)
{
    const juce::Point<float> position(10.0f, 10.0f);
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position,
                            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component,
                            now, position, now, 1, false);
}

void testChordPadLifecycle()
{
    SixthProcessor processor;
    setParameter(processor,"attack",0.001f);
    setParameter(processor,"chorus",0);
    processor.prepareToPlay(48000,256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    require(editor != nullptr,"chord pad test could not construct the actual editor");
    auto* chordView=findViewButton(*editor,"CHORDS");
    auto* synthView=findViewButton(*editor,"SYNTH");
    require(chordView != nullptr && static_cast<bool>(chordView->onClick)
            && synthView != nullptr && static_cast<bool>(synthView->onClick),
            "synth/chord view navigation is unavailable");
    chordView->onClick();
    const auto findPad=[&](int trigger) {
        return findComponent(*editor,[&](juce::Component& component) {
            return component.getComponentID()=="chordPad"+juce::String(trigger);
        });
    };
    // No octave page selector is needed: all 49 mapped keys must be visible together.
    for (int trigger=SixthChords::firstTrigger;trigger<=SixthChords::lastTrigger;++trigger) {
        auto* pad=findPad(trigger);
        require(pad != nullptr && pad->isVisible(),
                "the complete chord grid is missing visible MIDI "+std::to_string(trigger));
        require(pad->getParentComponent() != nullptr
                && pad->getParentComponent()->getLocalBounds().contains(pad->getBounds()),
                "a supposedly visible chord pad is clipped outside its containing panel");
    }
    const auto renderBlock=[&] {
        juce::AudioBuffer<float> audio(2,256);
        juce::MidiBuffer emptyInput;
        processor.processBlock(audio,emptyInput);
        float peak=0.0f;
        for (int channel=0;channel<2;++channel)
            for (int sample=0;sample<audio.getNumSamples();++sample) {
                const auto value=audio.getSample(channel,sample);
                require(std::isfinite(value),"GUI chord pad produced non-finite audio");
                peak=std::max(peak,std::abs(value));
            }
        return peak;
    };
    // Test the low, middle and high endpoints through the components' public mouse API.
    for (int trigger:{24,48,72}) {
        auto* pad=findPad(trigger);
        const auto generation=processor.chordGeneration.load();
        pad->mouseDown(leftClickEvent(*pad));
        const auto peak=renderBlock();
        require(processor.parameters.getRawParameterValue("chordMode")->load()==1.0f
                && processor.activeChordSlots.load()==slotMask(trigger-24) && peak>1.0e-6f,
                "pressing a GUI pad did not enable chord mode and render its chord");
        require(processor.chordGeneration.load()==generation+1
                && processor.getChordDisplay().trigger==trigger,
                "GUI chord feedback did not identify the played key");
        pad->mouseUp(leftClickEvent(*pad));renderBlock();
        require(processor.activeChordSlots.load()==0,"releasing a GUI pad left a held chord");
    }
    auto* highPad=findPad(72);
    highPad->mouseDown(leftClickEvent(*highPad));renderBlock();
    require(processor.activeChordSlots.load()==slotMask(48),"the C5 pad did not retrigger before changing views");
    synthView->onClick();
    require(!highPad->isVisible(),"synth navigation did not hide the chord grid");
    renderBlock();
    require(processor.activeChordSlots.load()==0,"hiding the chord grid left its pressed C5 pad sounding");

    chordView->onClick();
    for (int trigger=24;trigger<=72;++trigger)
        require(findPad(trigger)->isVisible(),"returning to chords did not restore the complete grid");
    highPad->mouseDown(leftClickEvent(*highPad));renderBlock();
    require(processor.activeChordSlots.load()==slotMask(48),"the C5 pad did not trigger before editor destruction");
    editor.reset();renderBlock();
    require(processor.activeChordSlots.load()==0,"destroying an editor left its pressed chord pad held");
    std::cout<<"PASS all 49 visible GUI pads, C1/C3/C5 mouse controls, view hiding and editor destruction\n";
}

void testFixedKeyboardGeometry()
{
    SixthProcessor processor;
    setParameter(processor,"chordMode",1);
    setParameter(processor,"chordLayout",0);
    processor.prepareToPlay(48000,256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    require(editor != nullptr,"keyboard geometry test could not construct the editor");
    auto* keyboard=dynamic_cast<juce::MidiKeyboardComponent*>(findComponent(*editor,[](juce::Component& component) {
        return component.getComponentID()=="performanceKeyboard";
    }));
    auto* popup=findComponent(*editor,[](juce::Component& component) {
        return component.getComponentID()=="chordPopup";
    });
    require(keyboard != nullptr && popup != nullptr,"keyboard/popup components are unavailable for interaction testing");
    const auto keyWidth=keyboard->getKeyWidth();
    const auto lowest=keyboard->getLowestVisibleKey();
    const auto bounds=keyboard->getBounds();
    require(keyboard->getRangeStart()==24 && keyboard->getRangeEnd()==72 && lowest==24,
            "the performance keyboard is not fixed to C1-C5");
    require(keyWidth>=24.0f,"the fixed-range keyboard has not retained comfortably sized keys");
    for (int note:{24,72}) {
        const auto keyBounds=keyboard->getRectangleForKey(note);
        require(keyBounds.getX()>=-0.01f && keyBounds.getRight()<=keyboard->getWidth()+0.01f,
                "a C1/C5 endpoint is clipped out of the fixed keyboard");
    }
    const auto assertFixed=[&] {
        require(keyboard->getRangeStart()==24 && keyboard->getRangeEnd()==72
                && keyboard->getLowestVisibleKey()==lowest
                && std::abs(keyboard->getKeyWidth()-keyWidth)<0.0001f
                && keyboard->getBounds()==bounds,
                "MIDI notes or chord feedback automatically zoomed/scrolled the fixed keyboard");
    };
    const auto processInput=[&](std::initializer_list<Event> input) {
        juce::MidiBuffer midi;
        for (const auto& event:input) midi.addEvent(event.message,event.position);
        juce::AudioBuffer<float> audio(2,256);processor.processBlock(audio,midi);
    };
    const auto waitForPopup=[&](bool expectedVisible) {
        // Timer dispatch is asynchronous even when the test drives callbacks on
        // the message thread. Wait for observable behavior, not one guessed tick.
        const double deadline=juce::Time::getMillisecondCounterHiRes()+1000.0;
        do {
            juce::Timer::callPendingTimersSynchronously();
            assertFixed();
            if (popup->isVisible()==expectedVisible) return true;
            juce::Thread::sleep(10);
        } while (juce::Time::getMillisecondCounterHiRes()<deadline);
        juce::Timer::callPendingTimersSynchronously();
        assertFixed();
        return popup->isVisible()==expectedVisible;
    };
    for (const auto input:std::array<std::pair<int,int>,2>{{{0,24},{127,72}}}) {
        processInput({on(input.first),on(input.second,1)});
        require(waitForPopup(true),"the editor timer did not show feedback for a new mapped chord");
        assertFixed();
        processor.requestPanic();processInput({});
        require(waitForPopup(false),"panic did not clear the timed chord popup");
        assertFixed();
    }
    // A higher selected register changes the sounding pitches, not the keyboard viewport.
    setParameter(processor,"chordKey",11);setParameter(processor,"chordRegister",4);
    processInput({on(60)});
    require(waitForPopup(true),"transposed chord feedback did not run through the editor timer");
    assertFixed();
    processor.requestPanic();processInput({});
    require(waitForPopup(false),"panic did not clear transposed chord feedback");
    assertFixed();
    auto* bank=dynamic_cast<juce::ComboBox*>(findComponent(*editor,[](juce::Component& component) {
        return component.getComponentID()=="chordBank";
    }));
    require(bank != nullptr,"the harmony bank browser is unavailable");
    setParameter(processor,"chordLayout",1);
    bank->setSelectedId(2,juce::sendNotificationSync);
    require(processor.parameters.getRawParameterValue("chordBank")->load()==1
            && processor.parameters.getRawParameterValue("chordLayout")->load()==0,
            "selecting a new harmony bank retained the twelve-chord compatibility map");
    assertFixed();
    std::cout<<"PASS fixed C1-C5 keyboard geometry during low/high MIDI, transposed chords, popups and bank selection\n";
}

void testProcessorIntegration()
{
    SixthProcessor chordProcessor, explicitProcessor;
    for (auto* processor : {&chordProcessor, &explicitProcessor})
    {
        processor->setCurrentProgram(0);
        setParameter(*processor, "drift", 0);
        setParameter(*processor, "noise", 0);
        setParameter(*processor, "chorus", 0);
    }
    // Chord mode applies a fixed 0.65 headroom gain before the shared linear FX.
    // Match that gain on the explicit-note reference before its final tanh stage.
    setParameter(explicitProcessor, "volume", 0.7f * 0.65f);
    require(chordProcessor.parameters.getRawParameterValue("chordLayout")->load() == 0, "new sessions do not default to the full 49-key map");
    setParameter(chordProcessor, "chordMode", 1);
    setParameter(chordProcessor, "chordBank", 1);
    setParameter(chordProcessor, "chordKey", 5);
    setParameter(chordProcessor, "chordVoicing", 2);
    setParameter(chordProcessor, "chordRegister", 2); // neutral register choice
    setParameter(chordProcessor, "chordLayout", 0);
    chordProcessor.prepareToPlay(48000, 256);
    explicitProcessor.prepareToPlay(48000, 256);
    const auto notes = SixthChords::resolve(1, 49-SixthChords::firstTrigger, 5, 2, 0);
    juce::MidiBuffer trigger, expanded;
    trigger.addEvent(on(49, 17, 1, 81).message, 17);
    for (int note : pitches(notes))
        expanded.addEvent(on(note, 17, 1, 81).message, 17);
    double errorEnergy = 0, signalEnergy = 0;
    for (int block = 0; block < 40; ++block)
    {
        juce::AudioBuffer<float> chordAudio(2,256), explicitAudio(2,256);
        chordAudio.clear(); explicitAudio.clear();
        chordProcessor.processBlock(chordAudio, trigger);
        explicitProcessor.processBlock(explicitAudio, expanded);
        trigger.clear(); expanded.clear();
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 256; ++sample)
            {
                const auto actual = chordAudio.getSample(channel,sample);
                const auto expected = explicitAudio.getSample(channel,sample);
                require(std::isfinite(actual) && std::isfinite(expected), "mapped processor emitted non-finite audio");
                signalEnergy += static_cast<double>(expected) * expected;
                errorEnergy += static_cast<double>(actual - expected) * (actual - expected);
            }
    }
    require(signalEnergy > 1.0e-5 && errorEnergy < signalEnergy * 1.0e-6,
            "mapped processor audio differs from the corresponding explicit MIDI chord");
    require(chordProcessor.activeChordSlots.load() == slotMask(49-SixthChords::firstTrigger), "processor did not publish held chord state");
    trigger.addEvent(off(49,33).message,33);
    juce::AudioBuffer<float> releaseAudio(2,256);
    chordProcessor.processBlock(releaseAudio,trigger);
    require(chordProcessor.activeChordSlots.load() == 0, "processor key release left a held chord");

    trigger.clear(); trigger.addEvent(on(50).message,0);
    chordProcessor.processBlock(releaseAudio,trigger);
    require(chordProcessor.activeChordSlots.load() != 0, "panic integration test did not start a chord");
    chordProcessor.requestPanic();
    trigger.clear(); chordProcessor.processBlock(releaseAudio,trigger);
    require(chordProcessor.activeChordSlots.load() == 0 && chordProcessor.lastChordSlot.load() == -1,
            "processor panic did not reset the held chord/display state");

    setParameter(chordProcessor,"chordBank",static_cast<float>(SixthChords::banks().size()-1));
    setParameter(chordProcessor,"chordKey",7); setParameter(chordProcessor,"chordVoicing",4);
    setParameter(chordProcessor,"chordRegister",3);
    setParameter(chordProcessor,"chordLayout",1);
    chordProcessor.setCurrentProgram(1);
    require(chordProcessor.parameters.getRawParameterValue("chordMode")->load() == 1
            && chordProcessor.parameters.getRawParameterValue("chordBank")->load() == static_cast<float>(SixthChords::banks().size()-1)
            && chordProcessor.parameters.getRawParameterValue("chordKey")->load() == 7
            && chordProcessor.parameters.getRawParameterValue("chordVoicing")->load() == 4
            && chordProcessor.parameters.getRawParameterValue("chordRegister")->load() == 3
            && chordProcessor.parameters.getRawParameterValue("chordLayout")->load() == 1,
            "loading a synth sound reset the independent chord-performance preset");
    require(!chordProcessor.isCurrentProgramModified(), "chord settings incorrectly mark a factory synth sound edited");
    setParameter(chordProcessor,"chordLayout",0);
    juce::MemoryBlock state;
    chordProcessor.getStateInformation(state);
    SixthProcessor restored;
    setParameter(restored,"chordLayout",1);
    restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    for (const char* id : {"chordMode","chordBank","chordKey","chordVoicing","chordRegister","chordLayout"})
        require(restored.parameters.getRawParameterValue(id)->load() == chordProcessor.parameters.getRawParameterValue(id)->load(),
                std::string("host state did not restore ") + id);
    require(restored.getCurrentProgram() == 1, "host state did not restore the synth sound program");
    const auto& all = restored.getParameters();
    require(all.size() >= 44, "expanded chord parameters are not exposed to the host");
    for (int index = 0; index < 38; ++index)
        require(all[index]->getVersionHint() == (index < 19 ? 1 : 2),
                "published synth parameter version hints changed");
    const char* originalChordIds[]={"chordMode","chordBank","chordKey","chordVoicing","chordRegister"};
    for (int index = 38; index < 43; ++index) {
        const auto* parameter=dynamic_cast<const juce::RangedAudioParameter*>(all[index]);
        require(parameter != nullptr && parameter->paramID==originalChordIds[index-38]
                && parameter->getVersionHint()==3, "published chord IDs/order/version hints changed");
    }
    require(restored.parameters.getParameter("chordLayout")->getVersionHint()==4,
            "the new keyboard layout would reorder existing AU automation");
    const auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
    require(xml != nullptr,"could not inspect host state for layout migration regression");
    // Earlier releases either lacked the field or persisted an automatic Legacy
    // selection. Both must reopen the visible full map after this UI correction.
    for (int oldSchema:{3,4}) {
        auto oldTree=juce::ValueTree::fromXml(*xml);
        oldTree.setProperty("schema",oldSchema,nullptr);
        auto layoutChild=oldTree.getChildWithProperty("id","chordLayout");
        if (oldSchema==3) oldTree.removeChild(layoutChild,nullptr);
        else layoutChild.setProperty("value",1.0f,nullptr);
        juce::MemoryBlock oldState;
        juce::AudioProcessor::copyXmlToBinary(*oldTree.createXml(),oldState);
        SixthProcessor migrated;
        migrated.setStateInformation(oldState.getData(),static_cast<int>(oldState.getSize()));
        require(migrated.parameters.getRawParameterValue("chordLayout")->load()==0,
                "an earlier saved session reopened the hidden twelve-pad layout");
        migrated.prepareToPlay(48000,256);
        juce::MidiBuffer migratedTrigger;migratedTrigger.addEvent(on(60).message,0);
        juce::AudioBuffer<float> migratedAudio(2,256);
        migrated.processBlock(migratedAudio,migratedTrigger);
        const auto displayed=migrated.getChordDisplay();
        const auto expected=SixthChords::resolve(static_cast<int>(SixthChords::banks().size()-1),36,7,4,1);
        require(displayed.layout==0 && displayed.slot==36 && displayed.trigger==60
                && pitches(displayed.notes)==pitches(expected),
                "a migrated session did not actually render the full-map chord for its key");
    }
    // An intentional Legacy selection made after this correction remains valid.
    setParameter(chordProcessor,"chordLayout",1);
    juce::MemoryBlock intentionalLegacy;
    chordProcessor.getStateInformation(intentionalLegacy);
    const auto currentXml=juce::AudioProcessor::getXmlFromBinary(intentionalLegacy.getData(),static_cast<int>(intentionalLegacy.getSize()));
    require(currentXml != nullptr && currentXml->getIntAttribute("schema")==5,
            "current host state does not identify the corrected layout schema");
    SixthProcessor legacy;
    legacy.setStateInformation(intentionalLegacy.getData(),static_cast<int>(intentionalLegacy.getSize()));
    require(legacy.parameters.getRawParameterValue("chordLayout")->load()==1,
            "a current explicit Legacy selection was not preserved");
    legacy.prepareToPlay(48000,256);
    juce::MidiBuffer legacyTrigger;legacyTrigger.addEvent(on(60).message,0);
    juce::AudioBuffer<float> legacyAudio(2,256);
    legacy.processBlock(legacyAudio,legacyTrigger);
    const auto legacyDisplay=legacy.getChordDisplay();
    const auto legacyExpected=SixthChords::resolveLegacy(static_cast<int>(SixthChords::banks().size()-1),0,7,4,2);
    require(legacyDisplay.layout==1 && legacyDisplay.slot==0 && legacyDisplay.trigger==60
            && pitches(legacyDisplay.notes)==pitches(legacyExpected),
            "an explicit current Legacy state did not render its intended octave mapping");
    std::cout<<"PASS processor chord rendering, independent presets, Full49 migration and explicit Legacy recall\n";
}
}

void runChordTests()
{
    testLibrary();
    testKeyboardMapping();
    testOwnership();
    testModesAndControllers();
    testProcessorIntegration();
    testSoundingFeedback();
    testChordPadLifecycle();
    testFixedKeyboardGeometry();
}

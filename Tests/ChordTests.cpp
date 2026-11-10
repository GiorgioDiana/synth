#include "ChordTests.h"
#include "ChordLibrary.h"
#include "ChordMapper.h"
#include "PluginProcessor.h"
#include <algorithm>
#include <array>
#include <cmath>
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
    require(banks.size() >= 48, "the requested large chord-bank library is missing");
    std::set<std::string> bankNames, palettes;
    bool alternateVoicing = false;
    bool variedChordQualities = false;
    size_t chordCount = 0;
    for (size_t bankIndex = 0; bankIndex < banks.size(); ++bankIndex)
    {
        const auto& bank = banks[bankIndex];
        const auto nonempty = [](const char* value) { return value != nullptr && *value != '\0'; };
        require(nonempty(bank.name) && nonempty(bank.category) && nonempty(bank.description),
                "a bank is missing its name, category or musical description");
        require(bankNames.insert(bank.name).second, "duplicate bank name: " + std::string(bank.name));
        std::ostringstream palette;
        std::set<std::string> intervalShapes;
        for (int slot = 0; slot < SixthChords::bankSize; ++slot)
        {
            ++chordCount;
            const auto& chord = bank.chords[static_cast<size_t>(slot)];
            require(nonempty(chord.name), std::string(bank.name) + " has an unnamed chord");
            const auto context = std::string(bank.name) + " / " + chord.name;
            const auto written = SixthChords::resolve(static_cast<int>(bankIndex), slot, 0, 0);
            validateNotes(written, chord.size, context);
            auto expected = std::vector<int>(chord.notes.begin(), chord.notes.begin() + chord.size);
            std::sort(expected.begin(), expected.end());
            require(pitches(written) == expected, context + " does not preserve the written voicing");
            std::ostringstream shape;
            for (int note : expected)
            {
                palette << note << ',';
                shape << (note - expected.front()) % 12 << ',';
            }
            palette << ';';
            intervalShapes.insert(shape.str());
            for (int key = 0; key < 12; ++key)
                for (int voicing = 0; voicing < SixthChords::voicingCount; ++voicing)
                    for (int octave : {-6, -2, 0, 2, 8})
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
                            context + " ignores the octave register");
        }
        require(palettes.insert(palette.str()).second,
                std::string(bank.name) + " duplicates an entire existing written palette");
        variedChordQualities = variedChordQualities || intervalShapes.size() > 1;
    }
    require(alternateVoicing, "the voicing selector has no effect on the library");
    require(variedChordQualities, "banks only transpose one fixed chord shape");
    for (int voicing = 0; voicing < SixthChords::voicingCount; ++voicing)
        require(SixthChords::voicingName(voicing) != nullptr && *SixthChords::voicingName(voicing) != '\0',
                "a voicing choice is unnamed");
    require(SixthChords::resolve(-1, 0, 0, 0).size == 0
            && SixthChords::resolve(static_cast<int>(banks.size()), 0, 0, 0).size == 0
            && SixthChords::resolve(0, -1, 0, 0).size == 0
            && SixthChords::resolve(0, 12, 0, 0).size == 0,
            "invalid library indices did not return an empty chord");
    std::cout << "PASS chord library: " << banks.size() << " distinct banks, " << chordCount
              << " named slots, all keys/voicings/registers valid\n";
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

void testKeyboardMapping()
{
    Mapper mapper;
    mapper.prepare();
    auto mapping = enabledMapping();
    for (int slot = 0; slot < 12; ++slot)
    {
        const auto written = SixthChords::resolve(mapping.bank, slot, 0, 0);
        expectNotes(process(mapper, mapping, {on(48 + slot, 17, 3, 81)}), pitches(written), true, 3, 17, 81);
        require(mapper.lastSlot() == slot && mapper.lastTrigger() == 48 + slot
                && mapper.activeSlots() == (1 << slot), "active/last chord display state is incorrect");
        expectNotes(process(mapper, mapping, {off(48 + slot, 61, 3)}), pitches(written), false, 3, 61, 31);
        require(mapper.activeSlots() == 0, "released key remains marked active");
    }
    mapping.key = 7;
    mapping.voicing = 3;
    mapping.registerShift = -1;
    for (int note : {0, 36, 48, 60, 72, 127})
    {
        const auto expected = SixthChords::resolve(mapping.bank, note % 12, mapping.key,
                                                  mapping.voicing, note / 12 - 4 + mapping.registerShift);
        expectNotes(process(mapper, mapping, {on(note, 9, 16)}), pitches(expected), true, 16, 9, 93);
        expectNotes(process(mapper, mapping, {off(note, 127, 16)}), pitches(expected), false, 16, 127, 31);
    }
    std::cout << "PASS 12-key chord mapping, velocity/channel/timestamps, key/register/voicing and MIDI edges\n";
}

void testOwnership()
{
    int selectedBank = -1, slotA = -1, slotB = -1;
    for (size_t bank = 0; bank < SixthChords::banks().size() && selectedBank < 0; ++bank)
        for (int a = 0; a < 12 && selectedBank < 0; ++a)
            for (int b = a + 1; b < 12; ++b)
            {
                const auto notesA = pitches(SixthChords::resolve(static_cast<int>(bank), a, 0, 0));
                const auto notesB = pitches(SixthChords::resolve(static_cast<int>(bank), b, 0, 0));
                if (subtract(notesA, notesB).size() < notesA.size()
                    && !subtract(notesA, notesB).empty() && !subtract(notesB, notesA).empty())
                {
                    selectedBank = static_cast<int>(bank); slotA = a; slotB = b; break;
                }
            }
    require(selectedBank >= 0, "no pair of palette chords shares a common tone");
    auto mapping = enabledMapping(selectedBank);
    const auto a = pitches(SixthChords::resolve(selectedBank, slotA, 0, 0));
    const auto b = pitches(SixthChords::resolve(selectedBank, slotB, 0, 0));
    Mapper mapper;
    mapper.prepare();
    expectNotes(process(mapper, mapping, {on(48 + slotA)}), a, true, 1, 0, 93);
    expectNotes(process(mapper, mapping, {on(48 + slotB, 5)}), subtract(b, a), true, 1, 5, 93);
    require(mapper.activeSlots() == ((1 << slotA) | (1 << slotB)), "overlapping chords lost their held slots");
    expectNotes(process(mapper, mapping, {off(48 + slotA, 14)}), subtract(a, b), false, 1, 14, 31);
    expectNotes(process(mapper, mapping, {off(48 + slotB, 31)}), b, false, 1, 31, 31);
    require(mapper.activeSlots() == 0, "common-tone ownership left a stuck chord");

    const auto original = pitches(SixthChords::resolve(selectedBank, slotA, 0, 0));
    expectNotes(process(mapper, mapping, {on(48 + slotA)}), original, true, 1, 0, 93);
    mapping.bank = (mapping.bank + 1) % static_cast<int>(SixthChords::banks().size());
    mapping.key = 5; mapping.voicing = 4; mapping.registerShift = 1;
    require(events(process(mapper, mapping)).empty(), "bank changes unexpectedly rewrote held notes");
    expectNotes(process(mapper, mapping, {off(48 + slotA, 47)}), original, false, 1, 47, 31);
    const auto changed = pitches(SixthChords::resolve(mapping.bank, slotA, mapping.key, mapping.voicing, mapping.registerShift));
    expectNotes(process(mapper, mapping, {on(48 + slotA, 7)}), changed, true, 1, 7, 93);
    const auto repeated = events(process(mapper, mapping, {on(48 + slotA, 13)}));
    require(repeated.size() == changed.size() * 2, "repeated key did not replace its previous ownership");
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
    expectNotes(process(mapper, mapping, {off(48 + slotA, 19)}), changed, false, 1, 19, 31);
    std::cout << "PASS shared-note ownership, held-bank changes and repeated-key retriggering\n";
}

void testModesAndControllers()
{
    Mapper mapper;
    mapper.prepare();
    Mapping mapping;
    expectNotes(process(mapper, mapping, {on(60, 4)}), {60}, true, 1, 4, 93);
    mapping.enabled = true;
    expectNotes(process(mapper, mapping), {60}, false, 1, 0, 0);
    require(events(process(mapper, mapping, {off(60, 15)})).empty(), "stale raw note-off cut a chord after enabling mode");
    const auto chord = pitches(SixthChords::resolve(0, 0, 0, 0));
    expectNotes(process(mapper, mapping, {on(48, 7)}), chord, true, 1, 7, 93);
    mapping.enabled = false;
    expectNotes(process(mapper, mapping), chord, false, 1, 0, 0);
    require(mapper.activeSlots() == 0, "bypassing chord mode did not clear held chord display");

    mapping.enabled = true;
    process(mapper, mapping, {on(48, 0, 1), on(48, 1, 2)});
    expectNotes(process(mapper, mapping, {off(48, 5, 1)}), chord, false, 1, 5, 31);
    require(mapper.activeSlots() == 1, "one channel released another channel's chord");
    const auto allOff = events(process(mapper, mapping, {{juce::MidiMessage::allNotesOff(2), 9}}));
    require(allOff.size() == chord.size() + 1, "all-notes-off failed to flush channel ownership");
    require(allOff.back().message.isControllerOfType(123) && allOff.back().position == 9,
            "all-notes-off controller was lost or retimed");
    for (size_t i = 0; i < chord.size(); ++i)
        require(allOff[i].message.isNoteOff() && allOff[i].message.getChannel() == 2,
                "all-notes-off affected the wrong MIDI channel");
    require(mapper.activeSlots() == 0, "all-notes-off left active chord ownership");
    process(mapper, mapping, {on(48, 0, 4)});
    const auto soundOff = events(process(mapper, mapping, {{juce::MidiMessage::allSoundOff(4), 12}}));
    require(soundOff.size() == chord.size() + 1 && soundOff.back().message.isControllerOfType(120),
            "all-sound-off did not flush chord ownership and pass through");

    const auto sustain = juce::MidiMessage::controllerEvent(7, 64, 127);
    const auto pitch = juce::MidiMessage::pitchWheel(7, 10000);
    const auto mod = juce::MidiMessage::controllerEvent(7, 1, 85);
    const auto sustainOff = juce::MidiMessage::controllerEvent(7, 64, 0);
    const auto forwarded = events(process(mapper, mapping, {{sustain, 3}, {pitch, 11}, {mod, 17}, {sustainOff, 29}}));
    const std::array<juce::MidiMessage,4> expected {sustain, pitch, mod, sustainOff};
    require(forwarded.size() == expected.size(), "controller passthrough added or removed events");
    for (size_t i = 0; i < expected.size(); ++i)
    {
        require(forwarded[i].message.getRawDataSize() == expected[i].getRawDataSize()
                && std::equal(expected[i].getRawData(), expected[i].getRawData() + expected[i].getRawDataSize(),
                              forwarded[i].message.getRawData()), "controller bytes were altered");
        require(forwarded[i].position == std::array<int,4>{3,11,17,29}[i], "controller timestamp changed");
    }
    process(mapper, mapping, {on(48, 0, 1), on(49, 2, 2)});
    juce::MidiBuffer resetOutput;
    mapper.reset(resetOutput, 23);
    require(!events(resetOutput).empty() && mapper.activeSlots() == 0
            && mapper.lastSlot() == -1 && mapper.lastTrigger() == -1, "panic reset did not clear mapper state");
    for (const auto& event : events(resetOutput))
        require(event.message.isNoteOff() && event.position == 23, "panic reset lost note-off timing");
    require(events(process(mapper, mapping, {off(48), off(49, 1, 2)})).empty(),
            "stale note-offs survived mapper panic");
    std::cout << "PASS mode changes, MIDI channel isolation, sustain/pitch/mod controllers and panic\n";
}

void setParameter(SixthProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter(id);
    require(parameter != nullptr, std::string("missing chord parameter ") + id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
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
    setParameter(processor, "attack", 0.001f);
    setParameter(processor, "chorus", 0);
    processor.prepareToPlay(48000, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    require(editor != nullptr, "chord pad test could not construct the actual editor");
    auto* chordView = findViewButton(*editor, "CHORDS");
    auto* synthView = findViewButton(*editor, "SYNTH");
    require(chordView != nullptr && static_cast<bool>(chordView->onClick)
            && synthView != nullptr && static_cast<bool>(synthView->onClick),
            "synth/chord page navigation is unavailable");
    chordView->onClick();
    auto* pad = findComponent(*editor, [](juce::Component& component)
    {
        return component.getName() == "Chord pad C3";
    });
    require(pad != nullptr && pad->isVisible(), "the C3 chord pad is not visible on the chord page");
    const auto mouse = leftClickEvent(*pad);
    const auto renderBlock = [&]
    {
        juce::AudioBuffer<float> audio(2,256);
        juce::MidiBuffer emptyInput;
        processor.processBlock(audio,emptyInput);
        float peak = 0.0f;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const auto value = audio.getSample(channel,sample);
                require(std::isfinite(value), "GUI chord pad produced non-finite audio");
                peak = std::max(peak,std::abs(value));
            }
        return peak;
    };
    // Exercise the component's virtual mouse API rather than calling editor-private helpers.
    pad->mouseDown(mouse);
    const auto peak = renderBlock();
    require(processor.parameters.getRawParameterValue("chordMode")->load() == 1.0f
            && processor.activeChordSlots.load() == 1 && peak > 1.0e-6f,
            "pressing a GUI pad did not enable chord mode and render its chord");
    pad->mouseUp(mouse);
    renderBlock();
    require(processor.activeChordSlots.load() == 0, "releasing a GUI pad left a held chord");

    pad->mouseDown(mouse);
    renderBlock();
    require(processor.activeChordSlots.load() == 1, "GUI pad did not retrigger before page switch");
    synthView->onClick();
    require(!pad->isVisible(), "synth navigation did not hide the chord pads");
    renderBlock();
    require(processor.activeChordSlots.load() == 0, "hiding the chord page left its pressed pad sounding");

    chordView->onClick();
    require(pad->isVisible(), "chord navigation did not restore the chord pads");
    pad->mouseDown(mouse);
    renderBlock();
    require(processor.activeChordSlots.load() == 1, "GUI pad did not trigger before editor destruction");
    editor.reset();
    renderBlock();
    require(processor.activeChordSlots.load() == 0, "destroying an editor left its pressed chord pad held");
    std::cout << "PASS actual GUI chord-pad mouse down/up, page hiding and editor-destruction lifetimes\n";
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
    setParameter(chordProcessor, "chordMode", 1);
    setParameter(chordProcessor, "chordBank", 1);
    setParameter(chordProcessor, "chordKey", 5);
    setParameter(chordProcessor, "chordVoicing", 2);
    setParameter(chordProcessor, "chordRegister", 2); // neutral register choice
    chordProcessor.prepareToPlay(48000, 256);
    explicitProcessor.prepareToPlay(48000, 256);
    const auto notes = SixthChords::resolve(1, 1, 5, 2, 0);
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
    require(chordProcessor.activeChordSlots.load() == (1 << 1), "processor did not publish held chord state");
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
    chordProcessor.setCurrentProgram(1);
    require(chordProcessor.parameters.getRawParameterValue("chordMode")->load() == 1
            && chordProcessor.parameters.getRawParameterValue("chordBank")->load() == static_cast<float>(SixthChords::banks().size()-1)
            && chordProcessor.parameters.getRawParameterValue("chordKey")->load() == 7
            && chordProcessor.parameters.getRawParameterValue("chordVoicing")->load() == 4
            && chordProcessor.parameters.getRawParameterValue("chordRegister")->load() == 3,
            "loading a synth sound reset the independent chord-performance preset");
    require(!chordProcessor.isCurrentProgramModified(), "chord settings incorrectly mark a factory synth sound edited");
    juce::MemoryBlock state;
    chordProcessor.getStateInformation(state);
    SixthProcessor restored;
    restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    for (const char* id : {"chordMode","chordBank","chordKey","chordVoicing","chordRegister"})
        require(restored.parameters.getRawParameterValue(id)->load() == chordProcessor.parameters.getRawParameterValue(id)->load(),
                std::string("host state did not restore ") + id);
    require(restored.getCurrentProgram() == 1, "host state did not restore the synth sound program");
    const auto& all = restored.getParameters();
    require(all.size() >= 43, "chord parameters are not exposed to the host");
    for (int index = 0; index < 38; ++index)
        require(all[index]->getVersionHint() == (index < 19 ? 1 : 2),
                "published synth parameter version hints changed");
    for (int index = 38; index < all.size(); ++index)
        require(all[index]->getVersionHint() >= 3, "new chord controls would reorder existing AU automation");
    std::cout << "PASS actual processor chord rendering, independent sound/chord presets and host-state recall\n";
}
}

void runChordTests()
{
    testLibrary();
    testKeyboardMapping();
    testOwnership();
    testModesAndControllers();
    testProcessorIntegration();
    testChordPadLifecycle();
}

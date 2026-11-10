#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "ChordLibrary.h"
#include "ChordMapper.h"
#include <array>
#include <atomic>

class SixthProcessor final : public juce::AudioProcessor
{
public:
    SixthProcessor();
    const juce::String getName() const override { return "Sixth"; }
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.5; }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return selectedProgram.load(); }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    bool isCurrentProgramModified() const;
    struct ChordDisplay {
        int slot=-1,trigger=-1,bank=0,key=0,voicing=0,registerShift=0;
        SixthChords::Notes notes;
    };
    ChordDisplay getChordDisplay() const noexcept;
    void requestPanic() noexcept { panicPending.store(true,std::memory_order_release); }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> outputLevel {0.0f};
    std::atomic<int> lastChordSlot {-1},lastChordTrigger {-1},activeChordSlots {0};
private:
    class PerformanceSynth final : public juce::Synthesiser {
    public:
        std::array<float,16> wheels {};
        void handleController(int channel, int number, int val) override {
            if(channel>=1 && channel<=16 && number==1) wheels[static_cast<size_t>(channel-1)]=val/127.0f;
            if(channel>=1 && channel<=16 && number==121) wheels[static_cast<size_t>(channel-1)]=0.0f;
            juce::Synthesiser::handleController(channel,number,val);
        }
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    float value(const char* id) const { return parameters.getRawParameterValue(id)->load(); }
    void publishChordDisplay() noexcept;
    PerformanceSynth synth;
    SixthChords::Mapper chordMapper;
    juce::MidiBuffer chordMidi;
    bool previousChordMode=false;
    std::atomic<bool> panicPending {false};
    std::atomic<unsigned> chordDisplaySequence {0};
    std::array<std::atomic<int>,13> chordDisplayValues {};
    std::array<juce::dsp::Chorus<float>,2> chorus;
    juce::dsp::StateVariableTPTFilter<float> highpass;
    juce::SmoothedValue<float> gain;
    std::atomic<int> selectedProgram {0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SixthProcessor)
};

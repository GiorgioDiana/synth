#pragma once
#include "PluginProcessor.h"
class SixthEditor final : public juce::AudioProcessorEditor {
public:
    explicit SixthEditor(SixthProcessor&);
    ~SixthEditor() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Control {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::vector<std::unique_ptr<Control>> controls;
    juce::ComboBox chorus;
    juce::Label chorusLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chorusAttachment;
    juce::MidiKeyboardComponent keyboard;
    juce::LookAndFeel_V4 look;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SixthEditor)
};

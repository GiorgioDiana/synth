#pragma once
#include "PluginProcessor.h"

class SixthEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SixthEditor(SixthProcessor&);
    ~SixthEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class HardwareLook final : public juce::LookAndFeel_V4
    {
    public:
        HardwareLook();
        void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                              juce::Slider::SliderStyle, juce::Slider&) override;
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float,
                              juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
        juce::Font getTextButtonFont(juce::TextButton&, int) override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
        juce::Font getLabelFont(juce::Label&) override;
    };
    class Panel final : public juce::Component
    {
    public:
        explicit Panel(SixthEditor& e) : owner(e) {}
        void paint(juce::Graphics& g) override { owner.paintPanel(g); }
    private:
        SixthEditor& owner;
    };
    struct Control
    {
        juce::String id;
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void timerCallback() override;
    void paintPanel(juce::Graphics&);
    void paintFilter(juce::Graphics&, juce::Rectangle<float>);
    void paintEnvelope(juce::Graphics&, juce::Rectangle<float>, const char* prefix);
    void addControl(const char*, const char*, const char*, juce::Rectangle<int>, bool rotary = true, bool horizontal = false);
    void chooseProgram(int);
    void chooseParameter(const char*, int);
    float read(const char*) const;
    static juce::String formatValue(const juce::String&, double);
    static double parseValue(const juce::String&, const juce::String&);

    SixthProcessor& processor;
    HardwareLook look;
    Panel panel {*this};
    std::vector<std::unique_ptr<Control>> controls;
    juce::ComboBox presets, lfoShape;
    juce::TextButton previous {"<"}, next {">"}, initialise {"INIT"}, retrigger {"KEY TRIG"};
    std::array<juce::TextButton, 3> octaveButtons, chorusButtons;
    juce::Label presetLabel, filterDisplayLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoShapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> retriggerAttachment;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips {this, 650};
    float meter = 0.0f;
    bool midiActive = false;
    static constexpr int designWidth = 1200, designHeight = 800;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SixthEditor)
};

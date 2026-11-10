#pragma once
#include "PluginProcessor.h"
#include "ChordLibrary.h"

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
    class ChordPad final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ChordPad(SixthEditor& e, int s) : owner(e), slot(s) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
        ~ChordPad() override { release(); }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override { release(); }
        void release();
        bool held = false;
    private:
        SixthEditor& owner;
        int slot;
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
    void showChordView(bool);
    void buildChordControls();
    void rebuildChordBanks(bool followCurrent);
    void paintChordPage(juce::Graphics&);
    void paintChordPad(juce::Graphics&, int, juce::Rectangle<int>, bool);
    void playChordPad(int, bool);
    void releaseChordPads();
    juce::String chordNotes(const SixthChords::Notes&) const;
    static juce::String inputNoteName(int);
    int currentChordBank() const;

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
    juce::TextButton synthView {"SYNTH"}, chordView {"CHORDS"};
    juce::TextButton chordEnable {"CHORD MODE OFF"}, panic {"PANIC"};
    juce::ComboBox chordCategory, chordBanks, chordKey, chordVoicing, chordRegister;
    juce::StringArray chordCategories;
    std::array<std::unique_ptr<ChordPad>, 12> chordPads;
    std::vector<juce::Component*> synthComponents, chordComponents;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chordEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chordKeyAttachment, chordVoicingAttachment, chordRegisterAttachment;
    bool chordPageVisible = false;
    int lastDisplayedBank = -1;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips {this, 650};
    float meter = 0.0f;
    bool midiActive = false;
    static constexpr int designWidth = 1200, designHeight = 800;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SixthEditor)
};

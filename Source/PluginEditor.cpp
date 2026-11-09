#include "PluginEditor.h"
SixthEditor::SixthEditor(SixthProcessor& p) : AudioProcessorEditor(p),
    keyboard(p.keyboard,juce::MidiKeyboardComponent::horizontalKeyboard) {
    look.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff64d8c6));
    look.setColour(juce::Slider::thumbColourId,juce::Colour(0xfff0be72));
    look.setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffe8eeee));
    setLookAndFeel(&look);
    const char* ids[]={"saw","pulse","sub","noise","width","rate","pitch","pwm","cutoff","resonance","env","key","attack","decay","sustain","release","hpf","volume"};
    for(const auto* id:ids) {
        auto c=std::make_unique<Control>();
        c->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        c->slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,19);
        c->slider.setNumDecimalPlacesToDisplay(2);
        c->label.setText(p.parameters.getParameter(id)->getName(40),juce::dontSendNotification);
        c->label.setJustificationType(juce::Justification::centred);
        c->label.setColour(juce::Label::textColourId,juce::Colour(0xffd3dddd));
        addAndMakeVisible(c->slider); addAndMakeVisible(c->label);
        c->attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,id,c->slider);
        controls.push_back(std::move(c));
    }
    chorus.addItemList({"Off","I","II"},1);
    chorusAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"chorus",chorus);
    chorusLabel.setText("CHORUS",juce::dontSendNotification); addAndMakeVisible(chorusLabel); addAndMakeVisible(chorus);
    addAndMakeVisible(keyboard); keyboard.setAvailableRange(36,96); keyboard.setKeyWidth(26.0f); keyboard.setLowestVisibleKey(48);
    setSize(960,540);
}
void SixthEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff171e25));
    g.setColour(juce::Colour(0xfff0be72)); g.setFont(juce::FontOptions(32.0f));
    g.drawText("S I X T H",24,12,300,46,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffa6b7bf)); g.setFont(juce::FontOptions(14.0f));
    g.drawText("SIX VOICES  /  ANALOG-INSPIRED SYNTHESIZER",360,20,570,30,juce::Justification::centredRight);
    g.setColour(juce::Colour(0xff64d8c6)); g.fillRect(24,66,getWidth()-48,2);
}
void SixthEditor::resized() {
    const int width=(getWidth()-40)/6;
    for(size_t i=0;i<controls.size();++i) {
        const int x=20+static_cast<int>(i%6)*width,y=78+static_cast<int>(i/6)*116;
        controls[i]->label.setBounds(x,y,width,22); controls[i]->slider.setBounds(x+8,y+22,width-16,92);
    }
    chorusLabel.setBounds(28,432,90,26); chorus.setBounds(120,432,140,26);
    keyboard.setBounds(24,472,getWidth()-48,56);
}

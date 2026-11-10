#include "PluginEditor.h"
#include <cmath>

namespace
{
const juce::Colour background(0xff10151d), panelColour(0xff1b232e), border(0xff2d3a49);
const juce::Colour cream(0xffe5edf5), muted(0xff8b9caf), red(0xff70dec4), mint(0xff70dec4);
const juce::Colour blue(0xff78adff), violet(0xffb39af5), amber(0xfff2bb78);
void text(juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, float size,
          juce::Colour colour, juce::Justification alignment = juce::Justification::centredLeft, bool bold = false)
{
    g.setColour(colour); g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(s, r, alignment, true);
}
void section(juce::Graphics& g, juce::Rectangle<int> bounds, const char* title, const char* subtitle)
{
    auto f = bounds.toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.22f)); g.fillRoundedRectangle(f.translated(0, 3), 10);
    g.setColour(panelColour); g.fillRoundedRectangle(f, 10);
    g.setColour(border); g.drawRoundedRectangle(f.reduced(0.5f), 10, 1);
    g.setColour(mint.withAlpha(0.8f)); g.fillRoundedRectangle(static_cast<float>(bounds.getX()+13), static_cast<float>(bounds.getY()+15), 4.0f, 10.0f, 2.0f);
    text(g, title, {bounds.getX()+24, bounds.getY()+8, bounds.getWidth()-36, 24}, 13, cream, juce::Justification::centredLeft, true);
    text(g, subtitle, {bounds.getX()+15, bounds.getY()+31, bounds.getWidth()-30, 17}, 9, muted);
}
bool isTime(const juce::String& id) { return id.contains("attack") || id.contains("decay") || id.contains("release") || id=="lfodelay"; }
bool isFrequency(const juce::String& id) { return id=="cutoff" || id=="hpf" || id=="rate" || id=="chorusrate"; }
}

SixthEditor::HardwareLook::HardwareLook()
{
    setColour(juce::Slider::rotarySliderFillColourId, mint);
    setColour(juce::Slider::textBoxTextColourId, cream);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, red.withAlpha(0.35f));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff11171b));
    setColour(juce::ComboBox::textColourId, cream);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::arrowColourId, mint);
    setColour(juce::PopupMenu::backgroundColourId, panelColour);
    setColour(juce::PopupMenu::textColourId, cream);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, red.darker(0.5f));
    setColour(juce::Label::textColourId, muted);
    setColour(juce::TooltipWindow::backgroundColourId, cream);
    setColour(juce::TooltipWindow::textColourId, background);
}
void SixthEditor::HardwareLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    const bool vertical=slider.isVertical();
    const float centre=vertical ? x+width*0.5f : y+height*0.5f;
    auto track=vertical ? juce::Rectangle<float>(centre-2,static_cast<float>(y),4,static_cast<float>(height))
                        : juce::Rectangle<float>(static_cast<float>(x),centre-2,static_cast<float>(width),4);
    const auto colour=slider.findColour(juce::Slider::rotarySliderFillColourId);
    g.setColour(border); g.fillRoundedRectangle(track,2);
    g.setColour(colour);
    if(vertical) g.fillRoundedRectangle(centre-2,sliderPos,4.0f,juce::jmax(0.0f,y+height-sliderPos),2);
    else g.fillRoundedRectangle(static_cast<float>(x),centre-2,juce::jmax(0.0f,sliderPos-x),4.0f,2);
    const auto point=vertical ? juce::Point<float>(centre,sliderPos) : juce::Point<float>(sliderPos,centre);
    g.setColour(colour.withAlpha(0.13f)); g.fillEllipse(point.x-9,point.y-9,18,18);
    g.setColour(cream); g.fillEllipse(point.x-5,point.y-5,10,10);
}
void SixthEditor::HardwareLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider& slider)
{
    auto r=juce::Rectangle<int>(x,y,w,h).toFloat().reduced(4);
    const float radius=juce::jmin(r.getWidth(),r.getHeight())*0.5f;
    auto centre=r.getCentre(); const float angle=start+position*(end-start);
    const auto colour=slider.findColour(juce::Slider::rotarySliderFillColourId);
    juce::Path track; track.addCentredArc(centre.x,centre.y,radius-2,radius-2,0,start,end,true);
    g.setColour(border); g.strokePath(track,juce::PathStrokeType(3,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    juce::Path active; active.addCentredArc(centre.x,centre.y,radius-2,radius-2,0,start,angle,true);
    g.setColour(colour.withAlpha(0.12f)); g.strokePath(active,juce::PathStrokeType(8,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(colour); g.strokePath(active,juce::PathStrokeType(3,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    auto knob=juce::Rectangle<float>(centre.x-radius+7,centre.y-radius+7,2*radius-14,2*radius-14);
    g.setColour(juce::Colours::black.withAlpha(0.25f)); g.fillEllipse(knob.translated(0,2));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff354254),knob.getTopLeft(),juce::Colour(0xff1c2531),knob.getBottomRight(),false)); g.fillEllipse(knob);
    g.setColour(juce::Colour(0xff455366).withAlpha(0.6f)); g.drawEllipse(knob.reduced(0.5f),1);
    juce::Path pointer; pointer.addRoundedRectangle(-1.4f,-radius+11,2.8f,juce::jmax(4.0f,radius*0.29f),1.3f);
    g.setColour(cream); g.fillPath(pointer,juce::AffineTransform::rotation(angle).translated(centre.x,centre.y));
}
void SixthEditor::HardwareLook::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down)
{
    auto r=b.getLocalBounds().toFloat().reduced(0.5f);
    const bool selected=b.getToggleState();
    g.setColour(selected ? red.darker(0.65f) : (down ? border : juce::Colour(0xff13191d)));
    g.fillRoundedRectangle(r,3); g.setColour(selected ? red.brighter(0.2f) : (over ? muted : border)); g.drawRoundedRectangle(r,3,1);
}
juce::Font SixthEditor::HardwareLook::getTextButtonFont(juce::TextButton&,int) { return juce::Font(juce::FontOptions(11.0f,juce::Font::bold)); }
juce::Font SixthEditor::HardwareLook::getComboBoxFont(juce::ComboBox&) { return juce::Font(juce::FontOptions(12.0f)); }
juce::Font SixthEditor::HardwareLook::getLabelFont(juce::Label& label) { return label.getFont().withHeight(juce::jmin(11.0f,label.getFont().getHeight())); }

juce::String SixthEditor::formatValue(const juce::String& id,double value)
{
    if(isTime(id)) return value<1.0 ? juce::String(juce::roundToInt(value*1000))+" ms" : juce::String(value,2)+" s";
    if(isFrequency(id)) return value>=1000 ? juce::String(value/1000.0,2)+" kHz" : juce::String(value,value>=100 ? 0 : 2)+" Hz";
    if(id=="tune") return juce::String(value>0 ? "+" : "")+juce::String(value,1)+" ct";
    if(id=="pitch" || id=="wheel") return juce::String(value,1)+" st";
    return juce::String(juce::roundToInt(value*100))+" %";
}
double SixthEditor::parseValue(const juce::String& id,const juce::String& text)
{
    const auto s=text.trim().toLowerCase(); double value=s.getDoubleValue();
    if(isTime(id)) return s.contains("ms") ? value*0.001 : value;
    if(isFrequency(id)) return s.contains("k") ? value*1000.0 : value;
    if(id=="tune" || id=="pitch" || id=="wheel") return value;
    return value*0.01;
}
float SixthEditor::read(const char* id) const { return processor.parameters.getRawParameterValue(id)->load(); }
void SixthEditor::addControl(const char* id,const char* name,const char* tip,juce::Rectangle<int> bounds,bool rotary,bool horizontal)
{
    auto c=std::make_unique<Control>(); c->id=id;
    c->slider.setLookAndFeel(&look);
    c->slider.setSliderStyle(rotary ? juce::Slider::RotaryHorizontalVerticalDrag : (horizontal ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical));
    c->slider.setTextBoxStyle(horizontal ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow,false,horizontal ? 70 : bounds.getWidth(),19);
    const juce::String parameterName(id);
    c->slider.setColour(juce::Slider::rotarySliderFillColourId,parameterName.startsWith("f") || parameterName=="cutoff" || parameterName=="resonance" || parameterName=="env" || parameterName=="key" || parameterName=="hpf" || parameterName=="drive" ? blue : (parameterName.contains("chorus") || parameterName=="rate" || parameterName=="pitch" || parameterName=="pwm" || parameterName.startsWith("lfo") ? violet : (isTime(parameterName) || parameterName=="sustain" ? amber : mint)));
    c->slider.setTooltip(tip); c->slider.setScrollWheelEnabled(false);
    c->slider.setVelocityModeParameters(0.2,1,0.0,true,juce::ModifierKeys::shiftModifier);
    c->label.setText(name,juce::dontSendNotification); c->label.setFont(juce::Font(juce::FontOptions(10.0f,juce::Font::bold)));
    c->label.setJustificationType(juce::Justification::centred); c->label.setColour(juce::Label::textColourId,cream.withAlpha(0.85f));
    c->label.setTooltip(tip); panel.addAndMakeVisible(c->label); panel.addAndMakeVisible(c->slider);
    c->label.setBounds(bounds.removeFromTop(19)); c->slider.setBounds(bounds);
    c->attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,id,c->slider);
    auto* parameter=processor.parameters.getParameter(id);
    c->slider.setDoubleClickReturnValue(true,parameter->convertFrom0to1(parameter->getDefaultValue()));
    const juce::String parameterId(id);
    c->slider.textFromValueFunction=[parameterId](double v) { return formatValue(parameterId,v); };
    c->slider.valueFromTextFunction=[parameterId](const juce::String& t) { return parseValue(parameterId,t); };
    c->slider.updateText();
    controls.push_back(std::move(c));
}
void SixthEditor::chooseParameter(const char* id,int index)
{
    auto* parameter=processor.parameters.getParameter(id);
    parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(index))); parameter->endChangeGesture();
}
void SixthEditor::chooseProgram(int index)
{
    const int count=processor.getNumPrograms(); if(count<=0) return;
    processor.setCurrentProgram((index%count+count)%count);
    presets.setSelectedId(processor.getCurrentProgram()+1,juce::dontSendNotification);
    repaint();
}

SixthEditor::SixthEditor(SixthProcessor& p) : AudioProcessorEditor(p),processor(p),keyboard(p.keyboard,juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&look); addAndMakeVisible(panel); panel.setBounds(0,0,designWidth,designHeight);
    for(auto* component:std::initializer_list<juce::Component*>{&presets,&previous,&next,&initialise,&presetLabel,&filterDisplayLabel,&lfoShape,&retrigger,&keyboard}) panel.addAndMakeVisible(component);
    presets.setTooltip("Choose a factory sound. Harmony-bank settings stay unchanged.");
    for(int i=0;i<processor.getNumPrograms();++i) presets.addItem(processor.getProgramName(i),i+1);
    presets.setSelectedId(processor.getCurrentProgram()+1,juce::dontSendNotification);
    presets.onChange=[this] { if(presets.getSelectedId()>0) chooseProgram(presets.getSelectedId()-1); };
    previous.onClick=[this] { chooseProgram(processor.getCurrentProgram()-1); }; next.onClick=[this] { chooseProgram(processor.getCurrentProgram()+1); };
    initialise.onClick=[this] { chooseProgram(0); }; initialise.setTooltip("Reset the synthesizer sound. The harmony bank stays unchanged.");
    filterDisplayLabel.setText("FILTER SHAPE",juce::dontSendNotification);
    filterDisplayLabel.setFont(juce::Font(juce::FontOptions(8.0f)));
    filterDisplayLabel.setColour(juce::Label::textColourId,muted);
    filterDisplayLabel.setJustificationType(juce::Justification::bottomLeft);
    filterDisplayLabel.setTooltip("Base low-pass and high-pass filter shape. The envelope and LFO move the cutoff while you play; this display is not a spectrum analyzer.");
    filterDisplayLabel.setBounds(610,169,304,35);
    presetLabel.setText("FACTORY PATCH",juce::dontSendNotification); presetLabel.setFont(juce::Font(juce::FontOptions(9.5f,juce::Font::bold)));
    presetLabel.setBounds(660,14,200,15); previous.setBounds(660,33,30,29); presets.setBounds(697,33,310,29); next.setBounds(1014,33,30,29); initialise.setBounds(1065,33,82,29);

    const char* lfoIds[]={"rate","pitch","pwm","filterlfo"}; const char* lfoNames[]={"RATE","PITCH","PWM","FILTER"};
    const char* lfoTips[]={"LFO speed.","Pitch modulation depth in semitones.","Pulse-width modulation depth.","LFO modulation of filter cutoff."};
    for(int i=0;i<4;++i) addControl(lfoIds[i],lfoNames[i],lfoTips[i],{37+(i%2)*107,172+(i/2)*104,94,101});
    lfoShape.addItemList({"Sine","Triangle","Square","Sample & hold"},1); lfoShape.setBounds(34,144,122,24); lfoShape.setTooltip("LFO waveform.");
    lfoShapeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,"lfoshape",lfoShape);
    retrigger.setClickingTogglesState(true); retrigger.setBounds(164,144,76,24); retrigger.setTooltip("Restart the LFO when a note is played.");
    retriggerAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"lfotrigger",retrigger);

    const char* oscIds[]={"saw","pulse","sub","noise"}; const char* oscNames[]={"SAW","PULSE","SUB","NOISE"};
    const char* oscTips[]={"Sawtooth oscillator level.","Pulse oscillator level.","Sub oscillator level, one octave below the DCO.","Noise level."};
    for(int i=0;i<4;++i) addControl(oscIds[i],oscNames[i],oscTips[i],{278+i*74,170,68,105});
    for(int i=0;i<3;++i) { auto& button=octaveButtons[static_cast<size_t>(i)]; button.setButtonText(i==0 ? "16'" : (i==1 ? "8'" : "4'")); button.setBounds(471+i*33,123,31,24); button.setTooltip("DCO octave range."); button.onClick=[this,i] { chooseParameter("octave",i); }; panel.addAndMakeVisible(button); }
    addControl("width","PULSE WIDTH","Static pulse width, before PWM modulation.",{280,280,94,102});
    addControl("tune","FINE TUNE","Fine tuning in cents.",{381,280,94,102});
    addControl("drift","VOICE DRIFT","Small differences in oscillator tuning between the six voices.",{482,280,94,102});

    addControl("cutoff","CUTOFF","Low-pass cutoff frequency.",{608,214,125,165});
    const char* filterIds[]={"resonance","env","key","hpf","drive","filtervelocity"};
    const char* filterNames[]={"RESO","ENV +/-","KEY TRK","HIGH PASS","DRIVE","VELOCITY"};
    const char* filterTips[]={"Filter resonance.","Bipolar filter envelope amount. Uses the separate FILTER ENV below.","How strongly the filter follows the keyboard.","High-pass cutoff frequency.","Saturation before the resonant low-pass filter.","How much note velocity opens the filter."};
    for(int i=0;i<6;++i) addControl(filterIds[i],filterNames[i],filterTips[i],{735+(i%3)*62,214+(i/3)*86,59,84});

    addControl("volume","OUTPUT","Master output level.",{958,169,100,116});
    addControl("velocity","VELOCITY","Amplitude response to MIDI note velocity.",{1064,169,100,116});
    addControl("stereo","WIDTH","Stereo width of the chorus output.",{952,287,72,95});
    addControl("wheel","WHEEL","Additional pitch modulation from MIDI CC1, in semitones.",{1028,287,72,95});
    addControl("lfodelay","LFO DELAY","Delay before modulation fades in over 250 ms.",{1104,287,72,95});

    const char* envSuffix[]={"attack","decay","sustain","release"}; const char* envNames[]={"ATTACK","DECAY","SUSTAIN","RELEASE"};
    for(int i=0;i<4;++i) {
        addControl(envSuffix[i],envNames[i],"Amplifier envelope: changes the volume of each voice over time.",{38+i*78,508,70,128});
        const auto id=juce::String("f")+envSuffix[i];
        addControl(id.toRawUTF8(),envNames[i],"Independent filter envelope. Set its depth with VCF ENV +/-.",{382+i*78,508,70,128});
    }
    for(int i=0;i<3;++i) { auto& button=chorusButtons[static_cast<size_t>(i)]; button.setButtonText(i==0 ? "OFF" : (i==1 ? "I" : "II")); button.setBounds(854+i*85,450,77,28); button.setTooltip("Chorus mode: Off, I or II."); button.onClick=[this,i] { chooseParameter("chorus",i); }; panel.addAndMakeVisible(button); }
    addControl("chorusmix","MIX","Blend dry signal with the chorus.",{738,491,128,133},true);
    addControl("chorusrate","RATE","Chorus modulation speed.",{875,491,128,133},true);
    addControl("chorusdepth","DEPTH","Chorus modulation depth.",{1012,491,128,133},true);

    buildChordControls();

    keyboard.setOctaveForMiddleC(4);
    keyboard.setAvailableRange(36,96); keyboard.setKeyWidth(32.0f); keyboard.setLowestVisibleKey(48);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,cream);
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,juce::Colour(0xff13181c));
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,red.withAlpha(0.65f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,mint.withAlpha(0.35f));
    keyboard.setBounds(24,696,1152,80);
    setResizable(true,true); setResizeLimits(900,600,1800,1200); getConstrainer()->setFixedAspectRatio(1.5);
    setSize(designWidth,designHeight); showChordView(read("chordMode")>=0.5f); startTimerHz(24); timerCallback();
}
SixthEditor::~SixthEditor() { stopTimer(); releaseChordPads(); setLookAndFeel(nullptr); }
void SixthEditor::paint(juce::Graphics& g) { g.fillAll(background); }
void SixthEditor::resized()
{
    const float scale=juce::jmin(getWidth()/static_cast<float>(designWidth),getHeight()/static_cast<float>(designHeight));
    panel.setTransform(juce::AffineTransform::scale(scale).translated((getWidth()-designWidth*scale)*0.5f,(getHeight()-designHeight*scale)*0.5f));
}
void SixthEditor::timerCallback()
{
    meter=juce::jmax(processor.outputLevel.load(),meter*0.86f);
    presetLabel.setText(processor.isCurrentProgramModified() ? "SOUND PATCH  /  EDITED" : "SOUND PATCH",juce::dontSendNotification);
    midiActive=false;
    for(int note=0;note<=127;++note) midiActive=midiActive || processor.keyboard.isNoteOnForChannels(0xffff,note);
    for(int i=0;i<3;++i) {
        octaveButtons[static_cast<size_t>(i)].setToggleState(juce::roundToInt(read("octave"))==i,juce::dontSendNotification);
        chorusButtons[static_cast<size_t>(i)].setToggleState(juce::roundToInt(read("chorus"))==i,juce::dontSendNotification);
    }
    if(presets.getSelectedId()!=processor.getCurrentProgram()+1) presets.setSelectedId(processor.getCurrentProgram()+1,juce::dontSendNotification);
    const bool enabled=read("chordMode")>=0.5f;
    chordEnable.setButtonText(enabled ? "CHORD MODE ON" : "CHORD MODE OFF");
    chordView.setButtonText(enabled ? "CHORDS ON" : "CHORDS");
    if(lastDisplayedBank!=currentChordBank()) rebuildChordBanks(true);
    for(auto& pad:chordPads) if(pad) pad->repaint();
    panel.repaint();
}
void SixthEditor::paintFilter(juce::Graphics& g,juce::Rectangle<float> r)
{
    g.setColour(juce::Colour(0xff111923)); g.fillRoundedRectangle(r.expanded(3),4);
    for(int i=1;i<5;++i) { g.setColour(border.withAlpha(0.45f)); g.drawVerticalLine(juce::roundToInt(r.getX()+r.getWidth()*i/5),r.getY(),r.getBottom()); }
    juce::Path curve;
    const float cutoff=read("cutoff"),highpass=read("hpf"),resonance=read("resonance");
    for(int i=0;i<=160;++i) {
        const float frequency=20.0f*std::pow(1000.0f,i/160.0f);
        const float ratio=frequency/cutoff;
        const float db=-10.0f*std::log10(1.0f+std::pow(ratio,8.0f))
            -10.0f*std::log10(1.0f+std::pow(highpass/frequency,4.0f))
            +resonance*12.0f*std::exp(-std::pow(std::log2(ratio)/0.33f,2.0f));
        const float x=r.getX()+r.getWidth()*i/160.0f,y=juce::jmap(juce::jlimit(-42.0f,6.0f,db),-42.0f,6.0f,r.getBottom(),r.getY());
        if(i==0) curve.startNewSubPath(x,y); else curve.lineTo(x,y);
    }
    auto fill=curve; fill.lineTo(r.getBottomRight()); fill.lineTo(r.getBottomLeft()); fill.closeSubPath();
    g.setColour(blue.withAlpha(0.09f)); g.fillPath(fill); g.setColour(blue); g.strokePath(curve,juce::PathStrokeType(1.5f));
}
void SixthEditor::paintEnvelope(juce::Graphics& g,juce::Rectangle<float> r,const char* prefix)
{
    auto parameter=[&](const char* suffix) { const auto id=juce::String(prefix)+suffix; return read(id.toRawUTF8()); };
    const float a=std::sqrt(parameter("attack")),d=std::sqrt(parameter("decay")),s=parameter("sustain"),release=std::sqrt(parameter("release"));
    const float total=a+d+release+0.85f;
    const float x1=r.getX()+r.getWidth()*a/total,x2=x1+r.getWidth()*d/total,x3=x2+r.getWidth()*0.85f/total;
    const float sy=r.getBottom()-s*r.getHeight();
    g.setColour(juce::Colour(0xff111923)); g.fillRoundedRectangle(r.expanded(8,6),3);
    g.setColour(border); g.drawHorizontalLine(juce::roundToInt(r.getBottom()),r.getX(),r.getRight());
    juce::Path path; path.startNewSubPath(r.getX(),r.getBottom()); path.lineTo(x1,r.getY()); path.lineTo(x2,sy); path.lineTo(x3,sy); path.lineTo(r.getRight(),r.getBottom());
    auto fill=path; fill.closeSubPath(); g.setColour(mint.withAlpha(0.10f)); g.fillPath(fill);
    g.setColour(mint); g.strokePath(path,juce::PathStrokeType(1.6f));
}
void SixthEditor::paintPanel(juce::Graphics& g)
{
    g.fillAll(background);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1c2937),0,0,background,0,105,false)); g.fillRect(0,0,designWidth,85);
    text(g,"SIXTH",{27,13,260,44},40,cream,juce::Justification::centredLeft,true);
    text(g,read("chordMode")>=0.5f ? "CHORD PERFORMANCE / 24 VOICES" : "SIX-VOICE POLYPHONIC SYNTHESIZER",{30,57,390,15},10,muted);
    g.setColour(border); g.fillRect(20,85,designWidth-40,1);
    if(chordPageVisible) {
        paintChordPage(g);
    } else {
    text(g,"MODULATE",{23,93,220,16},9,muted);
    text(g,"GENERATE",{268,93,280,16},9,muted);
    text(g,"SHAPE",{599,93,280,16},9,muted);
    text(g,"PERFORM",{943,93,220,16},9,muted);
    section(g,{20,114,234,274},"MODULATION","");
    section(g,{266,114,318,274},"OSCILLATOR","WAVEFORM MIX / PULSE SHAPING / TUNING");
    section(g,{596,114,332,274},"FILTER","24 dB LOW PASS + HIGH PASS");
    paintFilter(g,{610,169,304,35});
    section(g,{940,114,240,274},"OUTPUT","DYNAMICS / STEREO / EXPRESSION");
    section(g,{20,400,332,248},"AMP ENVELOPE","SHAPE THE VOLUME OF EACH NOTE");
    section(g,{364,400,332,248},"FILTER ENVELOPE","INDEPENDENT FROM THE AMPLIFIER");
    section(g,{708,400,472,248},"CHORUS","TWO MODES / CONTINUOUS RATE + DEPTH");
    paintEnvelope(g,{43,457,285,35},""); paintEnvelope(g,{387,457,285,35},"f");
    const auto waveArea=juce::Rectangle<float>(729,451,110,29);
    g.setColour(juce::Colour(0xff111923)); g.fillRoundedRectangle(waveArea.expanded(3),4);
    for(int channel=0;channel<2;++channel) {
        juce::Path wave;
        for(int i=0;i<=80;++i) {
            const float x=waveArea.getX()+waveArea.getWidth()*i/80.0f;
            const float phase=i/80.0f*juce::MathConstants<float>::twoPi*1.6f+channel*read("stereo")*1.4f;
            const float y=waveArea.getCentreY()+std::sin(phase)*(3.0f+read("chorusdepth")*8.0f);
            if(i==0) wave.startNewSubPath(x,y); else wave.lineTo(x,y);
        }
        g.setColour((channel==0 ? mint : violet).withAlpha(read("chorus")<0.5f ? 0.25f : 0.85f)); g.strokePath(wave,juce::PathStrokeType(1.2f));
    }
    }
    g.setColour(border); g.drawHorizontalLine(666,24,1176);
    text(g,chordPageVisible ? "12 KEYS = 12 CHORDS   /   NEXT OCTAVE = NEXT REGISTER   /   HOLD A PAD OR PLAY MIDI" : "DOUBLE CLICK: RESET   /   SHIFT + DRAG: FINE ADJUST   /   HOVER: HELP",{26,671,950,16},9,muted);
    g.setColour(midiActive ? red : border); g.fillEllipse(1000,675,6,6);
    text(g,"MIDI",{1011,670,39,17},9,muted);
    for(int i=0;i<18;++i) {
        const float threshold=std::pow(10.0f,(-48.0f+i*48.0f/17.0f)/20.0f);
        g.setColour(meter>=threshold ? (i>15 ? red : mint) : border.darker(0.35f));
        g.fillRect(1058+i*6,673,4,10);
    }
    text(g,"MIDI KEYBOARD",{26,780,300,16},8,muted);
    text(g,read("chordMode")>=0.5f ? "CHORD MODE ON  /  MIDI 48-59 = THE DISPLAYED PALETTE" : "SIX VOICES  /  EXPANDED SOUND DESIGN",{690,780,483,16},8,muted,juce::Justification::centredRight);
}

void SixthEditor::ChordPad::paint(juce::Graphics& g) { owner.paintChordPad(g,slot,getLocalBounds(),held); }
void SixthEditor::ChordPad::mouseDown(const juce::MouseEvent& event)
{
    if(!event.mods.isLeftButtonDown() || held) return;
    held=true; owner.playChordPad(slot,true); repaint();
}
void SixthEditor::ChordPad::release()
{
    if(!held) return;
    held=false; owner.playChordPad(slot,false); repaint();
}
juce::String SixthEditor::inputNoteName(int note)
{
    return juce::MidiMessage::getMidiNoteName(juce::jlimit(0,127,note),true,true,4);
}
int SixthEditor::currentChordBank() const
{
    return juce::jlimit(0,static_cast<int>(SixthChords::banks().size())-1,juce::roundToInt(read("chordBank")));
}
juce::String SixthEditor::chordNotes(const SixthChords::Notes& notes) const
{
    juce::String result;
    for(int i=0;i<notes.size;++i) {
        if(i>0) result+="  ";
        result+=inputNoteName(notes.notes[static_cast<size_t>(i)]);
    }
    return result;
}
void SixthEditor::playChordPad(int slot,bool down)
{
    if(down) {
        if(read("chordMode")<0.5f) chooseParameter("chordMode",1);
        processor.keyboard.noteOn(1,48+slot,0.8f);
    } else processor.keyboard.noteOff(1,48+slot,0.0f);
}
void SixthEditor::releaseChordPads() { for(auto& pad:chordPads) if(pad) pad->release(); }
void SixthEditor::showChordView(bool show)
{
    if(chordPageVisible!=show) releaseChordPads();
    chordPageVisible=show;
    for(auto* component:synthComponents) component->setVisible(!show);
    for(auto* component:chordComponents) component->setVisible(show);
    synthView.setToggleState(!show,juce::dontSendNotification); chordView.setToggleState(show,juce::dontSendNotification);
    panel.repaint();
}
void SixthEditor::buildChordControls()
{
    for(auto& control:controls) { synthComponents.push_back(&control->label); synthComponents.push_back(&control->slider); }
    for(auto* component:std::initializer_list<juce::Component*>{&lfoShape,&retrigger,&filterDisplayLabel}) synthComponents.push_back(component);
    for(auto& button:octaveButtons) synthComponents.push_back(&button);
    for(auto& button:chorusButtons) synthComponents.push_back(&button);

    panel.addAndMakeVisible(synthView); panel.addAndMakeVisible(chordView);
    synthView.setBounds(320,32,106,30); chordView.setBounds(438,32,106,30);
    synthView.onClick=[this] { showChordView(false); }; chordView.onClick=[this] { showChordView(true); };
    synthView.setTooltip("Synthesizer sound controls. Chord mode can stay enabled while editing the sound.");
    chordView.setTooltip("Related chord palettes mapped to your keyboard.");
    for(auto* component:std::initializer_list<juce::Component*>{&chordCategory,&chordBanks,&chordKey,&chordVoicing,&chordRegister,&chordEnable,&panic}) {
        panel.addAndMakeVisible(component); chordComponents.push_back(component);
    }
    chordCategories.add("All styles");
    for(const auto& bank:SixthChords::banks()) if(!chordCategories.contains(bank.category)) chordCategories.add(bank.category);
    chordCategory.addItemList(chordCategories,1); chordCategory.setSelectedId(1,juce::dontSendNotification);
    chordCategory.setBounds(36,176,190,30); chordCategory.setTooltip("Browse a family of harmony banks. Filtering doesn't change the playing bank.");
    chordCategory.onChange=[this] { rebuildChordBanks(false); };
    chordBanks.setBounds(244,176,400,30); chordBanks.setTextWhenNothingSelected("Choose a bank in this family");
    chordBanks.setTooltip("Each bank contains twelve related chords. This is independent from the synthesizer sound patch.");
    chordBanks.onChange=[this] { if(chordBanks.getSelectedId()>0) chooseParameter("chordBank",chordBanks.getSelectedId()-1); };
    chordKey.addItemList({"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"},1);
    chordKey.setBounds(662,176,115,30); chordKey.setTooltip("Transpose the whole harmony bank to another key.");
    chordKeyAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,"chordKey",chordKey);
    for(int i=0;i<SixthChords::voicingCount;++i) chordVoicing.addItem(SixthChords::voicingName(i),i+1);
    chordVoicing.setBounds(795,176,190,30); chordVoicing.setTooltip("Keep the written voicing, or rearrange its notes. Rootless chords stay rootless.");
    chordVoicingAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,"chordVoicing",chordVoicing);
    chordRegister.addItemList({"-2 oct","-1 oct","Written","+1 oct","+2 oct"},1);
    chordRegister.setBounds(1003,176,96,30); chordRegister.setTooltip("Shift all generated chords by octaves. Your input keyboard's octaves also shift the voicing.");
    chordRegisterAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,"chordRegister",chordRegister);
    chordEnable.setBounds(1016,126,146,28); chordEnable.setClickingTogglesState(true);
    chordEnable.setTooltip("When enabled, every MIDI note triggers its chord slot. Off plays individual notes normally.");
    chordEnableAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"chordMode",chordEnable);
    panic.setBounds(1110,176,52,30); panic.setTooltip("Stop all sounding notes, including sustained chords.");
    panic.onClick=[this] { releaseChordPads(); processor.requestPanic(); };
    for(int slot=0;slot<12;++slot) {
        auto& pad=chordPads[static_cast<size_t>(slot)]; pad=std::make_unique<ChordPad>(*this,slot);
        pad->setName("Chord pad "+inputNoteName(48+slot));
        pad->setTooltip("Hold to audition this chord. Pressing a pad enables chord mode; a sustain pedal can hold released notes.");
        pad->setBounds(25+(slot%6)*194,296+(slot/6)*146,180,132);
        panel.addAndMakeVisible(*pad); chordComponents.push_back(pad.get());
    }
    rebuildChordBanks(false);
}
void SixthEditor::rebuildChordBanks(bool followCurrent)
{
    const int selected=currentChordBank();
    const auto& bank=SixthChords::banks()[static_cast<size_t>(selected)];
    if(followCurrent && chordCategory.getSelectedId()>1 && chordCategory.getText()!=bank.category) {
        const int category=chordCategories.indexOf(bank.category);
        if(category>=0) chordCategory.setSelectedId(category+1,juce::dontSendNotification);
    }
    const auto filter=chordCategory.getSelectedId()>1 ? chordCategory.getText() : juce::String();
    chordBanks.clear(juce::dontSendNotification);
    for(size_t i=0;i<SixthChords::banks().size();++i) {
        const auto& entry=SixthChords::banks()[i];
        if(filter.isEmpty() || filter==entry.category) chordBanks.addItem(entry.name,static_cast<int>(i)+1);
    }
    if(filter.isEmpty() || filter==bank.category) chordBanks.setSelectedId(selected+1,juce::dontSendNotification);
    lastDisplayedBank=selected;
    panel.repaint();
}
void SixthEditor::paintChordPad(juce::Graphics& g,int slot,juce::Rectangle<int> bounds,bool down)
{
    const bool active=down || (processor.activeChordSlots.load()&(1<<slot))!=0;
    const bool enabled=read("chordMode")>=0.5f;
    auto area=bounds.toFloat().reduced(1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.2f)); g.fillRoundedRectangle(area.translated(0,2),9);
    g.setColour(active ? mint.darker(0.78f) : panelColour); g.fillRoundedRectangle(area,9);
    g.setColour(active ? mint : border); g.drawRoundedRectangle(area.reduced(0.5f),9,active ? 1.5f : 1.0f);
    const int bank=currentChordBank(),key=juce::roundToInt(read("chordKey")),voice=juce::roundToInt(read("chordVoicing"));
    const int registerShift=juce::roundToInt(read("chordRegister"))-2;
    const auto notes=SixthChords::resolve(bank,slot,key,voice,registerShift);
    const juce::String name(SixthChords::chordName(bank,slot,key));
    text(g,juce::String(slot+1).paddedLeft('0',2),{14,10,40,20},10,active ? mint : muted,juce::Justification::centredLeft,true);
    text(g,inputNoteName(48+slot),{90,10,76,20},12,cream,juce::Justification::centredRight,true);
    g.setColour(cream.withAlpha(enabled || active ? 1.0f : 0.6f)); g.setFont(juce::FontOptions(19.0f,juce::Font::bold));
    g.drawFittedText(name,{14,35,152,47},juce::Justification::centredLeft,2,0.7f);
    text(g,chordNotes(notes),{14,86,152,21},10,active ? cream : muted);
    g.setColour(active ? mint : border); g.fillRoundedRectangle(14.0f,115.0f,152.0f,2.0f,1.0f);
}
void SixthEditor::paintChordPage(juce::Graphics& g)
{
    text(g,"PLAY HARMONY",{23,93,500,16},9,muted);
    text(g,juce::String(SixthChords::banks().size())+" BANKS  /  "+juce::String(SixthChords::banks().size()*12)+" CHORD SLOTS  /  6 VOICINGS",{640,93,535,16},9,muted,juce::Justification::centredRight);
    section(g,{20,114,1160,142},"CHORD PERFORMANCE","ONE KEY / A RELATED CHORD PALETTE");
    text(g,"STYLE",{36,157,190,16},9,muted);
    text(g,"HARMONY BANK",{244,157,400,16},9,muted);
    text(g,"KEY",{662,157,115,16},9,muted);
    text(g,"VOICING",{795,157,190,16},9,muted);
    text(g,"REGISTER",{1003,157,96,16},9,muted);
    const auto& bank=SixthChords::banks()[static_cast<size_t>(currentChordBank())];
    g.setColour(muted); g.setFont(juce::FontOptions(11.0f));
    g.drawFittedText(juce::String(bank.name)+" - "+bank.description,{36,215,1125,31},juce::Justification::centredLeft,2);
    text(g,"NEXT KEY MAP  /  C3-B3 (MIDI 48-59)",{25,269,700,18},10,cream,juce::Justification::centredLeft,true);
    text(g,"Hold a pad to audition / Higher/lower octaves move the voicing",{585,269,590,18},10,muted,juce::Justification::centredRight);

    const auto display=processor.getChordDisplay();
    auto readout=juce::Rectangle<float>(20,592,1160,57);
    g.setColour(panelColour); g.fillRoundedRectangle(readout,9); g.setColour(border); g.drawRoundedRectangle(readout.reduced(0.5f),9,1);
    if(display.slot>=0 && display.trigger>=0 && display.bank>=0 && display.bank<static_cast<int>(SixthChords::banks().size())) {
        text(g,"LAST PLAYED",{36,600,330,13},9,muted);
        const auto name=juce::String(SixthChords::chordName(display.bank,display.slot,display.key));
        text(g,name,{36,615,340,26},19,cream,juce::Justification::centredLeft,true);
        text(g,chordNotes(display.notes),{410,610,470,28},14,mint);
        text(g,"INPUT "+inputNoteName(display.trigger)+" / MIDI "+juce::String(display.trigger),{882,605,278,18},11,cream,juce::Justification::centredRight);
        const auto& playedBank=SixthChords::banks()[static_cast<size_t>(display.bank)];
        text(g,playedBank.name,{820,625,340,15},9,muted,juce::Justification::centredRight);
    } else {
        text(g,"Choose a bank, then play one key.",{37,601,670,35},19,cream,juce::Justification::centredLeft,true);
        text(g,"Individual keys become complete voicings.",{742,609,418,24},12,muted,juce::Justification::centredRight);
    }
}

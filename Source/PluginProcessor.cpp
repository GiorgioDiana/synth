#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
struct Sound final : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
struct Settings {
    float saw, pulse, sub, noise, width, cutoff, resonance, env, key, rate, pitch, pwm, attack, decay, sustain, release;
};
class Voice final : public juce::SynthesiserVoice {
public:
    Settings settings {};
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<Sound*>(s) != nullptr; }
    void setCurrentPlaybackSampleRate(double rate) override {
        SynthesiserVoice::setCurrentPlaybackSampleRate(rate);
        envelope.setSampleRate(rate);
        juce::dsp::ProcessSpec spec {rate * 2.0, 1, 1};
        filter1.prepare(spec); filter2.prepare(spec);
        filter1.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter2.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter2.setResonance(0.7071f);
    }
    void startNote(int note, float v, juce::SynthesiserSound*, int wheel) override {
        frequency = juce::MidiMessage::getMidiNoteInHertz(note); midiNote = note;
        velocity = v; phase = 0; subPhase = 0; pitchWheelMoved(wheel);
        filter1.reset(); filter2.reset(); envelope.reset(); updateEnvelope(); envelope.noteOn();
    }
    void stopNote(float, bool tail) override {
        if(tail) envelope.noteOff(); else { envelope.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int wheel) override { bend = std::pow(2.0, ((wheel - 8192) / 8192.0) * 2.0 / 12.0); }
    void controllerMoved(int, int) override {}
    static float blep(float t, float dt) {
        if(t < dt) { t /= dt; return t+t-t*t-1; }
        if(t > 1-dt) { t=(t-1)/dt; return t*t+t+t+1; }
        return 0;
    }
    void updateEnvelope() { envelope.setParameters({settings.attack,settings.decay,settings.sustain,settings.release}); }
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int start, int count) override {
        if(!isVoiceActive()) return;
        updateEnvelope();
        const auto sr = getSampleRate();
        filter1.setResonance(0.7071f + settings.resonance * 4.5f);
        for(int n=0;n<count;++n) {
            const float e = envelope.getNextSample();
            if(!envelope.isActive()) { clearCurrentNote(); break; }
            const float lfo=std::sin(lfoPhase * juce::MathConstants<double>::twoPi);
            lfoPhase += settings.rate/sr; lfoPhase -= std::floor(lfoPhase);
            const float pw=juce::jlimit(0.05f,0.95f,settings.width + lfo*settings.pwm*0.4f);
            const double hz=frequency*bend*std::pow(2.0, lfo*settings.pitch/12.0);
            const float dt=static_cast<float>(juce::jmin(0.45, hz/(sr*2)));
            const float cutoff=juce::jlimit(25.0f,static_cast<float>(juce::jmin(18000.0,sr*0.44)),
                settings.cutoff * std::pow(2.0f,e*settings.env*6.0f + (midiNote-60)*settings.key/12.0f));
            filter1.setCutoffFrequency(cutoff); filter2.setCutoffFrequency(cutoff);
            float sample=0;
            for(int os=0;os<2;++os) {
                const float t=static_cast<float>(phase);
                const float saw=2*t-1-blep(t,dt);
                float shifted=t-pw; if(shifted<0) shifted+=1;
                const float pulse=(t<pw ? 1.0f : -1.0f)+blep(t,dt)-blep(shifted,dt);
                const float st=static_cast<float>(subPhase);
                float subShift=st-0.5f; if(subShift<0) subShift+=1;
                const float sub=(st<0.5f ? 1.0f : -1.0f)+blep(st,dt*0.5f)-blep(subShift,dt*0.5f);
                const float mixed=settings.saw*saw+settings.pulse*pulse+settings.sub*sub
                                  +settings.noise*(random.nextFloat()*2-1);
                sample += filter2.processSample(0,filter1.processSample(0,std::tanh(mixed*0.7f)))*0.5f;
                phase+=dt; phase-=std::floor(phase);
                subPhase+=dt*0.5; subPhase-=std::floor(subPhase);
            }
            sample *= e*velocity*0.2f;
            for(int ch=0;ch<buffer.getNumChannels();++ch) buffer.addSample(ch,start+n,sample);
        }
        filter1.snapToZero(); filter2.snapToZero();
    }
private:
    juce::ADSR envelope;
    juce::dsp::StateVariableTPTFilter<float> filter1,filter2;
    juce::Random random {42};
    double frequency=440, phase=0, subPhase=0, lfoPhase=0, bend=1;
    int midiNote=60; float velocity=1;
};
}

juce::AudioProcessorValueTreeState::ParameterLayout SixthProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto add=[&](const char* id,const char* name,float lo,float hi,float initial,float skew=1.0f) {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
            juce::NormalisableRange<float>(lo,hi,0.0f,skew),initial,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float v,int) { return juce::String(v,std::abs(v)>=100 ? 0 : 2); })
                .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue(); })));
    };
    add("saw","Saw",0,1,0.8f); add("pulse","Pulse",0,1,0.4f);
    add("sub","Sub",0,1,0.3f); add("noise","Noise",0,1,0);
    add("width","Pulse width",0.05f,0.95f,0.5f);
    add("cutoff","Cutoff Hz",25,18000,1800,0.3f); add("resonance","Resonance",0,1,0.2f);
    add("env","Filter envelope",-1,1,0.4f); add("key","Keyboard tracking",0,1,0.5f);
    add("rate","LFO Hz",0.05f,20,0.8f,0.4f); add("pitch","LFO pitch semitones",0,2,0);
    add("pwm","LFO pulse width",0,1,0.2f);
    add("attack","Attack seconds",0.001f,4,0.01f,0.3f);
    add("decay","Decay seconds",0.01f,4,0.4f,0.3f); add("sustain","Sustain",0,1,0.7f);
    add("release","Release seconds",0.01f,4,0.3f,0.3f);
    add("hpf","High-pass Hz",20,2000,20,0.3f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"chorus",1},"Chorus",juce::StringArray{"Off","I","II"},1));
    add("volume","Output",0,1,0.7f);
    return p;
}
SixthProcessor::SixthProcessor() : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    parameters(*this,nullptr,"SixthState",layout()) {
    for(int i=0;i<6;++i) synth.addVoice(new Voice());
    synth.addSound(new Sound()); synth.setNoteStealingEnabled(true);
}
bool SixthProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainInputChannelSet().isDisabled() && l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
}
void SixthProcessor::prepareToPlay(double sr,int block) {
    synth.allNotesOff(0,false); synth.setCurrentPlaybackSampleRate(sr);
    juce::dsp::ProcessSpec spec{sr,static_cast<juce::uint32>(juce::jmax(1,block)),2};
    chorus.prepare(spec); chorus.reset(); chorus.setCentreDelay(7.0f); chorus.setFeedback(0.05f);
    highpass.prepare(spec); highpass.setType(juce::dsp::StateVariableTPTFilterType::highpass); highpass.setResonance(0.7071f); highpass.reset();
    gain.reset(sr,0.02); gain.setCurrentAndTargetValue(value("volume")); keyboard.reset();
}
void SixthProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals guard;
    audio.clear(); keyboard.processNextMidiBuffer(midi,0,audio.getNumSamples(),true);
    Settings s{value("saw"),value("pulse"),value("sub"),value("noise"),value("width"),value("cutoff"),value("resonance"),value("env"),value("key"),value("rate"),value("pitch"),value("pwm"),value("attack"),value("decay"),value("sustain"),value("release")};
    for(int i=0;i<synth.getNumVoices();++i) static_cast<Voice*>(synth.getVoice(i))->settings=s;
    synth.renderNextBlock(audio,midi,0,audio.getNumSamples());
    juce::dsp::AudioBlock<float> block(audio);
    juce::dsp::ProcessContextReplacing<float> context(block);
    highpass.setCutoffFrequency(value("hpf")); highpass.process(context);
    const int mode=juce::roundToInt(value("chorus"));
    chorus.setRate(mode==2 ? 0.85f : 0.5f); chorus.setDepth(mode==2 ? 0.65f : 0.4f);
    chorus.setMix(mode==0 ? 0.0f : 0.45f); chorus.process(context);
    gain.setTargetValue(value("volume"));
    for(int n=0;n<audio.getNumSamples();++n) {
        const float g=gain.getNextValue();
        for(int ch=0;ch<audio.getNumChannels();++ch)
            audio.setSample(ch,n,std::tanh(audio.getSample(ch,n)*g));
    }
    midi.clear();
}
juce::AudioProcessorEditor* SixthProcessor::createEditor() { return new SixthEditor(*this); }
void SixthProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto xml=parameters.copyState().createXml(); copyXmlToBinary(*xml,data);
}
void SixthProcessor::setStateInformation(const void* data,int size) {
    auto xml=getXmlFromBinary(data,size);
    if(xml && xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SixthProcessor(); }

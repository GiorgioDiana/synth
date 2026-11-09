#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
struct Sound final : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
struct Settings {
    float saw, pulse, sub, noise, width, cutoff, resonance, env, key, rate, pitch, pwm;
    float attack, decay, sustain, release, fattack, fdecay, fsustain, frelease;
    float tune, drift, drive, filterLfo, lfoDelay, velocity, filterVelocity, wheel;
    int octave, lfoShape;
    bool lfoTrigger;
};
class Voice final : public juce::SynthesiserVoice {
public:
    Voice(int index, const std::array<float,16>& wheelValues)
        : wheels(wheelValues), voiceIndex(index), random(42+index*101), lfoRandom(7+index*37) { held=lfoRandom.nextFloat()*2-1; }
    Settings settings {};
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<Sound*>(s) != nullptr; }
    void setCurrentPlaybackSampleRate(double rate) override {
        SynthesiserVoice::setCurrentPlaybackSampleRate(rate);
        ampEnvelope.setSampleRate(rate); filterEnvelope.setSampleRate(rate);
        ampEnvelope.setParameters(ampEnvelope.getParameters()); filterEnvelope.setParameters(filterEnvelope.getParameters());
        cutoffSmooth.reset(rate,0.02); driveSmooth.reset(rate,0.02);
        juce::dsp::ProcessSpec spec {rate*2.0,1,1};
        filter1.prepare(spec); filter2.prepare(spec);
        filter1.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter2.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter2.setResonance(0.7071f);
    }
    void startNote(int note,float v,juce::SynthesiserSound*,int wheel) override {
        frequency=juce::MidiMessage::getMidiNoteInHertz(note); midiNote=note;
        velocity=v; phase=0; subPhase=0; age=0; pitchWheelMoved(wheel);
        for(int ch=1;ch<=16;++ch) if(isPlayingChannel(ch)) wheelSource=&wheels[static_cast<size_t>(ch-1)];
        if(settings.lfoTrigger) { lfoPhase=0; lfoRandom.setSeed(7+voiceIndex*37); held=lfoRandom.nextFloat()*2-1; }
        filter1.reset(); filter2.reset(); ampEnvelope.reset(); filterEnvelope.reset();
        cutoffSmooth.setCurrentAndTargetValue(settings.cutoff); driveSmooth.setCurrentAndTargetValue(settings.drive);
        updateEnvelopes(); ampEnvelope.noteOn(); filterEnvelope.noteOn();
    }
    void stopNote(float,bool tail) override {
        if(tail) { ampEnvelope.noteOff(); filterEnvelope.noteOff(); }
        else { ampEnvelope.reset(); filterEnvelope.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int wheel) override { bend=std::pow(2.0,((wheel-8192)/8192.0)*2.0/12.0); }
    void controllerMoved(int,int) override {}
    static float blep(float t,float dt) {
        if(t<dt) { t/=dt; return t+t-t*t-1; }
        if(t>1-dt) { t=(t-1)/dt; return t*t+t+t+1; }
        return 0;
    }
    void updateEnvelopes() {
        // Avoid recalculating release slopes every block when parameters haven't changed.
        const auto update=[](juce::ADSR& envelope,juce::ADSR::Parameters next) {
            const auto& old=envelope.getParameters();
            if(old.attack!=next.attack || old.decay!=next.decay || old.sustain!=next.sustain || old.release!=next.release)
                envelope.setParameters(next);
        };
        update(ampEnvelope,{settings.attack,settings.decay,settings.sustain,settings.release});
        update(filterEnvelope,{settings.fattack,settings.fdecay,settings.fsustain,settings.frelease});
    }
    void renderNextBlock(juce::AudioBuffer<float>& buffer,int start,int count) override {
        if(!isVoiceActive()) return;
        updateEnvelopes();
        cutoffSmooth.setTargetValue(settings.cutoff); driveSmooth.setTargetValue(settings.drive);
        const auto sr=getSampleRate();
        filter1.setResonance(0.7071f+settings.resonance*4.5f);
        const float velocityGain=1.0f-settings.velocity+settings.velocity*velocity;
        const float offset=static_cast<float>(std::fmod(voiceIndex*0.61803398875,1.0)*2-1);
        for(int n=0;n<count;++n) {
            const float amp=ampEnvelope.getNextSample(), filterEnv=filterEnvelope.getNextSample();
            if(!ampEnvelope.isActive()) { clearCurrentNote(); break; }
            float lfo=0;
            switch(settings.lfoShape) {
                case 1: lfo=static_cast<float>(1.0-4.0*std::abs(lfoPhase-0.5)); break;
                case 2: lfo=lfoPhase<0.5 ? 1.0f : -1.0f; break;
                case 3: lfo=held; break;
                default: lfo=static_cast<float>(std::sin(lfoPhase*juce::MathConstants<double>::twoPi)); break;
            }
            if(settings.lfoDelay>0) lfo*=juce::jlimit(0.0f,1.0f,static_cast<float>((age-settings.lfoDelay)/0.25));
            lfoPhase+=settings.rate/sr;
            if(lfoPhase>=1.0) { lfoPhase-=std::floor(lfoPhase); held=lfoRandom.nextFloat()*2-1; }
            const float pw=juce::jlimit(0.05f,0.95f,settings.width+lfo*settings.pwm*0.4f);
            const double cents=settings.tune+settings.drift*(offset*12+3*std::sin(age*0.8+voiceIndex));
            const double modWheel=wheelSource ? *wheelSource : 0;
            const double hz=frequency*bend*std::pow(2.0,(settings.octave-1)+cents/1200.0+lfo*(settings.pitch+modWheel*settings.wheel)/12.0);
            const float dt=static_cast<float>(juce::jlimit(0.000001,0.45,hz/(sr*2)));
            const float octaves=filterEnv*settings.env*6.0f+(midiNote-60)*settings.key/12.0f
                               +lfo*settings.filterLfo*4.0f+settings.filterVelocity*(velocity-0.5f)*4.0f;
            const float cutoff=juce::jlimit(25.0f,static_cast<float>(juce::jmin(18000.0,sr*0.44)),cutoffSmooth.getNextValue()*std::pow(2.0f,octaves));
            filter1.setCutoffFrequency(cutoff); filter2.setCutoffFrequency(cutoff);
            const float drive=0.7f*(1.0f+driveSmooth.getNextValue()*4.0f);
            float sample=0;
            for(int os=0;os<2;++os) {
                const float t=static_cast<float>(phase);
                const float saw=2*t-1-blep(t,dt);
                float shifted=t-pw; if(shifted<0) shifted+=1;
                const float pulse=(t<pw ? 1.0f : -1.0f)+blep(t,dt)-blep(shifted,dt);
                const float st=static_cast<float>(subPhase);
                float subShift=st-0.5f; if(subShift<0) subShift+=1;
                const float sub=(st<0.5f ? 1.0f : -1.0f)+blep(st,dt*0.5f)-blep(subShift,dt*0.5f);
                const float mixed=settings.saw*saw+settings.pulse*pulse+settings.sub*sub+settings.noise*(random.nextFloat()*2-1);
                sample+=filter2.processSample(0,filter1.processSample(0,std::tanh(mixed*drive)))*0.5f;
                phase+=dt; phase-=std::floor(phase); subPhase+=dt*0.5; subPhase-=std::floor(subPhase);
            }
            sample*=amp*velocityGain*0.2f;
            for(int ch=0;ch<buffer.getNumChannels();++ch) buffer.addSample(ch,start+n,sample);
            age+=1.0/sr;
        }
        filter1.snapToZero(); filter2.snapToZero();
    }
private:
    const std::array<float,16>& wheels;
    const float* wheelSource=nullptr;
    int voiceIndex;
    juce::ADSR ampEnvelope,filterEnvelope;
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    juce::SmoothedValue<float> driveSmooth;
    juce::dsp::StateVariableTPTFilter<float> filter1,filter2;
    juce::Random random,lfoRandom;
    double frequency=440,phase=0,subPhase=0,lfoPhase=0,age=0,bend=1;
    int midiNote=60; float velocity=1,held=0;
};
struct Patch { const char* name; std::vector<std::pair<const char*,float>> values; };
const std::vector<Patch>& patches() {
    static const std::vector<Patch> bank {
        {"Init / Basic",{}},
        {"Velvet Strings",{{"saw",0.8f},{"pulse",0.25f},{"sub",0.15f},{"attack",0.8f},{"decay",1.2f},{"sustain",0.8f},{"release",1.8f},{"cutoff",2400},{"env",0.2f},{"fattack",1.0f},{"fdecay",1.4f},{"fsustain",0.55f},{"frelease",1.8f},{"drift",0.3f},{"chorus",2},{"chorusrate",0.85f},{"chorusdepth",0.65f},{"chorusmix",0.55f},{"stereo",1}}},
        {"Round Bass",{{"octave",0},{"saw",0.45f},{"pulse",0.35f},{"sub",0.75f},{"cutoff",350},{"resonance",0.3f},{"env",0.5f},{"attack",0.002f},{"decay",0.3f},{"sustain",0.6f},{"release",0.12f},{"fattack",0.002f},{"fdecay",0.2f},{"fsustain",0.05f},{"frelease",0.1f},{"drive",0.3f},{"chorus",0}}},
        {"Brass Section",{{"saw",0.8f},{"pulse",0.6f},{"cutoff",650},{"resonance",0.25f},{"env",0.65f},{"attack",0.04f},{"decay",0.5f},{"sustain",0.7f},{"release",0.3f},{"fattack",0.08f},{"fdecay",0.7f},{"fsustain",0.35f},{"frelease",0.3f},{"drift",0.2f}}},
        {"Glass Pluck",{{"saw",0.2f},{"pulse",0.8f},{"width",0.25f},{"sub",0.15f},{"cutoff",900},{"resonance",0.6f},{"env",0.7f},{"attack",0.001f},{"decay",0.9f},{"sustain",0},{"release",0.5f},{"fattack",0.001f},{"fdecay",0.23f},{"fsustain",0},{"frelease",0.2f},{"chorusmix",0.3f},{"velocity",0.7f},{"filtervelocity",0.5f}}},
        {"Slow Motion",{{"attack",1.6f},{"decay",2},{"sustain",0.8f},{"release",3.2f},{"cutoff",550},{"env",0.55f},{"fattack",2.5f},{"fdecay",1.5f},{"fsustain",0.6f},{"frelease",3},{"rate",0.16f},{"filterlfo",0.3f},{"pwm",0.7f},{"drift",0.4f},{"chorus",2},{"chorusrate",0.85f},{"chorusdepth",0.65f},{"chorusmix",0.6f}}},
        {"Pulse Lead",{{"saw",0.2f},{"pulse",0.9f},{"width",0.3f},{"sub",0.25f},{"cutoff",3500},{"env",0.2f},{"resonance",0.35f},{"attack",0.008f},{"release",0.18f},{"pitch",0.12f},{"rate",5.0f},{"lfodelay",0.25f},{"drift",0.15f},{"drive",0.2f},{"chorusmix",0.2f}}},
        {"Resonant Steps",{{"cutoff",400},{"resonance",0.65f},{"env",0.15f},{"filterlfo",0.8f},{"lfoshape",3},{"lfotrigger",1},{"rate",5.5f},{"pwm",0.4f},{"chorus",0},{"drive",0.1f}}},
        {"Soft Organ",{{"saw",0},{"pulse",0.7f},{"sub",0.6f},{"width",0.5f},{"cutoff",4200},{"env",0},{"pwm",0},{"attack",0.01f},{"decay",0.1f},{"sustain",1},{"release",0.08f},{"chorusmix",0.35f},{"velocity",0.25f}}}
    };
    return bank;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout SixthProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto add=[&](const char* id,const char* name,float lo,float hi,float initial,float skew=1.0f) {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
            juce::NormalisableRange<float>(lo,hi,0.0f,skew),initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v,int) { return juce::String(v,std::abs(v)>=100 ? 0 : 2); })
                .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue(); })));
    };
    // Keep the original parameter IDs and their order for existing DAW sessions.
    add("saw","Saw",0,1,0.8f); add("pulse","Pulse",0,1,0.4f); add("sub","Sub",0,1,0.3f); add("noise","Noise",0,1,0);
    add("width","Pulse width",0.05f,0.95f,0.5f);
    add("cutoff","Cutoff Hz",25,18000,1800,0.3f); add("resonance","Resonance",0,1,0.2f);
    add("env","Filter envelope",-1,1,0.4f); add("key","Keyboard tracking",0,1,0.5f);
    add("rate","LFO Hz",0.05f,20,0.8f,0.4f); add("pitch","LFO pitch semitones",0,2,0); add("pwm","LFO pulse width",0,1,0.2f);
    add("attack","Attack seconds",0.001f,4,0.01f,0.3f); add("decay","Decay seconds",0.01f,4,0.4f,0.3f);
    add("sustain","Sustain",0,1,0.7f); add("release","Release seconds",0.01f,4,0.3f,0.3f);
    add("hpf","High-pass Hz",20,2000,20,0.3f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"chorus",1},"Chorus",juce::StringArray{"Off","I","II"},1));
    add("volume","Output",0,1,0.7f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"octave",1},"Oscillator range",juce::StringArray{"16'","8'","4'"},1));
    add("tune","Fine tune cents",-50,50,0); add("drift","Analog drift",0,1,0.08f); add("drive","Filter drive",0,1,0);
    add("fattack","Filter attack seconds",0.001f,4,0.01f,0.3f); add("fdecay","Filter decay seconds",0.01f,4,0.4f,0.3f);
    add("fsustain","Filter sustain",0,1,0.7f); add("frelease","Filter release seconds",0.01f,4,0.3f,0.3f);
    add("filterlfo","LFO filter depth",0,1,0); add("lfodelay","LFO delay seconds",0,3,0,0.4f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"lfoshape",1},"LFO waveform",juce::StringArray{"Sine","Triangle","Square","Sample & Hold"},0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"lfotrigger",1},"LFO retrigger",false));
    add("velocity","Amp velocity",0,1,1); add("filtervelocity","Filter velocity",0,1,0);
    add("wheel","Mod wheel vibrato semitones",0,12,2);
    add("chorusmix","Chorus mix",0,1,0.45f); add("chorusrate","Chorus rate Hz",0.05f,4,0.5f,0.5f);
    add("chorusdepth","Chorus depth",0,1,0.4f); add("stereo","Stereo width",0,1,1);
    return p;
}
SixthProcessor::SixthProcessor() : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    parameters(*this,nullptr,"SixthState",layout()) {
    for(int i=0;i<6;++i) synth.addVoice(new Voice(i,synth.wheels));
    synth.addSound(new Sound()); synth.setNoteStealingEnabled(true);
    (void) patches();
}
bool SixthProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainInputChannelSet().isDisabled() && l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
}
void SixthProcessor::prepareToPlay(double sr,int block) {
    synth.allNotesOff(0,false); synth.wheels.fill(0); synth.setCurrentPlaybackSampleRate(sr);
    juce::dsp::ProcessSpec spec{sr,static_cast<juce::uint32>(juce::jmax(1,block)),2};
    for(size_t i=0;i<chorus.size();++i) {
        auto mono=spec; mono.numChannels=1;
        chorus[i].prepare(mono); chorus[i].reset(); chorus[i].setCentreDelay(i==0 ? 7.0f : 9.0f); chorus[i].setFeedback(0.05f);
    }
    highpass.prepare(spec); highpass.setType(juce::dsp::StateVariableTPTFilterType::highpass); highpass.setResonance(0.7071f); highpass.reset();
    gain.reset(sr,0.02); gain.setCurrentAndTargetValue(value("volume")); keyboard.reset(); outputLevel.store(0);
}
void SixthProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals guard;
    audio.clear(); keyboard.processNextMidiBuffer(midi,0,audio.getNumSamples(),true);
    Settings s{value("saw"),value("pulse"),value("sub"),value("noise"),value("width"),value("cutoff"),value("resonance"),value("env"),value("key"),value("rate"),value("pitch"),value("pwm"),
        value("attack"),value("decay"),value("sustain"),value("release"),value("fattack"),value("fdecay"),value("fsustain"),value("frelease"),
        value("tune"),value("drift"),value("drive"),value("filterlfo"),value("lfodelay"),value("velocity"),value("filtervelocity"),value("wheel"),
        juce::roundToInt(value("octave")),juce::roundToInt(value("lfoshape")),value("lfotrigger")>=0.5f};
    for(int i=0;i<synth.getNumVoices();++i) static_cast<Voice*>(synth.getVoice(i))->settings=s;
    synth.renderNextBlock(audio,midi,0,audio.getNumSamples());
    juce::dsp::AudioBlock<float> block(audio);
    juce::dsp::ProcessContextReplacing<float> context(block);
    highpass.setCutoffFrequency(value("hpf")); highpass.process(context);
    const int mode=juce::roundToInt(value("chorus"));
    for(size_t i=0;i<chorus.size();++i) {
        chorus[i].setRate(value("chorusrate")*(i==1 ? 1.07f : 1.0f));
        chorus[i].setCentreDelay((mode==2 ? 6.0f : 7.0f)+(i==1 ? 2.0f : 0.0f));
        chorus[i].setFeedback(mode==2 ? 0.12f : 0.05f);
        chorus[i].setDepth(value("chorusdepth"));
        chorus[i].setMix(mode==0 ? 0.0f : value("chorusmix"));
        auto channel=block.getSingleChannelBlock(i);
        juce::dsp::ProcessContextReplacing<float> channelContext(channel); chorus[i].process(channelContext);
    }
    gain.setTargetValue(value("volume"));
    const float width=value("stereo"); float peak=0;
    for(int n=0;n<audio.getNumSamples();++n) {
        const float g=gain.getNextValue();
        const float mid=(audio.getSample(0,n)+audio.getSample(1,n))*0.5f;
        const float side=(audio.getSample(0,n)-audio.getSample(1,n))*0.5f*width;
        const float left=std::tanh((mid+side)*g),right=std::tanh((mid-side)*g);
        audio.setSample(0,n,left); audio.setSample(1,n,right); peak=juce::jmax(peak,std::abs(left),std::abs(right));
    }
    outputLevel.store(juce::jmax(peak,outputLevel.load()*0.85f),std::memory_order_relaxed);
    midi.clear();
}
int SixthProcessor::getNumPrograms() { return static_cast<int>(patches().size()); }
const juce::String SixthProcessor::getProgramName(int index) {
    return juce::isPositiveAndBelow(index,getNumPrograms()) ? patches()[static_cast<size_t>(index)].name : juce::String();
}
bool SixthProcessor::isCurrentProgramModified() const {
    const auto& patch=patches()[static_cast<size_t>(juce::jlimit(0,static_cast<int>(patches().size())-1,selectedProgram.load()))];
    for(auto* base:getParameters()) {
        const auto* parameter=dynamic_cast<const juce::RangedAudioParameter*>(base);
        if(!parameter) continue;
        float expected=parameter->getDefaultValue();
        for(const auto& item:patch.values)
            if(parameter->paramID==item.first) expected=parameter->convertTo0to1(item.second);
        if(std::abs(parameter->getValue()-expected)>0.0001f) return true;
    }
    return false;
}
void SixthProcessor::setCurrentProgram(int index) {
    if(!juce::isPositiveAndBelow(index,getNumPrograms())) return;
    for(auto* parameter:getParameters()) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    for(const auto& item:patches()[static_cast<size_t>(index)].values) {
        auto* parameter=parameters.getParameter(item.first);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(item.second));
    }
    selectedProgram.store(index);
}
juce::AudioProcessorEditor* SixthProcessor::createEditor() { return new SixthEditor(*this); }
void SixthProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto state=parameters.copyState(); state.setProperty("program",selectedProgram.load(),nullptr); state.setProperty("schema",2,nullptr);
    auto xml=state.createXml(); copyXmlToBinary(*xml,data);
}
void SixthProcessor::setStateInformation(const void* data,int size) {
    auto xml=getXmlFromBinary(data,size);
    if(!xml || !xml->hasTagName(parameters.state.getType())) return;
    auto state=juce::ValueTree::fromXml(*xml);
    const bool legacy=static_cast<int>(state.getProperty("schema",1))<2;
    for(auto* base:getParameters()) {
        auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(base);
        if(!parameter || state.getChildWithProperty("id",parameter->paramID).isValid()) continue;
        float raw=parameter->convertFrom0to1(parameter->getDefaultValue());
        if(legacy) {
            const juce::String id=parameter->paramID;
            if(id=="fattack" || id=="fdecay" || id=="fsustain" || id=="frelease") {
                const auto source=state.getChildWithProperty("id",id.substring(1));
                if(source.isValid()) raw=static_cast<float>(source.getProperty("value",raw));
            }
            if(id=="drift" || id=="stereo") raw=0;
            const auto oldChorus=state.getChildWithProperty("id","chorus");
            if(oldChorus.isValid() && static_cast<int>(oldChorus.getProperty("value",1))==2) {
                if(id=="chorusrate") raw=0.85f;
                if(id=="chorusdepth") raw=0.65f;
            }
        }
        juce::ValueTree child("PARAM"); child.setProperty("id",parameter->paramID,nullptr); child.setProperty("value",raw,nullptr); state.addChild(child,-1,nullptr);
    }
    selectedProgram.store(juce::jlimit(0,getNumPrograms()-1,static_cast<int>(state.getProperty("program",0))));
    parameters.replaceState(state);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SixthProcessor(); }

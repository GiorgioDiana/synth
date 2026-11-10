#include "PluginProcessor.h"
#include "ChordTests.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void set(SixthProcessor& p,const char* id,float v) {
    auto* parameter=p.parameters.getParameter(id);
    check(parameter!=nullptr,(juce::String("Unknown parameter: ")+id).toRawUTF8());
    parameter->setValueNotifyingHost(parameter->convertTo0to1(v));
}
float render(juce::AudioProcessor& p,int blocks,int samples,juce::MidiBuffer midi={}) {
    float peak=0;
    for(int b=0;b<blocks;++b) {
        juce::AudioBuffer<float> audio(2,samples); audio.clear();
        p.processBlock(audio,midi); midi.clear();
        for(int ch=0;ch<2;++ch) for(int n=0;n<samples;++n) {
            const float v=audio.getSample(ch,n);
            check(std::isfinite(v),"Non-finite output"); check(std::abs(v)<=1.001f,"Output exceeded bound");
            peak=juce::jmax(peak,std::abs(v));
        }
    }
    return peak;
}

namespace {
constexpr double testRate=48000.0;
using Samples=std::vector<float>;

void neutralPatch(SixthProcessor& p) {
    // Remove random, time-varying and nonlinear layers before measuring controls.
    set(p,"saw",0.8f); set(p,"pulse",0); set(p,"sub",0); set(p,"noise",0);
    set(p,"octave",1); set(p,"tune",0); set(p,"drift",0); set(p,"drive",0);
    set(p,"cutoff",18000); set(p,"resonance",0); set(p,"env",0); set(p,"key",0);
    set(p,"filterlfo",0); set(p,"filtervelocity",0); set(p,"velocity",0);
    set(p,"pitch",0); set(p,"pwm",0); set(p,"wheel",0); set(p,"lfodelay",0);
    set(p,"lfoshape",0); set(p,"lfotrigger",1); set(p,"rate",3);
    set(p,"attack",0.001f); set(p,"decay",0.1f); set(p,"sustain",1); set(p,"release",0.05f);
    set(p,"fattack",0.001f); set(p,"fdecay",0.1f); set(p,"fsustain",1); set(p,"frelease",0.05f);
    set(p,"hpf",20); set(p,"chorus",0); set(p,"chorusmix",0); set(p,"stereo",1); set(p,"volume",0.5f);
}

struct StereoSamples { Samples left,right; };
StereoSamples captureStereo(const std::function<void(SixthProcessor&)>& configure={},float velocity=0.8f,int blocks=150) {
    SixthProcessor p;
    neutralPatch(p);
    if(configure) configure(p);
    p.prepareToPlay(testRate,256);
    StereoSamples result; result.left.reserve(static_cast<size_t>(blocks)*256); result.right.reserve(static_cast<size_t>(blocks)*256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,69,velocity),0);
    for(int b=0;b<blocks;++b) {
        juce::AudioBuffer<float> audio(2,256); audio.clear();
        p.processBlock(audio,midi); midi.clear();
        for(int n=0;n<256;++n) {
            for(int ch=0;ch<2;++ch)
                check(std::isfinite(audio.getSample(ch,n)),"Non-finite output in control regression");
            result.left.push_back(audio.getSample(0,n)); result.right.push_back(audio.getSample(1,n));
        }
    }
    return result;
}
Samples capture(const std::function<void(SixthProcessor&)>& configure={},float velocity=0.8f,int blocks=150) {
    return captureStereo(configure,velocity,blocks).left;
}

double rms(const Samples& samples,size_t first=12000) {
    check(first<samples.size(),"Invalid RMS window");
    double energy=0;
    for(size_t i=first;i<samples.size();++i) energy+=static_cast<double>(samples[i])*samples[i];
    return std::sqrt(energy/static_cast<double>(samples.size()-first));
}

double difference(const Samples& a,const Samples& b,size_t first=12000) {
    check(a.size()==b.size() && first<a.size(),"Invalid comparison window");
    double energy=0;
    for(size_t i=first;i<a.size();++i) {
        const double delta=static_cast<double>(a[i])-b[i]; energy+=delta*delta;
    }
    return std::sqrt(energy/static_cast<double>(a.size()-first));
}

double frequency(const Samples& samples) {
    constexpr size_t first=12000;
    int crossings=0;
    for(size_t i=first+1;i<samples.size();++i)
        if(samples[i-1]<=0 && samples[i]>0) ++crossings;
    return crossings*testRate/static_cast<double>(samples.size()-first-1);
}

void testExpandedControls() {
    {
        SixthProcessor p;
        const char* legacyIds[]={"saw","pulse","sub","noise","width","cutoff","resonance","env","key","rate","pitch","pwm","attack","decay","sustain","release","hpf","chorus","volume"};
        const auto& parameters=p.getParameters();
        check(parameters.size()>19,"Expanded parameters are missing");
        for(int i=0;i<19;++i) {
            const auto* parameter=dynamic_cast<const juce::RangedAudioParameter*>(parameters[i]);
            check(parameter && parameter->paramID==legacyIds[i] && parameter->getVersionHint()==1,
                  "Published v0.1 parameter ID/order/version changed");
        }
        for(int i=19;i<parameters.size();++i)
            check(parameters[i]->getVersionHint()>1,"New controls would reorder legacy Audio Unit automation");
        std::cout<<"PASS stable published parameter IDs/order and AU automation versioning\n";
    }
    const auto reference=capture();
    const auto lower=capture([](SixthProcessor& p){set(p,"octave",0);});
    const auto upper=capture([](SixthProcessor& p){set(p,"octave",2);});
    const double centreHz=frequency(reference), lowHz=frequency(lower), highHz=frequency(upper);
    check(centreHz>430 && centreHz<450,"Reference note is not concert A");
    check(std::abs(centreHz/lowHz-2.0)<0.04 && std::abs(highHz/centreHz-2.0)<0.04,
          "Octave range did not transpose by an octave");
    std::cout<<"PASS octave range: "<<lowHz<<", "<<centreHz<<", "<<highHz<<" Hz\n";

    const auto dark=capture([](SixthProcessor& p){
        set(p,"cutoff",150); set(p,"env",0.8f); set(p,"fsustain",0);
    });
    const auto bright=capture([](SixthProcessor& p){
        set(p,"cutoff",150); set(p,"env",0.8f); set(p,"fsustain",1);
    });
    check(rms(bright)>rms(dark)*1.5,"Independent filter envelope did not change sustained brightness");
    check(rms(dark)>1e-6,"Filter envelope test did not render a sustained amp envelope");
    std::cout<<"PASS independent filter envelope with unchanged sustained amplifier\n";

    const auto staticFilter=capture([](SixthProcessor& p){set(p,"cutoff",400);});
    const auto movingFilter=capture([](SixthProcessor& p){set(p,"cutoff",400);set(p,"filterlfo",0.8f);});
    check(difference(staticFilter,movingFilter)>rms(staticFilter)*0.1,
          "Filter LFO depth did not modulate the sound");
    std::cout<<"PASS filter LFO depth changes rendered audio\n";

    const auto holdStatic=capture([](SixthProcessor& p){
        set(p,"cutoff",400);set(p,"lfoshape",3);set(p,"rate",0.05f);set(p,"lfotrigger",0);
    },0.8f,94);
    const auto holdModulated=capture([](SixthProcessor& p){
        set(p,"cutoff",400);set(p,"lfoshape",3);set(p,"rate",0.05f);set(p,"lfotrigger",0);set(p,"filterlfo",0.8f);
    },0.8f,94);
    check(difference(holdStatic,holdModulated,6000)>rms(holdStatic,6000)*0.1,
          "Free-running Sample & Hold did not modulate during its first half-second");
    std::cout<<"PASS slow Sample & Hold starts with an audible held value\n";

    SixthProcessor reused;
    neutralPatch(reused); set(reused,"attack",0.4f);
    reused.prepareToPlay(44100,256);
    // Prime every voice so the second rate cannot accidentally select a voice
    // whose ADSR parameters have never been configured at the first rate.
    juce::MidiBuffer prime;
    for(int note=60;note<66;++note) prime.addEvent(juce::MidiMessage::noteOn(1,note,0.8f),0);
    render(reused,1,256,prime);
    const auto attackRatio=[&](double rate) {
        reused.prepareToPlay(rate,256);
        Samples samples;
        const int blocks=static_cast<int>(std::ceil(rate*0.7/256));
        samples.reserve(static_cast<size_t>(blocks)*256);
        juce::MidiBuffer events; events.addEvent(juce::MidiMessage::noteOn(1,69,0.8f),0);
        for(int b=0;b<blocks;++b) {
            juce::AudioBuffer<float> audio(2,256); audio.clear();
            reused.processBlock(audio,events); events.clear();
            for(int n=0;n<256;++n) {
                const float value=audio.getSample(0,n);
                check(std::isfinite(value),"Non-finite audio after sample-rate change");
                samples.push_back(value);
            }
        }
        const auto windowRms=[&](double begin,double end) {
            const size_t first=static_cast<size_t>(begin*rate), last=static_cast<size_t>(end*rate);
            double energy=0;
            for(size_t i=first;i<last;++i) energy+=static_cast<double>(samples[i])*samples[i];
            return std::sqrt(energy/static_cast<double>(last-first));
        };
        return windowRms(0.08,0.12)/windowRms(0.55,0.65);
    };
    // Keep every parameter unchanged between these prepares: cached envelope
    // slopes must follow the sample rate without requiring a knob movement.
    const double attack44100=attackRatio(44100),attack96000=attackRatio(96000);
    check(attack44100>0.15 && attack44100<0.35,"Attack timing reference did not reach the expected envelope region");
    check(std::abs(attack96000/attack44100-1.0)<0.1,
          "Attack duration changed after reusing the processor at a new sample rate");
    std::cout<<"PASS unchanged attack timing across sample-rate changes: "<<attack44100<<", "<<attack96000<<"\n";

    const auto softUnscaled=capture({},0.2f), hardUnscaled=capture({},1.0f);
    check(std::abs(rms(softUnscaled)/rms(hardUnscaled)-1.0)<0.02,
          "Disabling velocity sensitivity did not remove amplitude response");
    const auto softScaled=capture([](SixthProcessor& p){set(p,"velocity",1);},0.2f);
    const auto hardScaled=capture([](SixthProcessor& p){set(p,"velocity",1);},1.0f);
    check(rms(softScaled)<rms(hardScaled)*0.4,"Velocity sensitivity did not respond to note velocity");
    std::cout<<"PASS adjustable velocity sensitivity\n";

    const auto dryChorus=capture([](SixthProcessor& p){
        set(p,"chorus",1);set(p,"chorusmix",0);set(p,"chorusrate",0.8f);set(p,"chorusdepth",0.8f);
    });
    const auto wetChorus=capture([](SixthProcessor& p){
        set(p,"chorus",1);set(p,"chorusmix",1);set(p,"chorusrate",0.8f);set(p,"chorusdepth",0.8f);
    });
    check(difference(dryChorus,wetChorus)>rms(dryChorus)*0.1,"Chorus mix did not affect rendered audio");
    std::cout<<"PASS continuously adjustable chorus mix\n";

    auto driven=capture([](SixthProcessor& p){set(p,"drive",1);});
    const double driveLevel=rms(driven),referenceLevel=rms(reference);
    check(driveLevel>1e-5,"Drive control muted the signal");
    for(auto& sample:driven) sample*=static_cast<float>(referenceLevel/driveLevel);
    check(difference(reference,driven)>referenceLevel*0.05,
          "Filter drive changed level without changing the waveform");
    std::cout<<"PASS filter drive changes waveform after level matching\n";

    const auto mono=captureStereo([](SixthProcessor& p){
        set(p,"chorus",1);set(p,"chorusmix",0.65f);set(p,"chorusrate",0.8f);set(p,"chorusdepth",0.8f);set(p,"stereo",0);
    });
    const auto wide=captureStereo([](SixthProcessor& p){
        set(p,"chorus",1);set(p,"chorusmix",0.65f);set(p,"chorusrate",0.8f);set(p,"chorusdepth",0.8f);set(p,"stereo",1);
    });
    check(difference(mono.left,mono.right)<1e-8,"Zero stereo width did not produce mono output");
    check(difference(wide.left,wide.right)>rms(wide.left)*0.05,
          "Full stereo width did not produce side energy with chorus");
    std::cout<<"PASS stereo width controls chorus side energy\n";

    SixthProcessor presets;
    check(presets.getNumPrograms()>=6,"Factory preset bank is missing");
    presets.setCurrentProgram(1);
    check(presets.getCurrentProgram()==1 && presets.getProgramName(1).isNotEmpty(),"Factory preset selection failed");
    check(!presets.isCurrentProgramModified(),"Fresh factory preset incorrectly marked edited");
    set(presets,"fattack",0.23f); set(presets,"chorusmix",0.63f);
    check(presets.isCurrentProgramModified(),"Edited preset was not marked modified");
    juce::MemoryBlock saved; presets.getStateInformation(saved);
    presets.setCurrentProgram(0); set(presets,"fattack",0.001f); set(presets,"chorusmix",0);
    presets.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
    check(presets.getCurrentProgram()==1,"Selected preset was not restored with host state");
    check(std::abs(presets.parameters.getRawParameterValue("fattack")->load()-0.23f)<0.001f
          && std::abs(presets.parameters.getRawParameterValue("chorusmix")->load()-0.63f)<0.001f,
          "Expanded sound controls were not restored with host state");
    const auto initPreset=capture([](SixthProcessor& p){p.setCurrentProgram(0);});
    const auto nextPreset=capture([](SixthProcessor& p){p.setCurrentProgram(1);});
    check(rms(initPreset)>1e-5 && rms(nextPreset)>1e-5
          && difference(initPreset,nextPreset)>rms(initPreset)*0.05,
          "Factory preset change did not produce a distinct sound");
    std::cout<<"PASS audible factory presets and complete host-state recall\n";

    // Recreate a v0.1 host state without any new IDs or schema/preset metadata.
    juce::ValueTree legacy("SixthState");
    const std::pair<const char*,float> oldValues[]={{"attack",0.12f},{"decay",0.75f},{"sustain",0.43f},{"release",0.9f},{"cutoff",812.0f}};
    for(const auto& item:oldValues) {
        juce::ValueTree parameter("PARAM");
        parameter.setProperty("id",item.first,nullptr); parameter.setProperty("value",item.second,nullptr);
        legacy.addChild(parameter,-1,nullptr);
    }
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),oldState);
    presets.setCurrentProgram(1);
    presets.setStateInformation(oldState.getData(),static_cast<int>(oldState.getSize()));
    check(std::abs(presets.parameters.getRawParameterValue("cutoff")->load()-812.0f)<0.1f,
          "Legacy cutoff was overwritten during migration");
    const char* envelopeIds[]={"fattack","fdecay","fsustain","frelease"};
    for(int i=0;i<4;++i)
        check(std::abs(presets.parameters.getRawParameterValue(envelopeIds[i])->load()-oldValues[i].second)<0.001f,
              "Legacy shared envelope was not copied into independent filter ADSR");
    check(presets.parameters.getRawParameterValue("drift")->load()==0
          && presets.parameters.getRawParameterValue("stereo")->load()==0,
          "Legacy migration introduced new detuning or stereo width");
    presets.prepareToPlay(testRate,256);
    juce::MidiBuffer note; note.addEvent(juce::MidiMessage::noteOn(1,60,0.8f),0);
    check(render(presets,100,256,note)>1e-5,"Migrated legacy patch produced no audio");
    std::cout<<"PASS legacy session migration preserves cutoff, shared envelope and mono tuning\n";
}
}

int main(int argc,char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI init;
        const bool chordPreviewMode=argc>4 && juce::String(argv[4])=="chords";
        testExpandedControls();
        runChordTests();
        SixthProcessor p;
        for(double sr:{44100.0,48000.0,96000.0}) {
            set(p,"release",0.05f); set(p,"chorus",0);
            p.prepareToPlay(sr,256);
            check(render(p,2,256)<1e-7f,"Initial output not silent");
            juce::MidiBuffer midi;
            for(int note=60;note<68;++note) midi.addEvent(juce::MidiMessage::noteOn(1,note,0.8f),0);
            check(render(p,20,256,midi)>0.001f,"Polyphonic note-on silent");
            midi.clear(); midi.addEvent(juce::MidiMessage::allNotesOff(1),0);
            render(p,static_cast<int>(sr/256),256,midi);
            check(render(p,4,256)<1e-5f,"Notes stuck after release");
            p.releaseResources();
            std::cout<<"PASS polyphony/stealing, release, finite output at "<<sr<<" Hz\n";
        }
        set(p,"cutoff",733); set(p,"chorus",2);
        juce::MemoryBlock state; p.getStateInformation(state);
        set(p,"cutoff",100); set(p,"chorus",0); p.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        check(std::abs(p.parameters.getRawParameterValue("cutoff")->load()-733)<0.1f,"State cutoff restore failed");
        check(p.parameters.getRawParameterValue("chorus")->load()==2,"State chorus restore failed");
        std::cout<<"PASS parameter state round trip\n";
        {
            std::unique_ptr<SixthProcessor> chordPreview;
            auto* previewProcessor=&p;
            if(chordPreviewMode) {
                chordPreview=std::make_unique<SixthProcessor>();
                chordPreview->setCurrentProgram(8);
                int bank=0;
                const auto& banks=SixthChords::banks();
                for(size_t i=0;i<banks.size();++i) if(juce::String(banks[i].category)=="Bach / Chorale") { bank=static_cast<int>(i); break; }
                if(argc>5) bank=juce::jlimit(0,static_cast<int>(banks.size())-1,juce::String(argv[5]).getIntValue());
                set(*chordPreview,"chordMode",1); set(*chordPreview,"chordBank",static_cast<float>(bank));
                chordPreview->prepareToPlay(48000,256);
                previewProcessor=chordPreview.get();
            }
            std::unique_ptr<juce::AudioProcessorEditor> editor(previewProcessor->createEditor());
            check(editor && editor->getWidth()>=800 && editor->getHeight()>=400,"Editor construction failed");
            if(chordPreviewMode) {
                const int input=argc>6 ? juce::jlimit(24,72,juce::String(argv[6]).getIntValue()) : 48;
                juce::MidiBuffer trigger; trigger.addEvent(juce::MidiMessage::noteOn(1,input,0.8f),0);
                render(*chordPreview,40,256,trigger);
                juce::Thread::sleep(70);
                juce::Timer::callPendingTimersSynchronously();
                auto* popup=editor->findChildWithID("chordPopup");
                // The popup lives in the scaled panel rather than at the editor root.
                if(popup==nullptr)
                    for(auto* child:editor->getChildren())
                        if((popup=child->findChildWithID("chordPopup"))!=nullptr) break;
                check(popup!=nullptr && popup->isVisible(),"Live chord feedback did not appear");
            }
            juce::Image image(juce::Image::RGB,editor->getWidth(),editor->getHeight(),true);
            juce::Graphics graphics(image); editor->paintEntireComponent(graphics,true);
            if(argc>2) {
                juce::File file {juce::String(argv[2])};
                auto output=file.createOutputStream();
                check(output!=nullptr,"Editor image cannot be opened");
                output->setPosition(0); output->truncate();
                check(juce::PNGImageFormat().writeImageToStream(image,*output),"Editor image write failed");
            }
            std::cout<<"PASS editor construction and offscreen painting\n";
        }
        p.prepareToPlay(48000,64); set(p,"cutoff",18000); set(p,"resonance",1); set(p,"env",1);
        juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::noteOn(1,100,1.0f),0);
        check(render(p,300,64,midi)>0.001f,"Extreme filter test silent");
        std::cout<<"PASS extreme filter and chorus stability\n";
        check(argc>=2,"Missing VST3 bundle path");
        juce::VST3PluginFormat format; juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile(descriptions,argv[1]);
        check(descriptions.size()==1 && descriptions[0]->isInstrument,"VST3 scan failed");
        juce::String error;
        auto plugin=format.createInstanceFromDescription(*descriptions[0],48000,256,error);
        check(plugin!=nullptr,"VST3 load failed");
        check(plugin->acceptsMidi(),"VST3 MIDI input absent");
        plugin->prepareToPlay(48000,256); midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1,69,0.8f),0);
        check(render(*plugin,20,256,midi)>0.001f,"Loaded VST3 produced no audio");
        plugin->releaseResources();
        std::cout<<"PASS actual VST3 scan/load and MIDI audio\n";
        if(argc>3) {
            juce::File destination {juce::String(argv[3])};
            auto fileStream=destination.createOutputStream();
            check(fileStream!=nullptr,"Demo file cannot be opened");
            fileStream->setPosition(0); fileStream->truncate();
            std::unique_ptr<juce::OutputStream> stream=std::move(fileStream);
            juce::WavAudioFormat wav;
            auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(writer!=nullptr,"Demo writer unavailable");
            if(chordPreviewMode) {
                const char* families[]={"Funk / Neo-soul","Jazz / Evans-inspired","Bach / Chorale","Chopin / Romantic"};
                const int sounds[]={3,4,8,4};
                // Mix style-core chords with the new lower/upper-page landings.
                const int triggers[4][4]={{25,49,55,72},{50,51,48,72},{60,26,35,72},{48,50,57,72}};
                for(int section=0;section<4;++section) {
                    int bank=0;
                    const auto& banks=SixthChords::banks();
                    for(size_t i=0;i<banks.size();++i) if(juce::String(banks[i].category)==families[section]) { bank=static_cast<int>(i); break; }
                    SixthProcessor demo; demo.setCurrentProgram(sounds[section]);
                    set(demo,"chordMode",1); set(demo,"chordBank",static_cast<float>(bank));
                    set(demo,"release",0.15f); set(demo,"frelease",0.15f); set(demo,"chorus",0);
                    demo.prepareToPlay(48000,256);
                    for(int block=0;block<750;++block) {
                        juce::MidiBuffer events;
                        const int begin=block*256,end=begin+256;
                        for(int beat=0;beat<4;++beat) {
                            const int on=beat*48000,off=on+36000;
                            const int key=triggers[section][beat];
                            if(on>=begin && on<end) events.addEvent(juce::MidiMessage::noteOn(1,key,0.8f),on-begin);
                            if(off>=begin && off<end) events.addEvent(juce::MidiMessage::noteOff(1,key),off-begin);
                        }
                        juce::AudioBuffer<float> audio(2,256); demo.processBlock(audio,events);
                        check(writer->writeFromAudioSampleBuffer(audio,0,256),"Chord preview write failed");
                    }
                }
                std::cout<<"Rendered chord preview: funk, rootless jazz, chorale, Romantic harmony\n";
            } else {
                for(int patch:{1,2,4,7}) {
                    SixthProcessor demo; demo.setCurrentProgram(patch); demo.prepareToPlay(48000,256);
                    for(int block=0;block<750;++block) {
                        juce::MidiBuffer events;
                        if(block==0) {
                            if(patch==2) events.addEvent(juce::MidiMessage::noteOn(1,48,0.8f),0);
                            else for(int note:{60,64,67}) events.addEvent(juce::MidiMessage::noteOn(1,note,0.8f),0);
                        }
                        if(block==500) events.addEvent(juce::MidiMessage::allNotesOff(1),0);
                        juce::AudioBuffer<float> audio(2,256); demo.processBlock(audio,events);
                        check(writer->writeFromAudioSampleBuffer(audio,0,256),"Demo write failed");
                    }
                }
            }
            if(!chordPreviewMode) std::cout<<"Rendered 16-second factory preview: strings, bass, pluck, resonant steps\n";
        }
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
    return 0;
}

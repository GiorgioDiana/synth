#include "PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void set(SixthProcessor& p,const char* id,float v) {
    auto* parameter=p.parameters.getParameter(id);
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
int main(int argc,char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI init;
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
            std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
            check(editor && editor->getWidth()==960,"Editor construction failed");
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
            SixthProcessor demo; demo.prepareToPlay(48000,256);
            juce::File destination {juce::String(argv[3])};
            std::unique_ptr<juce::OutputStream> stream=destination.createOutputStream();
            check(stream!=nullptr,"Demo file cannot be opened");
            stream->setPosition(0);
            juce::WavAudioFormat wav;
            auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(writer!=nullptr,"Demo writer unavailable");
            const int chords[4][3]={{48,55,60},{53,60,65},{45,52,57},{43,50,55}};
            for(int b=0;b<1500;++b) {
                juce::MidiBuffer events;
                if(b%375==0) {
                    events.addEvent(juce::MidiMessage::allNotesOff(1),0);
                    for(int note:chords[b/375]) events.addEvent(juce::MidiMessage::noteOn(1,note,0.8f),0);
                }
                if(b%375==300) events.addEvent(juce::MidiMessage::allNotesOff(1),0);
                juce::AudioBuffer<float> audio(2,256); demo.processBlock(audio,events);
                check(writer->writeFromAudioSampleBuffer(audio,0,256),"Demo write failed");
            }
            std::cout<<"Rendered 8-second stereo demo\n";
        }
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
    return 0;
}

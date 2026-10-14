#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "GlitchEngine.h"
#include <atomic>
#include <memory>

struct SampleData
{
    juce::AudioBuffer<float> buffer;   // siempre 2 canales
    double sampleRate = 44100.0;
    juce::String path;
    juce::String name;
    std::vector<float> peaks;          // para dibujar la onda (pares min/max)
};

class CumbiaGlitchProcessor : public juce::AudioProcessor
{
public:
    CumbiaGlitchProcessor();
    ~CumbiaGlitchProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CumbiaGlitch"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // carga un sample en el canal 0 (A) o 1 (B). Devuelve false si no se pudo leer.
    bool loadSample (int channel, const juce::File& file);
    std::shared_ptr<const SampleData> getSample (int channel) const;
    int getSampleVersion (int channel) const { return sampleVersion[channel].load(); }
    float getPlayhead (int channel) const { return playhead[channel].load(); }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    struct Voice
    {
        double pos = 0.0;
        juce::dsp::StateVariableTPTFilter<float> filter;
        juce::SmoothedValue<float> cutoff;
    };

    float readSample (const SampleData& s, int ch, double pos) const;

    juce::AudioFormatManager formatManager;

    mutable juce::SpinLock sampleLock;
    std::shared_ptr<SampleData> samples[2];
    std::shared_ptr<SampleData> retired[2];
    std::atomic<int> sampleVersion[2] { {0}, {0} };
    std::atomic<float> playhead[2] { {-1.0f}, {-1.0f} };

    Voice voices[2];
    GlitchEngine glitch;

    // modo MIDI
    bool gate = false;
    int currentNote = -1;
    float velocity = 1.0f;
    double pitchRatio = 1.0;
    float env = 0.0f;
    double freePpq = 0.0;

    juce::SmoothedValue<float> xfadeSm, wetSm, gainSm;
    double hostSr = 44100.0;
    bool wasPlaying = false;

    std::atomic<float>* p[16] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CumbiaGlitchProcessor)
};

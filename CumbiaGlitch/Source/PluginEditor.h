#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class CumbiaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CumbiaLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};

// Muestra la onda del sample, la zona elegida (inicio/fin) y el cabezal de reproduccion.
// Se puede arrastrar el inicio y el fin directamente sobre la onda.
class WaveformView : public juce::Component
{
public:
    WaveformView (CumbiaGlitchProcessor&, int channel, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();

private:
    CumbiaGlitchProcessor& proc;
    int channel;
    juce::Colour accent;
    juce::RangedAudioParameter* startParam;
    juce::RangedAudioParameter* endParam;
    juce::RangedAudioParameter* dragging = nullptr;
    float lastPlayhead = -1.0f;
};

struct Knob
{
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ChannelPanel : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    ChannelPanel (CumbiaGlitchProcessor&, int channel, juce::Colour accent, const juce::String& title);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

private:
    void loadFile (const juce::File&);

    CumbiaGlitchProcessor& proc;
    int channel;
    juce::Colour accent;
    juce::String title;
    bool dragOver = false;
    int knownVersion = -1;

    juce::TextButton loadButton { "Cargar sample" };
    juce::Label fileLabel;
    WaveformView waveform;
    Knob start, end, cutoff;
    juce::ComboBox bars;
    juce::ToggleButton sync { "Sync al tempo" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> barsAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt;
    std::unique_ptr<juce::FileChooser> chooser;
};

class CumbiaGlitchEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit CumbiaGlitchEditor (CumbiaGlitchProcessor&);
    ~CumbiaGlitchEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& id, const juce::String& text, juce::Colour);

    CumbiaGlitchProcessor& proc;
    CumbiaLookAndFeel lnf;

    ChannelPanel panelA, panelB;
    Knob wetDry, chaos, texture, gain;
    juce::ComboBox mode;
    juce::Label modeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
    juce::Slider xfade;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xfadeAtt;
};

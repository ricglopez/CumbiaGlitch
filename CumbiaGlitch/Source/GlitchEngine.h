#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// Motor "glitch" estilo IDM: trocea, reordena, repite, granula y degrada el audio.
// Trabaja muestra a muestra y se sincroniza con el tempo (pasos de semicorchea).
class GlitchEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    // ppq = posicion en negras; samplesPerBeat = muestras por negra
    // chaos: probabilidad de que un paso se rompa; texture: intensidad de grano/degradacion
    void process (float& left, float& right, double ppq, double samplesPerBeat,
                  float chaos, float texture);

private:
    enum class Action { normal, reorder, reverse, stutter, granular, silence };

    struct Grain
    {
        bool active = false;
        double pos = 0.0, inc = 1.0;
        int length = 0, age = 0;
        float pan = 0.5f;
    };

    void startStep (double samplesPerBeat, float chaos, float texture);
    void spawnGrain (float texture);
    float readHistory (int channel, double pos) const;

    double sr = 44100.0;
    std::vector<float> history[2];
    int histSize = 0;
    int writePos = 0;

    juce::Random rng;
    long long lastStep = -1;
    Action action = Action::normal;

    int stepLen = 1, stepCounter = 0;
    double readPos = 0.0;
    double readInc = 1.0;

    // stutter
    double stutterStart = 0.0;
    double stutterLen = 64.0;
    double stutterShrink = 1.0;
    double stutterPhase = 0.0;

    // granular
    std::array<Grain, 24> grains {};
    int grainInterval = 1000, grainCounter = 0;

    // degradacion
    int downsample = 1, dsCounter = 0;
    float heldL = 0.0f, heldR = 0.0f;
    float bits = 16.0f;
    float ringFreq = 0.0f;
    double ringPhase = 0.0;

    // anti-clic en cambios de paso
    float lastOutL = 0.0f, lastOutR = 0.0f;
    float offsetL = 0.0f, offsetR = 0.0f;
    bool stepJustStarted = false;
};

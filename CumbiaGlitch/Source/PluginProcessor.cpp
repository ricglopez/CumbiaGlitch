#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    enum ParamIndex
    {
        aStart, aEnd, aCutoff, aBars, aSync,
        bStart, bEnd, bCutoff, bBars, bSync,
        xfade, wet, chaos, texture, mode, gain
    };

    const char* paramIds[] = {
        "aStart", "aEnd", "aCutoff", "aBars", "aSync",
        "bStart", "bEnd", "bCutoff", "bBars", "bSync",
        "xfade", "wet", "chaos", "texture", "mode", "gain"
    };

    const int barValues[] = { 1, 2, 4, 8, 16 };

    // limitador suave: lineal hasta 0.8, luego curva hacia 1.0 sin recortar en seco
    inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
        return x < 0.0f ? -y : y;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout CumbiaGlitchProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    NormalisableRange<float> cutRange (20.0f, 20000.0f);
    cutRange.setSkewForCentre (1000.0f);

    const StringArray barNames { String::fromUTF8 ("1 comp\xc3\xa1s"), "2 compases", "4 compases", "8 compases", "16 compases" };

    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String ((int) v) + " Hz"; });
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; });

    for (int ch = 0; ch < 2; ++ch)
    {
        const String n = ch == 0 ? "A" : "B";
        const int base = ch * 5;
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { paramIds[base + 0], 1 }, "Inicio " + n,
                                                           NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { paramIds[base + 1], 1 }, "Fin " + n,
                                                           NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { paramIds[base + 2], 1 }, "Filtro " + n,
                                                           cutRange, 20000.0f, hz));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramIds[base + 3], 1 }, "Compases " + n,
                                                            barNames, ch == 0 ? 1 : 0));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { paramIds[base + 4], 1 }, "Sync " + n, true));
    }

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "xfade", 1 }, "Crossfader A-B",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "wet", 1 }, "Wet/Dry",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "chaos", 1 }, "Caos",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "texture", 1 }, "Textura",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modo",
                                                        StringArray { "Loop", "MIDI" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Volumen",
                                                       NormalisableRange<float> (-36.0f, 6.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + " dB"; })));
    return layout;
}

CumbiaGlitchProcessor::CumbiaGlitchProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();
    for (int i = 0; i < 16; ++i)
        p[i] = apvts.getRawParameterValue (paramIds[i]);
}

bool CumbiaGlitchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CumbiaGlitchProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    for (int c = 0; c < 2; ++c)
    {
        auto& v = voices[c];
        v.filter.prepare (spec);
        v.filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        v.filter.setResonance (0.9f);
        v.cutoff.reset (sampleRate, 0.03);
        v.cutoff.setCurrentAndTargetValue (p[c * 5 + 2]->load());
        v.filter.setCutoffFrequency (v.cutoff.getCurrentValue());
    }
    xfadeSm.reset (sampleRate, 0.02);  xfadeSm.setCurrentAndTargetValue (p[xfade]->load());
    wetSm.reset (sampleRate, 0.03);    wetSm.setCurrentAndTargetValue (p[wet]->load());
    gainSm.reset (sampleRate, 0.03);   gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p[gain]->load()));
    glitch.prepare (sampleRate);
    gate = false; env = 0.0f; currentNote = -1;
}

float CumbiaGlitchProcessor::readSample (const SampleData& s, int ch, double pos) const
{
    const int n = s.buffer.getNumSamples();
    if (n < 2) return 0.0f;
    pos = juce::jlimit (0.0, (double) (n - 1), pos);
    const int i0 = (int) pos;
    const int i1 = juce::jmin (i0 + 1, n - 1);
    const float f = (float) (pos - i0);
    const float* d = s.buffer.getReadPointer (ch);
    return d[i0] + f * (d[i1] - d[i0]);
}

void CumbiaGlitchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    // --- informacion del DAW: tempo, posicion, play ---
    bool playing = false;
    double ppq = 0.0, bpm = 120.0;
    int num = 4, den = 4;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            if (auto q = pos->getPpqPosition()) ppq = *q;
            if (auto ts = pos->getTimeSignature()) { num = juce::jmax (1, ts->numerator); den = juce::jmax (1, ts->denominator); }
        }
    const double spb = hostSr * 60.0 / bpm;            // muestras por negra
    const double beatsPerBar = num * 4.0 / den;
    const bool midiMode = p[mode]->load() > 0.5f;

    // --- samples (sin bloquear el audio) ---
    std::shared_ptr<SampleData> s[2];
    {
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);
        if (! lock.isLocked()) return;
        s[0] = samples[0];
        s[1] = samples[1];
    }

    // --- region, velocidad y posicion de cada canal ---
    double regStart[2] {}, regLen[2] {}, rate[2] {};
    for (int c = 0; c < 2; ++c)
    {
        if (s[c] == nullptr) continue;
        const int n = s[c]->buffer.getNumSamples();
        double a = p[c * 5 + 0]->load(), b = p[c * 5 + 1]->load();
        if (a > b) std::swap (a, b);
        if (b - a < 0.002) b = juce::jmin (1.0, a + 0.002), a = b - 0.002;
        regStart[c] = a * n;
        regLen[c] = juce::jmax (64.0, (b - a) * n);

        const bool sync = p[c * 5 + 4]->load() > 0.5f;
        const double loopBeats = barValues[juce::jlimit (0, 4, (int) p[c * 5 + 3]->load())] * beatsPerBar;
        rate[c] = sync ? regLen[c] / (loopBeats * spb) : s[c]->sampleRate / hostSr;

        if (! midiMode && playing)
        {
            // posicion bloqueada al tiempo del DAW: siempre en fase con el proyecto
            double phase;
            if (sync)
            {
                phase = std::fmod (ppq, loopBeats) / loopBeats;
                if (phase < 0) phase += 1.0;
                voices[c].pos = regStart[c] + phase * regLen[c];
            }
            else
            {
                phase = std::fmod (ppq * spb * rate[c], regLen[c]);
                if (phase < 0) phase += regLen[c];
                voices[c].pos = regStart[c] + phase;
            }
        }
    }

    if (! midiMode && ! playing)
    {
        if (wasPlaying) glitch.reset();
        wasPlaying = false;
        playhead[0] = playhead[1] = -1.0f;
        return; // en modo Loop solo suena con el DAW en play
    }
    wasPlaying = playing;

    for (int c = 0; c < 2; ++c)
        voices[c].cutoff.setTargetValue (p[c * 5 + 2]->load());
    xfadeSm.setTargetValue (p[xfade]->load());
    wetSm.setTargetValue (p[wet]->load());
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (p[gain]->load()));
    const float chaosV = p[chaos]->load();
    const float textureV = p[texture]->load();

    const float attackCoef = 1.0f - std::exp (-1.0f / (0.003f * (float) hostSr));
    const float releaseCoef = 1.0f - std::exp (-1.0f / (0.040f * (float) hostSr));

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    auto midiIt = midi.cbegin();

    for (int i = 0; i < numSamples; ++i)
    {
        // eventos MIDI en su muestra exacta
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= i)
        {
            const auto msg = (*midiIt).getMessage();
            if (midiMode)
            {
                if (msg.isNoteOn())
                {
                    gate = true;
                    currentNote = msg.getNoteNumber();
                    velocity = msg.getFloatVelocity();
                    pitchRatio = std::pow (2.0, (currentNote - 60) / 12.0); // C3 = tono original
                    for (int c = 0; c < 2; ++c) voices[c].pos = regStart[c];
                }
                else if (msg.isNoteOff() && msg.getNoteNumber() == currentNote)
                {
                    gate = false;
                }
                else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                {
                    gate = false;
                }
            }
            ++midiIt;
        }

        float amp = 1.0f;
        double pitch = 1.0;
        if (midiMode)
        {
            env += ((gate ? 1.0f : 0.0f) - env) * (gate ? attackCoef : releaseCoef);
            amp = env * (0.3f + 0.7f * velocity);
            pitch = pitchRatio;
        }

        float chL[2] {}, chR[2] {};
        for (int c = 0; c < 2; ++c)
        {
            auto& v = voices[c];
            if ((i & 15) == 0)
                v.filter.setCutoffFrequency (v.cutoff.skip (16));

            if (s[c] == nullptr || amp < 1.0e-5f)
                continue;

            float l = readSample (*s[c], 0, v.pos) * amp;
            float r = readSample (*s[c], 1, v.pos) * amp;
            chL[c] = v.filter.processSample (0, l);
            chR[c] = v.filter.processSample (1, r);

            v.pos += rate[c] * pitch;
            const double regEnd = regStart[c] + regLen[c];
            while (v.pos >= regEnd) v.pos -= regLen[c];
            if (v.pos < regStart[c]) v.pos = regStart[c];
        }

        // crossfader: a la izquierda solo A, a la derecha solo B, en el centro los dos completos
        const float x = xfadeSm.getNextValue();
        const float gA = juce::jmin (1.0f, 2.0f * (1.0f - x));
        const float gB = juce::jmin (1.0f, 2.0f * x);
        const float dryL = (chL[0] * gA + chL[1] * gB) * 0.8f;
        const float dryR = (chR[0] * gA + chR[1] * gB) * 0.8f;

        // motor glitch (siempre corre para tener historial listo)
        double nowPpq;
        if (playing) nowPpq = ppq + i / spb;
        else         { nowPpq = freePpq; freePpq += 1.0 / spb; }

        float wL = dryL, wR = dryR;
        glitch.process (wL, wR, nowPpq, spb, chaosV, textureV);

        const float w = wetSm.getNextValue();
        const float g = gainSm.getNextValue();
        outL[i] = softClip ((dryL * (1.0f - w) + wL * w) * g);
        outR[i] = softClip ((dryR * (1.0f - w) + wR * w) * g);
    }

    for (int c = 0; c < 2; ++c)
        playhead[c] = (s[c] != nullptr && (! midiMode || env > 0.001f))
                          ? (float) (voices[c].pos / juce::jmax (1, s[c]->buffer.getNumSamples()))
                          : -1.0f;
}

bool CumbiaGlitchProcessor::loadSample (int channel, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 64)
        return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * 600.0); // maximo 10 minutos
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);

    auto data = std::make_shared<SampleData>();
    data->buffer.setSize (2, len);
    data->buffer.clear();
    reader->read (&data->buffer, 0, len, 0, true, reader->numChannels > 1);
    if (reader->numChannels == 1)
        data->buffer.copyFrom (1, 0, data->buffer, 0, 0, len);
    data->sampleRate = reader->sampleRate;
    data->path = file.getFullPathName();
    data->name = file.getFileName();

    const int bins = 600;
    data->peaks.resize (bins * 2);
    for (int b = 0; b < bins; ++b)
    {
        const int s0 = (int) ((juce::int64) b * len / bins);
        const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (b + 1) * len / bins));
        float mn = 0.0f, mx = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto range = juce::FloatVectorOperations::findMinAndMax (data->buffer.getReadPointer (ch, s0), s1 - s0);
            mn = juce::jmin (mn, range.getStart());
            mx = juce::jmax (mx, range.getEnd());
        }
        data->peaks[(size_t) b * 2] = mn;
        data->peaks[(size_t) b * 2 + 1] = mx;
    }

    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        retired[channel] = std::move (samples[channel]);
        samples[channel] = data;
    }
    apvts.state.setProperty (channel == 0 ? "pathA" : "pathB", data->path, nullptr);
    ++sampleVersion[channel];
    return true;
}

std::shared_ptr<const SampleData> CumbiaGlitchProcessor::getSample (int channel) const
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    return samples[channel];
}

void CumbiaGlitchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CumbiaGlitchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

        for (int c = 0; c < 2; ++c)
        {
            const juce::String path = apvts.state.getProperty (c == 0 ? "pathA" : "pathB").toString();
            if (path.isNotEmpty())
            {
                const juce::File f (path);
                if (f.existsAsFile()) loadSample (c, f);
            }
        }
    }
}

juce::AudioProcessorEditor* CumbiaGlitchProcessor::createEditor()
{
    return new CumbiaGlitchEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CumbiaGlitchProcessor();
}

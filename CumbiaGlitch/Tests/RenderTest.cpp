// Prueba offline: genera dos samples sinteticos, los carga, simula un DAW en play
// y renderiza el resultado en dry y en wet. Verifica que no haya NaN ni saturacion extrema.
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <iostream>

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i;
        i.setIsPlaying (playing);
        i.setBpm (bpm);
        i.setPpqPosition (ppq);
        i.setTimeSignature (TimeSignature { 4, 4 });
        return i;
    }
};

static juce::File writeTestSample (const juce::File& dir, const juce::String& name, double bpm, bool cumbia)
{
    const double sr = 44100.0;
    const int beats = 8;
    const int len = (int) (sr * 60.0 / bpm * beats);
    juce::AudioBuffer<float> b (2, len);
    b.clear();
    juce::Random rng (cumbia ? 1 : 2);
    const int spb = (int) (sr * 60.0 / bpm);
    for (int beat = 0; beat < beats * 2; ++beat)
    {
        const int start = beat * spb / 2;
        const bool kick = cumbia ? (beat % 2 == 0) : (beat % 4 == 0 || beat % 8 == 5);
        for (int i = 0; i < spb / 2 && start + i < len; ++i)
        {
            const float t = (float) i / (float) sr;
            float v = kick ? std::sin (6.2832f * 60.0f * t) * std::exp (-t * 18.0f)
                           : (rng.nextFloat() * 2.0f - 1.0f) * std::exp (-t * 40.0f) * 0.5f;
            if (cumbia) v += 0.2f * std::sin (6.2832f * 440.0f * t) * std::exp (-t * 6.0f);
            b.setSample (0, start + i, v * 0.8f);
            b.setSample (1, start + i, v * 0.8f);
        }
    }
    auto f = dir.getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, len);
    return f;
}

static bool render (CumbiaGlitchProcessor& proc, FakePlayHead& ph, float wet, const juce::File& out, bool midiMode)
{
    const double sr = 48000.0;
    const int block = 512;
    proc.setPlayHead (&ph);
    proc.prepareToPlay (sr, block);
    proc.apvts.getParameter ("wet")->setValueNotifyingHost (wet);
    proc.apvts.getParameter ("chaos")->setValueNotifyingHost (0.7f);
    proc.apvts.getParameter ("texture")->setValueNotifyingHost (0.6f);
    proc.apvts.getParameter ("mode")->setValueNotifyingHost (midiMode ? 1.0f : 0.0f);
    proc.apvts.getParameter ("aCutoff")->setValueNotifyingHost (0.6f);

    const int total = (int) (sr * 12.0);
    juce::AudioBuffer<float> all (2, total);
    juce::AudioBuffer<float> buf (2, block);
    ph.ppq = 0.0;
    float peak = 0.0f;
    bool nan = false;

    for (int pos = 0; pos < total; pos += block)
    {
        juce::MidiBuffer midi;
        if (midiMode)
        {
            if (pos % (int) sr < block) midi.addEvent (juce::MidiMessage::noteOn (1, 60 + (pos / (int) sr) % 5, 0.9f), 10);
            if ((pos + (int) (sr / 2)) % (int) sr < block) midi.addEvent (juce::MidiMessage::noteOff (1, 60 + (pos / (int) sr) % 5), 20);
        }
        proc.processBlock (buf, midi);
        const int n = juce::jmin (block, total - pos);
        for (int c = 0; c < 2; ++c)
        {
            all.copyFrom (c, pos, buf, c, 0, n);
            for (int i = 0; i < n; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) nan = true;
                peak = juce::jmax (peak, std::abs (v));
            }
        }
        ph.ppq += block / (sr * 60.0 / ph.bpm);
    }

    const float rms = all.getRMSLevel (0, 0, total);
    std::cout << out.getFileName() << ": pico=" << peak << " rms=" << rms << (nan ? " NAN!" : "") << std::endl;

    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (out), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (all, 0, total);
    return ! nan && peak <= 1.0f && rms > 0.005f;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? argv[1] : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();

    CumbiaGlitchProcessor proc;
    bool ok = proc.loadSample (0, writeTestSample (dir, "cumbia_test.wav", 95.0, true));
    ok &= proc.loadSample (1, writeTestSample (dir, "dnb_test.wav", 172.0, false));
    std::cout << "samples cargados: " << (ok ? "si" : "NO") << std::endl;

    FakePlayHead ph;
    ok &= render (proc, ph, 0.0f, dir.getChildFile ("render_dry.wav"), false);
    ok &= render (proc, ph, 1.0f, dir.getChildFile ("render_wet.wav"), false);
    ok &= render (proc, ph, 0.5f, dir.getChildFile ("render_midi.wav"), true);

    // guardar y restaurar estado
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    CumbiaGlitchProcessor proc2;
    proc2.setStateInformation (state.getData(), (int) state.getSize());
    ok &= proc2.getSample (0) != nullptr && proc2.getSample (1) != nullptr;
    std::cout << "estado restaurado: " << (proc2.getSample (1) != nullptr ? "si" : "NO") << std::endl;

    // el editor se puede crear sin fallar
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    std::cout << "editor: " << ed->getWidth() << "x" << ed->getHeight() << std::endl;
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = dir.getChildFile ("interfaz.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    ed.reset();

    std::cout << (ok ? "TODO OK" : "FALLO") << std::endl;
    return ok ? 0 : 1;
}

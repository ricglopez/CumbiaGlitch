#include "GlitchEngine.h"
#include <cmath>

void GlitchEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    histSize = (int) (sampleRate * 4.0);
    for (auto& h : history)
        h.assign ((size_t) histSize, 0.0f);
    reset();
}

void GlitchEngine::reset()
{
    for (auto& h : history)
        std::fill (h.begin(), h.end(), 0.0f);
    writePos = 0;
    lastStep = -1;
    action = Action::normal;
    for (auto& g : grains) g.active = false;
    offsetL = offsetR = lastOutL = lastOutR = 0.0f;
    heldL = heldR = 0.0f;
    dsCounter = 0;
}

float GlitchEngine::readHistory (int channel, double pos) const
{
    while (pos < 0.0) pos += histSize;
    while (pos >= histSize) pos -= histSize;
    const int i0 = (int) pos;
    const int i1 = (i0 + 1) % histSize;
    const float frac = (float) (pos - i0);
    const auto& h = history[channel];
    return h[(size_t) i0] + frac * (h[(size_t) i1] - h[(size_t) i0]);
}

void GlitchEngine::startStep (double samplesPerBeat, float chaos, float texture)
{
    stepLen = juce::jmax (64, (int) (samplesPerBeat / 4.0)); // semicorchea
    stepCounter = 0;
    stepJustStarted = true;

    const int maxBack = histSize - stepLen * 2;

    if (rng.nextFloat() >= chaos)
    {
        action = Action::normal;
    }
    else
    {
        const float r = rng.nextFloat();
        if      (r < 0.30f) action = Action::reorder;
        else if (r < 0.42f) action = Action::reverse;
        else if (r < 0.72f) action = Action::stutter;
        else if (r < 0.92f) action = Action::granular;
        else                action = Action::silence;
    }

    switch (action)
    {
        case Action::reorder:
        {
            const int back = juce::jmin (maxBack, stepLen * (1 + rng.nextInt (16)));
            readPos = writePos - back;
            readInc = rng.nextFloat() < 0.15f * texture ? 0.5 : 1.0; // a veces una octava abajo
            break;
        }
        case Action::reverse:
        {
            const int back = juce::jmin (maxBack, stepLen * (1 + rng.nextInt (8)));
            readPos = writePos - back + stepLen;
            readInc = -1.0;
            break;
        }
        case Action::stutter:
        {
            static const int divisions[] = { 2, 3, 4, 6, 8, 12, 16, 32 };
            const int div = divisions[rng.nextInt (8)];
            stutterLen = juce::jmax (32.0, (double) stepLen / div);
            stutterStart = writePos - stutterLen;
            stutterPhase = 0.0;
            // repeticiones que se aceleran (efecto "rattle")
            stutterShrink = rng.nextFloat() < 0.4f ? 0.80 + 0.15 * rng.nextDouble() : 1.0;
            break;
        }
        case Action::granular:
        {
            grainInterval = juce::jmax (32, (int) (stepLen / (2.0 + 14.0 * texture)));
            grainCounter = 0;
            break;
        }
        case Action::normal:
        case Action::silence:
            break;
    }

    // la degradacion cambia cada paso: mas fuerte con mas textura
    const float crushChance = 0.25f + 0.75f * texture;
    if (rng.nextFloat() < crushChance)
    {
        bits = 16.0f - texture * (4.0f + 9.0f * rng.nextFloat());
        downsample = 1 + (int) (texture * rng.nextFloat() * 24.0f);
    }
    else
    {
        bits = 16.0f;
        downsample = 1;
    }

    ringFreq = rng.nextFloat() < 0.3f * texture ? 40.0f + rng.nextFloat() * 1800.0f : 0.0f;
}

void GlitchEngine::spawnGrain (float texture)
{
    for (auto& g : grains)
    {
        if (g.active) continue;

        static const double ratios[] = { 0.5, 1.0, 1.0, 1.5, 2.0, -1.0, 0.75, 3.0 };
        g.active = true;
        g.age = 0;
        g.length = (int) (sr * (0.008 + 0.07 * rng.nextDouble() * (1.0 - 0.5 * texture)));
        g.inc = ratios[rng.nextInt (8)];
        const double back = sr * 0.05 + rng.nextDouble() * sr * 1.5;
        g.pos = writePos - back;
        g.pan = rng.nextFloat();
        return;
    }
}

void GlitchEngine::process (float& left, float& right, double ppq, double samplesPerBeat,
                            float chaos, float texture)
{
    if (histSize == 0) return;

    // guarda la entrada en el historial
    history[0][(size_t) writePos] = left;
    history[1][(size_t) writePos] = right;

    const long long step = (long long) std::floor (ppq * 4.0);
    if (step != lastStep)
    {
        lastStep = step;
        startStep (samplesPerBeat, chaos, texture);
    }

    float outL = left, outR = right;

    switch (action)
    {
        case Action::normal:
            break;

        case Action::reorder:
        case Action::reverse:
            outL = readHistory (0, readPos);
            outR = readHistory (1, readPos);
            readPos += readInc;
            break;

        case Action::stutter:
        {
            const double p = stutterStart + stutterPhase;
            outL = readHistory (0, p);
            outR = readHistory (1, p);
            // pequena ventana en cada repeticion
            const double edge = juce::jmin (32.0, stutterLen * 0.25);
            float w = 1.0f;
            if (stutterPhase < edge) w = (float) (stutterPhase / edge);
            else if (stutterPhase > stutterLen - edge) w = (float) ((stutterLen - stutterPhase) / edge);
            outL *= w; outR *= w;

            stutterPhase += 1.0;
            if (stutterPhase >= stutterLen)
            {
                stutterPhase = 0.0;
                stutterLen = juce::jmax (24.0, stutterLen * stutterShrink);
            }
            break;
        }

        case Action::granular:
        {
            if (--grainCounter <= 0)
            {
                spawnGrain (texture);
                grainCounter = grainInterval;
            }
            float gl = 0.0f, gr = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                         * (float) g.age / (float) g.length);
                const float sl = readHistory (0, g.pos) * w;
                const float sr2 = readHistory (1, g.pos) * w;
                gl += sl * (1.0f - g.pan) * 1.4f;
                gr += sr2 * g.pan * 1.4f;
                g.pos += g.inc;
                if (++g.age >= g.length) g.active = false;
            }
            outL = gl; outR = gr;
            break;
        }

        case Action::silence:
            outL = outR = 0.0f;
            break;
    }

    // anti-clic: al empezar un paso nuevo, desvanece el salto
    if (stepJustStarted)
    {
        offsetL = lastOutL - outL;
        offsetR = lastOutR - outR;
        stepJustStarted = false;
    }
    outL += offsetL; outR += offsetR;
    offsetL *= 0.97f; offsetR *= 0.97f;

    // degradacion digital
    if (downsample > 1)
    {
        if (dsCounter++ % downsample == 0) { heldL = outL; heldR = outR; }
        outL = heldL; outR = heldR;
    }
    if (bits < 15.9f)
    {
        const float q = std::pow (2.0f, bits - 1.0f);
        outL = std::round (outL * q) / q;
        outR = std::round (outR * q) / q;
    }
    if (ringFreq > 0.0f)
    {
        const float m = (float) std::sin (ringPhase);
        ringPhase += juce::MathConstants<double>::twoPi * ringFreq / sr;
        if (ringPhase > juce::MathConstants<double>::twoPi) ringPhase -= juce::MathConstants<double>::twoPi;
        outL = 0.4f * outL + 0.6f * outL * m;
        outR = 0.4f * outR + 0.6f * outR * m;
    }

    ++stepCounter;
    writePos = (writePos + 1) % histSize;

    lastOutL = outL; lastOutR = outR;
    left = outL; right = outR;
}

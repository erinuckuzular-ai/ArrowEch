#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DSPUtils.h"

namespace arrow
{

//==============================================================================
// Schroeder allpass used to smear repeats.
class AllpassDiffuser
{
public:
    void prepare (double sampleRate, double milliseconds)
    {
        buffer.assign ((size_t) juce::jmax (2, (int) (sampleRate * milliseconds / 1000.0)), 0.0f);
        index = 0;
    }

    void reset()                { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    float process (float x, float g)
    {
        const float delayed = buffer[(size_t) index];
        const float y = -g * x + delayed;
        buffer[(size_t) index] = x + g * y;
        index = (index + 1) % (int) buffer.size();
        return y;
    }

private:
    std::vector<float> buffer;
    int index = 0;
};

//==============================================================================
// First-order allpass; a cascade of these gives the "boing" dispersion of a spring tank.
struct FirstOrderAllpass
{
    float x1 = 0.0f, y1 = 0.0f;
    void reset() { x1 = y1 = 0.0f; }
    float process (float x, float a)
    {
        const float y = a * x + x1 - a * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

//==============================================================================
// Stereo delay with tone, saturation, modulation and diffusion inside the feedback loop.
class EchoEngine
{
public:
    enum Mode  { single = 0, dual, pingPong };
    enum Style { digital = 0, tape, analog, lofi, diffuse, dubSpring };

    struct Settings
    {
        double delayMsL = 350.0, delayMsR = 350.0;
        float feedback = 0.4f, mix = 0.35f;
        float lowCut = 80.0f, highCut = 12000.0f;
        float saturation = 0.0f, wow = 0.0f, wowRate = 0.8f, diffusion = 0.0f;
        float duck = 0.0f, width = 1.0f;
        int mode = single, style = digital;
        bool reverbOn = false;
        bool freeze = false;       // loop the current repeats forever, ignore new input
        bool throwOnly = false;    // only send input to the echo while `throwing`
        bool throwing = false;
        float reverbSize = 0.7f, reverbMix = 0.5f;
    };

    void prepare (double newSampleRate, int maxBlockSize, double maxSeconds)
    {
        sampleRate = newSampleRate;
        size = (int) (sampleRate * maxSeconds);

        for (auto& line : lines)
            line.assign ((size_t) size, 0.0f);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        lowCutFilter.prepare (spec);
        lowCutFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        highCutFilter.prepare (spec);
        highCutFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        const double diffuserMs[] = { 4.7, 7.3, 11.1, 13.9 };
        for (size_t ch = 0; ch < 2; ++ch)
            for (size_t k = 0; k < diffusers[ch].size(); ++k)
                diffusers[ch][k].prepare (sampleRate, diffuserMs[k] * (ch == 0 ? 1.0 : 1.13));

        sendGain.reset (sampleRate, 0.008);
        delayL.reset (sampleRate, 0.3);
        delayR.reset (sampleRate, 0.3);
        wet.setSize (2, maxBlockSize);
        reverb.setSampleRate (sampleRate);
        reset();
    }

    void reset()
    {
        for (auto& line : lines)
            std::fill (line.begin(), line.end(), 0.0f);

        for (auto& channel : diffusers)
            for (auto& d : channel)
                d.reset();

        for (auto& channel : springs)
            for (auto& a : channel)
                a.reset();

        lowCutFilter.reset();
        highCutFilter.reset();
        reverb.reset();
        writePos = 0;
        envelope = 0.0f;
        needsDelaySnap = true;
    }

    void process (juce::AudioBuffer<float>& buffer, const Settings& s)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        if (numChannels == 0 || size == 0)
            return;

        if (wet.getNumSamples() < numSamples)
            wet.setSize (2, numSamples, false, false, true);

        // Per-style character.
        static constexpr float styleHighCap[]  = { 20000.0f, 11000.0f, 6500.0f, 3800.0f, 15000.0f, 4500.0f };
        static constexpr float styleLowFloor[] = { 20.0f, 40.0f, 60.0f, 350.0f, 20.0f, 180.0f };
        static constexpr float styleSat[]      = { 0.0f, 0.3f, 0.2f, 0.35f, 0.0f, 0.15f };
        static constexpr float styleWowMs[]    = { 1.0f, 2.5f, 2.0f, 3.0f, 1.5f, 1.5f };
        static constexpr float styleFlutterMs[] = { 0.0f, 0.12f, 0.05f, 0.25f, 0.0f, 0.03f };
        static constexpr float styleDiffusion[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.2f };

        const int style = juce::jlimit (0, 5, s.style);
        sendGain.setTargetValue (s.freeze || (s.throwOnly && ! s.throwing) ? 0.0f : 1.0f);
        const float feedback = s.freeze ? 1.0f : s.feedback;
        const int mode = juce::jlimit (0, 2, s.mode);

        lowCutFilter.setCutoffFrequency (juce::jmax (s.lowCut, styleLowFloor[style]));
        highCutFilter.setCutoffFrequency (juce::jmin (s.highCut, styleHighCap[style], (float) (sampleRate * 0.45)));

        const float saturation = juce::jlimit (0.0f, 1.0f, s.saturation + styleSat[style]);
        const float drive = 1.0f + saturation * 8.0f;
        const float diffusion = juce::jlimit (0.0f, 1.0f, s.diffusion + styleDiffusion[style]);
        const float allpassGain = 0.35f + 0.35f * diffusion;

        const double wowDepth = s.wow * styleWowMs[style] * sampleRate / 1000.0;
        const double flutterDepth = styleFlutterMs[style] * sampleRate / 1000.0;
        const double maxDelay = size - 2.0 - wowDepth - flutterDepth;
        auto toSamples = [&] (double ms) { return juce::jlimit (1.0, maxDelay, ms * sampleRate / 1000.0); };

        if (needsDelaySnap)
        {
            delayL.setCurrentAndTargetValue (toSamples (s.delayMsL));
            delayR.setCurrentAndTargetValue (toSamples (s.delayMsR));
            needsDelaySnap = false;
        }
        delayL.setTargetValue (toSamples (s.delayMsL));
        delayR.setTargetValue (toSamples (s.delayMsR));

        const double lfoIncrement = s.wowRate / sampleRate;
        const double flutterIncrement = 6.3 / sampleRate;
        const float attack = std::exp (-1.0f / (float) (0.005 * sampleRate));
        const float release = std::exp (-1.0f / (float) (0.25 * sampleRate));
        const float levels = 48.0f;

        auto shape = [&] (float x)
        {
            if (saturation <= 0.001f)
                return x;
            if (style == analog)
                x += 0.08f * x * x;
            return std::tanh (x * drive) / drive;
        };

        auto character = [&] (int ch, float y)
        {
            y = lowCutFilter.processSample (ch, y);
            y = highCutFilter.processSample (ch, y);
            y = shape (y);

            if (style == dubSpring)
                for (auto& a : springs[(size_t) ch])
                    y = a.process (y, -0.62f);

            if (diffusion > 0.001f)
            {
                float a = y;
                for (auto& d : diffusers[(size_t) ch])
                    a = d.process (a, allpassGain);
                y += diffusion * (a - y);
            }
            return y;
        };

        auto* lineL = lines[0].data();
        auto* lineR = lines[1].data();

        for (int i = 0; i < numSamples; ++i)
        {
            const float send = sendGain.getNextValue();
            const float inL = buffer.getSample (0, i);
            const float inR = numChannels > 1 ? buffer.getSample (1, i) : inL;
            const float sendL = inL * send, sendR = inR * send;

            lfoPhase += lfoIncrement;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;
            flutterPhase += flutterIncrement;
            if (flutterPhase >= 1.0) flutterPhase -= 1.0;

            const double twoPi = juce::MathConstants<double>::twoPi;
            const double flutter = 0.5 + 0.5 * std::sin (twoPi * flutterPhase);
            const double modL = wowDepth * (0.5 + 0.5 * std::sin (twoPi * lfoPhase)) + flutterDepth * flutter;
            const double modR = wowDepth * (0.5 + 0.5 * std::sin (twoPi * (lfoPhase + 0.25))) + flutterDepth * flutter;

            float yL = readInterpolated (lineL, size, writePos - (delayL.getNextValue() + modL));
            float yR = readInterpolated (lineR, size, writePos - (delayR.getNextValue() + modR));

            const float rawL = yL, rawR = yR;
            yL = character (0, yL);
            yR = character (1, yR);

            // Frozen loops recirculate the untouched signal so they never decay.
            const float fbL = (s.freeze ? rawL : yL) * feedback;
            const float fbR = (s.freeze ? rawR : yR) * feedback;

            if (mode == pingPong)
            {
                lineL[writePos] = softLimit (0.5f * (sendL + sendR) + fbR);
                lineR[writePos] = softLimit (fbL);
            }
            else
            {
                lineL[writePos] = softLimit (sendL + fbL);
                lineR[writePos] = softLimit (sendR + fbR);
            }

            if (style == lofi)
            {
                yL = std::round (yL * levels) / levels;
                yR = std::round (yR * levels) / levels;
            }

            const float mid = 0.5f * (yL + yR);
            const float side = 0.5f * (yL - yR) * s.width;

            const float peak = juce::jmax (std::abs (inL), std::abs (inR));
            const float coeff = peak > envelope ? attack : release;
            envelope = coeff * envelope + (1.0f - coeff) * peak;
            const float duckGain = 1.0f - s.duck * juce::jmin (1.0f, envelope * 4.0f);

            wet.setSample (0, i, (mid + side) * duckGain);
            wet.setSample (1, i, (mid - side) * duckGain);

            writePos = (writePos + 1) % size;
        }

        if (s.reverbOn)
        {
            juce::Reverb::Parameters p;
            p.roomSize = s.reverbSize;
            p.damping = 0.4f;
            p.wetLevel = s.reverbMix;
            p.dryLevel = 1.0f - 0.5f * s.reverbMix;
            p.width = 1.0f;
            reverb.setParameters (p);
            reverb.processStereo (wet.getWritePointer (0), wet.getWritePointer (1), numSamples);
            reverbActive = true;
        }
        else if (reverbActive)
        {
            reverb.reset();
            reverbActive = false;
        }

        const auto gains = blendGains (s.mix);

        if (numChannels == 1)
        {
            auto* out = buffer.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
                out[i] = out[i] * gains.dry + 0.5f * (wet.getSample (0, i) + wet.getSample (1, i)) * gains.wet;
        }
        else
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* out = buffer.getWritePointer (ch);
                auto* w = wet.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    out[i] = out[i] * gains.dry + w[i] * gains.wet;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    int size = 0;
    int writePos = 0;
    std::array<std::vector<float>, 2> lines;

    juce::SmoothedValue<double> delayL, delayR;
    bool needsDelaySnap = true;

    juce::dsp::StateVariableTPTFilter<float> lowCutFilter, highCutFilter;
    std::array<std::array<AllpassDiffuser, 4>, 2> diffusers;
    std::array<std::array<FirstOrderAllpass, 12>, 2> springs;
    juce::SmoothedValue<float> sendGain { 1.0f };

    double lfoPhase = 0.0, flutterPhase = 0.0;
    float envelope = 0.0f;

    juce::AudioBuffer<float> wet;
    juce::Reverb reverb;
    bool reverbActive = false;
};

} // namespace arrow

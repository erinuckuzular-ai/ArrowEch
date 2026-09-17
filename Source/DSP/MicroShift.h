#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DSPUtils.h"

namespace arrow
{

// Stereo thickener: left voice pitched up, right voice pitched down by a few cents,
// each through a short delay. Built from two crossfaded taps on a sweeping delay line.
class MicroShift
{
public:
    enum Mode { tight = 0, wide, motion };

    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        size = (int) (sampleRate * 0.15) + 4;

        for (auto& b : buffers)
            b.assign ((size_t) size, 0.0f);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        focusFilter.prepare (spec);
        focusFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        reset();
    }

    void reset()
    {
        for (auto& b : buffers)
            std::fill (b.begin(), b.end(), 0.0f);

        phases = { 0.0, 0.5 };
        lfoPhase = 0.0;
        writePos = 0;
        focusFilter.reset();
    }

    void process (juce::AudioBuffer<float>& buffer, int mode, float detune, float delayAmount, float focusHz, float mix)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        if (numChannels == 0 || size == 0)
            return;

        static constexpr float maxCents[]     = { 9.0f, 16.0f, 24.0f };
        static constexpr float baseMs[]       = { 1.5f, 6.0f, 10.0f };
        static constexpr float windowMs[]     = { 28.0f, 40.0f, 55.0f };
        static constexpr float extraDelayMs[] = { 12.0f, 22.0f, 30.0f };

        mode = juce::jlimit (0, 2, mode);
        focusFilter.setCutoffFrequency (focusHz);

        const double window = windowMs[mode] * sampleRate / 1000.0;
        const double baseL = (baseMs[mode] + delayAmount * extraDelayMs[mode]) * sampleRate / 1000.0;
        const double baseR = (baseMs[mode] + delayAmount * extraDelayMs[mode] * 1.37f) * sampleRate / 1000.0;
        const double lfoIncrement = 0.21 / sampleRate;
        const auto gains = blendGains (mix);
        const double centsToRate = std::log (2.0) / 1200.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = buffer.getSample (0, i);
            const float inR = numChannels > 1 ? buffer.getSample (1, i) : inL;

            buffers[0][(size_t) writePos] = focusFilter.processSample (0, inL);
            buffers[1][(size_t) writePos] = focusFilter.processSample (1, inR);

            lfoPhase += lfoIncrement;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;

            double depthL = 1.0, depthR = 1.0;
            if (mode == motion)
            {
                const double s = std::sin (juce::MathConstants<double>::twoPi * lfoPhase);
                depthL = 0.55 + 0.45 * s;
                depthR = 0.55 - 0.45 * s;
            }

            const double cents[] = { maxCents[mode] * detune * depthL, -maxCents[mode] * detune * depthR };
            const double bases[] = { baseL, baseR };
            float out[2];

            for (size_t ch = 0; ch < 2; ++ch)
            {
                auto& phase = phases[ch];
                phase -= cents[ch] * centsToRate / window;
                phase -= std::floor (phase);

                double phase2 = phase + 0.5;
                phase2 -= std::floor (phase2);

                const float g1 = (float) std::pow (std::sin (juce::MathConstants<double>::pi * phase), 2.0);
                const auto* data = buffers[ch].data();

                out[ch] = g1 * readInterpolated (data, size, writePos - (bases[ch] + phase * window))
                        + (1.0f - g1) * readInterpolated (data, size, writePos - (bases[ch] + phase2 * window));
            }

            if (numChannels == 1)
            {
                buffer.setSample (0, i, inL * gains.dry + 0.5f * (out[0] + out[1]) * gains.wet);
            }
            else
            {
                buffer.setSample (0, i, inL * gains.dry + out[0] * gains.wet);
                buffer.setSample (1, i, inR * gains.dry + out[1] * gains.wet);
            }

            writePos = (writePos + 1) % size;
        }
    }

private:
    double sampleRate = 44100.0;
    int size = 0;
    int writePos = 0;
    std::array<std::vector<float>, 2> buffers;
    std::array<double, 2> phases { 0.0, 0.5 };
    double lfoPhase = 0.0;
    juce::dsp::StateVariableTPTFilter<float> focusFilter;
};

} // namespace arrow

#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DSPUtils.h"

namespace arrow
{

//==============================================================================
// Saturator with five characters, tilt tone, auto gain and parallel mix.
class FuzzBucket
{
public:
    enum Mode { tube = 0, transistor, tape, fuzz, broken };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        toneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 1200.0f / (float) sampleRate);
        reset();
    }

    void reset()
    {
        lowpass = { 0.0f, 0.0f };
        dcIn = { 0.0f, 0.0f };
        dcOut = { 0.0f, 0.0f };
    }

    void process (juce::AudioBuffer<float>& buffer, int mode, float drive, float tone, float mix, float outputDb, bool punish)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        const float gain = juce::Decibels::decibelsToGain (drive * 30.0f + (punish ? 18.0f : 0.0f));
        const float makeup = std::pow (gain, -0.4f) * juce::Decibels::decibelsToGain (outputDb);
        const auto gains = blendGains (mix);
        const float bias = std::tanh (0.2f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const auto c = (size_t) ch;

            for (int i = 0; i < numSamples; ++i)
            {
                const float x = data[i];
                float s = x * gain;
                float y = 0.0f;

                switch (mode)
                {
                    case tube:       y = std::tanh (s + 0.2f) - bias; break;
                    case transistor: y = s / (1.0f + std::abs (s)); break;
                    case tape:       y = std::tanh (s * 0.8f); break;
                    case fuzz:
                    {
                        s *= 2.0f;
                        y = std::copysign (1.0f - std::exp (-std::abs (s)), s);
                        if (std::abs (y) > 0.7f)
                            y = std::copysign (0.7f + (std::abs (y) - 0.7f) * 0.3f, y);
                        break;
                    }
                    default:         y = std::sin (s * 1.3f) * 0.9f; break;
                }

                // DC blocker (tube bias and folding leave offsets behind)
                const float blocked = y - dcIn[c] + 0.995f * dcOut[c];
                dcIn[c] = y;
                dcOut[c] = blocked;
                y = blocked;

                // Tilt tone
                lowpass[c] += toneCoeff * (y - lowpass[c]);
                y = tone >= 0.0f ? y + tone * 1.5f * (y - lowpass[c])
                                 : y + (-tone) * (lowpass[c] - y);

                data[i] = x * gains.dry + y * makeup * gains.wet;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    float toneCoeff = 0.1f;
    std::array<float, 2> lowpass {}, dcIn {}, dcOut {};
};

//==============================================================================
// Resonant filter swept by a tempo-synced LFO and an envelope follower.
class FilterGoblin
{
public:
    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        filter.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 2 });
        reset();
    }

    void reset()
    {
        filter.reset();
        envelope = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, int type, float cutoff, float resonance, float lfoDepth,
                  float envAmount, double blockStartBeat, double samplesPerBeat, double lfoBeats)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());

        using T = juce::dsp::StateVariableTPTFilterType;
        filter.setType (type == 0 ? T::lowpass : type == 1 ? T::bandpass : T::highpass);
        filter.setResonance (0.7f + resonance * 9.3f);

        const float attack = std::exp (-1.0f / (float) (0.005 * sampleRate));
        const float release = std::exp (-1.0f / (float) (0.12 * sampleRate));
        const float maxCutoff = (float) (sampleRate * 0.45);

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

            const float coeff = peak > envelope ? attack : release;
            envelope = coeff * envelope + (1.0f - coeff) * peak;

            if (i % 16 == 0)
            {
                const double beat = blockStartBeat + i / samplesPerBeat;
                const float lfo = (float) std::sin (juce::MathConstants<double>::twoPi * beat / lfoBeats);
                const float octaves = lfo * lfoDepth * 3.0f + envAmount * juce::jmin (1.0f, envelope * 3.0f) * 5.0f;
                filter.setCutoffFrequency (juce::jlimit (20.0f, maxCutoff, cutoff * std::pow (2.0f, octaves)));
            }

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, softLimit (filter.processSample (ch, buffer.getSample (ch, i))));
        }
    }

private:
    double sampleRate = 44100.0;
    juce::dsp::StateVariableTPTFilter<float> filter;
    float envelope = 0.0f;
};

//==============================================================================
// Tempo-synced tremolo and auto-pan with shape morph and random steps.
class MotionBox
{
public:
    enum Mode { tremolo = 0, pan, both };

    void reset()
    {
        lastCycle = std::numeric_limits<long long>::min();
        randomSmoothed = 0.0f;
        randomTarget = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, int mode, float depth, float shape, float spread, float randomness,
                  double blockStartBeat, double samplesPerBeat, double lfoBeats)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        const float sharpness = 1.0f + shape * 14.0f;
        const float norm = std::tanh (sharpness);

        auto lfoAt = [&] (double phase)
        {
            const float s = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            const float v = std::tanh (s * sharpness) / norm;
            return v + randomness * (randomSmoothed - v);
        };

        for (int i = 0; i < numSamples; ++i)
        {
            const double phase = (blockStartBeat + i / samplesPerBeat) / lfoBeats;
            const auto cycle = (long long) std::floor (phase);
            if (cycle != lastCycle)
            {
                lastCycle = cycle;
                randomTarget = random.nextFloat() * 2.0f - 1.0f;
            }
            randomSmoothed += 0.002f * (randomTarget - randomSmoothed);

            const float vL = lfoAt (phase);
            const float vR = lfoAt (phase + spread * 0.5);
            float gL = 1.0f, gR = 1.0f;

            if (mode != pan)
            {
                gL *= 1.0f - depth * 0.5f * (1.0f - vL);
                gR *= 1.0f - depth * 0.5f * (1.0f - vR);
            }
            if (mode != tremolo && numChannels > 1)
            {
                const float p = vL * depth;
                gL *= juce::jmin (1.0f, 1.0f - p);
                gR *= juce::jmin (1.0f, 1.0f + p);
            }

            buffer.setSample (0, i, buffer.getSample (0, i) * gL);
            if (numChannels > 1)
                buffer.setSample (1, i, buffer.getSample (1, i) * gR);
        }
    }

private:
    juce::Random random;
    long long lastCycle = std::numeric_limits<long long>::min();
    float randomSmoothed = 0.0f, randomTarget = 0.0f;
};

//==============================================================================
// Heavy compressor with makeup gain, parallel mix and a tempo-synced "pump" duck.
class Squish
{
public:
    void prepare (double newSampleRate, int maxBlockSize)
    {
        compressor.prepare ({ newSampleRate, (juce::uint32) maxBlockSize, 2 });
        dry.setSize (2, maxBlockSize);
        reset();
    }

    void reset() { compressor.reset(); }

    void process (juce::AudioBuffer<float>& buffer, float amount, float releaseMs, float pump, float mix,
                  double blockStartBeat, double samplesPerBeat, double pumpBeats)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());

        if (dry.getNumSamples() < numSamples)
            dry.setSize (2, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        const float ratio = 1.5f + amount * 18.5f;
        compressor.setThreshold (-amount * 40.0f);
        compressor.setRatio (ratio);
        compressor.setAttack (2.0f);
        compressor.setRelease (releaseMs);

        juce::dsp::AudioBlock<float> block (buffer);
        auto channels = block.getSubsetChannelBlock (0, (size_t) numChannels);
        compressor.process (juce::dsp::ProcessContextReplacing<float> (channels));

        const float makeup = juce::Decibels::decibelsToGain (amount * 40.0f * (1.0f - 1.0f / ratio) * 0.5f);
        const auto gains = blendGains (mix);

        for (int i = 0; i < numSamples; ++i)
        {
            float g = makeup;
            if (pumpBeats > 0.0 && pump > 0.0f)
            {
                double phase = (blockStartBeat + i / samplesPerBeat) / pumpBeats;
                phase -= std::floor (phase);
                const float duck = phase < 0.03 ? (float) (phase / 0.03)
                                                : (float) std::pow (1.0 - (phase - 0.03) / 0.97, 2.0);
                g *= 1.0f - pump * 0.9f * duck;
            }

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, dry.getSample (ch, i) * gains.dry + buffer.getSample (ch, i) * g * gains.wet);
        }
    }

private:
    juce::dsp::Compressor<float> compressor;
    juce::AudioBuffer<float> dry;
};

} // namespace arrow

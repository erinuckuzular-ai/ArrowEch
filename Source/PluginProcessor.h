#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/EchoEngine.h"
#include "DSP/MicroShift.h"
#include "DSP/ToneModules.h"

//==============================================================================
class ArrowEchAudioProcessor  : public juce::AudioProcessor
{
public:
    // Modules in the rack. The chain order is a permutation of these ids.
    enum ModuleId { modEcho = 0, modShift, modStutter, modGranular, modCrush,
                    modFuzz, modFilter, modMotion, modPhaser, modSquish, numModules };
    using ChainOrder = std::array<int, numModules>;
    static const ChainOrder defaultOrder;
    static const char* moduleEnableParam (int moduleId);

    ArrowEchAudioProcessor();
    ~ArrowEchAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static const juce::StringArray echoDivisionNames;
    static const juce::StringArray echoModeNames;
    static const juce::StringArray echoStyleNames;
    static const juce::StringArray shiftModeNames;
    static const juce::StringArray fxDivisionNames;
    static const juce::StringArray crushDivisionNames;
    static const juce::StringArray lfoDivisionNames;
    static const juce::StringArray pumpDivisionNames;
    static double getEchoDivisionBeats (int index);

    ChainOrder getChainOrder() const;
    void setChainOrder (const ChainOrder& order);

    /** Message thread only. */
    void randomize();
    /** Clears every delay line and tail on the next audio block. */
    void panic() { panicRequested = true; }

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<double> displayBpm { 120.0 };
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

private:
    //==============================================================================
    struct Grain
    {
        bool active = false;
        double readPos = 0.0;
        double ratio = 1.0;
        int length = 0;
        int age = 0;
    };

    void processStutter (juce::AudioBuffer<float>& buffer, double bpm, double blockStartBeat);
    void processGranularDelay (juce::AudioBuffer<float>& buffer, double bpm, double blockStartBeat);
    void processBitcrusher (juce::AudioBuffer<float>& buffer, double bpm, double blockStartBeat);
    void processEcho (juce::AudioBuffer<float>& buffer, double bpm);
    void processModule (int moduleId, juce::AudioBuffer<float>& buffer, double bpm, double blockStartBeat);
    void clearAllTails();
    static bool isValidOrder (const ChainOrder& order);

    double getHostBPM() const;
    double samplesPerBeat (double bpm) const { return (60.0 / bpm) * sampleRate; }
    static float param (std::atomic<float>* value) { return value->load(); }
    static bool isOn (std::atomic<float>* value) { return value->load() > 0.5f; }

    //==============================================================================
    double sampleRate = 44100.0;
    double freeRunningBeat = 0.0;
    std::atomic<bool> panicRequested { false };
    std::atomic<int> currentPreset { 0 };
    std::atomic<juce::uint64> packedOrder { 0 };

    juce::SmoothedValue<float> inputGainSmoothed, outputGainSmoothed;
    juce::AudioBuffer<float> rackDry;

    arrow::EchoEngine echo;
    bool echoWasEnabled = true;

    arrow::MicroShift shifter;
    bool shiftWasEnabled = false;

    arrow::FuzzBucket fuzz;
    arrow::FilterGoblin filterGoblin;
    arrow::MotionBox motion;
    arrow::Squish squish;
    juce::dsp::Phaser<float> phaser;
    bool phaserWasEnabled = false;

    juce::AudioBuffer<float> stutterBuffer;

    juce::AudioBuffer<float> granularBuffer;
    int granularWritePosition = 0;
    std::array<Grain, 24> grains;
    long long lastGrainStep = -1;
    juce::Random random;

    std::array<float, 2> crushHeld { 0.0f, 0.0f };
    int crushCounter = 0;

    struct Params
    {
        std::atomic<float>* inputGain, * outputGain, * rackMix;
        std::atomic<float>* echoEnabled, * echoSync, * echoDivision, * echoTimeMs, * echoMode, * echoStyle;
        std::atomic<float>* echoFeedback, * echoMix, * echoOffset, * echoWidth, * echoDuck;
        std::atomic<float>* echoLowCut, * echoHighCut, * echoSaturation, * echoWow, * echoWowRate, * echoDiffusion;
        std::atomic<float>* echoReverb, * reverbSize, * reverbMix;
        std::atomic<float>* echoFreeze, * echoThrowOnly, * echoThrow;
        std::atomic<float>* shiftEnabled, * shiftMode, * shiftDetune, * shiftDelay, * shiftFocus, * shiftMix;
        std::atomic<float>* stutterEnabled, * stutterDivision, * stutterMix;
        std::atomic<float>* granularEnabled, * granularDivision, * granularScatter, * granularPitch, * granularMix, * granularReverse;
        std::atomic<float>* bitcrusherEnabled, * bitcrusherDivision, * bitcrusherBitDepth, * bitcrusherDownsample;
        std::atomic<float>* fuzzEnabled, * fuzzMode, * fuzzDrive, * fuzzTone, * fuzzMix, * fuzzOutput, * fuzzPunish;
        std::atomic<float>* filterEnabled, * filterType, * filterDivision, * filterCutoff, * filterResonance, * filterLfoDepth, * filterEnvelope;
        std::atomic<float>* motionEnabled, * motionMode, * motionDivision, * motionDepth, * motionShape, * motionSpread, * motionRandom;
        std::atomic<float>* phaserEnabled, * phaserDivision, * phaserDepth, * phaserFeedback, * phaserCentre, * phaserMix;
        std::atomic<float>* squishEnabled, * squishPumpDivision, * squishAmount, * squishRelease, * squishPump, * squishMix;
    } p {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrowEchAudioProcessor)
};

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    const double echoDivisionBeats[] = { 0.0625, 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75,
                                         2.0 / 3.0, 1.0, 1.5, 4.0 / 3.0, 2.0, 3.0, 4.0, 8.0 };
    const double fxDivisionBeats[]    = { 1.0, 0.5, 0.25, 0.125 };
    const double crushDivisionBeats[] = { 0.0, 1.0, 0.5, 0.25 }; // 0 = always on
    const double lfoDivisionBeats[]   = { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    const double pumpDivisionBeats[]  = { 0.0, 1.0, 0.5, 0.25 }; // 0 = off

    constexpr double minBpm = 30.0;
    constexpr double maxBpm = 300.0;
    constexpr double beatsPerBar = 4.0;

    constexpr double maxEchoSeconds = 16.5;    // 2 bars at 30 BPM + headroom
    constexpr double maxStutterSeconds = 2.5;
    constexpr double maxGrainSeconds = 1.0;
    constexpr double granularBufferSeconds = 5.0;

    int choiceIndex (std::atomic<float>* value, int count) { return juce::jlimit (0, count - 1, (int) value->load()); }
}

const juce::StringArray ArrowEchAudioProcessor::echoDivisionNames { "1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                                                    "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1 bar", "2 bars" };
const juce::StringArray ArrowEchAudioProcessor::echoModeNames  { "Single", "Dual", "Ping-Pong" };
const juce::StringArray ArrowEchAudioProcessor::echoStyleNames { "Digital", "Tape", "Analog", "Lo-Fi", "Diffuse", "Dub Spring" };
const juce::StringArray ArrowEchAudioProcessor::shiftModeNames { "Tight", "Wide", "Motion" };
const juce::StringArray ArrowEchAudioProcessor::fxDivisionNames   { "1/4", "1/8", "1/16", "1/32" };
const juce::StringArray ArrowEchAudioProcessor::crushDivisionNames { "Always", "1/4", "1/8", "1/16" };
const juce::StringArray ArrowEchAudioProcessor::lfoDivisionNames { "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" };
const juce::StringArray ArrowEchAudioProcessor::pumpDivisionNames { "Off", "1/4", "1/8", "1/16" };

const ArrowEchAudioProcessor::ChainOrder ArrowEchAudioProcessor::defaultOrder {
    modStutter, modGranular, modCrush, modFuzz, modFilter, modSquish, modEcho, modPhaser, modMotion, modShift
};

double ArrowEchAudioProcessor::getEchoDivisionBeats (int index)
{
    return echoDivisionBeats[juce::jlimit (0, (int) std::size (echoDivisionBeats) - 1, index)];
}

const char* ArrowEchAudioProcessor::moduleEnableParam (int moduleId)
{
    static const char* ids[] = { "echoEnabled", "shiftEnabled", "stutterEnabled", "granularEnabled", "bitcrusherEnabled",
                                 "fuzzEnabled", "filterEnabled", "motionEnabled", "phaserEnabled", "squishEnabled" };
    return ids[juce::jlimit (0, (int) numModules - 1, moduleId)];
}

//==============================================================================
ArrowEchAudioProcessor::ArrowEchAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ArrowEchState", createParameterLayout())
{
    auto get = [this] (const char* id)
    {
        auto* value = apvts.getRawParameterValue (id);
        jassert (value != nullptr);
        return value;
    };

    p.inputGain = get ("inputGain");               p.outputGain = get ("outputGain");
    p.rackMix = get ("rackMix");
    p.echoEnabled = get ("echoEnabled");           p.echoSync = get ("echoSync");
    p.echoDivision = get ("echoDivision");         p.echoTimeMs = get ("echoTimeMs");
    p.echoMode = get ("echoMode");                 p.echoStyle = get ("echoStyle");
    p.echoFeedback = get ("echoFeedback");         p.echoMix = get ("echoMix");
    p.echoOffset = get ("echoOffset");             p.echoWidth = get ("echoWidth");
    p.echoDuck = get ("echoDuck");                 p.echoLowCut = get ("echoLowCut");
    p.echoHighCut = get ("echoHighCut");           p.echoSaturation = get ("echoSaturation");
    p.echoWow = get ("echoWow");                   p.echoWowRate = get ("echoWowRate");
    p.echoDiffusion = get ("echoDiffusion");       p.echoReverb = get ("echoReverb");
    p.reverbSize = get ("reverbSize");             p.reverbMix = get ("reverbMix");
    p.echoFreeze = get ("echoFreeze");             p.echoThrowOnly = get ("echoThrowOnly");
    p.echoThrow = get ("echoThrow");
    p.shiftEnabled = get ("shiftEnabled");         p.shiftMode = get ("shiftMode");
    p.shiftDetune = get ("shiftDetune");           p.shiftDelay = get ("shiftDelay");
    p.shiftFocus = get ("shiftFocus");             p.shiftMix = get ("shiftMix");
    p.stutterEnabled = get ("stutterEnabled");     p.stutterDivision = get ("stutterDivision");
    p.stutterMix = get ("stutterMix");
    p.granularEnabled = get ("granularEnabled");   p.granularDivision = get ("granularDivision");
    p.granularScatter = get ("granularScatter");   p.granularPitch = get ("granularPitch");
    p.granularMix = get ("granularMix");           p.granularReverse = get ("granularReverse");
    p.bitcrusherEnabled = get ("bitcrusherEnabled");       p.bitcrusherDivision = get ("bitcrusherDivision");
    p.bitcrusherBitDepth = get ("bitcrusherBitDepth");     p.bitcrusherDownsample = get ("bitcrusherDownsample");
    p.fuzzEnabled = get ("fuzzEnabled");           p.fuzzMode = get ("fuzzMode");
    p.fuzzDrive = get ("fuzzDrive");               p.fuzzTone = get ("fuzzTone");
    p.fuzzMix = get ("fuzzMix");                   p.fuzzOutput = get ("fuzzOutput");
    p.fuzzPunish = get ("fuzzPunish");
    p.filterEnabled = get ("filterEnabled");       p.filterType = get ("filterType");
    p.filterDivision = get ("filterDivision");     p.filterCutoff = get ("filterCutoff");
    p.filterResonance = get ("filterResonance");   p.filterLfoDepth = get ("filterLfoDepth");
    p.filterEnvelope = get ("filterEnvelope");
    p.motionEnabled = get ("motionEnabled");       p.motionMode = get ("motionMode");
    p.motionDivision = get ("motionDivision");     p.motionDepth = get ("motionDepth");
    p.motionShape = get ("motionShape");           p.motionSpread = get ("motionSpread");
    p.motionRandom = get ("motionRandom");
    p.phaserEnabled = get ("phaserEnabled");       p.phaserDivision = get ("phaserDivision");
    p.phaserDepth = get ("phaserDepth");           p.phaserFeedback = get ("phaserFeedback");
    p.phaserCentre = get ("phaserCentre");         p.phaserMix = get ("phaserMix");
    p.squishEnabled = get ("squishEnabled");       p.squishPumpDivision = get ("squishPumpDivision");
    p.squishAmount = get ("squishAmount");         p.squishRelease = get ("squishRelease");
    p.squishPump = get ("squishPump");             p.squishMix = get ("squishMix");

    setChainOrder (defaultOrder);
}

ArrowEchAudioProcessor::~ArrowEchAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout ArrowEchAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto withText = [] (std::function<String (float)> f)
    {
        return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); });
    };
    const auto percent  = withText ([] (float v) { return String (roundToInt (v * 100.0f)) + "%"; });
    const auto hertz    = withText ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });
    const auto decibels = withText ([] (float v) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; });
    const auto bipolarPercent = withText ([] (float v) { return (v > 0.0f ? "+" : "") + String (roundToInt (v * 100.0f)) + "%"; });

    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> range, float def, AudioParameterFloatAttributes attr = {})
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def, attr));
    };
    auto addBool = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto addChoice = [&] (const char* id, const char* name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    // Global
    addFloat ("inputGain", "Input", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, decibels);
    addFloat ("outputGain", "Output", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, decibels);
    addFloat ("rackMix", "Rack Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent);

    // Echo
    addBool   ("echoEnabled", "Echo On", true);
    addBool   ("echoSync", "Echo Sync", true);
    addChoice ("echoDivision", "Echo Division", echoDivisionNames, 6);
    addFloat  ("echoTimeMs", "Echo Time", skewed (1.0f, 2000.0f, 300.0f), 350.0f,
               withText ([] (float v) { return String (roundToInt (v)) + " ms"; }));
    addChoice ("echoMode", "Echo Mode", echoModeNames, 0);
    addChoice ("echoStyle", "Echo Style", echoStyleNames, 1);
    addFloat  ("echoFeedback", "Echo Feedback", NormalisableRange<float> (0.0f, 1.1f), 0.45f, percent);
    addFloat  ("echoMix", "Echo Mix", NormalisableRange<float> (0.0f, 1.0f), 0.35f, percent);
    addFloat  ("echoOffset", "Echo L/R Offset", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
               withText ([] (float v) { return String (roundToInt (v)) + "%"; }));
    addFloat  ("echoWidth", "Echo Width", NormalisableRange<float> (0.0f, 2.0f), 1.0f, percent);
    addFloat  ("echoDuck", "Echo Ducking", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent);
    addFloat  ("echoLowCut", "Echo Low Cut", skewed (20.0f, 2000.0f, 200.0f), 80.0f, hertz);
    addFloat  ("echoHighCut", "Echo High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 12000.0f, hertz);
    addFloat  ("echoSaturation", "Echo Saturation", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent);
    addFloat  ("echoWow", "Echo Wow", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent);
    addFloat  ("echoWowRate", "Echo Wow Rate", skewed (0.05f, 10.0f, 1.0f), 0.8f,
               withText ([] (float v) { return String (v, 2) + " Hz"; }));
    addFloat  ("echoDiffusion", "Echo Diffusion", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent);
    addBool   ("echoReverb", "Echo Reverb", false);
    addFloat  ("reverbSize", "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent);
    addFloat  ("reverbMix", "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);
    addBool   ("echoFreeze", "Echo Freeze", false);
    addBool   ("echoThrowOnly", "Echo Throw-Only Send", false);
    addBool   ("echoThrow", "Echo Throw", false);

    // Shift
    addBool   ("shiftEnabled", "Shift On", false);
    addChoice ("shiftMode", "Shift Mode", shiftModeNames, 1);
    addFloat  ("shiftDetune", "Shift Detune", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);
    addFloat  ("shiftDelay", "Shift Delay", NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent);
    addFloat  ("shiftFocus", "Shift Focus", skewed (20.0f, 1000.0f, 150.0f), 150.0f, hertz);
    addFloat  ("shiftMix", "Shift Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);

    // Stutter
    addBool   ("stutterEnabled", "Stutter On", false);
    addChoice ("stutterDivision", "Stutter Rate", fxDivisionNames, 1);
    addFloat  ("stutterMix", "Stutter Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent);

    // Granular
    addBool   ("granularEnabled", "Granular On", false);
    addChoice ("granularDivision", "Grain Size", fxDivisionNames, 1);
    addFloat  ("granularScatter", "Grain Scatter", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
               withText ([] (float v) { return String (roundToInt (v)) + "%"; }));
    addFloat  ("granularPitch", "Grain Pitch", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
               withText ([] (float v) { return (v > 0.0f ? "+" : "") + String (v, 1) + " st"; }));
    addFloat  ("granularMix", "Granular Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);
    addBool   ("granularReverse", "Grain Reverse", false);

    // Bitcrusher
    addBool   ("bitcrusherEnabled", "Crusher On", false);
    addChoice ("bitcrusherDivision", "Crusher Rhythm", crushDivisionNames, 0);
    addFloat  ("bitcrusherBitDepth", "Bit Depth", NormalisableRange<float> (2.0f, 16.0f, 1.0f), 8.0f,
               withText ([] (float v) { return String (roundToInt (v)) + " bit"; }));
    addFloat  ("bitcrusherDownsample", "Downsample", NormalisableRange<float> (1.0f, 32.0f, 1.0f), 4.0f,
               withText ([] (float v) { return String (roundToInt (v)) + "x"; }));

    // Fuzz
    addBool   ("fuzzEnabled", "Fuzz On", false);
    addChoice ("fuzzMode", "Fuzz Character", StringArray { "Tube", "Transistor", "Tape", "Fuzz", "Broken" }, 0);
    addFloat  ("fuzzDrive", "Fuzz Drive", NormalisableRange<float> (0.0f, 1.0f), 0.4f, percent);
    addFloat  ("fuzzTone", "Fuzz Tone", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPercent);
    addFloat  ("fuzzMix", "Fuzz Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent);
    addFloat  ("fuzzOutput", "Fuzz Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, decibels);
    addBool   ("fuzzPunish", "Fuzz Punish", false);

    // Filter
    addBool   ("filterEnabled", "Filter On", false);
    addChoice ("filterType", "Filter Type", StringArray { "Low-pass", "Band-pass", "High-pass" }, 0);
    addChoice ("filterDivision", "Filter LFO Rate", lfoDivisionNames, 5);
    addFloat  ("filterCutoff", "Filter Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 1200.0f, hertz);
    addFloat  ("filterResonance", "Filter Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent);
    addFloat  ("filterLfoDepth", "Filter LFO Depth", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);
    addFloat  ("filterEnvelope", "Filter Envelope", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPercent);

    // Motion
    addBool   ("motionEnabled", "Motion On", false);
    addChoice ("motionMode", "Motion Mode", StringArray { "Tremolo", "Auto-Pan", "Both" }, 0);
    addChoice ("motionDivision", "Motion Rate", lfoDivisionNames, 2);
    addFloat  ("motionDepth", "Motion Depth", NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent);
    addFloat  ("motionShape", "Motion Shape", NormalisableRange<float> (0.0f, 1.0f), 0.2f, percent);
    addFloat  ("motionSpread", "Motion Stereo Spread", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent);
    addFloat  ("motionRandom", "Motion Randomness", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent);

    // Phaser
    addBool   ("phaserEnabled", "Phaser On", false);
    addChoice ("phaserDivision", "Phaser Rate", lfoDivisionNames, 5);
    addFloat  ("phaserDepth", "Phaser Depth", NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent);
    addFloat  ("phaserFeedback", "Phaser Feedback", NormalisableRange<float> (-0.95f, 0.95f), 0.5f, bipolarPercent);
    addFloat  ("phaserCentre", "Phaser Centre", skewed (100.0f, 5000.0f, 800.0f), 800.0f, hertz);
    addFloat  ("phaserMix", "Phaser Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);

    // Squish
    addBool   ("squishEnabled", "Squish On", false);
    addChoice ("squishPumpDivision", "Squish Pump Rate", pumpDivisionNames, 1);
    addFloat  ("squishAmount", "Squish Amount", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent);
    addFloat  ("squishRelease", "Squish Release", skewed (10.0f, 500.0f, 100.0f), 100.0f,
               withText ([] (float v) { return String (roundToInt (v)) + " ms"; }));
    addFloat  ("squishPump", "Squish Pump", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent);
    addFloat  ("squishMix", "Squish Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent);

    return layout;
}

//==============================================================================
const juce::String ArrowEchAudioProcessor::getName() const { return JucePlugin_Name; }

bool ArrowEchAudioProcessor::acceptsMidi() const { return false; }
bool ArrowEchAudioProcessor::producesMidi() const { return false; }
bool ArrowEchAudioProcessor::isMidiEffect() const { return false; }
double ArrowEchAudioProcessor::getTailLengthSeconds() const { return maxEchoSeconds; }

//==============================================================================
bool ArrowEchAudioProcessor::isValidOrder (const ChainOrder& order)
{
    std::array<bool, numModules> seen {};
    for (int id : order)
    {
        if (! juce::isPositiveAndBelow (id, (int) numModules) || seen[(size_t) id])
            return false;
        seen[(size_t) id] = true;
    }
    return true;
}

ArrowEchAudioProcessor::ChainOrder ArrowEchAudioProcessor::getChainOrder() const
{
    const auto packed = packedOrder.load();
    ChainOrder order {};
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = (int) ((packed >> (i * 4)) & 0xf);
    return isValidOrder (order) ? order : defaultOrder;
}

void ArrowEchAudioProcessor::setChainOrder (const ChainOrder& order)
{
    if (! isValidOrder (order))
        return;

    juce::uint64 packed = 0;
    for (size_t i = 0; i < order.size(); ++i)
        packed |= (juce::uint64) order[i] << (i * 4);
    packedOrder = packed;
}

//==============================================================================
int ArrowEchAudioProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }
int ArrowEchAudioProcessor::getCurrentProgram() { return currentPreset; }

void ArrowEchAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    const auto& preset = presets[(size_t) index];

    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            if (ranged->getParameterID() != "inputGain" && ranged->getParameterID() != "outputGain")
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (const auto& [id, value] : preset.values)
        if (auto* ranged = apvts.getParameter (id))
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));

    ChainOrder order = defaultOrder;
    if (preset.order.size() == order.size())
        std::copy (preset.order.begin(), preset.order.end(), order.begin());
    setChainOrder (order);

    currentPreset = index;
}

const juce::String ArrowEchAudioProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : "";
}

void ArrowEchAudioProcessor::changeProgramName (int, const juce::String&) {}

void ArrowEchAudioProcessor::randomize()
{
    auto set = [this] (const char* id, float value)
    {
        if (auto* ranged = apvts.getParameter (id))
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
    };
    auto between = [this] (float lo, float hi) { return lo + random.nextFloat() * (hi - lo); };
    auto chance = [this] (float probability) { return random.nextFloat() < probability ? 1.0f : 0.0f; };
    auto pick = [this] (int count) { return (float) random.nextInt (count); };

    set ("echoEnabled", 1.0f);
    set ("echoSync", chance (0.8f));
    set ("echoDivision", (float) random.nextInt ({ 3, 13 }));
    set ("echoTimeMs", between (40.0f, 900.0f));
    set ("echoMode", pick (3));
    set ("echoStyle", pick (6));
    set ("echoFeedback", between (0.1f, 0.85f));
    set ("echoMix", between (0.15f, 0.55f));
    set ("echoOffset", random.nextBool() ? 0.0f : between (-60.0f, 60.0f));
    set ("echoWidth", between (0.6f, 1.6f));
    set ("echoDuck", random.nextBool() ? 0.0f : between (0.2f, 0.8f));
    set ("echoLowCut", between (20.0f, 400.0f));
    set ("echoHighCut", between (2500.0f, 16000.0f));
    set ("echoSaturation", between (0.0f, 0.6f));
    set ("echoWow", between (0.0f, 0.5f));
    set ("echoWowRate", between (0.2f, 3.0f));
    set ("echoDiffusion", between (0.0f, 0.6f));
    set ("echoReverb", chance (0.4f));
    set ("reverbSize", between (0.3f, 0.95f));
    set ("reverbMix", between (0.2f, 0.6f));
    set ("echoFreeze", 0.0f);
    set ("echoThrowOnly", 0.0f);

    set ("shiftEnabled", chance (0.35f));
    set ("shiftMode", pick (3));
    set ("shiftDetune", between (0.2f, 0.8f));
    set ("shiftDelay", between (0.0f, 0.7f));
    set ("shiftFocus", between (60.0f, 400.0f));
    set ("shiftMix", between (0.25f, 0.6f));

    set ("stutterEnabled", chance (0.2f));
    set ("stutterDivision", pick (4));
    set ("stutterMix", between (0.5f, 1.0f));

    set ("granularEnabled", chance (0.25f));
    set ("granularDivision", pick (4));
    set ("granularScatter", between (0.0f, 100.0f));
    set ("granularPitch", random.nextBool() ? 0.0f : (float) (random.nextBool() ? 12 : -12));
    set ("granularMix", between (0.2f, 0.6f));
    set ("granularReverse", chance (0.3f));

    set ("bitcrusherEnabled", chance (0.2f));
    set ("bitcrusherDivision", pick (4));
    set ("bitcrusherBitDepth", between (4.0f, 12.0f));
    set ("bitcrusherDownsample", between (1.0f, 12.0f));

    set ("fuzzEnabled", chance (0.35f));
    set ("fuzzMode", pick (5));
    set ("fuzzDrive", between (0.1f, 0.7f));
    set ("fuzzTone", between (-0.6f, 0.6f));
    set ("fuzzMix", between (0.4f, 1.0f));
    set ("fuzzPunish", chance (0.1f));

    set ("filterEnabled", chance (0.3f));
    set ("filterType", pick (3));
    set ("filterDivision", pick (8));
    set ("filterCutoff", between (300.0f, 4000.0f));
    set ("filterResonance", between (0.1f, 0.8f));
    set ("filterLfoDepth", between (0.0f, 0.8f));
    set ("filterEnvelope", between (-0.5f, 0.8f));

    set ("motionEnabled", chance (0.3f));
    set ("motionMode", pick (3));
    set ("motionDivision", pick (6));
    set ("motionDepth", between (0.2f, 0.9f));
    set ("motionShape", between (0.0f, 1.0f));
    set ("motionSpread", between (0.0f, 1.0f));
    set ("motionRandom", random.nextBool() ? 0.0f : between (0.2f, 1.0f));

    set ("phaserEnabled", chance (0.25f));
    set ("phaserDivision", pick (8));
    set ("phaserDepth", between (0.3f, 1.0f));
    set ("phaserFeedback", between (-0.8f, 0.8f));
    set ("phaserCentre", between (300.0f, 2500.0f));
    set ("phaserMix", between (0.3f, 0.7f));

    set ("squishEnabled", chance (0.3f));
    set ("squishPumpDivision", pick (4));
    set ("squishAmount", between (0.2f, 0.8f));
    set ("squishRelease", between (30.0f, 300.0f));
    set ("squishPump", random.nextBool() ? 0.0f : between (0.3f, 0.9f));
    set ("squishMix", between (0.5f, 1.0f));

    // Sometimes shuffle the rack order too. Chaos is a feature.
    auto order = defaultOrder;
    if (random.nextFloat() < 0.3f)
        for (int i = (int) order.size() - 1; i > 0; --i)
            std::swap (order[(size_t) i], order[(size_t) random.nextInt (i + 1)]);
    setChainOrder (order);
}

//==============================================================================
void ArrowEchAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    const int numChannels = juce::jmax (2, getTotalNumOutputChannels());

    inputGainSmoothed.reset (sampleRate, 0.05);
    inputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param (p.inputGain)));
    outputGainSmoothed.reset (sampleRate, 0.05);
    outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param (p.outputGain)));
    rackDry.setSize (numChannels, samplesPerBlock);

    echo.prepare (sampleRate, samplesPerBlock, maxEchoSeconds);
    shifter.prepare (sampleRate, samplesPerBlock);
    fuzz.prepare (sampleRate);
    filterGoblin.prepare (sampleRate, samplesPerBlock);
    squish.prepare (sampleRate, samplesPerBlock);
    phaser.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });

    stutterBuffer.setSize (numChannels, (int) (sampleRate * maxStutterSeconds));
    granularBuffer.setSize (numChannels, (int) (sampleRate * granularBufferSeconds));
    clearAllTails();
    freeRunningBeat = 0.0;
}

void ArrowEchAudioProcessor::releaseResources() {}

void ArrowEchAudioProcessor::clearAllTails()
{
    echo.reset();
    shifter.reset();
    fuzz.reset();
    filterGoblin.reset();
    motion.reset();
    squish.reset();
    phaser.reset();
    stutterBuffer.clear();
    granularBuffer.clear();
    granularWritePosition = 0;
    for (auto& g : grains)
        g.active = false;
    lastGrainStep = -1;
    crushHeld = { 0.0f, 0.0f };
    crushCounter = 0;
}

bool ArrowEchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void ArrowEchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    if (panicRequested.exchange (false))
        clearAllTails();

    const double bpm = getHostBPM();
    displayBpm = bpm;

    double blockStartBeat = freeRunningBeat;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (position->getIsPlaying())
                if (auto ppq = position->getPpqPosition())
                    blockStartBeat = *ppq;

    freeRunningBeat = blockStartBeat + numSamples / samplesPerBeat (bpm);

    auto applyGain = [&] (juce::SmoothedValue<float>& smoothed, float db)
    {
        smoothed.setTargetValue (juce::Decibels::decibelsToGain (db));
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = smoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * g);
        }
    };

    applyGain (inputGainSmoothed, param (p.inputGain));
    inputPeak = buffer.getMagnitude (0, numSamples);

    const float rackMix = param (p.rackMix);
    if (rackMix < 0.999f)
    {
        if (rackDry.getNumSamples() < numSamples || rackDry.getNumChannels() < numChannels)
            rackDry.setSize (juce::jmax (2, numChannels), numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            rackDry.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    for (int moduleId : getChainOrder())
        processModule (moduleId, buffer, bpm, blockStartBeat);

    if (rackMix < 0.999f)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.applyGain (ch, 0, numSamples, rackMix);
            buffer.addFrom (ch, 0, rackDry, ch, 0, numSamples, 1.0f - rackMix);
        }

    applyGain (outputGainSmoothed, param (p.outputGain));
    outputPeak = buffer.getMagnitude (0, numSamples);
}

void ArrowEchAudioProcessor::processModule (int moduleId, juce::AudioBuffer<float>& buffer, double bpm, double blockStartBeat)
{
    const double spb = samplesPerBeat (bpm);

    switch (moduleId)
    {
        case modEcho:
        {
            const bool on = isOn (p.echoEnabled);
            if (on)
                processEcho (buffer, bpm);
            else if (echoWasEnabled)
                echo.reset();
            echoWasEnabled = on;
            break;
        }

        case modShift:
        {
            const bool on = isOn (p.shiftEnabled);
            if (on)
                shifter.process (buffer, choiceIndex (p.shiftMode, 3), param (p.shiftDetune), param (p.shiftDelay),
                                 param (p.shiftFocus), param (p.shiftMix));
            else if (shiftWasEnabled)
                shifter.reset();
            shiftWasEnabled = on;
            break;
        }

        case modStutter:
            if (isOn (p.stutterEnabled))
                processStutter (buffer, bpm, blockStartBeat);
            break;

        case modGranular:
            if (isOn (p.granularEnabled))
                processGranularDelay (buffer, bpm, blockStartBeat);
            else
                for (auto& g : grains)
                    g.active = false;
            break;

        case modCrush:
            if (isOn (p.bitcrusherEnabled))
                processBitcrusher (buffer, bpm, blockStartBeat);
            break;

        case modFuzz:
            if (isOn (p.fuzzEnabled))
                fuzz.process (buffer, choiceIndex (p.fuzzMode, 5), param (p.fuzzDrive), param (p.fuzzTone),
                              param (p.fuzzMix), param (p.fuzzOutput), isOn (p.fuzzPunish));
            break;

        case modFilter:
            if (isOn (p.filterEnabled))
                filterGoblin.process (buffer, choiceIndex (p.filterType, 3), param (p.filterCutoff), param (p.filterResonance),
                                      param (p.filterLfoDepth), param (p.filterEnvelope), blockStartBeat, spb,
                                      lfoDivisionBeats[choiceIndex (p.filterDivision, 8)]);
            break;

        case modMotion:
            if (isOn (p.motionEnabled))
                motion.process (buffer, choiceIndex (p.motionMode, 3), param (p.motionDepth), param (p.motionShape),
                                param (p.motionSpread), param (p.motionRandom), blockStartBeat, spb,
                                lfoDivisionBeats[choiceIndex (p.motionDivision, 8)]);
            break;

        case modPhaser:
        {
            const bool on = isOn (p.phaserEnabled);
            if (on)
            {
                const double seconds = lfoDivisionBeats[choiceIndex (p.phaserDivision, 8)] * 60.0 / bpm;
                phaser.setRate ((float) juce::jlimit (0.01, 50.0, 1.0 / seconds));
                phaser.setDepth (param (p.phaserDepth));
                phaser.setFeedback (param (p.phaserFeedback));
                phaser.setCentreFrequency (param (p.phaserCentre));
                phaser.setMix (param (p.phaserMix));

                juce::dsp::AudioBlock<float> block (buffer);
                auto channels = block.getSubsetChannelBlock (0, (size_t) juce::jmin (2, buffer.getNumChannels()));
                phaser.process (juce::dsp::ProcessContextReplacing<float> (channels));
            }
            else if (phaserWasEnabled)
            {
                phaser.reset();
            }
            phaserWasEnabled = on;
            break;
        }

        case modSquish:
            if (isOn (p.squishEnabled))
                squish.process (buffer, param (p.squishAmount), param (p.squishRelease), param (p.squishPump),
                                param (p.squishMix), blockStartBeat, spb,
                                pumpDivisionBeats[choiceIndex (p.squishPumpDivision, 4)]);
            break;

        default:
            break;
    }
}

//==============================================================================
void ArrowEchAudioProcessor::processEcho (juce::AudioBuffer<float>& buffer, double bpm)
{
    arrow::EchoEngine::Settings s;

    const double baseMs = isOn (p.echoSync)
                            ? getEchoDivisionBeats ((int) param (p.echoDivision)) * 60000.0 / bpm
                            : (double) param (p.echoTimeMs);

    s.mode = choiceIndex (p.echoMode, 3);
    const double offset = s.mode == arrow::EchoEngine::single ? 0.0 : param (p.echoOffset) / 100.0 * 0.5;
    s.delayMsL = baseMs * (1.0 - offset);
    s.delayMsR = baseMs * (1.0 + offset);

    s.style = choiceIndex (p.echoStyle, 6);
    s.feedback = param (p.echoFeedback);
    s.mix = param (p.echoMix);
    s.width = param (p.echoWidth);
    s.duck = param (p.echoDuck);
    s.lowCut = param (p.echoLowCut);
    s.highCut = param (p.echoHighCut);
    s.saturation = param (p.echoSaturation);
    s.wow = param (p.echoWow);
    s.wowRate = param (p.echoWowRate);
    s.diffusion = param (p.echoDiffusion);
    s.reverbOn = isOn (p.echoReverb);
    s.reverbSize = param (p.reverbSize);
    s.reverbMix = param (p.reverbMix);
    s.freeze = isOn (p.echoFreeze);
    s.throwOnly = isOn (p.echoThrowOnly);
    s.throwing = isOn (p.echoThrow);

    echo.process (buffer, s);
}

// Each bar is cut into slices of the chosen division. The first slice of the bar is
// recorded and passed through; every following slice replays it.
void ArrowEchAudioProcessor::processStutter (juce::AudioBuffer<float>& buffer, double bpm, double blockStartBeat)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), stutterBuffer.getNumChannels());
    const double spb = samplesPerBeat (bpm);
    const double sliceBeats = fxDivisionBeats[choiceIndex (p.stutterDivision, 4)];
    const int sliceSamples = juce::jlimit (1, stutterBuffer.getNumSamples(), (int) (sliceBeats * spb));
    const float mix = param (p.stutterMix);
    const int fade = juce::jmin ((int) (0.004 * sampleRate), sliceSamples / 2);

    for (int i = 0; i < numSamples; ++i)
    {
        const double beat = blockStartBeat + i / spb;
        const double posInBar = beat - beatsPerBar * std::floor (beat / beatsPerBar);
        const int sliceIndex = (int) std::floor (posInBar / sliceBeats);
        const int offset = juce::jlimit (0, sliceSamples - 1, (int) ((posInBar - sliceIndex * sliceBeats) * spb));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float in = buffer.getSample (ch, i);

            if (sliceIndex == 0)
            {
                stutterBuffer.setSample (ch, offset, in);
            }
            else
            {
                float gain = 1.0f;
                if (fade > 0)
                    gain = juce::jmin (1.0f, (float) offset / fade, (float) (sliceSamples - 1 - offset) / fade);

                const float repeated = stutterBuffer.getSample (ch, offset) * gain;
                buffer.setSample (ch, i, in * (1.0f - mix) + repeated * mix);
            }
        }
    }
}

// Windowed grains read from a rolling buffer, spawned twice per division.
void ArrowEchAudioProcessor::processGranularDelay (juce::AudioBuffer<float>& buffer, double bpm, double blockStartBeat)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), granularBuffer.getNumChannels());
    const int bufferSize = granularBuffer.getNumSamples();
    const double spb = samplesPerBeat (bpm);

    const double grainBeats = fxDivisionBeats[choiceIndex (p.granularDivision, 4)];
    const double stepBeats = grainBeats * 0.5;
    const int grainLength = juce::jlimit (64, (int) (sampleRate * maxGrainSeconds), (int) (grainBeats * spb));
    const double ratio = std::pow (2.0, param (p.granularPitch) / 12.0);
    const double scatter = param (p.granularScatter) / 100.0;
    const float mix = param (p.granularMix);
    const bool reverse = isOn (p.granularReverse);

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            granularBuffer.setSample (ch, granularWritePosition, buffer.getSample (ch, i));

        const long long step = (long long) std::floor ((blockStartBeat + i / spb) / stepBeats);
        if (step != lastGrainStep)
        {
            lastGrainStep = step;

            for (auto& g : grains)
            {
                if (g.active)
                    continue;

                // Forward grains start far enough back that a pitched-up read head never passes the
                // write head. Reverse grains start just behind it and run backwards through the past.
                const double baseDelay = reverse ? 1.0 : grainLength * juce::jmax (1.0, ratio);
                const double delay = juce::jmin ((double) bufferSize - 2.0 - grainLength * ratio,
                                                 baseDelay + random.nextDouble() * scatter * grainLength * 2.0);
                g.active = true;
                g.readPos = granularWritePosition - delay;
                g.ratio = reverse ? -ratio : ratio;
                g.length = grainLength;
                g.age = 0;
                break;
            }
        }

        float grainL = 0.0f, grainR = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active)
                continue;

            const float phase = (float) g.age / (float) g.length;
            const float window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);

            grainL += window * arrow::readInterpolated (granularBuffer.getReadPointer (0), bufferSize, g.readPos);
            if (numChannels > 1)
                grainR += window * arrow::readInterpolated (granularBuffer.getReadPointer (1), bufferSize, g.readPos);

            g.readPos += g.ratio;
            if (++g.age >= g.length)
                g.active = false;
        }

        buffer.setSample (0, i, buffer.getSample (0, i) * (1.0f - mix) + grainL * mix);
        if (numChannels > 1)
            buffer.setSample (1, i, buffer.getSample (1, i) * (1.0f - mix) + grainR * mix);

        granularWritePosition = (granularWritePosition + 1) % bufferSize;
    }
}

// Sample-and-hold downsampling plus bit reduction. With a rhythm selected,
// crushing is gated on every other step.
void ArrowEchAudioProcessor::processBitcrusher (juce::AudioBuffer<float>& buffer, double bpm, double blockStartBeat)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const double spb = samplesPerBeat (bpm);
    const double stepBeats = crushDivisionBeats[choiceIndex (p.bitcrusherDivision, 4)];
    const float levels = std::pow (2.0f, param (p.bitcrusherBitDepth) - 1.0f);
    const int downsample = juce::jmax (1, (int) param (p.bitcrusherDownsample));

    for (int i = 0; i < numSamples; ++i)
    {
        if (crushCounter == 0)
            for (int ch = 0; ch < numChannels; ++ch)
                crushHeld[(size_t) ch] = std::round (buffer.getSample (ch, i) * levels) / levels;

        crushCounter = (crushCounter + 1) % downsample;

        bool active = true;
        if (stepBeats > 0.0)
            active = ((long long) std::floor ((blockStartBeat + i / spb) / stepBeats)) % 2 == 1;

        if (active)
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, crushHeld[(size_t) ch]);
    }
}

//==============================================================================
double ArrowEchAudioProcessor::getHostBPM() const
{
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm())
                return juce::jlimit (minBpm, maxBpm, *bpm);

    return 120.0;
}

//==============================================================================
bool ArrowEchAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* ArrowEchAudioProcessor::createEditor() { return new ArrowEchAudioProcessorEditor (*this); }

//==============================================================================
void ArrowEchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::StringArray order;
    for (int id : getChainOrder())
        order.add (juce::String (id));

    apvts.state.setProperty ("preset", currentPreset.load(), nullptr);
    apvts.state.setProperty ("chain", order.joinIntoString (","), nullptr);

    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ArrowEchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    currentPreset = (int) apvts.state.getProperty ("preset", 0);

    const auto tokens = juce::StringArray::fromTokens (apvts.state.getProperty ("chain").toString(), ",", "");
    ChainOrder order = defaultOrder;
    if (tokens.size() == (int) order.size())
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = tokens[(int) i].getIntValue();
    setChainOrder (isValidOrder (order) ? order : defaultOrder);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArrowEchAudioProcessor();
}

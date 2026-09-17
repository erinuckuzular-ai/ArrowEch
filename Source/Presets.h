#pragma once

#include <utility>
#include <vector>

// Factory presets. Each preset starts from parameter defaults, then applies these values.
// Choice parameters take an item index, toggles take 0 or 1. `order` optionally sets the
// rack order (module ids from ArrowEchAudioProcessor::ModuleId); empty means default order.
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
    std::vector<int> order {};
};

inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    // Module ids for rack orders
    enum { E = 0, T, S, G, C, F, W, M, P, Q };

    // Echo division: 3 = 1/16, 5 = 1/8T, 6 = 1/8, 7 = 1/8D, 9 = 1/4, 10 = 1/4D, 12 = 1/2
    // Echo style: 0 Digital, 1 Tape, 2 Analog, 3 Lo-Fi, 4 Diffuse, 5 Dub Spring
    // LFO division: 0 1/32, 1 1/16, 2 1/8, 3 1/4, 4 1/2, 5 1 bar, 6 2 bars, 7 4 bars
    static const std::vector<FactoryPreset> presets
    {
        { "Init (boring on purpose)", {} },

        //==============================================================================
        // Dub corner
        { "DUB: Steppers Delay", { { "echoDivision", 7 }, { "echoStyle", 1 }, { "echoFeedback", 0.62f }, { "echoMix", 0.38f },
                                   { "echoLowCut", 180 }, { "echoHighCut", 3800 }, { "echoSaturation", 0.35f }, { "echoWow", 0.12f },
                                   { "echoMode", 2 }, { "echoDuck", 0.25f }, { "echoReverb", 1 }, { "reverbSize", 0.55f },
                                   { "reverbMix", 0.25f } } },

        { "DUB: Spring Tank Splash", { { "echoStyle", 5 }, { "echoDivision", 9 }, { "echoFeedback", 0.5f }, { "echoMix", 0.4f },
                                       { "echoReverb", 1 }, { "reverbSize", 0.4f }, { "reverbMix", 0.35f }, { "echoHighCut", 5000 },
                                       { "echoLowCut", 250 } } },

        { "DUB: Throw It Down The Stairs (hold THROW)", { { "echoThrowOnly", 1 }, { "echoDivision", 10 }, { "echoFeedback", 0.78f },
                                                          { "echoMix", 0.55f }, { "echoStyle", 1 }, { "echoSaturation", 0.4f },
                                                          { "echoHighCut", 3200 }, { "echoLowCut", 200 }, { "echoWow", 0.2f },
                                                          { "echoMode", 2 } } },

        { "DUB: Siren In The Basement", { { "echoDivision", 6 }, { "echoFeedback", 0.72f }, { "echoMix", 0.45f }, { "echoStyle", 2 },
                                          { "filterEnabled", 1 }, { "filterType", 1 }, { "filterCutoff", 900 }, { "filterResonance", 0.75f },
                                          { "filterLfoDepth", 0.9f }, { "filterDivision", 5 } },
          { S, G, C, F, Q, E, W, P, M, T } },

        { "DUB: Freeze Ray (hit FREEZE)", { { "echoDivision", 9 }, { "echoFeedback", 0.7f }, { "echoMix", 0.5f }, { "echoStyle", 5 },
                                            { "echoReverb", 1 }, { "reverbSize", 0.6f }, { "reverbMix", 0.3f } } },

        { "DUB: Sound System Stack", { { "echoDivision", 10 }, { "echoFeedback", 0.6f }, { "echoMix", 0.35f }, { "echoStyle", 1 },
                                       { "echoLowCut", 300 }, { "echoHighCut", 4000 }, { "fuzzEnabled", 1 }, { "fuzzMode", 0 },
                                       { "fuzzDrive", 0.35f }, { "fuzzMix", 0.6f }, { "squishEnabled", 1 }, { "squishAmount", 0.4f },
                                       { "squishMix", 0.8f }, { "squishPumpDivision", 0 }, { "motionEnabled", 1 }, { "motionMode", 1 },
                                       { "motionDivision", 4 }, { "motionDepth", 0.35f } } },

        { "DUB: Melodica Wash", { { "echoDivision", 7 }, { "echoStyle", 4 }, { "echoFeedback", 0.55f }, { "echoMix", 0.35f },
                                  { "shiftEnabled", 1 }, { "shiftMode", 1 }, { "shiftMix", 0.35f }, { "echoReverb", 1 },
                                  { "reverbSize", 0.7f }, { "reverbMix", 0.35f } } },

        { "DUB: Wobbly Tape Skank", { { "echoDivision", 6 }, { "echoStyle", 1 }, { "echoWow", 0.55f }, { "echoWowRate", 0.5f },
                                      { "echoFeedback", 0.5f }, { "echoMix", 0.35f }, { "echoSaturation", 0.3f }, { "echoHighCut", 4500 },
                                      { "motionEnabled", 1 }, { "motionMode", 0 }, { "motionDivision", 2 }, { "motionDepth", 0.5f },
                                      { "motionShape", 0.6f } } },

        { "DUB: Riddim Pump", { { "echoDivision", 7 }, { "echoStyle", 5 }, { "echoFeedback", 0.55f }, { "echoMix", 0.4f },
                                { "echoMode", 2 }, { "squishEnabled", 1 }, { "squishAmount", 0.3f }, { "squishPump", 0.7f },
                                { "squishPumpDivision", 1 } } },

        //==============================================================================
        // Echo & space
        { "Rockabilly Bathroom", { { "echoSync", 0 }, { "echoTimeMs", 110 }, { "echoFeedback", 0.15f }, { "echoMix", 0.35f },
                                   { "echoStyle", 1 }, { "echoSaturation", 0.3f }, { "echoWow", 0.15f }, { "echoHighCut", 6000 } } },

        { "Stadium Dad", { { "echoDivision", 7 }, { "echoFeedback", 0.55f }, { "echoMix", 0.3f }, { "echoStyle", 4 },
                           { "echoDiffusion", 0.5f }, { "echoHighCut", 9000 }, { "echoReverb", 1 }, { "reverbSize", 0.8f },
                           { "reverbMix", 0.4f } } },

        { "Tennis Match", { { "echoMode", 2 }, { "echoStyle", 2 }, { "echoDivision", 9 }, { "echoFeedback", 0.5f },
                            { "echoMix", 0.35f }, { "echoWow", 0.25f }, { "echoWowRate", 0.6f }, { "echoHighCut", 5000 } } },

        { "Two Drunk Robots", { { "echoMode", 1 }, { "echoDivision", 6 }, { "echoOffset", 50 }, { "echoFeedback", 0.45f },
                                { "echoMix", 0.35f }, { "echoWidth", 1.4f } } },

        { "Lo-Fi Basement (Moist)", { { "echoStyle", 3 }, { "echoDivision", 10 }, { "echoFeedback", 0.85f }, { "echoMix", 0.45f },
                                      { "echoSaturation", 0.5f }, { "echoLowCut", 250 }, { "echoWow", 0.3f } } },

        //==============================================================================
        // Width
        { "Instant Twins", { { "echoEnabled", 0 }, { "shiftEnabled", 1 }, { "shiftMode", 1 }, { "shiftDetune", 0.6f },
                             { "shiftDelay", 0.4f }, { "shiftMix", 0.5f } } },

        { "Choir Of Yous", { { "echoDivision", 6 }, { "echoFeedback", 0.3f }, { "echoMix", 0.22f }, { "echoStyle", 1 },
                             { "echoDuck", 0.6f }, { "echoReverb", 1 }, { "reverbSize", 0.6f }, { "reverbMix", 0.3f },
                             { "shiftEnabled", 1 }, { "shiftMode", 2 }, { "shiftDetune", 0.45f }, { "shiftDelay", 0.35f },
                             { "shiftMix", 0.4f }, { "shiftFocus", 200 } } },

        //==============================================================================
        // Glitch garage
        { "Skipping CD", { { "echoMix", 0.2f }, { "stutterEnabled", 1 }, { "stutterDivision", 2 }, { "stutterMix", 1.0f } } },

        { "Grain Salad Supreme", { { "granularEnabled", 1 }, { "granularDivision", 1 }, { "granularScatter", 80 }, { "granularPitch", 12 },
                                   { "granularMix", 0.5f }, { "echoFeedback", 0.6f }, { "echoMix", 0.3f }, { "echoStyle", 4 },
                                   { "echoReverb", 1 } } },

        { "Reverse Crystals", { { "granularEnabled", 1 }, { "granularReverse", 1 }, { "granularDivision", 1 }, { "granularScatter", 40 },
                                { "granularPitch", 12 }, { "granularMix", 0.5f }, { "echoStyle", 4 }, { "echoFeedback", 0.6f },
                                { "echoMix", 0.35f }, { "echoReverb", 1 }, { "reverbSize", 0.85f }, { "reverbMix", 0.4f } } },

        { "Toaster Beat", { { "bitcrusherEnabled", 1 }, { "bitcrusherDivision", 2 }, { "bitcrusherBitDepth", 6 },
                            { "bitcrusherDownsample", 8 }, { "echoDivision", 3 }, { "echoFeedback", 0.25f },
                            { "echoMix", 0.2f }, { "echoStyle", 3 } } },

        //==============================================================================
        // Tone zoo
        { "Angry Little Radio", { { "fuzzEnabled", 1 }, { "fuzzMode", 3 }, { "fuzzDrive", 0.7f }, { "fuzzTone", 0.4f },
                                  { "filterEnabled", 1 }, { "filterType", 1 }, { "filterCutoff", 1500 }, { "filterResonance", 0.4f },
                                  { "filterLfoDepth", 0.2f }, { "filterDivision", 6 }, { "bitcrusherEnabled", 1 },
                                  { "bitcrusherBitDepth", 10 }, { "bitcrusherDownsample", 2 }, { "echoMix", 0.2f } } },

        { "Wah Goblin Funk", { { "filterEnabled", 1 }, { "filterType", 0 }, { "filterCutoff", 500 }, { "filterResonance", 0.7f },
                               { "filterEnvelope", 0.8f }, { "filterLfoDepth", 0.0f }, { "fuzzEnabled", 1 }, { "fuzzDrive", 0.3f },
                               { "echoEnabled", 0 } } },

        { "Seasick Tremolo", { { "motionEnabled", 1 }, { "motionMode", 0 }, { "motionDivision", 2 }, { "motionDepth", 0.8f },
                               { "motionShape", 0.7f }, { "motionSpread", 1.0f }, { "echoMix", 0.15f } } },

        { "Pan Galactic", { { "motionEnabled", 1 }, { "motionMode", 1 }, { "motionDivision", 1 }, { "motionDepth", 0.9f },
                            { "motionRandom", 0.6f }, { "phaserEnabled", 1 }, { "phaserDivision", 6 }, { "phaserDepth", 0.7f },
                            { "phaserFeedback", 0.6f }, { "phaserMix", 0.5f }, { "echoMix", 0.2f } } },

        { "Jet Plane In A Jar", { { "phaserEnabled", 1 }, { "phaserDivision", 5 }, { "phaserDepth", 1.0f }, { "phaserFeedback", 0.85f },
                                  { "phaserCentre", 1200 }, { "phaserMix", 0.6f }, { "fuzzEnabled", 1 }, { "fuzzMode", 2 },
                                  { "fuzzDrive", 0.3f }, { "echoEnabled", 0 } } },

        { "Squished Drums", { { "echoEnabled", 0 }, { "squishEnabled", 1 }, { "squishAmount", 0.8f }, { "squishRelease", 60 },
                              { "squishMix", 0.6f }, { "squishPumpDivision", 0 }, { "fuzzEnabled", 1 }, { "fuzzDrive", 0.3f },
                              { "fuzzMix", 0.5f } } },

        { "Pumpin' Like It's 2009", { { "squishEnabled", 1 }, { "squishPumpDivision", 1 }, { "squishPump", 0.85f },
                                      { "squishAmount", 0.3f }, { "echoDivision", 7 }, { "echoFeedback", 0.4f }, { "echoMix", 0.25f } } },

        { "Broken Toy Robot", { { "fuzzEnabled", 1 }, { "fuzzMode", 4 }, { "fuzzDrive", 0.6f }, { "fuzzMix", 0.5f },
                                { "bitcrusherEnabled", 1 }, { "bitcrusherDivision", 3 }, { "bitcrusherBitDepth", 5 },
                                { "bitcrusherDownsample", 6 }, { "stutterEnabled", 1 }, { "stutterDivision", 2 },
                                { "stutterMix", 0.7f }, { "shiftEnabled", 1 }, { "shiftMode", 2 } } },

        //==============================================================================
        // Chaos
        { "Feedback Party (Hit OH NO)", { { "echoDivision", 7 }, { "echoFeedback", 1.04f }, { "echoMix", 0.4f }, { "echoStyle", 1 },
                                          { "echoSaturation", 0.4f }, { "echoHighCut", 4500 }, { "echoLowCut", 200 },
                                          { "echoWow", 0.35f }, { "echoMode", 2 } } },

        { "Everything Everywhere (Sorry)", { { "echoDivision", 7 }, { "echoMode", 2 }, { "echoStyle", 5 }, { "echoFeedback", 0.6f },
                                             { "echoMix", 0.35f }, { "shiftEnabled", 1 }, { "stutterEnabled", 1 }, { "stutterMix", 0.4f },
                                             { "granularEnabled", 1 }, { "granularMix", 0.3f }, { "bitcrusherEnabled", 1 },
                                             { "bitcrusherDivision", 2 }, { "fuzzEnabled", 1 }, { "fuzzDrive", 0.3f },
                                             { "filterEnabled", 1 }, { "filterLfoDepth", 0.6f }, { "motionEnabled", 1 },
                                             { "motionMode", 1 }, { "phaserEnabled", 1 }, { "phaserMix", 0.35f },
                                             { "squishEnabled", 1 }, { "squishAmount", 0.3f }, { "rackMix", 0.8f } },
          { G, S, C, F, W, Q, P, E, M, T } },
    };

    return presets;
}

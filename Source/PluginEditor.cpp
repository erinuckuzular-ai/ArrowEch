#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using LAF = ArrowLookAndFeel;
using Proc = ArrowEchAudioProcessor;

namespace
{
    struct ModuleInfo
    {
        const char* chipName;
        juce::Colour colour;
        int page;
        const char* blurb;
    };

    const ModuleInfo& moduleInfo (int moduleId)
    {
        static const ModuleInfo infos[] = {
            { "ECHO-ER", LAF::sky,       -1, "the echo. the main character." },
            { "TWINS",   LAF::mint,       0, "wobble twins. stereo detune thickener." },
            { "STUTTER", LAF::bubblegum,  0, "s-s-stutter. bar-synced repeats." },
            { "GRAINS",  LAF::lilac,      0, "grain salad. granular delay." },
            { "CRUNCH",  LAF::tomato,     0, "crunch-o-matic. bitcrusher." },
            { "FUZZ",    LAF::tangerine,  1, "fuzz bucket. saturation." },
            { "GOBLIN",  LAF::lime,       1, "wah goblin. swept resonant filter." },
            { "SEASICK", LAF::peach,      1, "seasick. tremolo and auto-pan." },
            { "SWOOSH",  LAF::aqua,       1, "swoosh-a-tron. phaser." },
            { "SQUISH",  LAF::orchid,     1, "squish. compressor with pump." },
        };
        return infos[juce::jlimit (0, (int) Proc::numModules - 1, moduleId)];
    }
}

//==============================================================================
ArrowEchAudioProcessorEditor::ArrowEchAudioProcessorEditor (ArrowEchAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), rackStrip (p), tapDisplay (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (canvas);
    canvas.onPaint = [this] (juce::Graphics& g) { paintCanvas (g); };

    //==============================================================================
    // Header
    canvas.addAndMakeVisible (blob);
    blob.onPoke = [this]
    {
        static const juce::StringArray pokes {
            "hey. don't poke me.",
            "ow.",
            "i'm telling your DAW.",
            "that tickles. do it again. no wait, don't.",
            "poking the blob does not make the mix better.",
            "fine. a secret: hold THROW on a snare. you're welcome.",
            "i have 10 effects and zero chill."
        };
        pokeText = pokes[pokeCount++ % pokes.size()];
        pokeFrames = 75;
    };

    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i)
        presetBox.addItem (audioProcessor.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (audioProcessor.getCurrentProgram(), juce::dontSendNotification);
    presetBox.setColour (juce::ComboBox::arrowColourId, LAF::butter);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        if (index >= 0 && index != audioProcessor.getCurrentProgram())
        {
            audioProcessor.setCurrentProgram (index);
            pokeText = "loaded '" + audioProcessor.getProgramName (index) + "'. excellent taste.";
            pokeFrames = 90;
        }
    };
    canvas.addAndMakeVisible (presetBox);
    addQuip (presetBox, [] { return juce::String ("presets. lovingly made by a professional goofball. the DUB ones are extra moist."); });

    auto stepPreset = [this] (int delta)
    {
        const int count = audioProcessor.getNumPrograms();
        presetBox.setSelectedItemIndex ((audioProcessor.getCurrentProgram() + delta + count) % count);
    };
    prevPreset.onClick = [stepPreset] { stepPreset (-1); };
    nextPreset.onClick = [stepPreset] { stepPreset (1); };
    canvas.addAndMakeVisible (prevPreset);
    canvas.addAndMakeVisible (nextPreset);
    addQuip (prevPreset, [] { return juce::String ("previous preset. go back. relive the past."); });
    addQuip (nextPreset, [] { return juce::String ("next preset. onward!"); });

    diceButton.setColour (juce::TextButton::buttonColourId, LAF::mint);
    diceButton.onClick = [this]
    {
        audioProcessor.randomize();
        static const juce::StringArray rolls { "dice rolled. no refunds.", "chaos applied. good luck.",
                                               "i picked these with my eyes closed.", "science!",
                                               "this one's a banger. probably.", "i also shuffled the rack. maybe. who knows." };
        pokeText = rolls[juce::Random::getSystemRandom().nextInt (rolls.size())];
        pokeFrames = 90;
    };
    canvas.addAndMakeVisible (diceButton);
    addQuip (diceButton, [] { return juce::String ("randomizes everything (except your levels) and sometimes the rack order. fortune favours the reckless."); });

    panicButton.setColour (juce::TextButton::buttonColourId, LAF::tomato);
    panicButton.onClick = [this]
    {
        audioProcessor.panic();
        setParam ("echoFreeze", 0.0f);
        pokeText = "ALL TAILS DELETED. freeze off. we're safe. we're safe now.";
        pokeFrames = 90;
    };
    canvas.addAndMakeVisible (panicButton);
    addQuip (panicButton, [] { return juce::String ("emergency button. clears every echo, grain and tail, and unfreezes."); });

    setupKnob (inputGain, "inputGain", "IN", LAF::butter,
               [this] { return "input: " + text ("inputGain") + ". how hard you shove sound into me."; });
    setupKnob (outputGain, "outputGain", "OUT", LAF::butter,
               [this] { return "output: " + text ("outputGain") + ". how loud i yell back."; });
    setupKnob (rackMix, "rackMix", "RACK", LAF::butter,
               [this] { return "rack mix: " + text ("rackMix") + ". blends the whole rack with your dry signal. 100% = all goblin."; });
    for (auto* k : { &inputGain, &outputGain, &rackMix })
    {
        k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->slider.setPopupDisplayEnabled (true, true, this);
    }
    canvas.addAndMakeVisible (inputMeter);
    canvas.addAndMakeVisible (outputMeter);

    canvas.addAndMakeVisible (rackStrip);
    rackStrip.onSelect = [this] (int moduleId) { selectModule (moduleId); };
    addQuip (rackStrip, [this]
    {
        const int id = rackStrip.moduleAt (rackStrip.getMouseXYRelative());
        const juce::String base = "the rack! sound flows left to right. drag chips to reorder, click the dot to power, click the name to jump.";
        return id < 0 ? base : juce::String (moduleInfo (id).chipName) + ": " + moduleInfo (id).blurb + " drag me somewhere else.";
    });

    garageTab.onClick = [this] { setPage (0); };
    zooTab.onClick = [this] { setPage (1); };
    for (auto* tab : { &garageTab, &zooTab })
    {
        tab->setColour (juce::TextButton::buttonOnColourId, LAF::butter);
        canvas.addAndMakeVisible (*tab);
    }
    addQuip (garageTab, [] { return juce::String ("the glitch garage: twins, stutter, grains and crunch."); });
    addQuip (zooTab, [] { return juce::String ("the tone zoo: fuzz, goblin, seasick, swoosh and squish. feed them."); });

    //==============================================================================
    // Echo
    const auto echoColour = LAF::sky;
    setupToggle (echoToggle, "echoEnabled", "ON",
                 [this] { return value ("echoEnabled") > 0.5f ? juce::String ("echo is ON. i will repeat you. i will repeat you.")
                                                               : juce::String ("echo is off. i have nothing to say. for once."); });
    setupToggle (echoSync, "echoSync", "SYNC",
                 [this] { return value ("echoSync") > 0.5f ? juce::String ("sync ON: echo time locks to your DAW tempo. pick a division.")
                                                            : juce::String ("sync OFF: free-range milliseconds. use the TIME knob."); });
    setupSelector (echoDivision, "echoDivision", Proc::echoDivisionNames, echoColour,
                   [] { return juce::String ("note length for the echo. T = triplet, D = dotted. 1/8D = instant stadium."); });
    setupSelector (echoMode, "echoMode", { "Single", "Dual (2 brains)", "Ping-Pong (tennis)" }, echoColour,
                   [] { return juce::String ("single = both sides together. dual = L and R do their own thing. ping-pong = bounces side to side."); });
    setupSelector (echoStyle, "echoStyle", { "Digital (clean nerd)", "Tape (warm grandpa)", "Analog (fuzzy bucket)",
                                             "Lo-Fi (broken radio)", "Diffuse (cloud mode)", "Dub Spring (boing)" }, echoColour,
                   [] { return juce::String ("the personality of the repeats. tape wobbles, analog gets dark, dub spring goes BOING."); });

    setupToggle (echoFreeze, "echoFreeze", "FREEZE", [this]
    {
        return value ("echoFreeze") > 0.5f ? juce::String ("FROZEN. the current repeats loop forever and new sound stays out. play over it!")
                                           : juce::String ("freeze: traps whatever is echoing right now in an infinite loop. dub essential.");
    });
    setupToggle (echoThrowOnly, "echoThrowOnly", "THROW MODE",
                 [] { return juce::String ("throw mode: nothing goes into the echo unless you hold THROW. classic dub send trick."); });

    throwButton.setColour (juce::TextButton::buttonColourId, LAF::tomato);
    throwButton.setColour (juce::TextButton::buttonOnColourId, LAF::butter);
    throwButton.onStateChange = [this]
    {
        const bool down = throwButton.isDown();
        if (down != throwHeld)
        {
            throwHeld = down;
            setParam ("echoThrow", down ? 1.0f : 0.0f);
        }
    };
    canvas.addAndMakeVisible (throwButton);
    addQuip (throwButton, [this]
    {
        return value ("echoThrowOnly") > 0.5f ? juce::String ("HOLD ME to throw sound into the echo. let go and the tail rides out.")
                                              : juce::String ("turn on THROW MODE first, then hold me to throw sound into the echo.");
    });

    setupKnob (echoTime, "echoTimeMs", "TIME", echoColour, [this]
    {
        if (value ("echoSync") > 0.5f)
            return juce::String ("i'm napping because SYNC is on. turn it off to use milliseconds.");
        return "echo time: " + text ("echoTimeMs") + ". under 120ms = slapback. over 500ms = canyon.";
    });
    setupKnob (echoFeedback, "echoFeedback", "AGAIN-NESS", echoColour, [this]
    {
        const float v = value ("echoFeedback");
        const auto t = "feedback " + text ("echoFeedback") + ": ";
        if (v < 0.3f)  return t + "a polite number of repeats.";
        if (v < 0.7f)  return t + "repeats! repeats! repeats!";
        if (v <= 1.0f) return t + "this is getting out of hand.";
        return t + "OVER 100%. it will never stop. you did this.";
    });
    setupKnob (echoMix, "echoMix", "MIX", echoColour,
               [this] { return "mix " + text ("echoMix") + ". at 50% you get all of you AND all of me."; });
    setupKnob (echoOffset, "echoOffset", "L/R OFFSET", echoColour, [this]
    {
        if ((int) value ("echoMode") == 0)
            return juce::String ("offset only works in dual or ping-pong. single mode is too committed.");
        return "offset " + text ("echoOffset") + ". makes left and right disagree about time. bouncy.";
    });
    setupKnob (echoWidth, "echoWidth", "WIDTH", echoColour, [this]
    {
        const float v = value ("echoWidth");
        return "width " + text ("echoWidth") + (v < 0.2f ? ". mono potato." : v > 1.5f ? ". extremely wide boy." : ". a reasonable width.");
    });
    setupKnob (echoDuck, "echoDuck", "DUCK", echoColour,
               [this] { return "ducking " + text ("echoDuck") + ". i shut up while you play, then yell in the gaps. quack."; });
    setupKnob (echoLowCut, "echoLowCut", "LOW CUT", echoColour,
               [this] { return "low cut " + text ("echoLowCut") + ". scoops the mud out of the repeats. dub tip: 200-300 Hz."; });
    setupKnob (echoHighCut, "echoHighCut", "HIGH CUT", echoColour, [this]
    {
        return "high cut " + text ("echoHighCut") + (value ("echoHighCut") < 3000.0f ? ". very cave. much underwater." : ". lower = darker, older repeats.");
    });
    setupKnob (echoSaturation, "echoSaturation", "FUZZ", echoColour,
               [this] { return "saturation " + text ("echoSaturation") + ". crunches every repeat a bit more. crank for crispy regret."; });
    setupKnob (echoWow, "echoWow", "WOBBLE", echoColour,
               [this] { return "wow " + text ("echoWow") + ". wobbly tape pitch. seasickness sold separately."; });
    setupKnob (echoWowRate, "echoWowRate", "WOB SPEED", echoColour,
               [this] { return "wobble speed " + text ("echoWowRate") + ". slow = drifty. fast = vibrato goat."; });
    setupKnob (echoDiffusion, "echoDiffusion", "SMEAR", echoColour,
               [this] { return "diffusion " + text ("echoDiffusion") + ". smears the repeats into a lovely soup."; });
    setupToggle (echoReverb, "echoReverb", "BIG ROOM",
                 [] { return juce::String ("puts the echoes (only the echoes) inside a big tiled bathroom."); });
    setupKnob (reverbSize, "reverbSize", "ROOM SIZE", echoColour,
               [this] { return "room size " + text ("reverbSize") + ". closet -> cathedral."; });
    setupKnob (reverbMix, "reverbMix", "ROOM MIX", echoColour,
               [this] { return "room mix " + text ("reverbMix") + ". how much bathroom is on the echoes."; });

    canvas.addAndMakeVisible (tapDisplay);
    addQuip (tapDisplay, [] { return juce::String ("this is what the echoes look like. top = left ear, bottom = right ear."); });

    dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("echoEnabled"),
                         { &echoSync.button, &echoDivision.box, &echoMode.box, &echoStyle.box, &echoTime.slider, &echoFeedback.slider,
                           &echoMix.slider, &echoOffset.slider, &echoWidth.slider, &echoDuck.slider, &echoLowCut.slider,
                           &echoHighCut.slider, &echoSaturation.slider, &echoWow.slider, &echoWowRate.slider, &echoDiffusion.slider,
                           &echoReverb.button, &reverbSize.slider, &reverbMix.slider, &tapDisplay, &echoFreeze.button,
                           &echoThrowOnly.button, &throwButton } });

    //==============================================================================
    // Glitch garage (page 0)
    {
        const auto colour = LAF::mint;
        setupToggle (shiftToggle, "shiftEnabled", "ON",
                     [] { return juce::String ("summons two slightly-wrong clones of you. one sharp, one flat. instant wide."); }, 0);

        const juce::StringArray modeLabels { "SNUG", "WIDE", "SEASICK" };
        const juce::StringArray modeQuips { "snug: subtle and tight. the polite twins.",
                                            "wide: bigger detune, later twins. the classic fat stereo thing.",
                                            "seasick: the detune slowly drifts around. twins on a boat." };
        for (int i = 0; i < 3; ++i)
        {
            auto& b = shiftModeButtons[(size_t) i];
            b.setButtonText (modeLabels[i]);
            b.onClick = [this, i] { setParam ("shiftMode", (float) i); };
            canvas.addAndMakeVisible (b);
            addToPage (b, 0);
            const auto quip = modeQuips[i];
            addQuip (b, [quip] { return quip; });
        }

        setupKnob (shiftDetune, "shiftDetune", "DETUNE", colour, [this]
        {
            return "detune " + text ("shiftDetune") + (value ("shiftDetune") > 0.75f ? ". the twins are drunk now." : ". how out of tune the twins are.");
        }, 0);
        setupKnob (shiftDelay, "shiftDelay", "LATENESS", colour,
                   [this] { return "delay " + text ("shiftDelay") + ". how fashionably late the twins arrive."; }, 0);
        setupKnob (shiftFocus, "shiftFocus", "FOCUS", colour,
                   [this] { return "focus " + text ("shiftFocus") + ". keeps bass out of the twins so your low end stays solid."; }, 0);
        setupKnob (shiftMix, "shiftMix", "MIX", colour,
                   [this] { return "twin mix " + text ("shiftMix") + ". how loud the clones are."; }, 0);

        dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("shiftEnabled"),
                             { &shiftModeButtons[0], &shiftModeButtons[1], &shiftModeButtons[2], &shiftDetune.slider,
                               &shiftDelay.slider, &shiftFocus.slider, &shiftMix.slider } });
    }
    {
        const auto colour = LAF::bubblegum;
        setupToggle (stutterToggle, "stutterEnabled", "ON",
                     [] { return juce::String ("records the start of each bar, then repeats it. like a CD that fell down the stairs."); }, 0);
        setupSelector (stutterDivision, "stutterDivision", Proc::fxDivisionNames, colour,
                       [] { return juce::String ("slice size. 1/32 = machine gun. 1/4 = polite hiccup."); }, 0);
        setupKnob (stutterMix, "stutterMix", "MIX", colour,
                   [this] { return "stutter mix " + text ("stutterMix") + ". b-b-b-b-balance."; }, 0);
        dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("stutterEnabled"), { &stutterDivision.box, &stutterMix.slider } });
    }
    {
        const auto colour = LAF::lilac;
        setupToggle (granularToggle, "granularEnabled", "ON",
                     [] { return juce::String ("chops you into tiny grains and flings them around in time."); }, 0);
        setupToggle (granularReverse, "granularReverse", "REVERSE",
                     [] { return juce::String ("grains play BACKWARDS. instant crystal cave. pair with +12 pitch for sparkle."); }, 0);
        setupSelector (granularDivision, "granularDivision", Proc::fxDivisionNames, colour,
                       [] { return juce::String ("grain size, synced to tempo. smaller = sparklier."); }, 0);
        setupKnob (granularScatter, "granularScatter", "SCATTER", colour,
                   [this] { return "scatter " + text ("granularScatter") + ". how randomly the grains get tossed back in time."; }, 0);
        setupKnob (granularPitch, "granularPitch", "PITCH", colour, [this]
        {
            const float v = value ("granularPitch");
            return "grain pitch " + text ("granularPitch") + (v >= 11.9f ? ". chipmunk choir unlocked." : v <= -11.9f ? ". grumpy giant mode." : ".");
        }, 0);
        setupKnob (granularMix, "granularMix", "MIX", colour,
                   [this] { return "grain mix " + text ("granularMix") + ". how much salad on the plate."; }, 0);
        dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("granularEnabled"),
                             { &granularDivision.box, &granularReverse.button, &granularScatter.slider, &granularPitch.slider, &granularMix.slider } });
    }
    {
        const auto colour = LAF::tomato;
        setupToggle (bitcrusherToggle, "bitcrusherEnabled", "ON",
                     [] { return juce::String ("removes bits. adds attitude. may void your warranty."); }, 0);
        setupSelector (bitcrusherDivision, "bitcrusherDivision", { "Always (committed)", "1/4", "1/8", "1/16" }, colour,
                       [] { return juce::String ("rhythm: crush on every other step, or ALWAYS for the fully committed."); }, 0);
        setupKnob (bitcrusherBitDepth, "bitcrusherBitDepth", "BITS", colour, [this]
        {
            const float v = value ("bitcrusherBitDepth");
            return text ("bitcrusherBitDepth") + (v <= 4.0f ? ". this is a game boy now." : v >= 14.0f ? ". basically fine. suspiciously fine." : ". crunchy.");
        }, 0);
        setupKnob (bitcrusherDownsample, "bitcrusherDownsample", "DOWNSAMPLE", colour,
                   [this] { return "downsample " + text ("bitcrusherDownsample") + ". fewer samples, more robot."; }, 0);
        dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("bitcrusherEnabled"),
                             { &bitcrusherDivision.box, &bitcrusherBitDepth.slider, &bitcrusherDownsample.slider } });
    }

    //==============================================================================
    // Tone zoo (page 1)
    {
        const auto colour = LAF::tangerine;
        setupToggle (fuzzToggle, "fuzzEnabled", "", [] { return juce::String ("fuzz bucket: warm, crunchy, borderline illegal saturation."); }, 1);
        setupSelector (fuzzMode, "fuzzMode", { "Tube", "Transistor", "Tape", "Fuzz (angry)", "Broken (?!)" }, colour, [this]
        {
            static const juce::StringArray lines { "tube: warm and round, like a hug from an amp.", "transistor: bright, edgy, a bit rude.",
                                                   "tape: smooth squash with a vintage smile.", "fuzz: ANGRY. gated. 1970s bedroom.",
                                                   "broken: wavefolding chaos. nobody knows what it does. not even me." };
            return lines[(int) value ("fuzzMode")];
        }, 1);
        punishButton.setColour (juce::TextButton::buttonOnColourId, LAF::tomato);
        punishButton.onClick = [this] { setParam ("fuzzPunish", value ("fuzzPunish") > 0.5f ? 0.0f : 1.0f); };
        canvas.addAndMakeVisible (punishButton);
        addToPage (punishButton, 1);
        addQuip (punishButton, [this]
        {
            return value ("fuzzPunish") > 0.5f ? juce::String ("PUNISH IS ON. +18 dB of drive. your speakers have filed a complaint.")
                                               : juce::String ("punish: adds a rude +18 dB of drive. press at your own risk.");
        });
        setupKnob (fuzzDrive, "fuzzDrive", "DRIVE", colour, [this] { return "drive " + text ("fuzzDrive") + ". how hard i squeeze the tubes."; }, 1);
        setupKnob (fuzzTone, "fuzzTone", "TONE", colour, [this] { return "tone " + text ("fuzzTone") + ". left = dark and cosy, right = fizzy and bright."; }, 1);
        setupKnob (fuzzMix, "fuzzMix", "MIX", colour, [this] { return "fuzz mix " + text ("fuzzMix") + ". blend in clean signal for parallel crunch."; }, 1);
        setupKnob (fuzzOutput, "fuzzOutput", "OUTPUT", colour, [this] { return "fuzz output " + text ("fuzzOutput") + ". i auto-level, but you can still boss me."; }, 1);
        dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("fuzzEnabled"),
                             { &fuzzMode.box, &punishButton, &fuzzDrive.slider, &fuzzTone.slider, &fuzzMix.slider, &fuzzOutput.slider } });
    }
    {
        const auto colour = LAF::lime;
        setupToggle (filterToggle, "filterEnabled", "", [] { return juce::String ("wah goblin: a resonant filter that lives in your track and wiggles."); }, 1);
        setupSelector (filterType, "filterType", { "Low", "Band", "High" }, colour,
                       [] { return juce::String ("filter type. low = muffled, band = wah/radio, high = thin and tinny."); }, 1);
        setupSelector (filterDivision, "filterDivision", Proc::lfoDivisionNames, colour,
                       [] { return juce::String ("how fast the goblin sweeps, synced to tempo. 1 bar + band-pass = dub siren."); }, 1);
        setupKnob (filterCutoff, "filterCutoff", "CUTOFF", colour, [this] { return "cutoff " + text ("filterCutoff") + ". where the goblin sits."; }, 1);
        setupKnob (filterResonance, "filterResonance", "RESO", colour, [this]
        {
            return "resonance " + text ("filterResonance") + (value ("filterResonance") > 0.8f ? ". SQUEALY. the goblin is screaming." : ". adds a peaky whistle.");
        }, 1);
        setupKnob (filterLfoDepth, "filterLfoDepth", "WIGGLE", colour, [this] { return "LFO depth " + text ("filterLfoDepth") + ". how far the goblin sweeps (up to ±3 octaves)."; }, 1);
        setupKnob (filterEnvelope, "filterEnvelope", "ENV", colour, [this] { return "envelope " + text ("filterEnvelope") + ". louder playing opens the filter. auto-wah! negative = backwards wah."; }, 1);
        dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("filterEnabled"),
                             { &filterType.box, &filterDivision.box, &filterCutoff.slider, &filterResonance.slider, &filterLfoDepth.slider, &filterEnvelope.slider } });
    }
    {
        const auto colour = LAF::peach;
        setupToggle (motionToggle, "motionEnabled", "", [] { return juce::String ("seasick: tempo-synced tremolo and auto-pan. bring a bucket."); }, 1);
        setupSelector (motionMode, "motionMode", { "Trem", "Pan", "Both" }, colour,
                       [] { return juce::String ("trem = volume wobble. pan = side to side. both = full boat ride."); }, 1);
        setupSelector (motionDivision, "motionDivision", Proc::lfoDivisionNames, colour,
                       [] { return juce::String ("wobble rate, synced to tempo."); }, 1);
        setupKnob (motionDepth, "motionDepth", "DEPTH", colour, [this] { return "depth " + text ("motionDepth") + ". how much the boat rocks."; }, 1);
        setupKnob (motionShape, "motionShape", "CHOP", colour, [this] { return "shape " + text ("motionShape") + ". 0 = smooth sine, 100 = choppy square gate."; }, 1);
        setupKnob (motionSpread, "motionSpread", "SPREAD", colour, [this] { return "stereo spread " + text ("motionSpread") + ". offsets left and right tremolo. wide wobbles."; }, 1);
        setupKnob (motionRandom, "motionRandom", "RANDOM", colour, [this] { return "randomness " + text ("motionRandom") + ". each step goes somewhere unpredictable. like a raccoon."; }, 1);
        dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("motionEnabled"),
                             { &motionMode.box, &motionDivision.box, &motionDepth.slider, &motionShape.slider, &motionSpread.slider, &motionRandom.slider } });
    }
    {
        const auto colour = LAF::aqua;
        setupToggle (phaserToggle, "phaserEnabled", "", [] { return juce::String ("swoosh-a-tron: a phaser. jet plane in a jar."); }, 1);
        setupSelector (phaserDivision, "phaserDivision", { "sweep 1/32", "sweep 1/16", "sweep 1/8", "sweep 1/4", "sweep 1/2",
                                                           "sweep 1 bar", "sweep 2 bars", "sweep 4 bars" }, colour,
                       [] { return juce::String ("how long one swoosh takes, synced to tempo."); }, 1);
        setupKnob (phaserDepth, "phaserDepth", "DEPTH", colour, [this] { return "depth " + text ("phaserDepth") + ". how big the swoosh is."; }, 1);
        setupKnob (phaserFeedback, "phaserFeedback", "FEEDBACK", colour, [this] { return "feedback " + text ("phaserFeedback") + ". more = whooshier and more robotic."; }, 1);
        setupKnob (phaserCentre, "phaserCentre", "CENTRE", colour, [this] { return "centre " + text ("phaserCentre") + ". where the swoosh happens."; }, 1);
        setupKnob (phaserMix, "phaserMix", "MIX", colour, [this] { return "phaser mix " + text ("phaserMix") + ". 50% = maximum swoosh."; }, 1);
        dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("phaserEnabled"),
                             { &phaserDivision.box, &phaserDepth.slider, &phaserFeedback.slider, &phaserCentre.slider, &phaserMix.slider } });
    }
    {
        const auto colour = LAF::orchid;
        setupToggle (squishToggle, "squishEnabled", "", [] { return juce::String ("squish: a very enthusiastic compressor with a built-in pump."); }, 1);
        setupSelector (squishPumpDivision, "squishPumpDivision", { "pump: off", "pump: 1/4", "pump: 1/8", "pump: 1/16" }, colour,
                       [] { return juce::String ("pump rate: fake sidechain ducking on the beat. 1/4 = EDM uncle."); }, 1);
        setupKnob (squishAmount, "squishAmount", "SQUISH", colour, [this] { return "squish " + text ("squishAmount") + ". more = flatter, louder, grumpier."; }, 1);
        setupKnob (squishRelease, "squishRelease", "RELEASE", colour, [this] { return "release " + text ("squishRelease") + ". short = breathy pumping, long = smooth."; }, 1);
        setupKnob (squishPump, "squishPump", "PUMP", colour, [this] { return "pump " + text ("squishPump") + ". how hard the beat ducks everything."; }, 1);
        setupKnob (squishMix, "squishMix", "MIX", colour, [this] { return "squish mix " + text ("squishMix") + ". parallel compression, aka the new york squeeze."; }, 1);
        dimmers.push_back ({ audioProcessor.apvts.getRawParameterValue ("squishEnabled"),
                             { &squishPumpDivision.box, &squishAmount.slider, &squishRelease.slider, &squishPump.slider, &squishMix.slider } });
    }

    //==============================================================================
    layoutCanvas();
    setPage (0);

    setResizable (true, true);
    setResizeLimits ((int) (baseWidth * 0.55), (int) (baseHeight * 0.55), (int) (baseWidth * 1.6), (int) (baseHeight * 1.6));
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / baseHeight);
    setSize ((int) (baseWidth * 0.85), (int) (baseHeight * 0.85));

    startTimerHz (30);
}

ArrowEchAudioProcessorEditor::~ArrowEchAudioProcessorEditor()
{
    stopTimer();
    if (throwHeld)
        setParam ("echoThrow", 0.0f);
    setLookAndFeel (nullptr);
}

//==============================================================================
void ArrowEchAudioProcessorEditor::addQuip (juce::Component& component, Quip quip)
{
    quips[&component] = std::move (quip);
    component.addMouseListener (this, true);
}

void ArrowEchAudioProcessorEditor::addToPage (juce::Component& component, int page)
{
    if (page >= 0)
        pageComponents[(size_t) page].push_back (&component);
}

void ArrowEchAudioProcessorEditor::setParam (const char* paramId, float plainValue)
{
    if (auto* param = audioProcessor.apvts.getParameter (paramId))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
        param->endChangeGesture();
    }
}

void ArrowEchAudioProcessorEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name, juce::Colour colour, Quip quip, int page)
{
    auto* param = audioProcessor.apvts.getParameter (paramId);
    jassert (param != nullptr);

    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, colour.darker (0.45f));
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    canvas.addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, paramId, k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (LAF::chunky (13.0f));
    k.label.setInterceptsMouseClicks (false, false);
    canvas.addAndMakeVisible (k.label);

    addToPage (k.slider, page);
    addToPage (k.label, page);
    addQuip (k.slider, std::move (quip));
}

void ArrowEchAudioProcessorEditor::setupSelector (Selector& s, const juce::String& paramId, const juce::StringArray& items, juce::Colour colour, Quip quip, int page)
{
    s.box.addItemList (items, 1);
    s.box.setColour (juce::ComboBox::arrowColourId, colour);
    canvas.addAndMakeVisible (s.box);
    s.attachment = std::make_unique<ComboAttachment> (audioProcessor.apvts, paramId, s.box);
    addToPage (s.box, page);
    addQuip (s.box, std::move (quip));
}

void ArrowEchAudioProcessorEditor::setupToggle (Toggle& t, const juce::String& paramId, const juce::String& name, Quip quip, int page)
{
    t.button.setButtonText (name);
    t.button.setColour (juce::ToggleButton::tickColourId, LAF::butter);
    canvas.addAndMakeVisible (t.button);
    t.attachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, paramId, t.button);
    addToPage (t.button, page);
    addQuip (t.button, std::move (quip));
}

float ArrowEchAudioProcessorEditor::value (const char* paramId) const
{
    return audioProcessor.apvts.getRawParameterValue (paramId)->load();
}

juce::String ArrowEchAudioProcessorEditor::text (const char* paramId) const
{
    return audioProcessor.apvts.getParameter (paramId)->getCurrentValueAsText();
}

//==============================================================================
void ArrowEchAudioProcessorEditor::setPage (int page)
{
    currentPage = page;
    for (size_t i = 0; i < pageComponents.size(); ++i)
        for (auto* c : pageComponents[i])
            c->setVisible ((int) i == page);

    garageTab.setToggleState (page == 0, juce::dontSendNotification);
    zooTab.setToggleState (page == 1, juce::dontSendNotification);

    if (hovered != nullptr && ! hovered->isShowing())
        hovered = nullptr;

    canvas.repaint();
}

void ArrowEchAudioProcessorEditor::selectModule (int moduleId)
{
    const int page = moduleInfo (moduleId).page;
    if (page >= 0 && page != currentPage)
        setPage (page);

    flashModule = moduleId;
    flashFrames = 24;
    pokeText = juce::String ("say hi to ") + moduleInfo (moduleId).chipName + ". " + moduleInfo (moduleId).blurb;
    pokeFrames = 60;
    canvas.repaint();
}

//==============================================================================
void ArrowEchAudioProcessorEditor::placeKnob (Knob& k, juce::Rectangle<int> cell, int knobSize)
{
    k.label.setBounds (cell.removeFromTop (16));
    const int w = juce::jmin (cell.getWidth(), knobSize + 30);
    const bool hasTextBox = k.slider.getTextBoxPosition() != juce::Slider::NoTextBox;
    k.slider.setBounds (cell.getCentreX() - w / 2, cell.getY(), w, knobSize + (hasTextBox ? 20 : 0));
}

void ArrowEchAudioProcessorEditor::layoutCanvas()
{
    sections.clear();
    captions.clear();

    //==============================================================================
    // Header
    blob.setBounds (10, 2, 100, 98);
    bubbleBounds = { 124, 14, 420, 74 };

    captions.push_back ({ "ARROWECH  /  PRESETS", { 566, 6, 330, 18 }, -1 });
    prevPreset.setBounds (566, 26, 40, 38);
    presetBox.setBounds (610, 26, 244, 38);
    nextPreset.setBounds (858, 26, 40, 38);
    diceButton.setBounds (566, 68, 164, 34);
    panicButton.setBounds (734, 68, 164, 34);

    placeKnob (inputGain, { 908, 10, 54, 84 }, 50);
    inputMeter.setBounds (962, 26, 10, 58);
    placeKnob (outputGain, { 976, 10, 54, 84 }, 50);
    outputMeter.setBounds (1030, 26, 10, 58);
    placeKnob (rackMix, { 1046, 10, 60, 84 }, 50);

    rackStrip.setBounds (12, 110, 1096, 52);

    //==============================================================================
    // Echo
    {
        const juce::Rectangle<int> b { 16, 172, 1084, 340 };
        sections.push_back ({ "THE ECHO-ER", "says it again. and again. and again.", LAF::sky, b, -1, Proc::modEcho, 0.0f, 24.0f });
        const int x = b.getX(), y = b.getY();

        echoToggle.button.setBounds (x + 262, y + 14, 80, 28);
        tapDisplay.setBounds (x + 18, y + 62, 324, 114);

        echoFreeze.button.setBounds (x + 14, y + 186, 104, 30);
        echoThrowOnly.button.setBounds (x + 124, y + 186, 132, 30);
        throwButton.setBounds (x + 258, y + 182, 88, 36);

        captions.push_back ({ "TEMPO SYNC",  { x + 18,  y + 222, 160, 16 }, -1 });
        captions.push_back ({ "DIVISION",    { x + 186, y + 222, 160, 16 }, -1 });
        captions.push_back ({ "MODE",        { x + 18,  y + 276, 160, 16 }, -1 });
        captions.push_back ({ "PERSONALITY", { x + 186, y + 276, 160, 16 }, -1 });
        echoSync.button.setBounds (x + 18, y + 238, 158, 34);
        echoDivision.box.setBounds (x + 186, y + 238, 158, 36);
        echoMode.box.setBounds (x + 18, y + 292, 160, 36);
        echoStyle.box.setBounds (x + 186, y + 292, 158, 36);

        const int gridX = x + 360, cellW = 90, cellH = 158;
        const std::array<Knob*, 6> row1 { &echoTime, &echoFeedback, &echoMix, &echoOffset, &echoWidth, &echoDuck };
        const std::array<Knob*, 6> row2 { &echoLowCut, &echoHighCut, &echoSaturation, &echoWow, &echoWowRate, &echoDiffusion };
        for (int i = 0; i < 6; ++i)
        {
            placeKnob (*row1[(size_t) i], { gridX + i * cellW, y + 22, cellW, cellH }, 66);
            placeKnob (*row2[(size_t) i], { gridX + i * cellW, y + 182, cellW, cellH }, 66);
        }

        echoReverb.button.setBounds (x + 924, y + 20, 150, 30);
        placeKnob (reverbSize, { x + 924, y + 70, 150, 130 }, 66);
        placeKnob (reverbMix, { x + 924, y + 200, 150, 130 }, 66);
    }

    //==============================================================================
    // Page tabs
    garageTab.setBounds (16, 526, 196, 38);
    zooTab.setBounds (218, 526, 150, 38);
    captions.push_back ({ "psst: drag the chips up top to reorder the rack. click a chip's dot to power it on or off.", { 384, 534, 720, 20 }, -1 });

    const int by = 574, bh = 278;

    //==============================================================================
    // Page 0: glitch garage
    {
        const juce::Rectangle<int> b { 16, by, 330, bh };
        sections.push_back ({ "WOBBLE TWINS", "two slightly wrong clones of you.", LAF::mint, b, 0, Proc::modShift, 0.0f, 24.0f });
        shiftToggle.button.setBounds (b.getX() + 236, by + 14, 80, 28);

        const int buttonW = 98;
        for (int i = 0; i < 3; ++i)
            shiftModeButtons[(size_t) i].setBounds (b.getX() + 16 + i * (buttonW + 4), by + 64, buttonW, 40);

        const std::array<Knob*, 4> knobs { &shiftDetune, &shiftDelay, &shiftFocus, &shiftMix };
        for (int i = 0; i < 4; ++i)
            placeKnob (*knobs[(size_t) i], { b.getX() + 12 + i * 76, by + 128, 76, 120 }, 58);
    }
    {
        const juce::Rectangle<int> b { 356, by, 190, bh };
        sections.push_back ({ "S-S-STUTTER", "b-b-b-bars.", LAF::bubblegum, b, 0, Proc::modStutter, 0.0f, 24.0f });
        stutterToggle.button.setBounds (b.getX() + 16, by + 62, 90, 28);
        captions.push_back ({ "SLICE", { b.getX() + 16, by + 98, 150, 16 }, 0 });
        stutterDivision.box.setBounds (b.getX() + 16, by + 114, 158, 36);
        placeKnob (stutterMix, { b.getX() + 16, by + 160, 158, 112 }, 70);
    }
    {
        const juce::Rectangle<int> b { 556, by, 300, bh };
        sections.push_back ({ "GRAIN SALAD", "chopped. tossed. pitched.", LAF::lilac, b, 0, Proc::modGranular, 0.0f, 24.0f });
        granularToggle.button.setBounds (b.getX() + 206, by + 14, 80, 28);
        captions.push_back ({ "GRAIN SIZE", { b.getX() + 16, by + 62, 200, 16 }, 0 });
        granularDivision.box.setBounds (b.getX() + 16, by + 78, 164, 36);
        granularReverse.button.setBounds (b.getX() + 186, by + 80, 106, 32);

        const std::array<Knob*, 3> knobs { &granularScatter, &granularPitch, &granularMix };
        for (int i = 0; i < 3; ++i)
            placeKnob (*knobs[(size_t) i], { b.getX() + 12 + i * 92, by + 130, 92, 130 }, 70);
    }
    {
        const juce::Rectangle<int> b { 866, by, 234, bh };
        sections.push_back ({ "CRUNCH-O-MATIC", "fewer bits. more attitude.", LAF::tomato, b, 0, Proc::modCrush, 0.0f, 24.0f });
        bitcrusherToggle.button.setBounds (b.getX() + 16, by + 62, 90, 28);
        captions.push_back ({ "RHYTHM", { b.getX() + 16, by + 98, 200, 16 }, 0 });
        bitcrusherDivision.box.setBounds (b.getX() + 16, by + 114, 202, 36);
        placeKnob (bitcrusherBitDepth, { b.getX() + 12, by + 160, 104, 112 }, 68);
        placeKnob (bitcrusherDownsample, { b.getX() + 116, by + 160, 104, 112 }, 68);
    }

    //==============================================================================
    // Page 1: tone zoo. Five equal panels: compact power toggle, a control row, then a 2x2 knob grid.
    {
        struct ZooPanel
        {
            const char* title, * subtitle;
            juce::Colour colour;
            int moduleId;
            Toggle* toggle;
            std::array<Knob*, 4> knobs;
        };

        const std::array<ZooPanel, 5> panels {{
            { "FUZZ BUCKET",   "warm. crunchy. illegal.",     LAF::tangerine, Proc::modFuzz,   &fuzzToggle,   { &fuzzDrive, &fuzzTone, &fuzzMix, &fuzzOutput } },
            { "WAH GOBLIN",    "lives in your filter.",       LAF::lime,      Proc::modFilter, &filterToggle, { &filterCutoff, &filterResonance, &filterLfoDepth, &filterEnvelope } },
            { "SEASICK",       "tremolo. autopan. nausea.",   LAF::peach,     Proc::modMotion, &motionToggle, { &motionDepth, &motionShape, &motionSpread, &motionRandom } },
            { "SWOOSH-A-TRON", "jet plane in a jar.",         LAF::aqua,      Proc::modPhaser, &phaserToggle, { &phaserDepth, &phaserFeedback, &phaserCentre, &phaserMix } },
            { "SQUISH",        "squash. squeeze. pump.",      LAF::orchid,    Proc::modSquish, &squishToggle, { &squishAmount, &squishRelease, &squishPump, &squishMix } },
        }};

        for (int i = 0; i < 5; ++i)
        {
            const auto& panel = panels[(size_t) i];
            const juce::Rectangle<int> b { 16 + i * 218, by, 208, bh };
            const int x = b.getX();
            sections.push_back ({ panel.title, panel.subtitle, panel.colour, b, 1, panel.moduleId, 128.0f, 20.0f });
            panel.toggle->button.setBounds (x + 150, by + 12, 52, 30);

            for (int k = 0; k < 4; ++k)
                placeKnob (*panel.knobs[(size_t) k], { x + 16 + (k % 2) * 88, by + 106 + (k / 2) * 84, 88, 84 }, 48);
        }

        fuzzMode.box.setBounds (16 + 16, by + 62, 112, 36);
        punishButton.setBounds (16 + 132, by + 62, 64, 38);

        filterType.box.setBounds (234 + 16, by + 62, 84, 36);
        filterDivision.box.setBounds (234 + 104, by + 62, 90, 36);

        motionMode.box.setBounds (452 + 16, by + 62, 84, 36);
        motionDivision.box.setBounds (452 + 104, by + 62, 90, 36);

        phaserDivision.box.setBounds (670 + 16, by + 62, 178, 36);
        squishPumpDivision.box.setBounds (888 + 16, by + 62, 178, 36);
    }
}

void ArrowEchAudioProcessorEditor::resized()
{
    canvas.setBounds (0, 0, baseWidth, baseHeight);
    canvas.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));
}

//==============================================================================
void ArrowEchAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (LAF::paper);
}

void ArrowEchAudioProcessorEditor::paintCanvas (juce::Graphics& g)
{
    g.fillAll (LAF::paper);

    // Polka dots, because why not.
    g.setColour (LAF::grey.withAlpha (0.6f));
    for (int y = 10; y < baseHeight; y += 22)
        for (int x = (y / 22) % 2 == 0 ? 10 : 21; x < baseWidth; x += 22)
            g.fillEllipse ((float) x, (float) y, 3.0f, 3.0f);

    for (const auto& s : sections)
        if (s.page < 0 || s.page == currentPage)
            paintSection (g, s);

    g.setColour (LAF::ink);
    g.setFont (LAF::chunky (12.0f));
    for (const auto& c : captions)
        if (c.page < 0 || c.page == currentPage)
            g.drawFittedText (c.text, c.bounds, juce::Justification::centredLeft, 1, 0.8f);

    // Speech bubble with a tail pointing at the blob.
    const auto bubble = bubbleBounds.toFloat();
    juce::Path tail;
    tail.addTriangle (bubble.getX() + 2.0f, bubble.getCentreY() - 10.0f,
                      bubble.getX() + 2.0f, bubble.getCentreY() + 10.0f,
                      bubble.getX() - 18.0f, bubble.getCentreY() + 8.0f);
    g.setColour (LAF::ink);
    g.fillPath (tail, juce::AffineTransform::translation (4.0f, 4.0f));
    LAF::drawSticker (g, bubble, LAF::white, 16.0f, 4.0f, 3.0f);
    g.setColour (LAF::white);
    g.fillPath (tail);
    g.setColour (LAF::ink);
    g.strokePath (tail, juce::PathStrokeType (3.0f));
    g.setColour (LAF::white);
    g.fillRect (bubble.getX() - 0.5f, bubble.getCentreY() - 8.5f, 4.0f, 17.0f);

    g.setColour (LAF::ink);
    g.setFont (LAF::chunky (16.0f));
    g.drawFittedText (bubbleText, bubbleBounds.reduced (16, 8), juce::Justification::centredLeft, 3, 0.85f);
}

void ArrowEchAudioProcessorEditor::paintSection (juce::Graphics& g, const Section& s)
{
    LAF::drawSticker (g, s.bounds.toFloat(), s.colour, 16.0f, 6.0f, 3.0f);

    if (s.moduleId == flashModule && flashFrames > 0)
    {
        g.setColour (LAF::butter.withAlpha (juce::jmin (1.0f, flashFrames / 12.0f)));
        g.drawRoundedRectangle (s.bounds.toFloat().expanded (5.0f), 20.0f, 6.0f);
    }

    auto header = s.bounds.reduced (18, 10);
    auto titleArea = header.removeFromTop (30);
    if (s.titleWidth > 0.0f)
        titleArea.setWidth ((int) s.titleWidth);

    g.setColour (LAF::ink);
    g.setFont (LAF::chunky (s.titleSize));
    g.drawFittedText (s.title, titleArea, juce::Justification::centredLeft, 1, 0.75f);
    g.setColour (LAF::ink.withAlpha (0.65f));
    g.setFont (LAF::chunky (13.0f));
    g.drawFittedText (s.subtitle, header.removeFromTop (16), juce::Justification::centredLeft, 1, 0.8f);
}

//==============================================================================
void ArrowEchAudioProcessorEditor::mouseEnter (const juce::MouseEvent& e)
{
    for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
    {
        if (quips.count (c) > 0)
        {
            hovered = c;
            return;
        }
    }
}

void ArrowEchAudioProcessorEditor::mouseExit (const juce::MouseEvent& e)
{
    for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
    {
        if (c == hovered)
        {
            hovered = nullptr;
            return;
        }
    }
}

juce::String ArrowEchAudioProcessorEditor::idleQuip()
{
    const bool echoOn = value ("echoEnabled") > 0.5f;
    bool anythingOn = false;
    for (int id = 0; id < Proc::numModules; ++id)
        anythingOn = anythingOn || value (Proc::moduleEnableParam (id)) > 0.5f;

    if (echoOn && value ("echoFreeze") > 0.5f)
        return "FROZEN. the echo is looping forever. play something on top. hit FREEZE again (or OH NO) to thaw.";
    if (echoOn && value ("echoFeedback") > 1.0f)
        return "INFINITE FEEDBACK. it's never going to stop. hit OH NO if it gets spicy.";
    if (throwHeld)
        return "THROWIIIIING!";
    if (audioProcessor.outputPeak.load() > 0.98f)
        return "that's LOUD. i'm basically clipping. turn OUT down, hero.";
    if (! anythingOn)
        return "everything is off. i am currently a very expensive cable.";
    if (silentFrames > 150)
        return "it's very quiet in here. play me something?";

    static const juce::StringArray generic {
        "hi. i'm the blob. hover over anything and i'll explain it badly.",
        "nothing here is breakable. except maybe your ears.",
        "try ROLL DICE. fortune favours the reckless.",
        "dub tip: THROW MODE on, then tap THROW on the last snare of a phrase.",
        "dub tip: band-pass goblin AFTER the echo = instant siren. drag the chips!",
        "double-click any knob to reset it. like it never happened.",
        "i am an echo. i am an echo. i am an echo.",
        "put FUZZ after the ECHO-ER in the rack and each repeat gets angrier.",
        "the WOBBLE TWINS on a vocal = expensive-sounding. trust.",
        "SQUISH with pump on 1/4 = your track is now breathing. heavily.",
        "you can drag the corner to make me bigger. i contain multitudes."
    };
    return generic[idleIndex % generic.size()];
}

void ArrowEchAudioProcessorEditor::timerCallback()
{
    const float outLevel = audioProcessor.outputPeak.load();
    inputMeter.setLevel (audioProcessor.inputPeak.load());
    outputMeter.setLevel (outLevel);
    silentFrames = outLevel < 0.0005f ? silentFrames + 1 : 0;

    const bool echoOn = value ("echoEnabled") > 0.5f;
    blob.update (outLevel,
                 echoOn && (value ("echoFeedback") > 1.0f || value ("echoFreeze") > 0.5f),
                 value ("shiftEnabled") > 0.5f);
    tapDisplay.repaint();
    rackStrip.repaint();

    for (auto& d : dimmers)
    {
        const float alpha = d.enabled->load() > 0.5f ? 1.0f : 0.4f;
        for (auto* c : d.components)
            if (std::abs (c->getAlpha() - alpha) > 0.01f)
                c->setAlpha (alpha);
    }

    const bool sync = value ("echoSync") > 0.5f;
    echoTime.slider.setEnabled (! sync);
    echoDivision.box.setEnabled (sync);
    echoOffset.slider.setEnabled ((int) value ("echoMode") != 0);
    throwButton.setToggleState (value ("echoThrow") > 0.5f, juce::dontSendNotification);
    punishButton.setToggleState (value ("fuzzPunish") > 0.5f, juce::dontSendNotification);

    const int shiftMode = (int) value ("shiftMode");
    for (int i = 0; i < 3; ++i)
        shiftModeButtons[(size_t) i].setToggleState (i == shiftMode, juce::dontSendNotification);

    if (presetBox.getSelectedItemIndex() != audioProcessor.getCurrentProgram())
        presetBox.setSelectedItemIndex (audioProcessor.getCurrentProgram(), juce::dontSendNotification);

    if (flashFrames > 0 && --flashFrames % 3 == 0)
        canvas.repaint();

    juce::String newText;
    if (pokeFrames > 0)
    {
        --pokeFrames;
        newText = pokeText;
    }
    else if (hovered != nullptr && hovered->isShowing() && quips.count (hovered) > 0)
    {
        newText = quips[hovered]();
    }
    else
    {
        if (++idleFrames > 210)
        {
            idleFrames = 0;
            ++idleIndex;
        }
        newText = idleQuip();
    }

    if (newText != bubbleText)
    {
        bubbleText = newText;
        canvas.repaint (bubbleBounds.expanded (24));
    }
}

//==============================================================================
juce::Rectangle<float> ArrowEchAudioProcessorEditor::RackStrip::chipBounds (int slot) const
{
    const float gap = 14.0f;
    const float width = ((float) getWidth() - 8.0f - gap * (Proc::numModules - 1)) / Proc::numModules;
    return { 2.0f + slot * (width + gap), 4.0f, width, (float) getHeight() - 12.0f };
}

juce::Rectangle<float> ArrowEchAudioProcessorEditor::RackStrip::ledBounds (int slot) const
{
    const auto chip = chipBounds (slot);
    return juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ chip.getX() + 16.0f, chip.getCentreY() });
}

int ArrowEchAudioProcessorEditor::RackStrip::slotAt (float x) const
{
    for (int slot = 0; slot < Proc::numModules; ++slot)
        if (x < chipBounds (slot).getRight() + 7.0f)
            return slot;
    return Proc::numModules - 1;
}

int ArrowEchAudioProcessorEditor::RackStrip::moduleAt (juce::Point<int> point) const
{
    if (! getLocalBounds().contains (point))
        return -1;
    return processor.getChainOrder()[(size_t) slotAt ((float) point.x)];
}

void ArrowEchAudioProcessorEditor::RackStrip::paint (juce::Graphics& g)
{
    const auto order = processor.getChainOrder();

    for (int slot = 0; slot < Proc::numModules; ++slot)
    {
        const int id = order[(size_t) slot];
        const auto& info = moduleInfo (id);
        const bool on = processor.apvts.getRawParameterValue (Proc::moduleEnableParam (id))->load() > 0.5f;
        auto chip = chipBounds (slot);
        if (slot == dragSlot && dragged)
            chip = chip.translated (0.0f, -3.0f);

        LAF::drawSticker (g, chip, on ? info.colour : LAF::grey.brighter (0.1f), 10.0f, slot == dragSlot ? 5.0f : 3.0f, 2.5f);

        const auto led = ledBounds (slot).translated (0.0f, chip.getY() - chipBounds (slot).getY());
        g.setColour (on ? LAF::butter : LAF::white);
        g.fillEllipse (led);
        g.setColour (LAF::ink);
        g.drawEllipse (led, 2.0f);
        if (on)
            g.fillEllipse (led.reduced (4.5f));

        g.setColour (LAF::ink.withAlpha (on ? 1.0f : 0.5f));
        g.setFont (LAF::chunky (13.0f));
        g.drawFittedText (info.chipName, chip.withTrimmedLeft (28.0f).withTrimmedRight (4.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.7f);

        // Arrow to the next slot
        if (slot < Proc::numModules - 1)
        {
            const float ax = chipBounds (slot).getRight() + 4.0f;
            const float ay = chipBounds (slot).getCentreY();
            juce::Path arrow;
            arrow.addTriangle (ax, ay - 5.0f, ax, ay + 5.0f, ax + 7.0f, ay);
            g.setColour (LAF::ink);
            g.fillPath (arrow);
        }
    }
}

void ArrowEchAudioProcessorEditor::RackStrip::mouseDown (const juce::MouseEvent& e)
{
    dragSlot = slotAt ((float) e.x);
    dragged = false;
    downOnLed = ledBounds (dragSlot).expanded (6.0f).contains (e.position);
}

void ArrowEchAudioProcessorEditor::RackStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragSlot < 0 || e.getDistanceFromDragStart() < 6)
        return;

    dragged = true;
    const int target = slotAt ((float) e.x);
    if (target != dragSlot)
    {
        auto order = processor.getChainOrder();
        if (target > dragSlot)
            std::rotate (order.begin() + dragSlot, order.begin() + dragSlot + 1, order.begin() + target + 1);
        else
            std::rotate (order.begin() + target, order.begin() + dragSlot, order.begin() + dragSlot + 1);
        processor.setChainOrder (order);
        dragSlot = target;
    }
    repaint();
}

void ArrowEchAudioProcessorEditor::RackStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragSlot >= 0 && ! dragged)
    {
        const int id = processor.getChainOrder()[(size_t) dragSlot];
        if (downOnLed)
        {
            if (auto* param = processor.apvts.getParameter (Proc::moduleEnableParam (id)))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
                param->endChangeGesture();
            }
        }
        else if (onSelect)
        {
            onSelect (id);
        }
    }

    dragSlot = -1;
    dragged = false;
    repaint();
}

//==============================================================================
void ArrowEchAudioProcessorEditor::TapDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (LAF::ink);
    g.fillRoundedRectangle (bounds.translated (4.0f, 4.0f), 12.0f);
    g.setColour (LAF::ink.brighter (0.08f));
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (LAF::ink);
    g.drawRoundedRectangle (bounds.reduced (1.0f), 12.0f, 3.0f);

    auto inner = bounds.reduced (14.0f, 8.0f);
    auto v = [this] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };

    const double bpm = processor.displayBpm.load();
    const bool sync = v ("echoSync") > 0.5f;
    const int division = juce::jlimit (0, Proc::echoDivisionNames.size() - 1, (int) v ("echoDivision"));
    const int mode = juce::jlimit (0, 2, (int) v ("echoMode"));
    const int style = juce::jlimit (0, Proc::echoStyleNames.size() - 1, (int) v ("echoStyle"));
    const double baseMs = sync ? Proc::getEchoDivisionBeats (division) * 60000.0 / bpm : (double) v ("echoTimeMs");
    const double offset = mode == 0 ? 0.0 : v ("echoOffset") / 100.0 * 0.5;
    const double dL = baseMs * (1.0 - offset), dR = baseMs * (1.0 + offset);
    const bool frozen = v ("echoFreeze") > 0.5f;
    const float feedback = frozen ? 1.0f : v ("echoFeedback");

    static const juce::Colour styleColours[] = { LAF::sky, LAF::butter, LAF::tomato, LAF::mint, LAF::lilac, LAF::tangerine };
    const auto tapColour = frozen ? LAF::aqua : styleColours[style];

    auto top = inner.removeFromTop (18.0f);
    g.setFont (LAF::chunky (14.0f));
    g.setColour (LAF::paper);
    g.drawText ((sync ? Proc::echoDivisionNames[division] + "  =  " : juce::String())
                    + juce::String (juce::roundToInt (baseMs)) + " ms" + (frozen ? "   *FROZEN*" : ""),
                top, juce::Justification::centredLeft);
    g.setColour (LAF::paper.withAlpha (0.55f));
    g.drawText (juce::String (bpm, 1) + " BPM", top, juce::Justification::centredRight);

    auto bottom = inner.removeFromBottom (14.0f);
    g.setFont (LAF::chunky (11.0f));
    g.drawText (Proc::echoModeNames[mode].toUpperCase() + "  /  " + Proc::echoStyleNames[style].toUpperCase(),
                bottom, juce::Justification::centredRight);

    const auto plot = inner.reduced (0.0f, 3.0f).withTrimmedLeft (14.0f);
    const float midY = plot.getCentreY();
    const double windowMs = juce::jlimit (250.0, 20000.0, juce::jmax (dL, dR) * 7.5);
    auto xFor = [&] (double ms) { return plot.getX() + (float) (ms / windowMs) * plot.getWidth(); };

    g.setColour (LAF::paper.withAlpha (0.5f));
    g.drawText ("L", juce::Rectangle<float> (inner.getX(), plot.getY(), 12.0f, plot.getHeight() * 0.5f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (inner.getX(), midY, 12.0f, plot.getHeight() * 0.5f), juce::Justification::centred);

    const double beatMs = 60000.0 / bpm;
    if (windowMs / beatMs <= 48.0)
    {
        g.setColour (LAF::paper.withAlpha (0.1f));
        for (double t = beatMs; t < windowMs; t += beatMs)
            g.drawVerticalLine ((int) xFor (t), plot.getY(), plot.getBottom());
    }
    g.setColour (LAF::paper.withAlpha (0.2f));
    g.drawHorizontalLine ((int) midY, plot.getX(), plot.getRight());

    g.setColour (LAF::paper);
    g.fillRoundedRectangle (xFor (0.0) - 2.5f, plot.getY(), 5.0f, plot.getHeight(), 2.5f);

    auto drawTap = [&] (double ms, float amplitude, bool upper)
    {
        const float a = juce::jmin (1.0f, amplitude);
        const float h = plot.getHeight() * 0.5f * a;
        const float x = xFor (ms);
        g.setColour (tapColour.withAlpha (0.35f + 0.65f * a));
        g.fillRoundedRectangle (x - 3.0f, upper ? midY - h : midY, 6.0f, h, 3.0f);
    };

    const int maxTaps = 64;
    if (mode == 2)
    {
        double t = 0.0;
        for (int n = 1; n <= maxTaps; ++n)
        {
            t += (n % 2 == 1) ? dL : dR;
            const float amp = std::pow (feedback, (float) (n - 1));
            if (amp < 0.02f || t > windowMs) break;
            drawTap (t, amp, n % 2 == 1);
        }
    }
    else
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const double d = (mode == 0 || ch == 0) ? dL : dR;
            for (int n = 1; n <= maxTaps; ++n)
            {
                const float amp = std::pow (feedback, (float) (n - 1));
                if (amp < 0.02f || n * d > windowMs) break;
                drawTap (n * d, amp, ch == 0);
            }
        }
    }
}

void ArrowEchAudioProcessorEditor::LevelMeter::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (LAF::white);
    g.fillRoundedRectangle (r, 4.0f);

    const float shown = std::sqrt (juce::jlimit (0.0f, 1.0f, level));
    g.setColour (level > 0.95f ? LAF::tomato : LAF::mint);
    g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() * (1.0f - shown)), 4.0f);

    g.setColour (LAF::ink);
    g.drawRoundedRectangle (r, 4.0f, 2.0f);
}

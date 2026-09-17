#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/ArrowLookAndFeel.h"
#include "UI/Blob.h"

//==============================================================================
class ArrowEchAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit ArrowEchAudioProcessorEditor (ArrowEchAudioProcessor&);
    ~ArrowEchAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using Quip = std::function<juce::String()>;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Selector
    {
        juce::ComboBox box;
        std::unique_ptr<ComboAttachment> attachment;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    // Everything is laid out at a fixed base size on this canvas, which is then scaled.
    class Canvas : public juce::Component
    {
    public:
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    class TapDisplay : public juce::Component
    {
    public:
        explicit TapDisplay (ArrowEchAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;
    private:
        ArrowEchAudioProcessor& processor;
    };

    class LevelMeter : public juce::Component
    {
    public:
        void setLevel (float newLevel) { level = juce::jmax (newLevel, level * 0.88f); repaint(); }
        void paint (juce::Graphics&) override;
    private:
        float level = 0.0f;
    };

    // The signal chain as draggable chips: drag to reorder, click the dot to power, click the name to jump.
    class RackStrip : public juce::Component
    {
    public:
        explicit RackStrip (ArrowEchAudioProcessor& p) : processor (p) {}
        std::function<void (int moduleId)> onSelect;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        int moduleAt (juce::Point<int>) const;

    private:
        juce::Rectangle<float> chipBounds (int slot) const;
        juce::Rectangle<float> ledBounds (int slot) const;
        int slotAt (float x) const;

        ArrowEchAudioProcessor& processor;
        int dragSlot = -1;
        bool dragged = false, downOnLed = false;
    };

    struct Section
    {
        juce::String title, subtitle;
        juce::Colour colour;
        juce::Rectangle<int> bounds;
        int page;          // -1 = always visible
        int moduleId;
        float titleWidth;  // 0 = full width
        float titleSize;
    };

    struct Caption
    {
        juce::String text;
        juce::Rectangle<int> bounds;
        int page;
    };

    struct SectionDimmer
    {
        std::atomic<float>* enabled;
        std::vector<juce::Component*> components;
    };

    //==============================================================================
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, juce::Colour, Quip, int page = -1);
    void setupSelector (Selector&, const juce::String& paramId, const juce::StringArray& items, juce::Colour, Quip, int page = -1);
    void setupToggle (Toggle&, const juce::String& paramId, const juce::String& name, Quip, int page = -1);
    void addToPage (juce::Component&, int page);
    void addQuip (juce::Component&, Quip);
    void setParam (const char* paramId, float plainValue);

    void layoutCanvas();
    void placeKnob (Knob&, juce::Rectangle<int> cell, int knobSize);
    void setPage (int page);
    void selectModule (int moduleId);
    void paintCanvas (juce::Graphics&);
    void paintSection (juce::Graphics&, const Section&);
    void timerCallback() override;

    juce::String idleQuip();
    float value (const char* paramId) const;
    juce::String text (const char* paramId) const;

    //==============================================================================
    ArrowEchAudioProcessor& audioProcessor;
    ArrowLookAndFeel lookAndFeel;
    Canvas canvas;

    // Header
    Blob blob;
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, diceButton { "ROLL DICE" }, panicButton { "OH NO" };
    Knob inputGain, outputGain, rackMix;
    LevelMeter inputMeter, outputMeter;
    RackStrip rackStrip;
    juce::TextButton garageTab { "GLITCH GARAGE" }, zooTab { "TONE ZOO" };

    // Echo
    Toggle echoToggle, echoSync, echoReverb, echoFreeze, echoThrowOnly;
    juce::TextButton throwButton { "THROW!" };
    Selector echoDivision, echoMode, echoStyle;
    Knob echoTime, echoFeedback, echoMix, echoOffset, echoWidth, echoDuck;
    Knob echoLowCut, echoHighCut, echoSaturation, echoWow, echoWowRate, echoDiffusion;
    Knob reverbSize, reverbMix;
    TapDisplay tapDisplay;

    // Glitch garage
    Toggle shiftToggle;
    std::array<juce::TextButton, 3> shiftModeButtons;
    Knob shiftDetune, shiftDelay, shiftFocus, shiftMix;

    Toggle stutterToggle;
    Selector stutterDivision;
    Knob stutterMix;

    Toggle granularToggle, granularReverse;
    Selector granularDivision;
    Knob granularScatter, granularPitch, granularMix;

    Toggle bitcrusherToggle;
    Selector bitcrusherDivision;
    Knob bitcrusherBitDepth, bitcrusherDownsample;

    // Tone zoo
    Toggle fuzzToggle;
    Selector fuzzMode;
    juce::TextButton punishButton { "PUNISH" };
    Knob fuzzDrive, fuzzTone, fuzzMix, fuzzOutput;

    Toggle filterToggle;
    Selector filterType, filterDivision;
    Knob filterCutoff, filterResonance, filterLfoDepth, filterEnvelope;

    Toggle motionToggle;
    Selector motionMode, motionDivision;
    Knob motionDepth, motionShape, motionSpread, motionRandom;

    Toggle phaserToggle;
    Selector phaserDivision;
    Knob phaserDepth, phaserFeedback, phaserCentre, phaserMix;

    Toggle squishToggle;
    Selector squishPumpDivision;
    Knob squishAmount, squishRelease, squishPump, squishMix;

    //==============================================================================
    std::vector<Section> sections;
    std::vector<Caption> captions;
    std::vector<SectionDimmer> dimmers;
    std::map<juce::Component*, Quip> quips;
    std::array<std::vector<juce::Component*>, 2> pageComponents;

    juce::Component* hovered = nullptr;
    juce::String bubbleText, pokeText;
    int pokeFrames = 0, pokeCount = 0, idleFrames = 0, idleIndex = 0, silentFrames = 0;
    int currentPage = 0, flashModule = -1, flashFrames = 0;
    bool throwHeld = false;

    juce::Rectangle<int> bubbleBounds;

    static constexpr int baseWidth = 1120;
    static constexpr int baseHeight = 866;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrowEchAudioProcessorEditor)
};

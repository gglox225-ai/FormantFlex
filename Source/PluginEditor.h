#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ffcolours
{
    const juce::Colour background  { 0xff12141c };
    const juce::Colour panel       { 0xff1b1e29 };
    const juce::Colour panelEdge   { 0xff2a2f3f };
    const juce::Colour text        { 0xffe8eaf2 };
    const juce::Colour textDim     { 0xff8a90a6 };
    const juce::Colour accent      { 0xffff8a3d };   // formant (warm orange)
    const juce::Colour accent2     { 0xff4fd1c5 };   // secondary knobs (teal)
    const juce::Colour track       { 0xff2c3142 };
}

class FlexLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlexLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOverButton, bool isButtonDown) override;
};

class EnvelopeDisplay : public juce::Component
{
public:
    explicit EnvelopeDisplay (FormantFlexAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void refresh();

private:
    float xForFrequency (float hz, juce::Rectangle<float> area) const;
    FormantFlexAudioProcessor& processor;
    std::array<float, FormantFlexAudioProcessor::numDisplayPoints> original {}, shifted {};
    float topDb = -20.0f;
    bool hasSignal = false;
};

class FormantFlexAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FormantFlexAudioProcessorEditor (FormantFlexAudioProcessor&);
    ~FormantFlexAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Preset { const char* name; float semitones; };
    static constexpr int numPresets = 7;
    static const Preset presets[numPresets];

    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name, juce::Colour colour, bool bipolar);
    static juce::String describeFormant (float semitones);

    FormantFlexAudioProcessor& processor;
    FlexLookAndFeel lnf;
    EnvelopeDisplay display;

    juce::Slider formantKnob, detailKnob, mixKnob, outputKnob;
    juce::Label formantLabel, detailLabel, mixLabel, outputLabel, characterLabel, hintLabel;
    std::array<juce::TextButton, numPresets> presetButtons;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> formantAtt, detailAtt, mixAtt, outputAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FormantFlexAudioProcessorEditor)
};

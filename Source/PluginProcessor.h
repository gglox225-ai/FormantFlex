#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FormantCore.h"
#include <array>
#include <atomic>

class FormantFlexAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numDisplayPoints = 160;
    static constexpr float displayMinHz = 50.0f;
    static constexpr float displayMaxHz = 16000.0f;
    static float displayFrequency (int i)
    {
        return displayMinHz * std::pow (displayMaxHz / displayMinHz, (float) i / (float) (numDisplayPoints - 1));
    }

    FormantFlexAudioProcessor();
    ~FormantFlexAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setParameterFromUI (const juce::String& id, float plainValue);

    juce::AudioProcessorValueTreeState apvts;

    // Lock-free data for the editor's envelope display (dB)
    std::array<std::atomic<float>, numDisplayPoints> displayOriginal, displayShifted;
    std::atomic<int> latencySamples { 0 };
    std::atomic<double> currentSampleRate { 44100.0 };

private:
    void process (juce::AudioBuffer<float>&, bool bypassed);
    void updateDisplay();

    std::array<formantflex::FormantShifter, 2> shifters;
    juce::SmoothedValue<float> mixSmoothed, gainSmoothed;
    std::atomic<float>* formantParam = nullptr;
    std::atomic<float>* detailParam  = nullptr;
    std::atomic<float>* mixParam     = nullptr;
    std::atomic<float>* outputParam  = nullptr;
    unsigned int lastDisplayFrame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FormantFlexAudioProcessor)
};

#include "PluginProcessor.h"
#include "PluginEditor.h"

FormantFlexAudioProcessor::FormantFlexAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FormantFlexState", createParameterLayout())
{
    formantParam = apvts.getRawParameterValue ("formant");
    detailParam  = apvts.getRawParameterValue ("detail");
    mixParam     = apvts.getRawParameterValue ("mix");
    outputParam  = apvts.getRawParameterValue ("output");

    for (auto& v : displayOriginal) v.store (-120.0f);
    for (auto& v : displayShifted)  v.store (-120.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout FormantFlexAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    auto semitoneText = [] (float v, int)
    {
        if (std::abs (v) < 0.005f) return String ("0.00 st");
        return (v > 0.0f ? "+" : "") + String (v, 2) + " st";
    };
    auto semitoneParse = [] (const String& t) { return t.retainCharacters ("+-.0123456789").getFloatValue(); };
    auto percentText = [] (float v, int) { return String (juce::roundToInt (v)) + " %"; };
    auto dbText = [] (float v, int)
    {
        if (std::abs (v) < 0.05f) return String ("0.0 dB");
        return (v > 0.0f ? "+" : "") + String (v, 1) + " dB";
    };

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "formant", 1 }, "Formant",
        NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("st")
            .withStringFromValueFunction (semitoneText)
            .withValueFromStringFunction (semitoneParse)));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "detail", 1 }, "Detail",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (percentText)));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (percentText)));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

    return { params.begin(), params.end() };
}

void FormantFlexAudioProcessor::setParameterFromUI (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void FormantFlexAudioProcessor::prepareToPlay (double sampleRate, int)
{
    const int fftSize = formantflex::FormantShifter::chooseFftSize (sampleRate);
    for (auto& s : shifters)
    {
        s.prepare (sampleRate, fftSize);
        s.setFormantSemitones (formantParam->load());
        s.setDetail (detailParam->load() / 100.0f);
        s.reset();
    }

    latencySamples.store (shifters[0].getLatency());
    currentSampleRate.store (sampleRate);
    setLatencySamples (shifters[0].getLatency());

    mixSmoothed.reset (sampleRate, 0.05);
    gainSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() / 100.0f);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
}

bool FormantFlexAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void FormantFlexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process (buffer, false);
}

// Keep the latency constant while bypassed so FL Studio's delay compensation stays aligned.
void FormantFlexAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process (buffer, true);
}

void FormantFlexAudioProcessor::process (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numCh = juce::jmin (totalIn, (int) shifters.size(), buffer.getNumChannels());
    const float semis = formantParam->load();
    const float detail = detailParam->load() / 100.0f;
    for (auto& s : shifters) { s.setFormantSemitones (semis); s.setDetail (detail); }

    mixSmoothed.setTargetValue (bypassed ? 0.0f : mixParam->load() / 100.0f);
    gainSmoothed.setTargetValue (bypassed ? 1.0f : juce::Decibels::decibelsToGain (outputParam->load()));

    float* data[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numCh; ++ch) data[ch] = buffer.getWritePointer (ch);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix  = mixSmoothed.getNextValue();
        const float gain = gainSmoothed.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float dry = 0.0f;
            const float wet = shifters[(size_t) ch].processSample (data[ch][i], dry);
            data[ch][i] = (dry + mix * (wet - dry)) * gain;
        }
    }

    updateDisplay();
}

void FormantFlexAudioProcessor::updateDisplay()
{
    const auto& s = shifters[0];
    if (s.getFrameCounter() == lastDisplayFrame)
        return;
    lastDisplayFrame = s.getFrameCounter();

    const auto& orig = s.getDisplayOriginalDb();
    const auto& shifted = s.getDisplayShiftedDb();
    const int maxBin = (int) orig.size() - 2;
    if (maxBin < 1) return;
    const float binsPerHz = (float) s.getFftSize() / (float) s.getSampleRate();

    for (int i = 0; i < numDisplayPoints; ++i)
    {
        const float bin = juce::jlimit (0.0f, (float) maxBin, displayFrequency (i) * binsPerHz);
        const int b0 = (int) bin;
        const float fr = bin - (float) b0;
        displayOriginal[(size_t) i].store (orig[(size_t) b0] + fr * (orig[(size_t) b0 + 1] - orig[(size_t) b0]));
        displayShifted[(size_t) i].store (shifted[(size_t) b0] + fr * (shifted[(size_t) b0 + 1] - shifted[(size_t) b0]));
    }
}

juce::AudioProcessorEditor* FormantFlexAudioProcessor::createEditor()
{
    return new FormantFlexAudioProcessorEditor (*this);
}

void FormantFlexAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FormantFlexAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FormantFlexAudioProcessor();
}

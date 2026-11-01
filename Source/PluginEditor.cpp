#include "PluginEditor.h"

//==============================================================================
FlexLookAndFeel::FlexLookAndFeel()
{
    using namespace ffcolours;
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, accent);
}

void FlexLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace ffcolours;
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float lineW = juce::jmax (3.0f, radius * 0.10f);
    const float arcR = radius - lineW * 0.5f;
    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path bg;
    bg.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (track);
    g.strokePath (bg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    // bipolar knobs draw their value arc from the "centre" value (e.g. 0 st / 0 dB)
    const double centreValue = (double) slider.getProperties().getWithDefault ("centre", 0.0);
    const float centreAngle = startAngle + (float) slider.valueToProportionOfLength (centreValue) * (endAngle - startAngle);
    const float from = bipolar ? centreAngle : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (cx, cy, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (fill.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (lineW * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (fill);
        g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyR = arcR - lineW * 1.4f;
    juce::ColourGradient grad (juce::Colour (0xff353b4f), cx, cy - bodyR, juce::Colour (0xff1c2030), cx, cy + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (panelEdge);
    g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    const juce::Point<float> inner (cx + bodyR * 0.35f * std::sin (angle), cy - bodyR * 0.35f * std::cos (angle));
    const juce::Point<float> outer (cx + bodyR * 0.85f * std::sin (angle), cy - bodyR * 0.85f * std::cos (angle));
    g.setColour (fill);
    g.drawLine ({ inner, outer }, juce::jmax (2.0f, lineW * 0.6f));

    if (bipolar) // centre tick
    {
        const float a = centreAngle;
        g.setColour (textDim);
        g.drawLine (cx + (radius + 1.0f) * std::sin (a), cy - (radius + 1.0f) * std::cos (a),
                    cx + (radius + 6.0f) * std::sin (a), cy - (radius + 6.0f) * std::cos (a), 1.5f);
    }
}

void FlexLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool over, bool down)
{
    using namespace ffcolours;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    juce::Colour c = on ? accent : panel;
    if (! on && over) c = c.brighter (0.15f);
    if (down) c = c.darker (0.2f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? accent : panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

//==============================================================================
EnvelopeDisplay::EnvelopeDisplay (FormantFlexAudioProcessor& p) : processor (p)
{
    original.fill (-120.0f);
    shifted.fill (-120.0f);
    setInterceptsMouseClicks (false, false);
}

void EnvelopeDisplay::refresh()
{
    float peak = -200.0f;
    for (int i = 0; i < FormantFlexAudioProcessor::numDisplayPoints; ++i)
    {
        const float o = processor.displayOriginal[(size_t) i].load();
        const float s = processor.displayShifted[(size_t) i].load();
        original[(size_t) i] += 0.45f * (o - original[(size_t) i]);
        shifted[(size_t) i]  += 0.45f * (s - shifted[(size_t) i]);
        peak = juce::jmax (peak, original[(size_t) i], shifted[(size_t) i]);
    }
    hasSignal = peak > -110.0f;
    if (hasSignal)
    {
        const float target = std::ceil ((peak + 6.0f) / 6.0f) * 6.0f;
        topDb += (target > topDb ? 0.5f : 0.03f) * (target - topDb);
    }
    repaint();
}

float EnvelopeDisplay::xForFrequency (float hz, juce::Rectangle<float> area) const
{
    const float t = std::log (hz / FormantFlexAudioProcessor::displayMinHz)
                  / std::log (FormantFlexAudioProcessor::displayMaxHz / FormantFlexAudioProcessor::displayMinHz);
    return area.getX() + t * area.getWidth();
}

void EnvelopeDisplay::paint (juce::Graphics& g)
{
    using namespace ffcolours;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    auto plot = bounds.reduced (12.0f, 10.0f).withTrimmedBottom (14.0f);
    const float rangeDb = 66.0f;
    auto yFor = [&] (float db) { return plot.getY() + juce::jlimit (0.0f, 1.0f, (topDb - db) / rangeDb) * plot.getHeight(); };

    g.setFont (juce::FontOptions (11.0f));
    const float gridHz[] = { 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* gridTxt[] = { "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 7; ++i)
    {
        const float x = xForFrequency (gridHz[i], plot);
        g.setColour (panelEdge);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (textDim);
        g.drawText (gridTxt[i], juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 1.0f, 40.0f, 13.0f), juce::Justification::centred);
    }

    if (! hasSignal)
    {
        g.setColour (textDim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Play some vocals to see the formant envelope", plot, juce::Justification::centred);
        return;
    }

    auto makePath = [&] (const std::array<float, FormantFlexAudioProcessor::numDisplayPoints>& d, bool closed)
    {
        juce::Path p;
        for (int i = 0; i < FormantFlexAudioProcessor::numDisplayPoints; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (FormantFlexAudioProcessor::numDisplayPoints - 1);
            const float y = yFor (d[(size_t) i]);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (closed)
        {
            p.lineTo (plot.getRight(), plot.getBottom());
            p.lineTo (plot.getX(), plot.getBottom());
            p.closeSubPath();
        }
        return p;
    };

    g.setColour (textDim.withAlpha (0.18f));
    g.fillPath (makePath (original, true));
    g.setColour (textDim.withAlpha (0.8f));
    g.strokePath (makePath (original, false), juce::PathStrokeType (1.5f));

    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.35f), 0.0f, plot.getY(), accent.withAlpha (0.0f), 0.0f, plot.getBottom(), false));
    g.fillPath (makePath (shifted, true));
    g.setColour (accent);
    g.strokePath (makePath (shifted, false), juce::PathStrokeType (2.2f));

    // legend
    g.setFont (juce::FontOptions (11.0f));
    auto legend = juce::Rectangle<float> (plot.getRight() - 170.0f, plot.getY() + 2.0f, 170.0f, 14.0f);
    g.setColour (textDim);
    g.fillRect (legend.getX(), legend.getCentreY() - 1.0f, 14.0f, 2.0f);
    g.drawText ("original", legend.withTrimmedLeft (18.0f), juce::Justification::centredLeft);
    g.setColour (accent);
    g.fillRect (legend.getX() + 80.0f, legend.getCentreY() - 1.0f, 14.0f, 2.0f);
    g.drawText ("shifted", legend.withTrimmedLeft (98.0f), juce::Justification::centredLeft);
}

//==============================================================================
const FormantFlexAudioProcessorEditor::Preset FormantFlexAudioProcessorEditor::presets[numPresets] = {
    { "Giant",   -9.0f }, { "Deep",   -5.0f }, { "Warm",  -2.0f }, { "Natural", 0.0f },
    { "Bright",   2.0f }, { "Light",   5.0f }, { "Tiny",   9.0f }
};

FormantFlexAudioProcessorEditor::FormantFlexAudioProcessorEditor (FormantFlexAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), display (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (display);

    setupKnob (formantKnob, formantLabel, "FORMANT", ffcolours::accent, true);
    setupKnob (detailKnob, detailLabel, "DETAIL", ffcolours::accent2, false);
    setupKnob (mixKnob, mixLabel, "MIX", ffcolours::accent2, false);
    setupKnob (outputKnob, outputLabel, "OUTPUT", ffcolours::accent2, true);

    formantKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 120, 26);
    formantKnob.setMouseDragSensitivity (400);

    formantAtt = std::make_unique<Attachment> (p.apvts, "formant", formantKnob);
    detailAtt  = std::make_unique<Attachment> (p.apvts, "detail", detailKnob);
    mixAtt     = std::make_unique<Attachment> (p.apvts, "mix", mixKnob);
    outputAtt  = std::make_unique<Attachment> (p.apvts, "output", outputKnob);

    formantKnob.setDoubleClickReturnValue (true, 0.0);
    detailKnob.setDoubleClickReturnValue (true, 50.0);
    mixKnob.setDoubleClickReturnValue (true, 100.0);
    outputKnob.setDoubleClickReturnValue (true, 0.0);

    characterLabel.setJustificationType (juce::Justification::centred);
    characterLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    characterLabel.setColour (juce::Label::textColourId, ffcolours::accent);
    addAndMakeVisible (characterLabel);

    hintLabel.setJustificationType (juce::Justification::centred);
    hintLabel.setFont (juce::FontOptions (11.0f));
    hintLabel.setColour (juce::Label::textColourId, ffcolours::textDim);
    hintLabel.setText ("Drag / mouse wheel  |  Ctrl+drag = fine  |  Double-click = reset  |  Click value to type",
                       juce::dontSendNotification);
    addAndMakeVisible (hintLabel);

    for (int i = 0; i < numPresets; ++i)
    {
        auto& b = presetButtons[(size_t) i];
        const float semis = presets[i].semitones;
        juce::String txt (presets[i].name);
        b.setButtonText (txt + "  " + (semis > 0 ? "+" : "") + juce::String ((int) semis));
        b.setTooltip ("Set formant to " + juce::String (semis, 0) + " semitones");
        b.onClick = [this, semis] { processor.setParameterFromUI ("formant", semis); };
        addAndMakeVisible (b);
    }

    setResizable (true, true);
    setResizeLimits (560, 400, 1200, 860);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (660.0 / 470.0);
    setSize (660, 470);
    startTimerHz (30);
    timerCallback();
}

FormantFlexAudioProcessorEditor::~FormantFlexAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FormantFlexAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name,
                                                 juce::Colour colour, bool bipolar)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    s.setColour (juce::Slider::rotarySliderFillColourId, colour);
    s.getProperties().set ("bipolar", bipolar);
    s.setPopupDisplayEnabled (false, false, this);
    s.setScrollWheelEnabled (true);
    s.setVelocityModeParameters (0.5, 1, 0.05, true, juce::ModifierKeys::ctrlAltCommandModifiers);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, ffcolours::textDim);
    addAndMakeVisible (l);
}

juce::String FormantFlexAudioProcessorEditor::describeFormant (float s)
{
    if (s <= -7.0f)  return "Giant / monster";
    if (s <= -3.5f)  return "Deeper, more masculine";
    if (s <= -0.25f) return "Slightly deeper";
    if (s <   0.25f) return "Natural";
    if (s <   3.5f)  return "Slightly brighter";
    if (s <   7.0f)  return "Brighter, more feminine";
    return "Tiny / cartoon";
}

void FormantFlexAudioProcessorEditor::timerCallback()
{
    display.refresh();
    const float semis = (float) formantKnob.getValue();
    characterLabel.setText (describeFormant (semis), juce::dontSendNotification);
    for (int i = 0; i < numPresets; ++i)
        presetButtons[(size_t) i].setToggleState (std::abs (semis - presets[i].semitones) < 0.005f, juce::dontSendNotification);
}

void FormantFlexAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace ffcolours;
    g.fillAll (background);
    const float k = (float) getWidth() / 660.0f;

    g.setColour (text);
    g.setFont (juce::FontOptions (22.0f * k, juce::Font::bold));
    g.drawText ("FORMANT", juce::Rectangle<float> (20 * k, 10 * k, 120 * k, 30 * k), juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText ("FLEX", juce::Rectangle<float> (128 * k, 10 * k, 80 * k, 30 * k), juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f * k));
    const double sr = processor.currentSampleRate.load();
    const int ms = juce::roundToInt (1000.0 * processor.latencySamples.load() / (sr > 0 ? sr : 44100.0));
    g.drawText ("voice formant shifter  |  pitch stays  |  latency " + juce::String (ms) + " ms, auto-compensated",
                juce::Rectangle<float> (210 * k, 10 * k, 430 * k, 30 * k), juce::Justification::centredRight);

    // knob panels
    auto panelArea = [&] (juce::Rectangle<int> r)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    };
    panelArea (juce::Rectangle<int> ((int) (20 * k), (int) (210 * k), (int) (300 * k), (int) (190 * k)));
    panelArea (juce::Rectangle<int> ((int) (330 * k), (int) (210 * k), (int) (310 * k), (int) (190 * k)));
}

void FormantFlexAudioProcessorEditor::resized()
{
    const float k = (float) getWidth() / 660.0f;
    auto R = [k] (float x, float y, float w, float h)
    { return juce::Rectangle<int> ((int) (x * k), (int) (y * k), (int) (w * k), (int) (h * k)); };

    display.setBounds (R (20, 46, 620, 154));

    formantLabel.setBounds (R (20, 214, 300, 18));
    formantKnob.setBounds (R (80, 232, 180, 146));
    formantKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, (int) (120 * k), (int) (24 * k));
    characterLabel.setBounds (R (20, 378, 300, 20));

    const float kx[] = { 340, 442, 544 };
    juce::Slider* knobs[] = { &detailKnob, &mixKnob, &outputKnob };
    juce::Label* labels[] = { &detailLabel, &mixLabel, &outputLabel };
    for (int i = 0; i < 3; ++i)
    {
        labels[i]->setBounds (R (kx[i], 214, 90, 18));
        knobs[i]->setBounds (R (kx[i] - 2, 244, 94, 130));
        knobs[i]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, (int) (80 * k), (int) (20 * k));
    }

    // one-click formant presets across the full width
    const float bw = 624.0f / numPresets;
    for (int i = 0; i < numPresets; ++i)
        presetButtons[(size_t) i].setBounds (R (18 + bw * (float) i, 408, bw - 4, 32));

    hintLabel.setBounds (R (20, 446, 620, 18));
    hintLabel.setFont (juce::FontOptions (11.0f * k));
}

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

//==============================================================================
class ChoirLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChoirLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;

    static juce::Colour accent()     { return juce::Colour (0xffe9b45f); }
    static juce::Colour accentCool() { return juce::Colour (0xff6fb7c9); }
};

//==============================================================================
class ChoirAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChoirAudioProcessorEditor (ChoirAudioProcessor&);
    ~ChoirAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    Knob* addKnob (const juce::String& paramId, const juce::String& text);

    ChoirAudioProcessor& processor;
    ChoirLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ComboBox chordBox;
    juce::Label    chordLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chordAttachment;

    // layout rows: 0 = choir, 1 = realism, 2 = space & output
    std::vector<std::vector<juce::Component*>> rows;
    std::vector<std::vector<juce::Label*>>     rowLabels;
    juce::StringArray rowTitles { "THE CHOIR", "HUMAN FEEL", "SPACE & OUTPUT" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirAudioProcessorEditor)
};

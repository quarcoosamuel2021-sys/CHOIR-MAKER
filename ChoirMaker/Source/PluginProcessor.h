#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "ChoirEngine.h"

class ChoirAudioProcessor : public juce::AudioProcessor
{
public:
    ChoirAudioProcessor();
    ~ChoirAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ChoirMaker"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void updateParameters();
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    choir::Engine engine;
    juce::dsp::Reverb reverb;
    juce::dsp::StateVariableTPTFilter<float> lowCut, highCut;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 16 };

    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> dryGain, wetGain, outGain;

    double sampleRate = 44100.0;
    int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirAudioProcessor)
};

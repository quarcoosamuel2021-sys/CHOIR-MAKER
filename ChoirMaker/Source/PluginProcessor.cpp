#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String vowelText (float v)
{
    static const char* names = "AEIOU";
    v = juce::jlimit (0.0f, 4.0f, v);
    const int   i = juce::jmin ((int) v, 3);
    const float t = v - (float) i;
    if (t < 0.08f) return juce::String::charToString (names[i]);
    if (t > 0.92f) return juce::String::charToString (names[i + 1]);
    return juce::String::charToString (names[i]) + juce::String::fromUTF8 (" \xe2\x86\x92 ")
         + juce::String::charToString (names[i + 1]);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ChoirAudioProcessor::createLayout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;

    AudioProcessorValueTreeState::ParameterLayout l;

    StringArray chordNames;
    for (int i = 0; i < choir::kNumChords; ++i) chordNames.add (choir::kChords[i].name);

    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "voices", 1 }, "Voices", 1, choir::kMaxVoices, 8));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "chord", 1 },  "Harmony", chordNames, 0));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "vowel", 1 },  "Vowel",
                                                   NormalisableRange<float> (0.0f, 4.0f, 0.01f), 0.0f,
                                                   Attr().withStringFromValueFunction ([] (float v, int) { return vowelText (v); })));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "formant", 1 }, "Formant",
                                                   NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f,
                                                   Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));

    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "detune", 1 }, "Detune",
                                                   NormalisableRange<float> (0.0f, 50.0f, 0.1f), 15.0f, Attr().withLabel ("ct")));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "timing", 1 }, "Timing",
                                                   NormalisableRange<float> (0.0f, 60.0f, 0.1f), 25.0f, Attr().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "vibdepth", 1 }, "Vib Depth",
                                                   NormalisableRange<float> (0.0f, 50.0f, 0.1f), 12.0f, Attr().withLabel ("ct")));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "vibrate", 1 }, "Vib Rate",
                                                   NormalisableRange<float> (2.0f, 8.0f, 0.01f), 5.2f, Attr().withLabel ("Hz")));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "spread", 1 }, "Width",
                                                   NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f,
                                                   Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));

    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "rvsize", 1 }, "Room",
                                                   NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f,
                                                   Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "rvmix", 1 }, "Reverb",
                                                   NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f,
                                                   Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "tone", 1 }, "Air",
                                                   NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f,
                                                   Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mix", 1 }, "Mix",
                                                   NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f,
                                                   Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "output", 1 }, "Output",
                                                   NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, Attr().withLabel ("dB")));
    return l;
}

ChoirAudioProcessor::ChoirAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool ChoirAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    // mono or stereo in; mono vocal in -> stereo choir out is the intended use
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void ChoirAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    maxBlock   = juce::jmax (16, samplesPerBlock);

    engine.prepare (sr);
    setLatencySamples (engine.latencySamples());

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };

    dryDelay.setMaximumDelayInSamples (juce::jmax (4096, engine.latencySamples() + 16));
    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) engine.latencySamples());
    dryDelay.reset();

    reverb.prepare (spec);
    reverb.reset();

    lowCut.prepare (spec);
    lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lowCut.setCutoffFrequency (110.0f);
    lowCut.reset();

    highCut.prepare (spec);
    highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highCut.reset();

    wet.setSize (2, maxBlock);
    wet.clear();

    const double rampSeconds = 0.03;
    dryGain.reset (sr, rampSeconds);
    wetGain.reset (sr, rampSeconds);
    outGain.reset (sr, rampSeconds);

    updateParameters();
    dryGain.setCurrentAndTargetValue (dryGain.getTargetValue());
    wetGain.setCurrentAndTargetValue (wetGain.getTargetValue());
    outGain.setCurrentAndTargetValue (outGain.getTargetValue());
}

void ChoirAudioProcessor::updateParameters()
{
    choir::Params p;
    p.numVoices   = (int) param ("voices");
    p.chord       = (int) param ("chord");
    p.vowel       = param ("vowel");
    p.formantAmt  = param ("formant");
    p.detuneCents = param ("detune");
    p.timingMs    = param ("timing");
    p.vibDepth    = param ("vibdepth");
    p.vibRate     = param ("vibrate");
    p.spread      = param ("spread");
    engine.setParams (p);

    juce::dsp::Reverb::Parameters rp;
    const float rvMix = param ("rvmix");
    rp.roomSize   = 0.35f + 0.6f * param ("rvsize");
    rp.damping    = 0.45f;
    rp.width      = 1.0f;
    rp.wetLevel   = 0.4f * rvMix;
    rp.dryLevel   = 0.5f;              // juce's reverb scales dry by 2 -> unity
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    const float tone   = param ("tone");
    const float cutoff = juce::jmin (2000.0f * std::pow (9.0f, tone), 0.45f * (float) sampleRate);
    highCut.setCutoffFrequency (cutoff);

    const float mix = param ("mix");
    dryGain.setTargetValue (std::cos (mix * juce::MathConstants<float>::halfPi));
    wetGain.setTargetValue (std::sin (mix * juce::MathConstants<float>::halfPi) * 1.4142f);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (param ("output")));
}

void ChoirAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn    = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    for (int ch = totalIn; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    updateParameters();

    float* chL = buffer.getWritePointer (0);
    float* chR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool stereoIn = totalIn > 1 && chR != nullptr;

    int pos = 0;
    while (pos < numSamples)
    {
        const int n = juce::jmin (maxBlock, numSamples - pos);
        float* wl = wet.getWritePointer (0);
        float* wr = wet.getWritePointer (1);

        // 1. the choir (mono sum of the input feeds the voices)
        for (int i = 0; i < n; ++i)
        {
            const float inL = chL[pos + i];
            const float inR = stereoIn ? chR[pos + i] : inL;
            engine.process (0.5f * (inL + inR), wl[i], wr[i]);
        }

        // 2. tone shaping and space on the choir only
        for (int i = 0; i < n; ++i)
        {
            wl[i] = highCut.processSample (0, lowCut.processSample (0, wl[i]));
            wr[i] = highCut.processSample (1, lowCut.processSample (1, wr[i]));
        }
        {
            juce::dsp::AudioBlock<float> block (wet);
            auto sub = block.getSubBlock (0, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (sub);
            reverb.process (ctx);
        }

        // 3. mix with the (latency-aligned) dry signal
        for (int i = 0; i < n; ++i)
        {
            const float inL = chL[pos + i];
            const float inR = stereoIn ? chR[pos + i] : inL;

            dryDelay.pushSample (0, inL);
            dryDelay.pushSample (1, inR);
            const float dL = dryDelay.popSample (0);
            const float dR = dryDelay.popSample (1);

            const float dg = dryGain.getNextValue();
            const float wg = wetGain.getNextValue();
            const float og = outGain.getNextValue();

            const float oL = (dL * dg + wl[i] * wg) * og;
            const float oR = (dR * dg + wr[i] * wg) * og;

            if (chR != nullptr)
            {
                chL[pos + i] = oL;
                chR[pos + i] = oR;
            }
            else
            {
                chL[pos + i] = 0.5f * (oL + oR);
            }
        }

        pos += n;
    }
}

juce::AudioProcessorEditor* ChoirAudioProcessor::createEditor()
{
    return new ChoirAudioProcessorEditor (*this);
}

void ChoirAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChoirAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChoirAudioProcessor();
}

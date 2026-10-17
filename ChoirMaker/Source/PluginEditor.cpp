#include "PluginEditor.h"

//==============================================================================
ChoirLookAndFeel::ChoirLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffd8dce6));
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               juce::Colour (0xff8f97a8));
    setColour (juce::ComboBox::textColourId,            juce::Colour (0xffe8ebf2));
    setColour (juce::PopupMenu::backgroundColourId,     juce::Colour (0xff171b26));
    setColour (juce::PopupMenu::textColourId,           juce::Colour (0xffd8dce6));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a3142));
}

juce::Font ChoirLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (15.0f));
}

juce::Font ChoirLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (12.5f));
}

void ChoirLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float pos, float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const float trackR = radius - 3.0f;

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff262c3b));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (bipolar-looking from the start)
    const auto col = s.isEnabled() ? accent() : juce::Colour (0xff4a5163);
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, startAngle, angle, true);
    g.setColour (col.withAlpha (0.22f));
    g.strokePath (value, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    const float bodyR = trackR - 9.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4258), centre.x, centre.y - bodyR,
                                             juce::Colour (0xff171b26), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff4b5470));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // pointer
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.8f, -bodyR + 3.0f, 3.6f, bodyR * 0.42f, 1.8f);
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void ChoirLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (juce::Colour (0xff171b26));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accent() : juce::Colour (0xff343c52));
    g.drawRoundedRectangle (r, 7.0f, 1.2f);

    juce::Path arrow;
    const float cx = (float) width - 18.0f, cy = (float) height * 0.5f;
    arrow.addTriangle (cx - 5.0f, cy - 2.5f, cx + 5.0f, cy - 2.5f, cx, cy + 3.5f);
    g.setColour (accent());
    g.fillPath (arrow);
}

//==============================================================================
ChoirAudioProcessorEditor::ChoirAudioProcessorEditor (ChoirAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    // --- row 0: the choir
    chordBox.addItemList ([] { juce::StringArray a; for (int i = 0; i < choir::kNumChords; ++i) a.add (choir::kChords[i].name); return a; }(), 1);
    addAndMakeVisible (chordBox);
    chordAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "chord", chordBox);
    chordLabel.setText ("Harmony", juce::dontSendNotification);
    chordLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (chordLabel);

    auto* voices  = addKnob ("voices",  "Voices");
    auto* vowel   = addKnob ("vowel",   "Vowel");
    auto* formant = addKnob ("formant", "Formant");
    voices->slider.setNumDecimalPlacesToDisplay (0);

    // --- row 1: human feel
    auto* detune   = addKnob ("detune",   "Detune");
    auto* timing   = addKnob ("timing",   "Timing");
    auto* vibDepth = addKnob ("vibdepth", "Vib Depth");
    auto* vibRate  = addKnob ("vibrate",  "Vib Rate");
    auto* width    = addKnob ("spread",   "Width");

    // --- row 2: space & output
    auto* room   = addKnob ("rvsize", "Room");
    auto* reverb = addKnob ("rvmix",  "Reverb");
    auto* air    = addKnob ("tone",   "Air");
    auto* mix    = addKnob ("mix",    "Mix");
    auto* output = addKnob ("output", "Output");

    rows       = { { &chordBox, &voices->slider, &vowel->slider, &formant->slider },
                   { &detune->slider, &timing->slider, &vibDepth->slider, &vibRate->slider, &width->slider },
                   { &room->slider, &reverb->slider, &air->slider, &mix->slider, &output->slider } };
    rowLabels  = { { &chordLabel, &voices->label, &vowel->label, &formant->label },
                   { &detune->label, &timing->label, &vibDepth->label, &vibRate->label, &width->label },
                   { &room->label, &reverb->label, &air->label, &mix->label, &output->label } };

    setSize (760, 560);
}

ChoirAudioProcessorEditor::~ChoirAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

ChoirAudioProcessorEditor::Knob* ChoirAudioProcessorEditor::addKnob (const juce::String& id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 18);
    k->slider.setPopupDisplayEnabled (false, false, this);
    addAndMakeVisible (k->slider);

    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k->label);

    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, k->slider);

    knobs.push_back (std::move (k));
    return knobs.back().get();
}

void ChoirAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141826), 0.0f, 0.0f,
                                             juce::Colour (0xff0a0c13), 0.0f, b.getBottom(), false));
    g.fillAll();

    // soft glow behind the title
    g.setGradientFill (juce::ColourGradient (ChoirLookAndFeel::accent().withAlpha (0.10f), b.getCentreX(), 0.0f,
                                             juce::Colours::transparentBlack, b.getCentreX(), 150.0f, true));
    g.fillRect (b.withHeight (160.0f));

    g.setColour (juce::Colour (0xfff2f3f7));
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("CHOIRMAKER", 32, 20, 400, 36, juce::Justification::centredLeft);
    g.setColour (ChoirLookAndFeel::accent());
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("one voice in, a whole choir out", 34, 54, 400, 20, juce::Justification::centredLeft);

    // row titles
    const int top = 96, rowH = 150;
    for (int r = 0; r < 3; ++r)
    {
        const int y = top + r * rowH;
        g.setColour (juce::Colour (0xff8f97a8));
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (rowTitles[r], 32, y, 200, 16, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff232a3b));
        g.drawLine (32.0f, (float) y + 20.0f, (float) getWidth() - 32.0f, (float) y + 20.0f, 1.0f);
    }
}

void ChoirAudioProcessorEditor::resized()
{
    const int top = 96, rowH = 150, margin = 32;
    const int usable = getWidth() - 2 * margin;

    for (size_t r = 0; r < rows.size(); ++r)
    {
        const int n = (int) rows[r].size();
        const int cellW = usable / n;
        const int y = top + (int) r * rowH + 26;

        for (int i = 0; i < n; ++i)
        {
            const int cx = margin + i * cellW + cellW / 2;
            auto* comp  = rows[r][(size_t) i];
            auto* label = rowLabels[r][(size_t) i];

            if (comp == &chordBox)
            {
                chordBox.setBounds (cx - 80, y + 34, 160, 30);
                label->setBounds (cx - 60, y + 6, 120, 18);
            }
            else
            {
                comp->setBounds (cx - 48, y, 96, 100);
                label->setBounds (cx - 48, y + 100, 96, 18);
            }
        }
    }
}

#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class UncertainVoiceEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit UncertainVoiceEditor (UncertainVoiceProcessor&);
    ~UncertainVoiceEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    void drawTrace (juce::Graphics&, juce::Rectangle<float>);

    UncertainVoiceProcessor& proc;

    juce::Slider force { juce::Slider::RotaryHorizontalVerticalDrag,
                         juce::Slider::NoTextBox };
    juce::Label title, keyLabel, keySub, forceLabel;

    // palette Uncertain
    const juce::Colour cBg    { 0xff0A0B10 };
    const juce::Colour cPanel { 0xff131521 };
    const juce::Colour cLine  { 0xff232637 };
    const juce::Colour cText  { 0xffEDEEF4 };
    const juce::Colour cDim   { 0xff8A8FA3 };
    const juce::Colour cRaw   { 0xffFFB454 };
    const juce::Colour cCta   { 0xffB98CFF };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UncertainVoiceEditor)
};

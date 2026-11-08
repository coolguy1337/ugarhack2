#pragma once
#include "PluginProcessor.h"

// Look & feel approximating the Live "dark" device skin used by Pitch Hack.amxd
class PitchHackLook : public juce::LookAndFeel_V4
{
public:
    PitchHackLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
    static juce::Colour accent() { return juce::Colour (0xfff5d428); }   // 0.96 0.83 0.16 from the .amxd
    static juce::Colour panel()  { return juce::Colour (0xff282828); }   // obj-49 panel colour
};

class PitchHackEditor : public juce::AudioProcessorEditor
{
public:
    explicit PitchHackEditor (PitchHackProcessor&);
    ~PitchHackEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider s; juce::Label l;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    void addKnob (Knob&, const juce::String& id, const juce::String& name, juce::Slider::SliderStyle);

    PitchHackProcessor& proc;
    PitchHackLook look;
    Knob wheel, cents, variation, rate, level, reverse, recycle, drywet;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchHackEditor)
};

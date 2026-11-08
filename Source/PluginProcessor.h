#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchHackDSP.h"

class PitchHackProcessor : public juce::AudioProcessor
{
public:
    PitchHackProcessor();
    ~PitchHackProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pitch Hack"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray& rateNames();

private:
    pitchhack::Engine engine;
    std::atomic<float> *pInterval, *pCents, *pVariation, *pRate, *pReverse, *pRecycle, *pLevel, *pDryWet, *pRestart;
    bool wasPlaying = false, restartWasOn = false;
    std::vector<float> scratchL, scratchR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchHackProcessor)
};

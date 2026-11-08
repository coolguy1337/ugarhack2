#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using API = juce::AudioParameterInt;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;

const juce::StringArray& PitchHackProcessor::rateNames()
{
    static const juce::StringArray n { "1/128", "1/64", "1/32T", "1/64D", "1/32", "1/16T", "1/32D", "1/16",
                                       "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2T", "1/4D", "1/2",
                                       "1/1T", "1/2D", "1/1", "1/1D", "2/1", "3/1", "4/1" };
    return n;
}

juce::AudioProcessorValueTreeState::ParameterLayout PitchHackProcessor::createLayout()
{
    using juce::ParameterID;
    auto suffix = [] (const char* s) { return juce::AudioParameterIntAttributes().withLabel (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // Same names / ranges / defaults as the live.* objects in Pitch Hack.amxd
    p.push_back (std::make_unique<API> (ParameterID { "interval",  1 }, "Interval",  -36, 36, 0,  suffix (" st")));
    p.push_back (std::make_unique<API> (ParameterID { "cents",     1 }, "Cents",      -99, 99, 0,  suffix (" ct")));
    p.push_back (std::make_unique<API> (ParameterID { "variation", 1 }, "Variation",    0, 24, 0,  suffix (" st")));
    p.push_back (std::make_unique<APC> (ParameterID { "rate",      1 }, "Rate", rateNames(), 13));   // default 1/4
    p.push_back (std::make_unique<API> (ParameterID { "reverse",   1 }, "Reverse",      0, 100, 0, suffix (" %")));
    p.push_back (std::make_unique<API> (ParameterID { "recycle",   1 }, "Recycle",      0, 95,  0, suffix (" %")));
    p.push_back (std::make_unique<APF> (ParameterID { "level",     1 }, "Level",
                                        juce::NormalisableRange<float> (-96.f, 0.f, 0.01f), 0.f,
                                        juce::AudioParameterFloatAttributes().withLabel (" dB")));
    p.push_back (std::make_unique<API> (ParameterID { "drywet",    1 }, "Dry / Wet",    0, 100, 0, suffix (" %")));
    p.push_back (std::make_unique<APB> (ParameterID { "restart",   1 }, "Restart", false));          // live.button "Restart"
    return { p.begin(), p.end() };
}

PitchHackProcessor::PitchHackProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PitchHack", createLayout())
{
    pInterval  = apvts.getRawParameterValue ("interval");
    pCents     = apvts.getRawParameterValue ("cents");
    pVariation = apvts.getRawParameterValue ("variation");
    pRate      = apvts.getRawParameterValue ("rate");
    pReverse   = apvts.getRawParameterValue ("reverse");
    pRecycle   = apvts.getRawParameterValue ("recycle");
    pLevel     = apvts.getRawParameterValue ("level");
    pDryWet    = apvts.getRawParameterValue ("drywet");
    pRestart   = apvts.getRawParameterValue ("restart");
}

bool PitchHackProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PitchHackProcessor::prepareToPlay (double sr, int)
{
    engine.prepare (sr);         // also queues the "load" restart (live.thisdevice -> ---init_all)
    wasPlaying = false;
    restartWasOn = false;
}

void PitchHackProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    if (buf.getNumChannels() < 2) return;

    // host tempo + transport  (M4L.api.ObserveTransport -> sel 1 -> Restart -> ---init_all)
    double bpm = 120.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
        }
    if (playing && ! wasPlaying) engine.restart();
    wasPlaying = playing;

    const bool restartOn = pRestart->load() > 0.5f;
    if (restartOn && ! restartWasOn) engine.restart();
    restartWasOn = restartOn;

    pitchhack::Engine::Params p;
    p.interval  = (int) std::lround (pInterval->load());
    p.cents     = (int) std::lround (pCents->load());
    p.variation = (int) std::lround (pVariation->load());
    p.rateIndex = (int) std::lround (pRate->load());
    p.reverse   = (int) std::lround (pReverse->load());
    p.recycle   = (int) std::lround (pRecycle->load());
    p.levelDb   = pLevel->load();
    p.dryWet    = (int) std::lround (pDryWet->load());

    engine.process (buf.getWritePointer (0), buf.getWritePointer (1), n, p, bpm);
}

void PitchHackProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void PitchHackProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PitchHackProcessor::createEditor() { return new PitchHackEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchHackProcessor(); }

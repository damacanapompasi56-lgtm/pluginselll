#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class MadenciProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    MadenciProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MADENCITUNE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    mt::Engine engine;
    std::atomic<float> inPeak { 0.f }, outPeak { 0.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void timerCallback() override;   // Key/Scale degisince 12 nota anahtarini doldurur

    std::atomic<float> *pRef, *pSpeed, *pGlide, *pTolC, *pTolT, *pVibAmt, *pVibOn, *pSmart, *pRange, *pKey, *pScale;
    std::atomic<float>* pNote[12];
    int lastKey = 0, lastScale = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadenciProcessor)
};

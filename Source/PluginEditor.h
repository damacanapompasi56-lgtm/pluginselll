#pragma once
#include "PluginProcessor.h"
#include "KazmaKnob.h"

// PARÇA 2A: elmas kazma knob'ları (speed, note transition, tolerance cents/time). Diğer kontroller sonraki parçalarda.
class MadenciEditor : public juce::AudioProcessorEditor
{
public:
    explicit MadenciEditor (MadenciProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MadenciProcessor& proc;
    juce::Image pickaxe, minerWide, minerTall;
    KazmaKnob speedKnob, transKnob, centsKnob, timeKnob;
};

#pragma once
#include "PluginProcessor.h"
#include "KazmaKnob.h"
#include "VibRefControls.h"

// PARÇA 2A/2B: kazma knob'ları, vibrato, reference. Diğer kontroller sonraki parçalarda.
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
    vr::VibratoControl vibrato;
    vr::RefControl refCtl;
};

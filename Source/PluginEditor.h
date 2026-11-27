#pragma once
#include "PluginProcessor.h"

// PARÇA 1/6: sadece iskelet (yer tutucular, etiketler, alt yazılar). Kontrol yok.
class MadenciEditor : public juce::AudioProcessorEditor
{
public:
    explicit MadenciEditor (MadenciProcessor&);
    void paint (juce::Graphics&) override;

private:
    MadenciProcessor& proc;
    juce::Image pickaxe, minerWide, minerTall;
};

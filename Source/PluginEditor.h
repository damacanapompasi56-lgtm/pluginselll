#pragma once
#include "PluginProcessor.h"

class MadenciEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MadenciEditor (MadenciProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob { juce::Slider s { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow }; juce::Label l; std::unique_ptr<SA> a; };

    struct Piano : juce::Component
    {
        explicit Piano (MadenciProcessor& p);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        juce::Rectangle<float> keyRect (int pc) const;
        MadenciProcessor& proc;
        juce::RangedAudioParameter* np[12];
        int target = -1;   // hedef perde sinifi, -1 yok
    };

    void timerCallback() override;
    void addKnob (Knob&, const char* id, const char* text);
    void addCombo (juce::ComboBox&, std::unique_ptr<CA>&, const char* id);
    void setChoice (const char* id, int index);

    MadenciProcessor& proc;
    juce::TextButton detectBtn { "Key Detect" };
    juce::Label keyLabel, noteLabel;
    juce::ComboBox keyBox, scaleBox, rangeBox, formantBox;
    juce::Label formantLabel;
    juce::ToggleButton vibBtn { "Vibrato" };
    Knob kRef, kSpeed, kGlide, kVib, kTolC, kTolT;
    Piano piano;
    float inLevel = 0.f, outLevel = 0.f;
    std::unique_ptr<CA> keyAtt, scaleAtt, rangeAtt, formantAtt;
    std::unique_ptr<BA> vibAtt;
};

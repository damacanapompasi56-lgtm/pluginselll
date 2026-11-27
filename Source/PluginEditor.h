#pragma once
#include "PluginProcessor.h"
#include "KazmaKnob.h"
#include "VibRefControls.h"
#include "ScaleControls.h"
#include "RangeControls.h"
#include "PitchMeters.h"
#include "CctvView.h"
#include "Tooltips.h"
#include <vector>

// PARÇA 2A/2B/3A/3B/4A/4B/5: kazma knob'ları, vibrato, reference, scale, piyano, key detect, formant, range, nota göstergesi, metreler, CCTV fotoğraf, tooltip'ler.
class MadenciEditor : public juce::AudioProcessorEditor
{
public:
    explicit MadenciEditor (MadenciProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MadenciProcessor& proc;
    juce::Image pickaxe, minerTall;
    tt::Tip<KazmaKnob> speedKnob, transKnob, centsKnob, timeKnob;
    tt::Tip<vr::VibratoControl> vibrato;
    tt::Tip<vr::RefControl> refCtl;
    tt::Tip<sc::ChoiceBox> scaleBox, formantBox;
    tt::Tip<sc::KeyPicker> keyPick;
    tt::Tip<sc::Piano> piano;
    tt::Tip<sc::KeyDetectView> keyView;
    tt::Tip<rc::RangeControl> rangeCtl;
    tt::Tip<pm::PitchDisplay> pitchDisp;
    tt::Tip<pm::LevelMeter> inMeter, outMeter;
    cctv::CctvView cctvView;

    struct Zone { Theme::R r; std::unique_ptr<tt::Tip<juce::Component>> c; };   // etiket alanları için görünmez tooltip bölgeleri
    std::vector<Zone> zones;
    tt::TipLaf tipLaf;
    juce::TooltipWindow tip { this, Theme::kTipDelayMs };   // en sonda
};

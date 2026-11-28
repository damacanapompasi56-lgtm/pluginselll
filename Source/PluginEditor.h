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

// Formant: tıklayınca doğrudan diğer seçeneğe geçen tek düğme (açılır menü yok); yazı düğme boyuna göre ölçeklenir.
struct FormantButton : juce::Button
{
    FormantButton() : juce::Button ("formant") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// PARÇA 2A/2B/3A/3B/4A/4B/5/6A: kazma knob'ları, vibrato, reference, scale, piyano, key detect, formant, range, nota göstergesi, metreler, CCTV fotoğraf, tooltip'ler, negatif speed (hell) modu.
class MadenciEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MadenciEditor (MadenciProcessor&);
    ~MadenciEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void onSpeed (float v);        // speed < 0: hell modu (sallanma, sonra palet geçişi)
    void timerCallback() override; // sadece geçiş sürerken çalışır
    void applyPalette();
    void applyShake();
    MadenciProcessor& proc;
    juce::Image pickaxe, minerTall;
    tt::Tip<KazmaKnob> speedKnob, transKnob, centsKnob, timeKnob;
    tt::Tip<vr::VibratoControl> vibrato;
    tt::Tip<vr::RefControl> refCtl;
    tt::Tip<sc::ChoiceBox> scaleBox;
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
    juce::TooltipWindow tip { this, Theme::kTipDelayMs };

    bool hellTarget = false, animate = false;
    float hell = 0.f, shakeMs = 0.f;        // hell: 0..1 doğrusal ilerleme (paleti yumuşatılmış hali uygulanır)
    double lastTick = 0.0;
    juce::Point<int> shake;
    juce::Random rnd;
    FormantButton formantBtn;               // toggle açık = formant 1 (not corrected)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> formantAtt;   // butondan sonra tanımlı: önce o yok olur
    juce::ParameterAttachment speedAtt;     // en sonda
};

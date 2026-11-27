#pragma once
// PARÇA 3B: range kutusu. "range" parametresi hazır ses türleri (bass..generic) seçer; her birinin alt/üst sınırı
// mt::kRanges'ten (Params.h) okunur ve kutuda Hz olarak gösterilir. DSP'ye dokunulmaz.
#include "ScaleControls.h"
#include "Params.h"

namespace rc
{
class RangeControl : public juce::Component
{
public:
    explicit RangeControl (sc::APVTS& apvts)
        : choice (apvts, "range", Theme::kRangeNames, 7),
          att (*apvts.getParameter ("range"), [this] (float v) { index = juce::jlimit (0, 6, (int) std::lround (v)); repaint(); })
    {
        addAndMakeVisible (choice);
        att.sendInitialUpdate();
    }

    // box/choiceR/infoR: Theme::L kutuları (tasarım uzayı), s: pencere ölçeği
    void place (juce::Rectangle<float> box, juce::Rectangle<float> choiceR, juce::Rectangle<float> infoR, float s)
    {
        scale = s;
        setBounds ((box * s).toNearestInt());
        choice.fit (choiceR.translated (-box.getX(), -box.getY()), s);   // çocuk yerel koordinatta
        info = (infoR.translated (-box.getX(), -box.getY()) * s);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::kTeal.withAlpha (0.6f));
        g.drawRect (getLocalBounds().toFloat().reduced (scale), 1.5f * scale);
        const auto& r = mt::kRanges[index];
        g.setColour (Theme::kLight);
        g.setFont (Theme::font (info.getHeight() * 0.6f));
        g.drawText (juce::String ((int) r.lo) + " - " + juce::String ((int) r.hi) + " " + Theme::str (Theme::kHz),
                    info, juce::Justification::centred);
    }

private:
    sc::ChoiceBox choice;
    int index = mt::kGenericRange;
    float scale = 1.f;
    juce::Rectangle<float> info;
    juce::ParameterAttachment att;   // en sonda
};
} // namespace rc

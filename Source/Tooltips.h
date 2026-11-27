#pragma once
// PARÇA 5: tooltip'ler. Metinler Theme.h'da (kTip*). Her kontrol Tip<> ile sarılır; çocuk bileşenlerin (slider, etiket)
// üstündeyken de TooltipWindow ebeveynin tooltip'ini bulur. Görünüm: Theme font + palet.
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace tt
{
// Herhangi bir bileşen türünü tooltip'li yapar; ilk argüman Theme::kTip* metni, kalanlar T'nin kurucusuna gider.
template <class T>
struct Tip : T, juce::SettableTooltipClient
{
    template <class... A>
    explicit Tip (const char* text, A&&... a) : T (std::forward<A> (a)...) { setTooltip (Theme::str (text)); }
};

struct TipLaf : juce::LookAndFeel_V4
{
    static constexpr int kPad = 10;

    static juce::TextLayout layout (const juce::String& t)
    {
        juce::AttributedString as;
        as.setJustification (juce::Justification::centredLeft);
        as.append (t, Theme::font (Theme::kTipFont), Theme::kLight);
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (as, (float) (Theme::kTipMaxW - 2 * kPad));
        return tl;
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& t, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const auto tl = layout (t);
        return juce::Rectangle<int> (pos.x + 16, pos.y + 18, (int) std::ceil (tl.getWidth()) + 2 * kPad, (int) std::ceil (tl.getHeight()) + 2 * kPad)
                   .constrainedWithin (parent);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h) override
    {
        const auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (1.f);
        g.setColour (Theme::kDark.darker (0.3f));
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (Theme::kTeal);
        g.drawRoundedRectangle (r, 8.f, 1.5f);
        layout (t).draw (g, r.reduced ((float) kPad - 1.f));
    }
};
} // namespace tt

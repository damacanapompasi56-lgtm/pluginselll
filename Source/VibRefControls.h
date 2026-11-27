#pragma once
// PARÇA 2B: vibrato (on/off + kazma kollu kaydırıcı + değer kutusu) ve reference (- değer +).
// Aralıklar parametreden gelir (SliderAttachment). KazmaKnob.h'a dokunulmadı.
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace vr
{
using APVTS = juce::AudioProcessorValueTreeState;

// Kazmayı cache'li ölçekleyip rot kadar döndürerek c merkezine çizer (boyut değişmedikçe yeniden ölçekleme yok)
inline void drawPick (juce::Graphics& g, const juce::Image& src, juce::Image& cache, float side, juce::Point<float> c, float rot)
{
    if (src.isNull()) return;
    const float phys = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int   px   = juce::jmax (1, juce::roundToInt (side * phys));
    const auto  q    = Theme::kPixelStyle ? juce::Graphics::lowResamplingQuality : juce::Graphics::highResamplingQuality;
    if (cache.getWidth() != px) cache = src.rescaled (px, px, q);
    g.setImageResamplingQuality (q);
    g.drawImageTransformed (cache, juce::AffineTransform::translation ((float) -px * 0.5f, (float) -px * 0.5f)
                                       .scaled (1.f / phys).rotated (rot).translated (c.x, c.y));
}

// Slider'ın değerini gösteren kutu; çift tık = elle giriş. Aralık dışı/geçersiz girişte gerçek değer geri yazılır.
class ValueBox : public juce::Label
{
public:
    explicit ValueBox (juce::Slider& s) : slider (s)
    {
        setJustificationType (juce::Justification::centred);
        setEditable (false, true, false);
        setColour (textColourId, Theme::kLight);
        setColour (textWhenEditingColourId, Theme::kLight);
        setColour (backgroundWhenEditingColourId, Theme::kDark.darker (0.4f));
        setColour (outlineWhenEditingColourId, Theme::kTeal);
        onTextChange = [this] { commit(); };
    }

    void fit (juce::Rectangle<int> r, float s) { scale = s; setBounds (r); setFont (Theme::font ((float) r.getHeight() * 0.6f)); }

    void refresh()
    {
        const double v = slider.getValue();
        setText (std::abs (v - std::round (v)) < 0.05 ? juce::String ((int) std::round (v)) : juce::String (v, 1), juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        if (framed)
        {
            const auto r = getLocalBounds().toFloat();
            g.setColour (Theme::kDark.darker (0.3f));
            g.fillRoundedRectangle (r, 8.f * scale);
            g.setColour (Theme::kMagenta);
            g.drawRoundedRectangle (r.reduced (scale), 8.f * scale, 1.5f * scale);
        }
        Label::paint (g);
    }

    bool framed = true;

private:
    void commit()
    {
        const auto t = getText().trim();
        if (t.containsAnyOf ("0123456789"))
            slider.setValue (t.retainCharacters ("0123456789.,-").replaceCharacter (',', '.').getDoubleValue(), juce::sendNotificationSync);
        refresh();
    }

    juce::Slider& slider;
    float scale = 1.f;
};

class VibToggle : public juce::Button
{
public:
    VibToggle() : juce::Button ("vib") { setClickingTogglesState (true); }
    float scale = 1.f;

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        const auto r = getLocalBounds().toFloat().reduced (scale);
        const bool on = getToggleState();
        if (on) { g.setColour (Theme::kMagenta.withAlpha (0.35f)); g.fillRoundedRectangle (r, 8.f * scale); }
        g.setColour (Theme::kMagenta);
        g.drawRoundedRectangle (r, 8.f * scale, 1.5f * scale);
        g.setColour (on ? Theme::kLight : Theme::kLight.withAlpha (0.5f));
        g.setFont (Theme::font (r.getHeight() * 0.55f));
        g.drawText (Theme::str (on ? Theme::kOn : Theme::kOff), r, juce::Justification::centred);
    }
};

// Yatay kaydırıcı: çizgi + kol olarak elmas kazma (başı yukarı). Fare mantığı Slider'ın kendisi.
class VibSlider : public juce::Slider
{
public:
    VibSlider() : pickaxe (Theme::image (Theme::kPickaxeRes))
    {
        setSliderStyle (LinearHorizontal);
        setTextBoxStyle (NoTextBox, false, 0, 0);
    }
    float scale = 1.f;

    void paint (juce::Graphics& g) override
    {
        const float cy = (float) getHeight() * 0.5f;
        const float x0 = (float) getPositionOfValue (getMinimum()), x1 = (float) getPositionOfValue (getMaximum());
        const float x  = (float) getPositionOfValue (getValue());
        g.setColour (Theme::kLight.withAlpha (0.8f));
        g.drawLine (x0, cy, x1, cy, 2.f * scale);
        g.setColour (Theme::kMagenta);
        g.drawLine (x0, cy, x, cy, 2.f * scale);
        // shortcut: kol 0.6*yükseklik; uçlarda birkaç px kırpılabilir (şeffaf köşe), gerekirse LookAndFeel thumb yarıçapı ayarla.
        drawPick (g, pickaxe, cache, (float) getHeight() * 0.6f, { x, cy }, -Theme::kPickaxeTipAngle);
    }

private:
    juce::Image pickaxe, cache;
};

class VibratoControl : public juce::Component
{
public:
    explicit VibratoControl (APVTS& apvts) : box (slider)
    {
        slider.onValueChange = [this] { box.refresh(); };
        addAndMakeVisible (toggle); addAndMakeVisible (slider); addAndMakeVisible (box);
        toggleAtt = std::make_unique<APVTS::ButtonAttachment> (apvts, "vibOn", toggle);
        sliderAtt = std::make_unique<APVTS::SliderAttachment> (apvts, "vibAmt", slider);
        box.refresh();
    }

    void place (juce::Rectangle<float> t, juce::Rectangle<float> sl, juce::Rectangle<float> v, float s)
    {
        const auto ts = t * s, ss = sl * s, vs = v * s;
        setBounds (ts.getUnion (ss).getUnion (vs).getSmallestIntegerContainer());
        const auto o = getPosition().toFloat();
        toggle.scale = slider.scale = s;
        toggle.setBounds (ts.translated (-o.x, -o.y).toNearestInt());
        slider.setBounds (ss.translated (-o.x, -o.y).toNearestInt());
        box.fit (vs.translated (-o.x, -o.y).toNearestInt(), s);
    }

private:
    VibToggle toggle;
    VibSlider slider;
    ValueBox box;   // slider'dan sonra
    std::unique_ptr<APVTS::ButtonAttachment> toggleAtt;
    std::unique_ptr<APVTS::SliderAttachment> sliderAtt;
};

// "-  440  +": artı/eksi Theme::kRefStep kadar değiştirir, ortadaki değer elle girilebilir.
class RefControl : public juce::Component
{
public:
    explicit RefControl (APVTS& apvts) : box (slider)
    {
        box.framed = false;
        slider.onValueChange = [this] { box.refresh(); };
        addAndMakeVisible (box);
        att = std::make_unique<APVTS::SliderAttachment> (apvts, "ref", slider);   // slider görünmez, sadece model
        box.refresh();
    }

    void place (juce::Rectangle<float> r, float s)
    {
        scale = s;
        setBounds ((r * s).toNearestInt());
        box.fit (getLocalBounds().reduced (getWidth() / 4, 0), s);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (Theme::kDark.darker (0.3f));
        g.fillRoundedRectangle (r, 8.f * scale);
        g.setColour (Theme::kMagenta);
        g.drawRoundedRectangle (r.reduced (scale), 8.f * scale, 1.5f * scale);
        g.setColour (Theme::kLight);
        g.setFont (Theme::font (r.getHeight() * 0.6f));
        g.drawText (Theme::str (Theme::kMinus), r.withWidth (r.getWidth() / 4), juce::Justification::centred);
        g.drawText (Theme::str (Theme::kPlus), r.withTrimmedLeft (r.getWidth() * 0.75f), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int dir = e.x < getWidth() / 4 ? -1 : e.x > getWidth() * 3 / 4 ? 1 : 0;
        if (dir != 0) slider.setValue (slider.getValue() + dir * Theme::kRefStep, juce::sendNotificationSync);   // aralığa kırpar
    }

private:
    juce::Slider slider;
    ValueBox box;
    float scale = 1.f;
    std::unique_ptr<APVTS::SliderAttachment> att;
};
} // namespace vr

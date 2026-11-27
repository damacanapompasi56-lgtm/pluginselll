#pragma once
// Elmas kazma knob'u: kazma görseli dönerek değeri gösterir (ucu değer yönüne bakar).
// İçinde: değer kutusu (çift tık = elle giriş), çentik çizgileri, APVTS bağlantısı.
// Aralık/skew parametreden gelir (SliderAttachment), burada sabit sayı yok.
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

class KazmaKnob : public juce::Component
{
public:
    // decimals: değer kutusundaki ondalık sayısı. offAtMin: minimum değerde "off" yaz.
    KazmaKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, int decimalPlaces, bool showOffAtMin)
        : decimals (decimalPlaces), offAtMin (showOffAtMin), pickaxe (Theme::image (Theme::kPickaxeRes))
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (kStart, kEnd, true);
        slider.onValueChange = [this] { refresh(); };
        addAndMakeVisible (slider);
        slider.addMouseListener (this, false);   // hover/basılı halka için

        valueBox.setJustificationType (juce::Justification::centred);
        valueBox.setEditable (false, true, false);   // sadece çift tıkla düzenlenir
        valueBox.setColour (juce::Label::textColourId, Theme::kLight);
        valueBox.setColour (juce::Label::textWhenEditingColourId, Theme::kLight);
        valueBox.setColour (juce::Label::backgroundWhenEditingColourId, Theme::kDark.darker (0.4f));
        valueBox.setColour (juce::Label::outlineWhenEditingColourId, Theme::kTeal);
        valueBox.onTextChange = [this] { commitText(); };
        addAndMakeVisible (valueBox);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
        refresh();
    }

    // valueR/knobR: Theme::L kutuları (tasarım uzayı), s: pencere ölçeği
    void place (juce::Rectangle<float> valueR, juce::Rectangle<float> knobR, float s)
    {
        scale = s;
        const auto v = valueR * s, k = knobR * s;
        setBounds (v.getUnion (k).getSmallestIntegerContainer());
        const auto o = getPosition().toFloat();
        valueRect = v.translated (-o.x, -o.y);
        knobRect  = k.translated (-o.x, -o.y);
        valueBox.setBounds (valueRect.toNearestInt());
        valueBox.setFont (Theme::font (valueRect.getHeight() * 0.6f));
        slider.setBounds (knobRect.toNearestInt());
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseUp (const juce::MouseEvent&) override    { repaint(); }

    void paint (juce::Graphics& g) override
    {
        // değer kutusu çerçevesi
        g.setColour (Theme::kDark.darker (0.3f));
        g.fillRoundedRectangle (valueRect, 8.f * scale);
        g.setColour (Theme::kMagenta);
        g.drawRoundedRectangle (valueRect.reduced (scale), 8.f * scale, 1.5f * scale);

        const auto c = knobRect.getCentre();
        const float r = juce::jmin (knobRect.getWidth(), knobRect.getHeight()) * 0.5f - scale;
        const auto at = [&] (float ang, float rad) { return c + juce::Point<float> (std::sin (ang), -std::cos (ang)) * rad; };

        // çentikler (sol ve sağ yaylar, altta boşluk)
        g.setColour (Theme::kLight.withAlpha (0.75f));
        for (int i = 0; i < kTicks; ++i)
        {
            const float a = kStart + (kEnd - kStart) * (float) i / (float) (kTicks - 1);
            const bool major = i == 0 || i == kTicks - 1 || i == kTicks / 2;
            g.drawLine (juce::Line<float> (at (a, r), at (a, r * (major ? 0.82f : 0.88f))), (major ? 1.6f : 1.f) * scale);
        }

        // halka
        const bool hot = slider.isMouseOverOrDragging();
        g.setColour (hot ? Theme::kMagenta.brighter (0.4f) : Theme::kMagenta);
        g.drawEllipse (juce::Rectangle<float> (r * 1.56f, r * 1.56f).withCentre (c), (hot ? 2.5f : 1.5f) * scale);

        // kazma: bir kez ölçeklenir (cache), her karede sadece döner
        if (pickaxe.isNull()) return;
        const float side  = r * 1.02f;                     // köşegen yarısı = 0.72r < halka (0.78r)
        const float phys  = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int   px    = juce::jmax (1, juce::roundToInt (side * phys));
        const auto  q     = Theme::kPixelStyle ? juce::Graphics::lowResamplingQuality : juce::Graphics::highResamplingQuality;
        if (cache.getWidth() != px)
            cache = pickaxe.rescaled (px, px, q);
        const float prop = (float) slider.valueToProportionOfLength (slider.getValue());
        const float ang  = kStart + (kEnd - kStart) * prop;
        g.setImageResamplingQuality (q);
        g.drawImageTransformed (cache, juce::AffineTransform::translation ((float) -px * Theme::kPickaxePivot, (float) -px * Theme::kPickaxePivot)
                                           .scaled (1.f / phys)
                                           .rotated (ang - Theme::kPickaxeTipAngle)
                                           .translated (c.x, c.y));
    }

private:
    // shortcut: shift basılıysa hassasiyet sürükleme başında seçilir, sürüklerken shift'e basmak etkisiz; ihtiyaç olursa mouseDrag'de yeniden hesapla.
    struct FineSlider : juce::Slider
    {
        void paint (juce::Graphics&) override {}   // çizimi KazmaKnob yapar
        void mouseDown (const juce::MouseEvent& e) override
        {
            setMouseDragSensitivity (e.mods.isShiftDown() ? 1500 : 250);
            juce::Slider::mouseDown (e);
        }
    };

    void refresh()
    {
        const double v = slider.getValue();
        valueBox.setText (offAtMin && v <= slider.getMinimum() ? Theme::str (Theme::kOff) : juce::String (v, decimals),
                          juce::dontSendNotification);
        repaint();
    }

    void commitText()
    {
        const auto t = valueBox.getText().trim();
        if (offAtMin && t.startsWithIgnoreCase (Theme::kOff))
            slider.setValue (slider.getMinimum(), juce::sendNotificationSync);
        else if (t.containsAnyOf ("0123456789"))
            slider.setValue (t.retainCharacters ("0123456789.,-").replaceCharacter (',', '.').getDoubleValue(), juce::sendNotificationSync);
        refresh();   // geçersiz/aralık dışı girişte gerçek değeri geri yaz
    }

    static constexpr float kStart = 1.25f * juce::MathConstants<float>::pi, kEnd = 2.75f * juce::MathConstants<float>::pi;
    static constexpr int kTicks = 11;

    int decimals;
    bool offAtMin;
    juce::Image pickaxe, cache;
    float scale = 1.f;
    juce::Rectangle<float> valueRect, knobRect;
    FineSlider slider;
    juce::Label valueBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;   // en sona: slider'dan önce ölür
};

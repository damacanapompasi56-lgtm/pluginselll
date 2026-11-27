#pragma once
// PARÇA 4B: yatay madenci fotoğrafı, kamera/CCTV/eski monitör görünümü.
// Fotoğraf (gri tonlu) ve tarama çizgisi+vinyet katmanı boyut değişince BİR KEZ hazırlanır; her karede sadece
// hazır görseller çizilir (hafif kayma, hazır noise karoları, parlaklık titremesi). Zamanlayıcı 15 fps.
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace cctv
{
class CctvView : public juce::Component, private juce::Timer
{
public:
    CctvView() : src (Theme::image (Theme::kMinerWideRes))
    {
        setOpaque (true);
        for (auto& t : noise)   // 4 küçük gri-gren karosu, bir kez
        {
            t = juce::Image (juce::Image::ARGB, 64, 64, true);
            juce::Image::BitmapData d (t, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x)
                {
                    const auto v = (juce::uint8) rnd.nextInt (256);
                    d.setPixelColour (x, y, juce::Colour (v, v, v, (juce::uint8) rnd.nextInt (40)));
                }
        }
        startTimerHz (15);
    }
    ~CctvView() override { stopTimer(); }

    void place (juce::Rectangle<float> r, float s) { scale = s; setBounds ((r * s).toNearestInt()); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float phys = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int px = juce::jmax (1, juce::roundToInt (b.getWidth() * phys)), py = juce::jmax (1, juce::roundToInt (b.getHeight() * phys));
        if (px != cw || py != ch) rebuild (px, py, phys);

        g.fillAll (Theme::kDark);
        g.drawImage (base, b.expanded (2.f).translated (jx, jy));           // 2 px taşma: kayma kenarı açmasın
        g.drawImage (overlay, b);                                           // tarama çizgileri + vinyet
        g.setColour (Theme::kTeal.withAlpha (0.08f));                       // monitör rengi
        g.fillRect (b);
        g.setTiledImageFill (noise[(size_t) noiseIdx], ox, oy, 1.f);        // noise
        g.fillRect (b);
        g.setColour (juce::Colours::black.withAlpha (flicker));             // parlaklık titremesi
        g.fillRect (b);
        g.setColour (Theme::kMagenta);
        g.drawRoundedRectangle (b.reduced (Theme::kPhotoFrame * 0.5f * scale), 6.f * scale, Theme::kPhotoFrame * 0.6f * scale);
    }

private:
    void timerCallback() override
    {
        noiseIdx = rnd.nextInt (4); ox = rnd.nextInt (64); oy = rnd.nextInt (64);
        flicker = rnd.nextFloat() * 0.07f;
        const bool shake = rnd.nextFloat() < 0.15f;   // ara sıra 1 px kayma
        jx = shake ? (float) (rnd.nextInt (3) - 1) : 0.f;
        jy = shake ? (float) (rnd.nextInt (3) - 1) : 0.f;
        repaint();
    }

    void rebuild (int px, int py, float phys)
    {
        cw = px; ch = py;
        base = juce::Image (juce::Image::ARGB, px, py, true);
        {
            juce::Graphics gi (base);
            gi.setImageResamplingQuality (Theme::kPixelStyle ? juce::Graphics::lowResamplingQuality : juce::Graphics::highResamplingQuality);
            if (! src.isNull()) gi.drawImage (src, juce::Rectangle<float> (0.f, 0.f, (float) px, (float) py), juce::RectanglePlacement::fillDestination);
        }
        base.desaturate();

        overlay = juce::Image (juce::Image::ARGB, px, py, true);
        juce::Image::BitmapData d (overlay, juce::Image::BitmapData::writeOnly);
        const int period = juce::jmax (2, juce::roundToInt (3.f * phys));
        for (int y = 0; y < py; ++y)
        {
            const float line = (y % period) == 0 ? 0.28f : 0.f;
            for (int x = 0; x < px; ++x)
            {
                const float dx = ((float) x / (float) px - 0.5f) * 2.f, dy = ((float) y / (float) py - 0.5f) * 2.f;
                const float r = juce::jlimit (0.f, 1.f, (std::sqrt (dx * dx + dy * dy) - 0.5f) / 0.9f);
                const float vig = 0.65f * r * r;
                d.setPixelColour (x, y, juce::Colours::black.withAlpha (1.f - (1.f - line) * (1.f - vig)));
            }
        }
    }

    juce::Image src, base, overlay, noise[4];
    juce::Random rnd;
    int cw = 0, ch = 0, noiseIdx = 0, ox = 0, oy = 0;
    float scale = 1.f, jx = 0.f, jy = 0.f, flicker = 0.f;
};
} // namespace cctv

#pragma once
// PARÇA 4A: orta nota/cent göstergesi ve giriş/çıkış seviye metreleri.
// Veri: Engine::dispMidi/dispTarget ve MadenciProcessor::inPeak/outPeak (hepsi zaten atomic, DSP'ye dokunulmadı).
// Zamanlayıcı 30 Hz; sadece görünür değişim olunca repaint.
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Engine.h"

namespace pm
{
// Algılanan perde (veya hedef nota) harfi + cent sapmasını gösteren halka/ibre
class PitchDisplay : public juce::Component, private juce::Timer
{
public:
    explicit PitchDisplay (mt::Engine& e) : eng (e) { startTimerHz (30); }
    ~PitchDisplay() override { stopTimer(); }

    void place (juce::Rectangle<float> r, float s) { scale = s; setBounds ((r * s).toNearestInt()); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        const float rad = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 8.f * scale;
        const bool has = note >= 0;
        const auto col = Theme::kTeal.interpolatedWith (Theme::kMagenta, juce::jlimit (0.f, 1.f, std::abs (dev) / Theme::kPitchMaxCents));

        g.setColour (Theme::kLight.withAlpha (0.15f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f, 3.f * scale);
        g.setColour (Theme::kLight.withAlpha (0.5f));
        g.drawLine (c.x, c.y - rad - 6.f * scale, c.x, c.y - rad + 6.f * scale, 2.f * scale);   // 0 cent işareti

        if (has)
        {
            const float ang = dev / Theme::kPitchMaxCents * Theme::kPitchSweep;
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, rad, rad, 0.f, 0.f, ang, true);
            g.setColour (col);
            g.strokePath (arc, juce::PathStrokeType (6.f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float d = 8.f * scale;
            g.fillEllipse (c.x + std::sin (ang) * rad - d, c.y - std::cos (ang) * rad - d, d * 2.f, d * 2.f);
        }

        const auto txt = has ? Theme::str (Theme::kKeyNames[note % 12]).toUpperCase() : Theme::str (Theme::kNoDetect);
        g.setColour (has ? Theme::kLight : Theme::kLight.withAlpha (0.4f));
        g.setFont (Theme::font ((txt.length() > 1 ? 80.f : 100.f) * scale));
        g.drawText (txt, b, juce::Justification::centred);
    }

private:
    void timerCallback() override
    {
        const float m = eng.dispMidi.load();
        const int t = eng.dispTarget.load();
        const int n = t >= 0 ? t : (m >= 0.f ? (int) std::lround (m) : -1);
        const float want = (m >= 0.f && n >= 0) ? juce::jlimit (-Theme::kPitchMaxCents, Theme::kPitchMaxCents, (m - (float) n) * 100.f) : 0.f;
        dev += 0.35f * (want - dev);   // akıcı ibre
        if (std::abs (want - dev) < 0.05f) dev = want;
        if (n != note || std::abs (dev - painted) > 0.3f) { note = n; painted = dev; repaint(); }
    }

    mt::Engine& eng;
    int note = -1;
    float dev = 0.f, painted = 0.f, scale = 1.f;   // dev: cent (giriş - hedef)
};

// Dikey seviye çubuğu: peak atomik'i her karede okunup sıfırlanır; hızlı yükselir, yavaş düşer
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (std::atomic<float>& p) : peak (p) { startTimerHz (30); }
    ~LevelMeter() override { stopTimer(); }

    void place (juce::Rectangle<float> r, float s) { scale = s; setBounds ((r * s).toNearestInt()); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (scale);
        const float f = juce::jlimit (0.f, 1.f, (db - Theme::kMeterFloorDb) / -Theme::kMeterFloorDb);
        g.setColour (db > -3.f ? Theme::kMagenta : Theme::kTeal);
        g.fillRect (r.withTrimmedTop (r.getHeight() * (1.f - f)).reduced (2.f * scale, 2.f * scale));
        g.setColour (Theme::kTeal.withAlpha (0.6f));
        g.drawRect (r, 1.5f * scale);
    }

private:
    void timerCallback() override
    {
        const float v = peak.exchange (0.f);   // audio thread max'ı yazar; okuyup sıfırlamak yeterli
        db = juce::jmax (v > 1e-5f ? 20.f * std::log10 (v) : Theme::kMeterFloorDb, db - Theme::kMeterFallDb, Theme::kMeterFloorDb);
        if (std::abs (db - painted) > 0.3f) { painted = db; repaint(); }
    }

    std::atomic<float>& peak;
    float db = Theme::kMeterFloorDb, painted = Theme::kMeterFloorDb, scale = 1.f;
};
} // namespace pm

#pragma once
// PARÇA 3A: scale seçici, bemol/diyez + nota harfleri, piyano, key detect, formant seçici.
// Parametre ID'leri: scale, key, n0..n11, formant. Engine'in key detect atomikleri okunur (DSP'ye dokunulmaz).
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Engine.h"

namespace sc
{
using APVTS = juce::AudioProcessorValueTreeState;

// Seçili = kTeal dolgu, değilse koyu dolgu + magenta çerçeve
inline void cell (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool sel, float s, bool hover = false)
{
    r = r.reduced (2.f * s);
    g.setColour (sel ? Theme::kTeal : Theme::kDark.darker (0.3f));
    g.fillRoundedRectangle (r, 8.f * s);
    if (hover) { g.setColour ((sel ? Theme::kLight : Theme::kMagenta).withAlpha (Theme::kHoverAlpha)); g.fillRoundedRectangle (r, 8.f * s); }
    g.setColour (sel ? Theme::kTeal : Theme::kMagenta);
    g.drawRoundedRectangle (r, 8.f * s, 1.5f * s);
    g.setColour (sel ? Theme::kDark : Theme::kLight);
    g.setFont (Theme::font (r.getHeight() * 0.6f));
    g.drawText (t, r, juce::Justification::centred);
}

// Choice parametresi için açılır kutu (scale, formant). Seçenek metinleri Theme'den, sırası parametreyle aynı.
class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (APVTS& apvts, const juce::String& id, const char* const* names, int n)
        : count (n), att (*apvts.getParameter (id), [this] (float v) { index = juce::jlimit (0, count - 1, (int) std::lround (v)); repaint(); })
    {
        for (int i = 0; i < n; ++i) labels.add (Theme::str (names[i]));
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        att.sendInitialUpdate();
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

    void fit (juce::Rectangle<float> r, float s) { scale = s; laf.h = 20.f * s; setBounds ((r * s).toNearestInt()); }

    void paint (juce::Graphics& g) override
    {
        cell (g, getLocalBounds().toFloat(), {}, false, scale, hover);
        const auto r = getLocalBounds().toFloat().reduced (2.f * scale);
        g.setColour (Theme::kLight);
        g.setFont (Theme::font (r.getHeight() * 0.5f));
        g.drawText (labels[index], r.withTrimmedRight (r.getHeight() * 1.1f), juce::Justification::centred);
        const float a = r.getHeight() * 0.28f;
        const juce::Point<float> c (r.getRight() - r.getHeight() * 0.6f, r.getCentreY());
        juce::Path p;
        p.addTriangle (c.x - a, c.y - a * 0.5f, c.x + a, c.y - a * 0.5f, c.x, c.y + a * 0.6f);
        g.setColour (Theme::kMagenta);
        g.fillPath (p);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        juce::PopupMenu m;
        laf.setColour (juce::PopupMenu::backgroundColourId, Theme::kDark.darker (0.3f));   // canlı palet (hell modu)
        laf.setColour (juce::PopupMenu::textColourId, Theme::kLight);
        laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::kTeal);
        laf.setColour (juce::PopupMenu::highlightedTextColourId, Theme::kDark);
        m.setLookAndFeel (&laf);
        for (int i = 0; i < count; ++i) m.addItem (i + 1, labels[i], true, i == index);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                         [safe = juce::Component::SafePointer<ChoiceBox> (this)] (int r)
                         { if (safe != nullptr && r > 0) safe->att.setValueAsCompleteGesture ((float) (r - 1)); });
    }

private:
    struct Laf : juce::LookAndFeel_V4
    {
        float h = 20.f;
        juce::Font getPopupMenuFont() override { return Theme::font (h); }
    };

    int count, index = 0;
    bool hover = false;
    float scale = 1.f;
    juce::StringArray labels;
    Laf laf;
    juce::ParameterAttachment att;   // en sonda: callback diğer üyeleri kullanır
};

// bemol / diyez + A..G. "key" parametresi 0..11 (C, C#, ... B) tek sayı; yazım (D♭ / C#) burada hatırlanır.
class KeyPicker : public juce::Component
{
public:
    explicit KeyPicker (APVTS& apvts)
        : att (*apvts.getParameter ("key"), [this] (float v) { sync ((int) std::lround (v)); repaint(); })
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        att.sendInitialUpdate();
    }

    void place (juce::Rectangle<float> acc, juce::Rectangle<float> row, float s)
    {
        const auto a = acc * s, r = row * s;
        setBounds (a.getUnion (r).getSmallestIntegerContainer());
        const auto o = getPosition().toFloat();
        accR = a.translated (-o.x, -o.y);
        rowR = r.translated (-o.x, -o.y);
        scale = s;
    }

    // Bileşenin birleşik kutusu scale kutusunun üstüne biner; sadece kendi hücrelerimiz fareyi alsın
    bool hitTest (int x, int y) override { const juce::Point<float> p ((float) x, (float) y); return accR.contains (p) || rowR.contains (p); }

    void paint (juce::Graphics& g) override
    {
        const float w = accR.getWidth() * 0.5f;
        cell (g, accR.withWidth (w), Theme::str (Theme::kFlat), acc < 0, scale, hov == 0);
        cell (g, accR.withTrimmedLeft (w), Theme::str (Theme::kSharp), acc > 0, scale, hov == 1);
        const float cw = rowR.getWidth() / 7.f;
        for (int i = 0; i < 7; ++i)
            cell (g, { rowR.getX() + (float) i * cw, rowR.getY(), cw, rowR.getHeight() },
                  juce::String::charToString ((juce::juce_wchar) Theme::kNoteLetters[i]), i == letter, scale, hov == 2 + i);
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (cellAt (e.position)); }
    void mouseExit (const juce::MouseEvent&) override  { setHover (-1); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int c = cellAt (e.position);
        if (c < 0) return;
        int l = letter, a = acc;
        if (c < 2) { const int v = c == 0 ? -1 : 1; a = a == v ? 0 : v; }
        else l = c - 2;
        letter = l; acc = a;
        att.setValueAsCompleteGesture ((float) pc (l, a));
        repaint();   // aynı perde farklı yazım (C# <-> Db) parametreyi değiştirmez
    }

private:
    int cellAt (juce::Point<float> p) const   // 0/1: bemol/diyez, 2..8: A..G, -1: yok
    {
        if (accR.contains (p)) return p.x < accR.getCentreX() ? 0 : 1;
        if (rowR.contains (p)) return 2 + juce::jlimit (0, 6, (int) ((p.x - rowR.getX()) / (rowR.getWidth() / 7.f)));
        return -1;
    }
    void setHover (int c) { if (c != hov) { hov = c; repaint(); } }

    static constexpr int kNat[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G -> perde sınıfı (kNoteLetters sırası)
    static int pc (int l, int a) { return (kNat[l] + a + 12) % 12; }

    void sync (int k)   // dışarıdan (otomasyon, preset) gelen değer
    {
        if (pc (letter, acc) == k) return;
        for (int i = 0; i < 7; ++i) if (kNat[i] == k)     { letter = i; acc = 0; return; }
        for (int i = 0; i < 7; ++i) if (kNat[i] + 1 == k) { letter = i; acc = 1; return; }
    }

    int letter = 2, acc = 0, hov = -1;   // C, doğal (parametre varsayılanı 0 = C)
    float scale = 1.f;
    juce::Rectangle<float> accR, rowR;
    juce::ParameterAttachment att;
};

// 10 beyaz tuş (C..E, taslaktaki 7 siyah tuş), aktif perde sınıfları (n0..n11) teal; tıklayınca aç/kapa
class Piano : public juce::Component
{
public:
    explicit Piano (APVTS& apvts)
    {
        for (int i = 0; i < 12; ++i)
        {
            att[(size_t) i] = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("n" + juce::String (i)),
                                                                          [this, i] (float v) { on[i] = v > 0.5f; repaint(); });
            att[(size_t) i]->sendInitialUpdate();
        }
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void place (juce::Rectangle<float> r, float s) { scale = s; setBounds ((r * s).toNearestInt()); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        for (int w = 0; w < kWhites; ++w)
        {
            const auto k = white (w);
            g.setColour ((on[kWhitePc[w % 7]] ? Theme::kTeal : Theme::kLight).interpolatedWith (Theme::kLight, hov == w ? 0.3f : 0.f));
            g.fillRect (k);
            g.setColour (Theme::kDark);
            g.drawRect (k, 1.5f * scale);
        }
        for (int w = 0; w < kWhites - 1; ++w)
        {
            if (! hasBlack (w)) continue;
            const auto k = black (w);
            g.setColour ((on[kWhitePc[w % 7] + 1] ? Theme::kTeal.darker (0.4f) : Theme::kDark.darker (0.6f)).interpolatedWith (Theme::kLight, hov == 100 + w ? 0.3f : 0.f));
            g.fillRect (k);
            g.setColour (Theme::kDark);
            g.drawRect (k, 1.5f * scale);
        }
        g.setColour (Theme::kTeal.withAlpha (0.6f));
        g.drawRect (r, 1.5f * scale);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int pc = pcOf (keyAt (e.position));
        att[(size_t) pc]->setValueAsCompleteGesture (on[pc] ? 0.f : 1.f);
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (keyAt (e.position)); }
    void mouseExit (const juce::MouseEvent&) override  { setHover (-1); }

private:
    static constexpr int kWhites = 10;
    static constexpr int kWhitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static bool hasBlack (int w) { return w % 7 != 2 && w % 7 != 6; }   // E ve B'den sonra siyah tuş yok
    int keyAt (juce::Point<float> p) const
    {
        for (int w = 0; w < kWhites - 1; ++w)
            if (hasBlack (w) && black (w).contains (p)) return 100 + w;
        return juce::jlimit (0, kWhites - 1, (int) (p.x / ((float) getWidth() / (float) kWhites)));
    }
    static int pcOf (int k) { return k >= 100 ? kWhitePc[(k - 100) % 7] + 1 : kWhitePc[k % 7]; }
    void setHover (int k) { if (k != hov) { hov = k; repaint(); } }

    juce::Rectangle<float> white (int w) const { const float ww = (float) getWidth() / kWhites; return { (float) w * ww, 0.f, ww, (float) getHeight() }; }
    juce::Rectangle<float> black (int w) const
    {
        const float ww = (float) getWidth() / kWhites, bw = ww * 0.6f;
        return { (float) (w + 1) * ww - bw * 0.5f, 0.f, bw, (float) getHeight() * 0.6f };
    }

    bool on[12] = {};
    int hov = -1;   // beyaz tuş: w, siyah tuş: 100 + w
    float scale = 1.f;
    std::array<std::unique_ptr<juce::ParameterAttachment>, 12> att;
};

// "paylox'un kulağı": tıkla = dinlemeyi başlat (Engine::keyRequest), dinlerken %ilerleme, sonra "a minör"; sonuç yoksa "-"
class KeyDetectView : public juce::Component, private juce::Timer
{
public:
    explicit KeyDetectView (mt::Engine& e) : eng (e)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (10);
    }
    ~KeyDetectView() override { stopTimer(); }

    void place (juce::Rectangle<float> label, juce::Rectangle<float> result, float s)
    {
        const auto l = label * s, r = result * s;
        setBounds (l.getUnion (r).getSmallestIntegerContainer());
        const auto o = getPosition().toFloat();
        labelR = l.translated (-o.x, -o.y);
        resultR = r.translated (-o.x, -o.y);
        scale = s;
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::kLight);
        g.setFont (Theme::font (26.f * scale));
        g.drawText (Theme::str (Theme::kKeyDetect), labelR, juce::Justification::centredLeft);
        g.setFont (Theme::font (22.f * scale));
        g.setColour (hover ? Theme::kTeal : Theme::kLight);
        g.drawText (shown, resultR, juce::Justification::centredLeft);
    }

    bool hitTest (int x, int y) override { const juce::Point<float> p ((float) x, (float) y); return labelR.contains (p) || resultR.contains (p); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (eng.keyState.load() != 1) eng.keyRequest.store (true);   // sadece boştayken; dinleme sürerken yeniden başlatma yok
    }

private:
    void timerCallback() override
    {
        const int k = eng.keyResult.load();
        const juce::String t = eng.keyState.load() == 1 ? "%" + juce::String ((int) (eng.keyProgress.load() * 100.f))
                             : k < 0 ? Theme::str (Theme::kNoDetect)
                                     : Theme::str (Theme::kKeyNames[k % 12]) + " " + Theme::str (k < 12 ? Theme::kMajor : Theme::kMinor);
        if (t != shown) { shown = t; repaint(); }
    }

    mt::Engine& eng;
    juce::String shown = Theme::str (Theme::kNoDetect);
    bool hover = false;
    juce::Rectangle<float> labelR, resultR;
    float scale = 1.f;
};
} // namespace sc

#include "PluginEditor.h"
#include "Theme.h"

using namespace juce;
namespace L = Theme::L;

namespace
{
void text (Graphics& g, const char* s, Rectangle<float> r, float h, Justification j = Justification::centred)
{
    g.setColour (Theme::kLight);
    g.setFont (Theme::font (h));
    g.drawText (Theme::str (s), r, j);
}

void slot (Graphics& g, Rectangle<float> r, Colour c, bool round = false)
{
    g.setColour (c.withAlpha (0.6f));
    if (round) g.drawEllipse (r, 1.5f); else g.drawRoundedRectangle (r, 6.f, 1.5f);
}

// Görsel yoksa yer tutucu çizer; oran korunur, taşan kısım kırpılır
void photo (Graphics& g, const Image& img, Rectangle<float> r)
{
    if (img.isNull()) { slot (g, r, Theme::kMagenta); return; }
    g.saveState();
    g.reduceClipRegion (r.getSmallestIntegerContainer());
    g.drawImage (img, r, RectanglePlacement::fillDestination);
    g.restoreState();
}
}

MadenciEditor::MadenciEditor (MadenciProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    pickaxe   = Theme::image (Theme::kPickaxeRes);
    minerWide = Theme::image (Theme::kMinerWideRes);
    minerTall = Theme::image (Theme::kMinerTallRes);

    setResizable (true, true);
    setResizeLimits (Theme::kW / 2, Theme::kH / 2, Theme::kW * 2, Theme::kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) Theme::kW / Theme::kH);
    setSize (Theme::kW, Theme::kH);
}

void MadenciEditor::paint (Graphics& g)
{
    g.addTransform (AffineTransform::scale ((float) getWidth() / (float) Theme::kW));
    g.fillAll (Theme::kDark);
    g.setImageResamplingQuality (Theme::kPixelStyle ? Graphics::lowResamplingQuality : Graphics::highResamplingQuality);

    // başlık
    text (g, Theme::kTitle, L::titleText, 24, Justification::centredLeft);
    photo (g, pickaxe, L::titleIcon);

    // scale + nota harfleri
    text (g, Theme::kScale, L::scaleLabel, 40, Justification::centredLeft);
    slot (g, L::scaleBox, Theme::kMagenta);
    text (g, Theme::kSampleScale, L::scaleBox.withTrimmedRight (55), 30);
    for (int i = 0; i < 2; ++i)
    {
        auto c = L::accBox.withWidth (L::accBox.getWidth() / 2).translated ((float) i * L::accBox.getWidth() / 2, 0.f);
        slot (g, c, Theme::kMagenta);
        text (g, i == 0 ? Theme::kSampleFlat : Theme::kSampleSharp, c, 28);
    }
    const float cw = L::noteRow.getWidth() / 7.f;
    for (int i = 0; i < 7; ++i)
    {
        auto c = Rectangle<float> (L::noteRow.getX() + (float) i * cw, L::noteRow.getY(), cw, L::noteRow.getHeight());
        slot (g, c, Theme::kMagenta);
        text (g, String (Theme::kNoteLetters).substring (i, i + 1).toRawUTF8(), c, 36);
    }

    // piyano
    slot (g, L::piano, Theme::kTeal);

    // giriş / çıkış ölçerleri
    text (g, Theme::kIn, L::inLabel, 14);   slot (g, L::inMeter, Theme::kTeal);
    text (g, Theme::kOut, L::outLabel, 14); slot (g, L::outMeter, Theme::kTeal);

    // orta ekran
    slot (g, L::display, Theme::kTeal);
    text (g, Theme::kSampleNote, L::display, 110);
    text (g, Theme::kTitle, L::centerTitle, 30);

    // reference + yatay madenci
    text (g, Theme::kReference, L::refLabel, 14, Justification::centredRight);
    slot (g, L::refBox, Theme::kMagenta);
    text (g, Theme::kSampleRef, L::refBox, 24);
    photo (g, minerWide, L::minerWide);

    // speed / note transition
    text (g, Theme::kSpeed, L::speedLabel, 14);
    slot (g, L::speedValue, Theme::kMagenta);  text (g, Theme::kSampleSpeed, L::speedValue, 24);
    slot (g, L::speedKnob, Theme::kMagenta, true);
    text (g, Theme::kTransition, L::transLabel, 14);
    slot (g, L::transValue, Theme::kMagenta);  text (g, Theme::kSampleTrans, L::transValue, 24);
    slot (g, L::transKnob, Theme::kMagenta, true);

    // vibrato
    text (g, Theme::kVibrato, L::vibLabel, 14, Justification::centredLeft);
    slot (g, L::vibToggle, Theme::kMagenta);   text (g, Theme::kSampleOn, L::vibToggle, 16);
    slot (g, L::vibSlider, Theme::kMagenta);
    slot (g, L::vibValue, Theme::kMagenta);    text (g, Theme::kSampleVib, L::vibValue, 24);

    // tolerance
    text (g, Theme::kTolerance, L::tolLabel, 14);
    text (g, Theme::kCents, L::centsLabel, 12);
    slot (g, L::centsValue, Theme::kMagenta);  text (g, Theme::kSampleOff, L::centsValue, 16);
    slot (g, L::centsKnob, Theme::kMagenta, true);
    text (g, Theme::kTime, L::timeLabel, 12);
    slot (g, L::timeValue, Theme::kMagenta);   text (g, Theme::kSampleOff, L::timeValue, 16);
    slot (g, L::timeKnob, Theme::kMagenta, true);

    // key detect + dikey madenci
    photo (g, minerTall, L::keyPhoto);
    text (g, Theme::kKeyDetect, L::keyLabel, 26, Justification::centredLeft);
    text (g, Theme::kSampleKey, L::keyResult, 22, Justification::centredLeft);

    // range
    text (g, Theme::kRange, L::rangeLabel, 26);
    slot (g, L::rangeBox, Theme::kTeal);

    // alt yazılar
    text (g, Theme::kFootLeft, L::footLeft, 10, Justification::centredLeft);
    text (g, Theme::kFootMid, L::footMid, 10);
}

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

// Yazıyı kutuya sığdırır: yükseklik h (kutu yüksekliğini aşmaz), genişlik yetmezse orantılı küçülür
void fitText (Graphics& g, const char* s, Rectangle<float> r, float h, Justification j = Justification::centred)
{
    const auto str = Theme::str (s);
    h = jmin (h, r.getHeight());
    GlyphArrangement ga;
    ga.addLineOfText (Theme::font (h), str, 0.f, 0.f);
    const float w = ga.getBoundingBox (0, -1, true).getWidth();
    if (w > r.getWidth() && w > 0.f) h *= r.getWidth() / w;
    g.setColour (Theme::kLight);
    g.setFont (Theme::font (h));
    g.drawText (str, r, j);
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

void FormantButton::paintButton (Graphics& g, bool over, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (Theme::kDark.brighter (down ? 0.35f : over ? 0.25f : 0.15f));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (Theme::kTeal.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 6.f, 1.5f);
    // 0 = corrected, 1 = not corrected (toggle açık = 1)
    fitText (g, Theme::kFormantOpts[getToggleState() ? 1 : 0], r.reduced (6.f, 2.f), r.getHeight() * 0.55f);
}

MadenciEditor::MadenciEditor (MadenciProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      speedKnob (Theme::kTipSpeed, p.apvts, "speed", 1, false), transKnob (Theme::kTipTransition, p.apvts, "glide", 1, false),
      centsKnob (Theme::kTipCents, p.apvts, "tolC", 0, true),   timeKnob  (Theme::kTipTime, p.apvts, "tolT", 0, true),
      vibrato (Theme::kTipVibrato, p.apvts), refCtl (Theme::kTipRef, p.apvts),
      scaleBox (Theme::kTipScale, p.apvts, "scale", Theme::kScaleNames, 3),
      keyPick (Theme::kTipScale, p.apvts), piano (Theme::kTipPiano, p.apvts), keyView (Theme::kTipKeyDetect, p.engine), rangeCtl (Theme::kTipRange, p.apvts),
      pitchDisp (Theme::kTipDisplay, p.engine), inMeter (Theme::kTipIn, p.inPeak), outMeter (Theme::kTipOut, p.outPeak),
      speedAtt (*p.apvts.getParameter ("speed"), [this] (float v) { onSpeed (v); })
{
    for (auto* k : { &speedKnob, &transKnob, &centsKnob, &timeKnob })
        addAndMakeVisible (k);
    addAndMakeVisible (vibrato);
    addAndMakeVisible (refCtl);
    for (auto* c : std::initializer_list<Component*> { &scaleBox, &keyPick, &piano, &keyView, &rangeCtl, &pitchDisp, &inMeter, &outMeter, &cctvView })
        addAndMakeVisible (c);

    formantBtn.setTooltip (Theme::str (Theme::kTipFormant));
    addAndMakeVisible (formantBtn);
    formantAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "formant", formantBtn);   // aynı parametre: choice 0/1

    // etiket alanları da tooltip versin (kontrollerin üstüne binmezler)
    const auto zone = [this] (const Theme::R& r, const char* t)
    {
        zones.push_back ({ r, std::make_unique<tt::Tip<Component>> (t) });
        addAndMakeVisible (*zones.back().c);
    };
    zone (L::scaleLabel, Theme::kTipScale);     zone (L::formantLabel, Theme::kTipFormant);
    zone (L::rangeLabel, Theme::kTipRange);     zone (L::refLabel, Theme::kTipRef);
    zone (L::speedLabel, Theme::kTipSpeed);     zone (L::transLabel, Theme::kTipTransition);
    zone (L::vibLabel, Theme::kTipVibrato);     zone (L::tolLabel, Theme::kTipTolerance);
    zone (L::centsLabel, Theme::kTipCents);     zone (L::timeLabel, Theme::kTipTime);
    zone (L::inLabel, Theme::kTipIn);           zone (L::outLabel, Theme::kTipOut);
    tip.setLookAndFeel (&tipLaf);
    tip.toFront (false);   // sonradan eklenen çocukların altında kalmasın

    pickaxe   = Theme::image (Theme::kPickaxeRes);
    minerTall = Theme::image (Theme::kMinerTallRes);

    setResizable (true, true);
    setResizeLimits (Theme::kW / 2, Theme::kH / 2, Theme::kW * 2, Theme::kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) Theme::kW / Theme::kH);
    setSize (Theme::kW, Theme::kH);

    speedAtt.sendInitialUpdate();   // editör negatif speed ile açılırsa animasyonsuz doğrudan hell
    animate = true;
}

MadenciEditor::~MadenciEditor()
{
    stopTimer();
    Theme::setHell (0.f);   // paleti paylaşılan global olarak normale bırak
}

namespace
{
void recolourLabels (Component& c)   // Label renkleri kurucuda set edilmişti, palet değişince yenile
{
    for (auto* ch : c.getChildren())
    {
        if (auto* l = dynamic_cast<Label*> (ch))
        {
            l->setColour (Label::textColourId, Theme::kLight);
            l->setColour (Label::textWhenEditingColourId, Theme::kLight);
            l->setColour (Label::backgroundWhenEditingColourId, Theme::kDark.darker (0.4f));
            l->setColour (Label::outlineWhenEditingColourId, Theme::kTeal);
        }
        recolourLabels (*ch);
    }
}
}

void MadenciEditor::onSpeed (float v)
{
    const bool neg = v < 0.f;
    if (! animate) { hellTarget = neg; hell = neg ? 1.f : 0.f; applyPalette(); return; }
    if (neg == hellTarget) return;
    hellTarget = neg;
    shakeMs = neg ? Theme::kShakeMs : 0.f;      // sallanma sadece girişte
    if (! neg) { shake = {}; applyShake(); }
    lastTick = Time::getMillisecondCounterHiRes();
    startTimerHz (Theme::kHellFps);
}

void MadenciEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) (now - lastTick);
    lastTick = now;

    if (shakeMs > 0.f)
    {
        shakeMs -= dt;
        const float amp = Theme::kShakeAmp * ((float) getWidth() / (float) Theme::kW) * jmax (0.f, shakeMs) / Theme::kShakeMs;
        shake = shakeMs > 0.f ? Point<int> (roundToInt ((rnd.nextFloat() * 2.f - 1.f) * amp), roundToInt ((rnd.nextFloat() * 2.f - 1.f) * amp)) : Point<int>();
        applyShake();
        return;
    }
    const float step = dt / (hellTarget ? Theme::kHellInMs : Theme::kHellOutMs);
    hell = hellTarget ? jmin (1.f, hell + step) : jmax (0.f, hell - step);
    applyPalette();
    if (hell == (hellTarget ? 1.f : 0.f)) stopTimer();
}

void MadenciEditor::applyPalette()
{
    Theme::setHell (hell * hell * (3.f - 2.f * hell));   // smoothstep
    recolourLabels (*this);
    repaint();
}

void MadenciEditor::applyShake()
{
    const auto t = shake == Point<int>() ? AffineTransform() : AffineTransform::translation ((float) shake.x, (float) shake.y);
    for (auto* c : getChildren())
        if (c != &tip) c->setTransform (t);
    repaint();
}

void MadenciEditor::resized()
{
    const float s = (float) getWidth() / (float) Theme::kW;
    speedKnob.place (L::speedValue, L::speedKnob, s);
    transKnob.place (L::transValue, L::transKnob, s);
    centsKnob.place (L::centsValue, L::centsKnob, s);
    timeKnob .place (L::timeValue,  L::timeKnob,  s);
    vibrato.place (L::vibToggle, L::vibSlider, L::vibValue, s);
    refCtl.place (L::refBox, s);
    scaleBox.fit (L::scaleBox, s);
    formantBtn.setBounds ((L::formantBox * s).toNearestInt());
    keyPick.place (L::accBox, L::noteRow, s);
    piano.place (L::piano, s);
    keyView.place (L::keyLabel, L::keyResult, s);
    rangeCtl.place (L::rangeBox, L::rangeChoice, L::rangeInfo, s);
    pitchDisp.place (L::display, s);
    inMeter.place (L::inMeter, s);
    outMeter.place (L::outMeter, s);
    cctvView.place (L::minerWide, s);
    for (auto& z : zones) z.c->setBounds ((z.r * s).toNearestInt());
}

void MadenciEditor::paint (Graphics& g)
{
    g.addTransform (AffineTransform::translation ((float) shake.x, (float) shake.y));   // sallanma
    g.addTransform (AffineTransform::scale ((float) getWidth() / (float) Theme::kW));
    g.fillAll (Theme::kDark);
    g.setImageResamplingQuality (Theme::kPixelStyle ? Graphics::lowResamplingQuality : Graphics::highResamplingQuality);

    // başlık
    text (g, Theme::kTitle, L::titleText, 24, Justification::centredLeft);
    photo (g, pickaxe, L::titleIcon);

    // scale etiketi + formant etiketi (kutular, harfler, piyano ayrı bileşenler)
    text (g, Theme::kScale, L::scaleLabel, 40, Justification::centredLeft);
    fitText (g, Theme::kFormant, L::formantLabel, 24, Justification::centredLeft);

    // giriş / çıkış ölçerleri
    text (g, Theme::kIn, L::inLabel, 14);
    text (g, Theme::kOut, L::outLabel, 14);

    // orta ekran
    text (g, Theme::kTitle, L::centerTitle, 30);

    // reference + yatay madenci
    text (g, Theme::kReference, L::refLabel, 14, Justification::centredRight);

    // speed / note transition
    fitText (g, Theme::kSpeed, L::speedLabel, 22);
    fitText (g, Theme::kTransition, L::transLabel, 22);

    // vibrato
    fitText (g, Theme::kVibrato, L::vibLabel, 22, Justification::centredLeft);

    // tolerance
    fitText (g, Theme::kTolerance, L::tolLabel, 22);
    fitText (g, Theme::kCents, L::centsLabel, 20);
    fitText (g, Theme::kTime, L::timeLabel, 20);

    // key detect + dikey madenci
    photo (g, minerTall, L::keyPhoto.reduced (Theme::kPhotoFrame));
    g.setColour (Theme::kMagenta);
    g.drawRoundedRectangle (L::keyPhoto.reduced (Theme::kPhotoFrame * 0.5f), 6.f, Theme::kPhotoFrame * 0.6f);

    // range
    text (g, Theme::kRange, L::rangeLabel, 26);

    // alt yazılar
    text (g, Theme::kFootLeft, L::footLeft, 10, Justification::centredLeft);
    text (g, Theme::kFootMid, L::footMid, 10, Justification::centredLeft);
}

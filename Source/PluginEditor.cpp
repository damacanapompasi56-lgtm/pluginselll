#include "PluginEditor.h"

using namespace juce;

// Etiketler tek yerde (UTF-8 kacisli: derleyici kod sayfasindan bagimsiz)
static const String kLabelIn  = String::fromUTF8 ("Giri\xc5\x9f");
static const String kLabelOut = String::fromUTF8 ("Siki\xc5\x9f");

static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static String noteName (int midi) { return String (kNames[((midi % 12) + 12) % 12]) + String (midi / 12 - 1); }

static String listeningText (float progress)
{
    const int total = roundToInt (mt::kKeySeconds);
    return "Dinleniyor... " + String (jmin (total, (int) (progress * (float) total))) + " sn / " + String (total) + " sn";
}

MadenciEditor::Piano::Piano (MadenciProcessor& p) : proc (p)
{
    for (int i = 0; i < 12; ++i) np[i] = proc.apvts.getParameter ("n" + String (i));
}

Rectangle<float> MadenciEditor::Piano::keyRect (int pc) const
{
    static const int whiteIdx[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const float ww = (float) getWidth() / 7.f, bw = ww * 0.6f, bh = (float) getHeight() * 0.6f;
    if (whiteIdx[pc] >= 0) return { whiteIdx[pc] * ww, 0.f, ww, (float) getHeight() };
    return { whiteIdx[pc - 1] * ww + ww - bw / 2.f, 0.f, bw, bh };   // siyah: solundaki beyaz tusun sagi
}

void MadenciEditor::Piano::paint (Graphics& g)
{
    static const int whites[7] = { 0, 2, 4, 5, 7, 9, 11 }, blacks[5] = { 1, 3, 6, 8, 10 };
    auto draw = [&] (int pc, bool black)
    {
        const auto r = keyRect (pc);
        const bool on = np[pc]->getValue() > 0.5f;
        g.setColour (pc == target ? Colours::orange : on ? (black ? Colour (0xff2e7d32) : Colour (0xff81c784))
                                                         : (black ? Colours::black : Colours::white));
        g.fillRect (r);
        g.setColour (Colours::grey); g.drawRect (r);
        g.setColour (on || black ? Colours::white : Colours::black);
        g.drawText (kNames[pc], r.withTrimmedTop (r.getHeight() - 22.f), Justification::centred);
    };
    for (int pc : whites) draw (pc, false);
    for (int pc : blacks) draw (pc, true);
}

void MadenciEditor::Piano::mouseDown (const MouseEvent& e)
{
    static const int order[12] = { 1, 3, 6, 8, 10, 0, 2, 4, 5, 7, 9, 11 };   // siyahlar once
    for (int pc : order)
        if (keyRect (pc).contains (e.position))
        {
            auto* p = np[pc];
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.f : 1.f);
            p->endChangeGesture();
            repaint();
            return;
        }
}

void MadenciEditor::addKnob (Knob& k, const char* id, const char* text)
{
    k.l.setText (text, dontSendNotification);
    k.l.setJustificationType (Justification::centred);
    k.s.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 18);
    addAndMakeVisible (k.s); addAndMakeVisible (k.l);
    k.a = std::make_unique<SA> (proc.apvts, id, k.s);
}

void MadenciEditor::addCombo (ComboBox& c, std::unique_ptr<CA>& att, const char* id)
{
    auto* p = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id));
    c.addItemList (p->choices, 1);
    addAndMakeVisible (c);
    att = std::make_unique<CA> (proc.apvts, id, c);
}

void MadenciEditor::setChoice (const char* id, int index)
{
    if (auto* p = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
    {
        p->beginChangeGesture(); *p = index; p->endChangeGesture();
    }
}

MadenciEditor::MadenciEditor (MadenciProcessor& p) : AudioProcessorEditor (&p), proc (p), piano (p)
{
    setSize (1000, 440);   // Key Detect etiketi ("A# minor (alternatif: C# major)") icin genis
    addAndMakeVisible (detectBtn);
    detectBtn.onClick = [this] { proc.engine.keyRequest.store (true); keyLabel.setText (listeningText (0.f), dontSendNotification); };
    addAndMakeVisible (keyLabel); addAndMakeVisible (noteLabel);
    keyLabel.setColour (Label::textColourId, Colours::white);
    noteLabel.setColour (Label::textColourId, Colours::white);
    noteLabel.setFont (Font (FontOptions (18.f)));

    addCombo (keyBox, keyAtt, "key"); addCombo (scaleBox, scaleAtt, "scale"); addCombo (rangeBox, rangeAtt, "range");
    addAndMakeVisible (smartBtn); addAndMakeVisible (vibBtn);
    smartAtt = std::make_unique<BA> (proc.apvts, "smart", smartBtn);
    vibAtt = std::make_unique<BA> (proc.apvts, "vibOn", vibBtn);

    addKnob (kRef, "ref", "Reference Hz");   addKnob (kSpeed, "speed", "Speed");
    addKnob (kGlide, "glide", "Note Transition"); addKnob (kVib, "vibAmt", "Vibrato %");
    addKnob (kTolC, "tolC", "Tolerance Cents"); addKnob (kTolT, "tolT", "Tolerance Time");
    addAndMakeVisible (piano);
    startTimerHz (30);
}

void MadenciEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (28);
    detectBtn.setBounds (top.removeFromLeft (100)); top.removeFromLeft (6);
    keyLabel.setBounds (top.removeFromLeft (250)); top.removeFromLeft (6);
    keyBox.setBounds (top.removeFromLeft (70)); top.removeFromLeft (6);
    scaleBox.setBounds (top.removeFromLeft (100)); top.removeFromLeft (6);
    rangeBox.setBounds (top.removeFromLeft (130)); top.removeFromLeft (10);
    smartBtn.setBounds (top.removeFromLeft (120));
    vibBtn.setBounds (top.removeFromLeft (90));

    r.removeFromTop (10);
    auto row = r.removeFromTop (130);
    Knob* ks[6] = { &kRef, &kSpeed, &kGlide, &kVib, &kTolC, &kTolT };
    const int w = row.getWidth() / 6;
    for (auto* k : ks) { auto c = row.removeFromLeft (w); k->l.setBounds (c.removeFromTop (20)); k->s.setBounds (c); }

    r.removeFromTop (10);
    auto mid = r.removeFromTop (130);
    piano.setBounds (mid.removeFromLeft (490));
    noteLabel.setBounds (mid.reduced (16, 0));
}

static void drawMeter (Graphics& g, Rectangle<int> area, const String& name, float level)
{
    g.setColour (Colours::white);
    g.drawText (name, area.removeFromLeft (60), Justification::centredLeft);
    g.setColour (Colour (0xff111111)); g.fillRect (area);
    const float db = Decibels::gainToDecibels (level, -60.f);
    const float f = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
    g.setColour (db > -1.f ? Colours::red : Colours::limegreen);
    g.fillRect (area.withWidth (roundToInt (area.getWidth() * f)));
}

void MadenciEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff262626));
    auto m = getLocalBounds().reduced (12).removeFromBottom (60);
    drawMeter (g, m.removeFromTop (24), kLabelIn, inLevel);
    m.removeFromTop (8);
    drawMeter (g, m.removeFromTop (24), kLabelOut, outLevel);
}

void MadenciEditor::timerCallback()
{
    inLevel = jmax (proc.inPeak.exchange (0.f), inLevel * 0.85f);
    outLevel = jmax (proc.outPeak.exchange (0.f), outLevel * 0.85f);

    const float m = proc.engine.dispMidi.load();
    const int t = proc.engine.dispTarget.load();
    piano.target = t >= 0 ? ((t % 12) + 12) % 12 : -1;
    if (m < 0.f) noteLabel.setText ("Detected: --\nTarget: --", dontSendNotification);
    else
    {
        const int nearest = (int) std::lround (m);
        const int ref = t >= 0 ? t : nearest;
        noteLabel.setText ("Detected: " + noteName (nearest) + "  (" + String (roundToInt ((m - (float) ref) * 100.f)) + " c)\nTarget: "
                           + (t >= 0 ? noteName (t) : String ("--")), dontSendNotification);
    }

    const int st = proc.engine.keyState.load();
    if (st == 1)
        keyLabel.setText (listeningText (proc.engine.keyProgress.load()), dontSendNotification);
    else if (st == 2)
    {
        proc.engine.keyState.store (0);
        const int r = proc.engine.keyResult.load(), alt = proc.engine.keyAlt.load();
        if (r < 0) keyLabel.setText (String::fromUTF8 ("Ses alg\xc4\xb1lanamad\xc4\xb1"), dontSendNotification);   // hic sesli nota yok
        else
        {
            keyLabel.setText (String (mt::keyName (r)) + (alt >= 0 ? " (alternatif: " + String (mt::keyName (alt)) + ")" : String()), dontSendNotification);
            setChoice ("key", r % 12); setChoice ("scale", r >= 12 ? 1 : 0);   // maskeyi timer doldurur
        }
    }
    piano.repaint();
    repaint();
}

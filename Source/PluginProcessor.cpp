#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static const StringArray kKeyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const int kMajor[7] = { 0, 2, 4, 5, 7, 9, 11 }, kMinor[7] = { 0, 2, 3, 5, 7, 8, 10 };

static bool inScale (int pc, int key, int scale)   // scale: 0 Major, 1 Minor, 2 Chromatic
{
    if (scale == 2) return true;
    const int rel = (pc - key + 12) % 12;
    for (int i = 0; i < 7; ++i)
        if ((scale == 0 ? kMajor[i] : kMinor[i]) == rel) return true;
    return false;
}

AudioProcessorValueTreeState::ParameterLayout MadenciProcessor::makeLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto txt = [] (const char* unit, bool offAtZero) {
        return AudioParameterFloatAttributes().withStringFromValueFunction ([=] (float v, int)
        {
            if (offAtZero && v < 0.5f) return String ("Off");
            return String (v, unit[0] == 'H' ? 1 : 0) + " " + unit;
        });
    };

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", kKeyNames, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", StringArray { "Major", "Minor", "Chromatic" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "range", 1 }, "Range",
           StringArray { "Bass", "Baritone", "Tenor", "Alto", "Mezzo Soprano", "Soprano", "Generic" }, mt::kGenericRange));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "smart", 1 }, "Smart Retune", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "ref", 1 }, "Reference Hz",
           NormalisableRange<float> (mt::kRefMin, mt::kRefMax, 0.1f), mt::kRefDefault, txt ("Hz", false)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Speed",
           NormalisableRange<float> (mt::kSpeedMin, mt::kSpeedMax, 0.1f, 0.5f), 20.f, txt ("ms", false)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "glide", 1 }, "Note Transition",
           NormalisableRange<float> (mt::kGlideMin, mt::kGlideMax, 1.f, 0.5f), 30.f, txt ("ms", false)));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "vibOn", 1 }, "Vibrato On", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "vibAmt", 1 }, "Vibrato",
           NormalisableRange<float> (0.f, 300.f, 1.f), 100.f, txt ("%", false)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "tolC", 1 }, "Tolerance Cents",
           NormalisableRange<float> (0.f, 50.f, 1.f), 0.f, txt ("c", true)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "tolT", 1 }, "Tolerance Time",
           NormalisableRange<float> (0.f, 200.f, 1.f), 0.f, txt ("ms", true)));
    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "n" + String (i), 1 }, "Note " + kKeyNames[i], inScale (i, 0, 0)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "formant", 1 }, "Formant", StringArray { "Corrected", "Not Corrected" }, mt::kFormantDefault));   // eski otomasyon sirasi bozulmasin: yeni parametreler hep sona
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "slideOn", 1 }, String::fromUTF8 ("TEKNO\xC5\x9E'A KAY"), false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "slideAmount", 1 }, "Slide Amount",
           NormalisableRange<float> (mt::kSlideAmtMin, mt::kSlideAmtMax, 1.f), mt::kSlideAmtDefault, txt ("c", false)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "slideTime", 1 }, "Slide Time",
           NormalisableRange<float> (mt::kSlideTimeMin, mt::kSlideTimeMax, 1.f), mt::kSlideTimeDefault, txt ("ms", false)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "slideDir", 1 }, "Slide Direction",
           StringArray { String::fromUTF8 ("Alttan"), String::fromUTF8 ("\xC3\x9C" "stten") }, 0));
    return l;
}

MadenciProcessor::MadenciProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", makeLayout())
{
    auto raw = [this] (const String& id) { return apvts.getRawParameterValue (id); };
    pRef = raw ("ref"); pSpeed = raw ("speed"); pGlide = raw ("glide"); pTolC = raw ("tolC"); pTolT = raw ("tolT");
    pVibAmt = raw ("vibAmt"); pVibOn = raw ("vibOn"); pSmart = raw ("smart"); pRange = raw ("range");
    pKey = raw ("key"); pScale = raw ("scale"); pFormant = raw ("formant");
    pSlideOn = raw ("slideOn"); pSlideAmt = raw ("slideAmount"); pSlideTime = raw ("slideTime"); pSlideDir = raw ("slideDir");
    for (int i = 0; i < 12; ++i) pNote[i] = raw ("n" + String (i));
    lastKey = (int) pKey->load(); lastScale = (int) pScale->load();
    startTimerHz (15);
}

void MadenciProcessor::timerCallback()
{
    const int k = (int) pKey->load(), s = (int) pScale->load();
    if (k == lastKey && s == lastScale) return;
    lastKey = k; lastScale = s;
    for (int i = 0; i < 12; ++i)
        if (auto* p = apvts.getParameter ("n" + String (i)))
            p->setValueNotifyingHost (inScale (i, k, s) ? 1.f : 0.f);
}

bool MadenciProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& o = l.getMainOutputChannelSet();
    return o == l.getMainInputChannelSet() && (o == AudioChannelSet::mono() || o == AudioChannelSet::stereo());
}

void MadenciProcessor::prepareToPlay (double sr, int)
{
    engine.prepare (sr);
    setLatencySamples (engine.latency());
}

void MadenciProcessor::processBlock (AudioBuffer<float>& buf, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int nCh = buf.getNumChannels(), nS = buf.getNumSamples();
    if (nCh == 0 || ! engine.ready()) return;

    mt::EngineParams p;
    p.refHz = pRef->load(); p.speed = pSpeed->load(); p.glideMs = pGlide->load();
    p.tolCents = pTolC->load(); p.tolMs = pTolT->load(); p.vibPct = pVibAmt->load();
    p.vibOn = pVibOn->load() > 0.5f; p.smart = pSmart->load() > 0.5f; p.range = (int) pRange->load(); p.formantCorrected = (int) pFormant->load() == 0;
    p.slideOn = pSlideOn->load() > 0.5f; p.slideCents = pSlideAmt->load(); p.slideMs = pSlideTime->load(); p.slideDir = (int) pSlideDir->load();
    p.mask = 0;
    for (int i = 0; i < 12; ++i) if (pNote[i]->load() > 0.5f) p.mask |= 1u << i;
    engine.set (p);

    float* const* ch = buf.getArrayOfWritePointers();
    float inP = 0.f, outP = 0.f;
    for (int s = 0; s < nS; ++s)
    {
        float x = 0.f;
        for (int c = 0; c < nCh; ++c) x += ch[c][s];
        x /= (float) nCh;                       // stereo -> mono
        const float y = engine.process (x);
        inP = jmax (inP, std::abs (x)); outP = jmax (outP, std::abs (y));
        for (int c = 0; c < nCh; ++c) ch[c][s] = y;   // cikis tum kanallara
    }
    if (inP > inPeak.load()) inPeak.store (inP);
    if (outP > outPeak.load()) outPeak.store (outP);
}

AudioProcessorEditor* MadenciProcessor::createEditor() { return new MadenciEditor (*this); }

void MadenciProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void MadenciProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
    lastKey = (int) pKey->load(); lastScale = (int) pScale->load();   // elle degistirilmis maske korunur
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MadenciProcessor(); }

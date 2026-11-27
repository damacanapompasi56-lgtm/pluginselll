#pragma once
// Arayüzün tek ayar dosyası: renk, font, boyut, görsel adları, metinler, yerleşim.
// Metinler UTF-8 (CMake'te MSVC /utf-8). Gösterirken Theme::str() kullan.
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>

namespace Theme {

// ---------------------------------------------------------------- pencere
constexpr int  kW = 1160, kH = 650;     // tasarım uzayı; pencere bununla orantılı ölçeklenir
constexpr bool kPixelStyle = false;     // piksel stili bayrağı (şimdilik sadece görsel örnekleme kalitesi)

// ---------------------------------------------------------------- renkler
inline const juce::Colour kMagenta { 0xffbf2ead };
inline const juce::Colour kLight   { 0xffc5c5c5 };
inline const juce::Colour kDark    { 0xff3f3f3f };
inline const juce::Colour kTeal    { 0xff1ac5af };

// ---------------------------------------------------------------- dosyalar
// BinaryData adı = dosya adı, nokta yerine "_" (Assets/ klasörü, CMakeLists.txt'te listeli)
constexpr const char* kFontRes      = "SFPRODISPLAYBOLD_OTF";
constexpr const char* kPickaxeRes   = "knobicinkazma_png";      // elmas kazma
constexpr const char* kMinerWideRes = "madenciyatayfoto_png";    // yatay madenci
constexpr const char* kMinerTallRes = "madencidikeyfoto_jpg";    // dikey madenci
constexpr float kPickaxeTipAngle = -0.78539816f;                 // knobicinkazma.png dönmeden: ucu sol-üst çapraza (12 yönünden saat yönü, rad) bakar

// ---------------------------------------------------------------- metinler
constexpr const char* kTitle       = "MADENCITUNE";
constexpr const char* kScale       = "sikimin keyfi";
constexpr const char* kIn          = "giriş";
constexpr const char* kOut         = "sikiş";
constexpr const char* kReference   = "sikik ayar";
constexpr const char* kKeyDetect   = "paylox'un kulağı";
constexpr const char* kRange       = "göt genişliği";
constexpr const char* kSpeed       = "sikicilik";
constexpr const char* kTransition  = "çakıcılık";
constexpr const char* kTolerance   = "kasma knk";
constexpr const char* kVibrato     = "zangır zangır";
constexpr const char* kCents       = "kuruşlar";
constexpr const char* kTime        = "taym";
constexpr const char* kOff         = "off";      // tolerance knob'larında minimum değer
constexpr const char* kFootLeft    = "teknik olarak öndeyim sizden";
constexpr const char* kFootMid     = "madamcı ee şey benim bi arkadaş var kıtar çalıyor";

// Yer tutucu örnek değerler (Paint taslağından). Gerçekleri parametrelerden gelecek.
constexpr const char* kSampleScale = "minor";
constexpr const char* kSampleFlat  = "b";
constexpr const char* kSampleSharp = "#";
constexpr const char* kSampleNote  = "A";
constexpr const char* kSampleKey   = "a minör";
constexpr const char* kSampleSpeed = "15.0";
constexpr const char* kSampleTrans = "15.0";
constexpr const char* kSampleRef   = "440";
constexpr const char* kSampleVib   = "100";
constexpr const char* kSampleOn    = "on";
constexpr const char* kSampleOff   = "off";
constexpr const char* kNoteLetters = "ABCDEFG";

// ---------------------------------------------------------------- yerleşim (kW x kH uzayında, Paint taslağından)
using R = juce::Rectangle<float>;
namespace L {
inline const R titleText   { 15, 8, 190, 34 },     titleIcon  { 205, 8, 34, 34 };
inline const R scaleLabel  { 50, 66, 300, 56 };
inline const R scaleBox    { 50, 125, 215, 50 },   accBox     { 298, 128, 105, 48 };
inline const R noteRow     { 62, 198, 285, 55 };
inline const R piano       { 20, 265, 750, 135 };
inline const R inLabel     { 447, 40, 60, 22 },    inMeter    { 468, 70, 14, 165 };
inline const R outLabel    { 785, 40, 60, 22 },    outMeter   { 808, 70, 14, 165 };
inline const R display     { 495, 40, 270, 150 },  centerTitle { 495, 205, 270, 36 };
inline const R refLabel    { 940, 6, 100, 24 },    refBox     { 1035, 5, 100, 40 };
inline const R minerWide   { 870, 50, 265, 150 };
inline const R speedLabel  { 800, 222, 115, 24 },  speedValue { 800, 252, 115, 38 },  speedKnob { 805, 292, 105, 98 };
inline const R transLabel  { 960, 222, 125, 24 },  transValue { 960, 252, 125, 38 },  transKnob { 975, 292, 105, 98 };
inline const R vibLabel    { 800, 402, 85, 24 },   vibToggle  { 888, 405, 40, 30 };
inline const R vibSlider   { 930, 400, 135, 36 },  vibValue   { 1078, 398, 70, 42 };
inline const R tolLabel    { 870, 458, 200, 24 };
inline const R centsLabel  { 868, 484, 90, 20 },   centsValue { 885, 504, 55, 28 },   centsKnob { 868, 530, 92, 92 };
inline const R timeLabel   { 985, 484, 90, 20 },   timeValue  { 1003, 504, 55, 28 },  timeKnob  { 988, 530, 92, 92 };
inline const R keyPhoto    { 35, 438, 105, 174 };
inline const R keyLabel    { 180, 435, 270, 40 },  keyResult  { 185, 505, 270, 40 };
inline const R rangeLabel  { 515, 428, 285, 34 },  rangeBox   { 515, 465, 285, 105 };
inline const R footLeft    { 160, 603, 300, 18 },  footMid    { 462, 603, 420, 18 };
}

// ---------------------------------------------------------------- yardımcılar
inline juce::String str (const char* utf8) { return juce::String::fromUTF8 (utf8); }

inline juce::Font font (float h)
{
    static const juce::Typeface::Ptr tf = []
    {
        int n = 0;
        const auto* d = BinaryData::getNamedResource (kFontRes, n);
        return d != nullptr ? juce::Typeface::createSystemTypefaceFor (d, (size_t) n) : juce::Typeface::Ptr();
    }();
    return tf != nullptr ? juce::Font (juce::FontOptions (tf).withHeight (h)) : juce::Font (juce::FontOptions (h));
}

// Dosya yoksa boş Image döner (çağıran yer tutucu çizer)
inline juce::Image image (const char* res)
{
    int n = 0;
    const auto* d = BinaryData::getNamedResource (res, n);
    return d != nullptr ? juce::ImageCache::getFromMemory (d, n) : juce::Image();
}

} // namespace Theme

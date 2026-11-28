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
// Normal palet (taslak) ve "hell palette" (speed < 0). Çizim kodu kMagenta/kLight/kDark/kTeal'i okur;
// bunlar canlı paletin değerleridir ve setHell() ile normal <-> hell arasında karışır (sadece ana thread).
inline const juce::Colour kNormalMagenta { 0xffbf2ead }, kNormalLight { 0xffc5c5c5 }, kNormalDark { 0xff3f3f3f }, kNormalTeal { 0xff1ac5af };
inline const juce::Colour kHellMagenta   { 0xffff0a3c }, kHellLight   { 0xffffd2bd }, kHellDark   { 0xff1c0707 }, kHellTeal   { 0xffff6a00 };
inline juce::Colour kMagenta = kNormalMagenta, kLight = kNormalLight, kDark = kNormalDark, kTeal = kNormalTeal;
// t: 0 = normal, 1 = hell
inline void setHell (float t)
{
    kMagenta = kNormalMagenta.interpolatedWith (kHellMagenta, t);
    kLight   = kNormalLight.interpolatedWith (kHellLight, t);
    kDark    = kNormalDark.interpolatedWith (kHellDark, t);
    kTeal    = kNormalTeal.interpolatedWith (kHellTeal, t);
}
constexpr float kShakeMs = 450.f, kShakeAmp = 7.f;   // negatif speed'e girerken sallanma süresi / en büyük kayma (tasarım px)
constexpr float kHellInMs = 1200.f, kHellOutMs = 900.f;   // palet geçiş süreleri
constexpr int   kHellFps = 30;

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
constexpr const char* kOn          = "on";       // vibrato düğmesi
constexpr const char* kMinus       = "-";        // reference kontrolü
constexpr const char* kPlus        = "+";
constexpr float       kRefStep     = 1.f;        // reference artı/eksi adımı (Hz)
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

// Parça 3A: scale / formant / key detect metinleri
constexpr const char* kFlat        = "b";
constexpr const char* kSharp       = "#";
constexpr const char* kScaleNames[3]  = { "major", "minor", "chromatic" };      // "scale" parametresi sırası: Major, Minor, Chromatic
constexpr const char* kFormant     = "formant";
constexpr const char* kFormantOpts[2] = { "corrected", "not corrected" };       // "formant" parametresi sırası: Corrected, Not Corrected
constexpr const char* kKeyNames[12]   = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };   // "key" parametresi sırası
constexpr const char* kMajor       = "majör";
constexpr const char* kMinor       = "minör";
// Parça 6B
constexpr float kPickaxePivot = 0.4227f;   // knobicinkazma.png'nin ağırlık merkezi (görsel genişliğinin oranı, x ve y aynı); dönme bu noktadan
constexpr float kHoverAlpha   = 0.25f;     // hover dolgusu (basılıyken 2x)

// Parça 5: tooltip metinleri ve boyutları
constexpr int   kTipFont    = 15;       // px (tooltip ekran uzayında, pencere ölçeğinden bağımsız)
constexpr int   kTipMaxW    = 280;      // px, kutunun en geniş hali
constexpr int   kTipDelayMs = 400;
constexpr const char* kTipKeyDetect  = "Paylox'un harika kulaklarıyla sesini dinlemene izin ver. Sana hemen vokalinin hangi tonda okunduğunu söyler. Başlatmak için tıkla.";
constexpr const char* kTipRange      = "Vokalinin ses türünü seç. Tuner o türün nota aralığının dışına taşmaz.";
constexpr const char* kTipSpeed      = "Düzeltmenin ne kadar hızlı ve sert uygulanacağı (ms). Düşük değer notaya anında yapışır, yani robotik 'tune' sesi; yüksek değer yavaş ve doğal çeker. Sıfırın altı daha da sert.";
constexpr const char* kTipTransition = "Bir notadan diğerine geçişin ne kadar yumuşak olacağı (ms). Düşükse nottan notaya sekersin, yüksekse kayarak geçersin. Hız çok sert ayarlıysa etkisi azalır.";
constexpr const char* kTipTolerance  = "Tuner'ın ne zaman karışacağını belirler. Kuruşlar: hedef notadan kaç cent sapmaya göz yumulacağı. Taym: tolerans dışında ne kadar kalınca düzeltmenin başlayacağı.";
constexpr const char* kTipCents      = "Hedef notadan en fazla kaç cent sapmaya kadar dokunulmayacağı. 'off': küçücük sapma bile düzeltilir.";
constexpr const char* kTipTime       = "Tolerans dışında en az kaç ms kalırsa düzeltmenin devreye gireceği. 'off': bekleme yok.";
constexpr const char* kTipVibrato    = "Vibrato miktarı. Açıkken sesindeki doğal titreşim korunur: yüzde 100 olduğu gibi bırakır, altı azaltır, üstü abartır. Kapalıyken titreşim de düzeltilip düzleşir.";
constexpr const char* kTipRef        = "Referans frekansı: A notasının kaç Hz olduğu (A=440). Parçanın geri kalanı farklı akortluysa buradan uydur.";
constexpr const char* kTipScale      = "Skalayı (major, minor, chromatic) ve ana notayı seç. Tuner sesini sadece bu skaladaki notalara çeker.";
constexpr const char* kTipFormant    = "Vokalin tınısını korur: 'corrected' perde değişse de karakteri sabit tutar, 'not corrected' tınının perdeyle birlikte kaymasına izin verir.";
constexpr const char* kTipIn         = "Tuner'a giren sesin seviyesi. Magenta'ya vuruyorsa patlamaya yakın, kaynağın sesini kıs.";
constexpr const char* kTipOut        = "Tuner'dan çıkan sesin seviyesi. Magenta'ya vuruyorsa patlamaya yakın, kanalın sesini kıs.";
constexpr const char* kTipDisplay    = "Şu an söylediğin (veya hedeflenen) nota. Halka, hedeften kaç cent saptığını gösterir: teal tam tutturdun, magenta uzaklaştın.";
constexpr const char* kTipPiano      = "Tuner'ın kullanabileceği notalar. Teal tuşlar açık; bir tuşa tıklayarak o notayı aç ya da kapa.";

// Parça 4A: nota göstergesi + seviye metreleri
constexpr float kPitchMaxCents = 50.f;   // ibre/renk bu sapmada uca varır
constexpr float kPitchSweep    = 2.4f;   // ibrenin 12 yönünden her yana maksimum açısı (rad)
constexpr float kMeterFloorDb  = -60.f;  // metre tabanı
constexpr float kMeterFallDb   = 1.5f;   // metre düşüşü, 30 Hz karede dB

// Parça 3B
constexpr const char* kRangeNames[7] = { "bass", "baritone", "tenor", "alto", "mezzo soprano", "soprano", "generic" };   // "range" parametresi sırası
constexpr const char* kHz          = "hz";
constexpr float       kPhotoFrame  = 3.f;      // dikey fotoğraf çerçevesi kalınlığı/iç boşluğu (tasarım px)
constexpr const char* kNoDetect    = "-";        // key detect: sonuç yok

// ---------------------------------------------------------------- yerleşim (kW x kH uzayında, Paint taslağından)
using R = juce::Rectangle<float>;
namespace L {
inline const R titleText   { 15, 8, 190, 34 },     titleIcon  { 205, 8, 34, 34 };
inline const R scaleLabel  { 50, 66, 300, 56 };
inline const R scaleBox    { 50, 125, 230, 50 },   accBox     { 298, 128, 105, 48 };
inline const R noteRow     { 62, 198, 285, 55 };
inline const R piano       { 20, 265, 750, 135 };
inline const R inLabel     { 455, 40, 40, 22 },    inMeter    { 468, 70, 14, 150 };
inline const R outLabel    { 785, 40, 60, 22 },    outMeter   { 808, 70, 14, 150 };
inline const R display     { 495, 40, 270, 150 },  centerTitle { 495, 205, 270, 36 };
inline const R refLabel    { 940, 6, 90, 24 },    refBox     { 1035, 5, 100, 40 };
inline const R minerWide   { 870, 50, 265, 150 };
inline const R speedLabel  { 800, 222, 115, 26 },  speedValue { 800, 250, 115, 40 },  speedKnob { 805, 292, 105, 98 };
inline const R transLabel  { 960, 222, 125, 26 },  transValue { 960, 250, 125, 40 },  transKnob { 975, 292, 105, 98 };
// zangır zangır: etiket üstte, altında tek satır (on/off, kaydırıcı, değer); büyük yazı yan yana sığmadı
inline const R vibLabel    { 800, 394, 200, 26 },  vibToggle  { 800, 424, 62, 38 };
inline const R vibSlider   { 870, 422, 188, 42 },  vibValue   { 1066, 422, 82, 42 };
inline const R tolLabel    { 870, 466, 200, 26 };
inline const R centsLabel  { 868, 492, 100, 22 },  centsValue { 882, 514, 64, 34 },   centsKnob { 868, 550, 92, 92 };
inline const R timeLabel   { 984, 492, 100, 22 },  timeValue  { 1002, 514, 64, 34 },  timeKnob  { 988, 550, 92, 92 };
inline const R formantLabel { 170, 548, 98, 40 },  formantBox  { 272, 548, 178, 40 };   // key detect sonucunun hemen altı
inline const R rangeChoice { 530, 472, 255, 46 },  rangeInfo   { 530, 524, 255, 36 };   // rangeBox'un içi
inline const R keyPhoto    { 35, 438, 105, 174 };
inline const R keyLabel    { 180, 435, 270, 40 },  keyResult  { 185, 505, 270, 40 };
inline const R rangeLabel  { 515, 428, 285, 34 },  rangeBox   { 515, 465, 285, 105 };
inline const R footLeft    { 160, 603, 300, 18 },  footMid    { 462, 603, 400, 18 };
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

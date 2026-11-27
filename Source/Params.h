#pragma once
// Saf C++ sabitleri (JUCE yok). Min/max değerleri yalnızca burada tanımlı.

namespace mt {

constexpr float kSpeedMin = -10.f;     // <0: "ekstra sert bölge"
constexpr float kSpeedMax = 400.f;
constexpr float kGlideMin = 0.f;       // Note Transition (ms)
constexpr float kGlideMax = 400.f;
constexpr float kRetuneMaxMs = 400.f;  // motorda etkin retune süresi her zaman 0..400
constexpr int   kFormantDefault = 0;   // Formant: 0 = Corrected (formantlar sabit), 1 = Not Corrected (formantlar perdeyle kayar)
constexpr float kFormantSmoothMs = 50.f;   // Corrected <-> Not Corrected geçiş yumuşatma süresi


constexpr float kRefMin = 415.f, kRefMax = 466.f, kRefDefault = 440.f;
constexpr float kMinDetectHz = 70.f;   // en düşük Range (Generic) -> sabit gecikme buna göre
constexpr float kKeySeconds = 8.f;     // Key Detect dinleme süresi

struct RangeHz { float lo, hi; };
// Bass, Baritone, Tenor, Alto, Mezzo Soprano, Soprano, Generic
constexpr RangeHz kRanges[7] = { {82, 330}, {110, 440}, {130, 523}, {175, 698},
                                 {220, 880}, {262, 1047}, {70, 1100} };
constexpr int kGenericRange = 6;

// Krumhansl-Schmuckler profilleri
constexpr float kKsMajor[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
constexpr float kKsMinor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

} // namespace mt

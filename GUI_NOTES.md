# GUI_NOTES.md

DSP/parametre mantığına dokunulmaz. Sadece arayüz. Her parça başında bu dosya okunur, sonunda güncellenir.

## Sabit kurallar
- Tüm yazı ve sayılar `Theme::font()` ile (SFPRODISPLAYBOLD.OTF). Başka font yok.
- Renkler, metinler, görsel adları, yerleşim kutuları sadece `Source/Theme.h`'da. Başka dosyaya gömme.
- Görseller sadece `Assets/` içindekiler. İnternet/stok/üretilmiş görsel yok.
- Paint taslağı yerleşimi referans; koordinatlar `Theme::L` içinde (1160x650 tasarım uzayı).
- Her parça sonunda bu dosya güncellenir; yarım kalırsa "kaldığım yer" yazılır.

## PARÇA 1/6: Tema, varlıklar, yerleşim iskeleti  (TAMAMLANDI)
Yapılanlar:
- `Source/Theme.h` (yeni): renkler (kMagenta/kLight/kDark/kTeal), pencere 1160x650, `kPixelStyle` bayrağı (varsayılan false), kaynak adları, TÜM metinler (başlık, etiketler, iki alt yazı), yer tutucu örnek değerler (`kSample*`), yerleşim kutuları `Theme::L::*`, yardımcılar `Theme::str()`, `Theme::font(h)`, `Theme::image(res)`.
- `Assets/` (yeni): SFPRODISPLAYBOLD.OTF, knobicinkazma.png, madenciyatayfoto.png, madencidikeyfoto.jpg. Taslak JPG'si gömülmedi (sadece referans).
- `CMakeLists.txt`: `juce_add_binary_data(MadenciAssets ...)`, hedefe link, MSVC `/utf-8` eklendi (Türkçe UTF-8 metinler için).
- `Source/PluginEditor.h/.cpp`: eski editör tamamen atıldı. Yeni iskelet: 1160x650, yeniden boyutlanabilir, sabit en-boy oranı (yarım..iki kat), paint() `addTransform(scale)` ile tasarım uzayında çiziyor. Kontrol yok; yer tutucu çerçeveler, etiketler, alt yazılar, iki madenci fotoğrafı ve başlık kazması çiziliyor.

Atlananlar / sonraki parçalar:
- Hiçbir kontrol yok, eski editördeki bağlantılar (attachment, piyano, ölçer, key detect, timer) kaldırıldı. Parametre bağlama sonraki parçalarda `Theme::L` kutularına bağlanacak.
- Elmas kazma görseli şu an sadece başlık ikonunda; knob içi çizimi sonraki parçada.
- Taslakta elle çizilmiş dalgalı kutu/spiky "A" şekilleri yer tutucu dikdörtgen/elips; gerçek görünümü sonraki parçalarda.
- `kPixelStyle` şimdilik yalnızca görsel örnekleme kalitesini değiştiriyor.
- Formant seçici (eski editörde vardı) taslakta yok; sonraki parçalarda yeri/kararı belirlenmeli.

Kaldığım yer: yok, parça tam.

## PARÇA 2A: Elmas kazma knob'ları  (TAMAMLANDI, derlenmedi)
Yapılanlar:
- `Source/KazmaKnob.h` (yeni, sadece header, CMake değişmedi): tek yeniden kullanılabilir bileşen. İçinde rotary Slider (çizimi kapalı, sadece fare) + değer kutusu Label + çentik/halka çizimi + `SliderAttachment`. Kullanım: `KazmaKnob (apvts, paramId, ondalık, minimumdaOffYaz)`; yerleşim: `place (Theme::L değer kutusu, Theme::L knob kutusu, pencereÖlçeği)`.
- Kazma: `knobicinkazma.png` tek görsel, ibre yok. Boyut değişince bir kez `rescaled` ile cache'lenir, her karede sadece döndürülür. Ucu değeri gösterir: dönmemiş görselin ucu sol-üst çapraza baktığı için `Theme::kPickaxeTipAngle = -pi/4`; dönüş = değer açısı - bu açı. Açı aralığı 7:30 -> 4:30 (1.25pi..2.75pi), değer->açı `valueToProportionOfLength` (parametre skew'ını da izler).
- Aralık hardcode değil: SliderAttachment parametreden alır (speed -10..400, skew dahil).
- Değer kutusu: çift tık = elle giriş (Enter ile uygular; "off" yazılabilir, geçersiz girişte eski değer döner), shift+sürükle ince ayar (hassasiyet 250 -> 1500, sürükleme başında seçilir).
- Tolerance (tolC, tolT): minimum değerde "off" (`Theme::kOff`).
- Bağlar: speed->"speed", note transition->"glide", cents->"tolC", time->"tolT".
- `Source/Theme.h`: `kPickaxeTipAngle` ve `kOff` eklendi (başka değişiklik yok).
- `Source/PluginEditor.h/.cpp`: 4 knob üye, `resized()` ile `Theme::L` kutularına yerleşiyor; bu 4 knob'un yer tutucu çerçeve/örnek değer çizimi paint()'ten silindi (etiketler kaldı).

Atlananlar / riskler:
- Derleme yapılamadı (JUCE kaynağı ve ağ yok). İlk CI build'inde derleme hatası çıkabilir.
- Shift ince ayarı sürükleme ortasında shift'e basınca devreye girmez.
- Değer kutusu / halka / çentik renk ve kalınlıkları benim seçimim (taslakta elle çizilmiş dalgalı kutu); gerçek şekil sonraki parçada.
- Kazma görselinin dönme merkezi görselin ortası; baş sol-üstte olduğu için ibre merkezden hafif kayık görünebilir.

Kaldığım yer: yok. Sıradaki: Parça 2B (vibrato, reference).

## PARÇA 2B: Vibrato ve reference  (TAMAMLANDI, derlenmedi)
Yapılanlar:
- `Source/VibRefControls.h` (yeni, sadece header, namespace `vr`): `ValueBox` (çerçeveli değer kutusu, çift tık = elle giriş), `VibToggle` (on/off, "vibOn"), `VibSlider` (yatay, çizgi + kol olarak elmas kazma, başı yukarı; fare mantığı Slider'ın kendisi), `VibratoControl` (üçünü `vibToggle/vibSlider/vibValue` kutularına oturtur, "vibAmt"), `RefControl` ("-  440  +", "ref"; artı/eksi = `Theme::kRefStep`, ortadaki değer elle girilebilir).
- Aralıklar parametreden (SliderAttachment). Kazma çizimi cache'li ölçekleme (2A ile aynı yöntem, `vr::drawPick`).
- `Source/Theme.h`: `kOn`, `kMinus`, `kPlus`, `kRefStep` (=1 Hz) eklendi.
- `Source/PluginEditor.h/.cpp`: `vibrato`, `refCtl` üyeleri, `resized()` yerleşimi; bu kontrollerin yer tutucu çerçeve/örnek değer çizimi paint()'ten silindi (etiketler kaldı).
- `KazmaKnob.h` değişmedi; değer kutusu mantığı `ValueBox` ile kopya (ileride KazmaKnob `ValueBox` kullanacak şekilde sadeleşebilir).

Atlananlar / riskler:
- vibAmt parametresi 0..300 (DSP'ye dokunulmadığı için); kutu/kaydırıcı 0..300 gösterir, istenen 0-100 değil. 0-100 isteniyorsa parametre aralığı değişmeli (DSP kararı).
- Reference adımı 1 Hz seçildi (parametre ince adımı 0.1); isteniyorsa `Theme::kRefStep`.
- Kaydırıcı kolu uçlarda birkaç px kırpılabilir.

Kaldığım yer: yok. Sıradaki: Parça 3.

## Notlar sonraki parçalar için
- Alt kontrol eklerken `setBounds` için `Theme::L::x` kutusunu `getWidth()/Theme::kW` ile ölçekle.
- Yeni dosya adı eklenirse hem `Assets/`, hem CMake `SOURCES`, hem Theme.h; BinaryData adı = dosya adı, nokta yerine `_`.

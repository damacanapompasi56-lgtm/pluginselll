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

## PARÇA 3A: Scale, piyano, key detect, formant  (TAMAMLANDI, derlenmedi)
Yapılanlar:
- `Source/ScaleControls.h` (yeni, sadece header, namespace `sc`): `ChoiceBox` (açılır kutu, scale + formant için, popup fontu/renkleri Theme), `KeyPicker` (bemol/diyez + A..G, "key"), `Piano` (n0..n11, tıkla aç/kapa), `KeyDetectView` (etiket + sonuç, Engine atomikleri).
- Seçili öğe `Theme::kTeal` (#1ac5af) dolgulu. Hepsi `ParameterAttachment` ile (otomasyon/preset değişince güncellenir, tıklama gesture olarak yazılır).
- Scale: "scale" parametresi (major/minor/chromatic) açılır kutu. Key: "key" 0..11 tek sayı; harf + bemol/diyez bunun üstüne yazım olarak hatırlanır (C# <-> Db aynı değer, hangisi seçildiyse o vurgulanır). Bemol/diyez tekrar tıklanınca kalkar.
- Piyano: 10 beyaz tuş (C..E, taslaktaki 7 siyah tuşa uysun diye), aktif perde sınıfları teal. Tıklama "n0..n11"i çevirir. Key/scale değişince DSP tarafındaki timer notaları yeniden doldurur, piyano bunu izler.
- Key detect: etiket+sonuç alanına tıkla = `engine.keyRequest` (8 sn dinler). Dinlerken "%NN", bitince "a minör" / "c# majör"; sonuç yoksa "-". Algılanan ton Key/Scale'e otomatik uygulanmaz.
- Formant: "formant" parametresi, ChoiceBox, key detect sonucunun hemen altında (`Theme::L::formantLabel/formantBox`, 182x34; başka öğe kaymadı, sığdı). Seçenekler `Theme::kFormantOpts`: "corrected" / "not corrected".
- `Theme.h`: kFlat, kSharp, kScaleNames, kFormant, kFormantOpts, kKeyNames, kMajor, kMinor, kNoDetect, L::formantLabel/formantBox eklendi.
- `PluginEditor.h/.cpp`: yeni üyeler, resized() yerleşimi; scale/harf/piyano/key detect yer tutucu çizimleri paint()'ten silindi (scale ve formant etiketi kaldı).

Atlananlar / riskler:
- Derlenmedi. CI'de ilk derlemede hata çıkabilir (ParameterAttachment, PopupMenu::setLookAndFeel, constexpr static dizi).
- Zip'te git geçmişi ve eski editör yok; formant seçenekleri DSP tarafından (Params.h/PluginProcessor: Corrected, Not Corrected). Eski editördeki yazılar farklıysa `Theme::kFormantOpts` değişmeli.
- Key detect'i başlatan görünür bir düğme yok (taslakta yok): alana tıklamak gerekiyor, kullanıcı bunu kendiliğinden bilmez.

Kaldığım yer: yok. Sıradaki: Parça 3B.

## PARÇA 3B: Range ve dikey fotoğraf  (TAMAMLANDI, derlenmedi)
Yapılanlar:
- `Source/RangeControls.h` (yeni, sadece header, namespace `rc`): `RangeControl` = teal çerçeveli kutu + içinde `sc::ChoiceBox` ("range": bass, baritone, tenor, alto, mezzo soprano, soprano, generic) + seçili türün alt/üst sınırı "82 - 330 hz" (`mt::kRanges`, Params.h'dan okunur, hardcode yok).
- Dikey madenci fotoğrafı (`madencidikeyfoto.jpg`) `L::keyPhoto` kutusunda, magenta çerçeveli (`Theme::kPhotoFrame`), oran korunup kırpılıyor.
- `Theme.h`: kRangeNames, kHz, kPhotoFrame, L::rangeChoice/rangeInfo eklendi. `PluginEditor.h/.cpp`: `rangeCtl` üyesi, yerleşim; range yer tutucu çerçevesi paint()'ten silindi.

Atlananlar / riskler:
- Parametre serbest alt/üst sınır değil, 7 hazır tür; kullanıcı Hz sınırlarını ayrı ayrı seçemez (DSP kararı). Sınırlar sadece gösterilir.
- Derlenmedi; `RangeControl` içindeki iç içe ChoiceBox yerleşimi (yerel koordinat) CI'de/gözle kontrol edilmeli.

Kaldığım yer: yok. Sıradaki: Parça 4.

## PARÇA 4A: Orta nota göstergesi ve giriş/çıkış metreleri  (TAMAMLANDI, derlenmedi)
Yapılanlar:
- `Source/PitchMeters.h` (yeni, sadece header, namespace `pm`): `PitchDisplay` (büyük harf nota + halka/ibre), `LevelMeter` (dikey çubuk). Processor'a/Engine'e EKLEME YOK: veri zaten atomic vardı (`Engine::dispMidi/dispTarget`, `MadenciProcessor::inPeak/outPeak`).
- Nota: hedef nota (`dispTarget`) varsa o, yoksa algılanan perdeye en yakın nota; perde yoksa "-". Sapma = (algılanan - gösterilen nota) cent, ±50'ye kırpılır. Ibre 12 yönünden ± `kPitchSweep`; renk sapma yokken kTeal, 50 cent'te kMagenta'ya geçer. Ibre 30 Hz'de üstel yumuşatılır, sadece görünür değişimde repaint.
- Metreler: -60..0 dB, anlık yükselir, `kMeterFallDb` ile düşer; -3 dB üstü kMagenta. Peak her karede `exchange(0)` ile okunur/sıfırlanır.
- `Theme.h`: kPitchMaxCents, kPitchSweep, kMeterFloorDb, kMeterFallDb eklendi. `PluginEditor.h/.cpp`: üç yeni üye, yerleşim; display/metre yer tutucuları ve örnek nota paint()'ten silindi ("madencitune" yazısı ve giriş/çıkış etiketleri kaldı).

Atlananlar / riskler:
- Derlenmedi. `exchange(0)` sadece tek editör açıkken doğru ölçer; iki editör aynı anda açıksa peak'i paylaşırlar. Audio thread'in max yazması (load+store) nadiren bir peak'i kaçırabilir, görsel olarak önemsiz.
- Nota harfi sapma ibresi giriş perdesini hedefe göre gösterir (düzeltilmiş çıkışı değil); çıkış sapması istenirse `dispCorr` eklenebilir.

Kaldığım yer: yok. Sıradaki: Parça 4B.

## PARÇA 4B: Yatay madenci fotoğrafı, CCTV görünümü  (TAMAMLANDI, derlenmedi)
Yapılanlar:
- `Source/CctvView.h` (yeni, sadece header, namespace `cctv`): `CctvView` `L::minerWide` kutusunda. Fotoğraf gri tona çevrilip kutuya (kırparak) oturtulur; tarama çizgileri + vinyet tek ARGB katmanı olarak boyut/DPI değişince BİR KEZ üretilir. Her karede sadece: hazır görseller, 4 hazır noise karosundan biri (rastgele kaydırma), parlaklık titremesi (siyah alpha ≤ 0.07), ara sıra 1 px kayma, kTeal %8 renk tonu, magenta çerçeve. Timer 15 Hz, yıkıcıda durur.
- `PluginEditor.h/.cpp`: `cctvView` üyesi, yerleşim; `minerWide` görseli ve photo() çağrısı editörden kaldırıldı (`minerTall` duruyor).

Atlananlar / riskler:
- Derlenmedi. Katman üretimi piksel döngüsüyle (≈0.15 Mpx @2x) boyut değişince bir kez çalışır; pencere sürüklenerek boyutlanırken her adımda yeniden üretilir, takılma olursa boyutlandırma bitince üretmek gerekir.
- Noise tane boyutu mantıksal px (retina'da iri); 15 fps'te tam kutu repaint her karede yapılıyor.

Kaldığım yer: yok. Sıradaki: Parça 5.

## PARÇA 5: Tooltip'ler  (TAMAMLANDI, derlenmedi)
Yapılanlar:
- `Source/Tooltips.h` (yeni, sadece header, namespace `tt`): `Tip<T>` (herhangi bir bileşeni `SettableTooltipClient` ile sarar; ilk kurucu argümanı Theme metni) ve `TipLaf` (Theme fontu, kDark dolgu + kTeal çerçeve, 280 px'e kadar sarmalı kutu). Editörde tüm kontroller `Tip<>` ile sarıldı, `juce::TooltipWindow` (400 ms, editörün çocuğu) + `TipLaf` bağlandı.
- Etiket alanları (scale, formant, range, reference, speed, transition, vibrato, tolerance, kuruşlar, taym, giriş, sikiş) için görünmez `Tip<Component>` bölgeleri; `Theme::L` kutularına oturur.
- `Theme.h`: tüm tooltip metinleri `kTip*`, boyutlar `kTipFont/kTipMaxW/kTipDelayMs`. Scale tooltip'i scale kutusu, harf/bemol/diyez ve etiket üzerinde aynı. Key detect metnine "Başlatmak için tıkla." eklendi (3A'daki "kullanıcı tıklamayı bilmez" riskine karşı).
- Alt yazı kontrolü: `footLeft` x=160, `footMid` x=462, ikisi y≈603-621, font 10 px (SF Pro Bold'da genişlikleri ≈131 ve ≈223 px; taslakta ≈219 ve ≈375 px, yani taslaktan küçük, istenen "çok küçük"). Taslakta ikinci yazı x=463'ten başlıyordu, kutu içinde ORTALANMIŞTI (başlangıcı ≈560'a kayıyordu): sola hizalandı.

Atlananlar / riskler:
- Derlenmedi. `TooltipWindow` çocuk bileşenin üstünde ebeveynin tooltip'ini bulmasına güveniyor (JUCE davranışı, test edilmedi); açılan kutu editör sınırlarına kırpılır (alt/sağ kenarda yukarı/sola kayar). ChoiceBox açılır menüsü açıkken tooltip çıkmaz.
- Tooltip yazısı pencere ölçeğinden bağımsız sabit 15 px; pencere küçültülünce kutu görece büyük kalır. DSP'ye uyan anlatımlar (speed/glide/tolerans/vibrato) Engine.h okunarak yazıldı, glide metni "sert hızda etkisi azalır" ayrıntısını da içerir.

Kaldığım yer: yok. Sıradaki: Parça 6.

## PARÇA 6A: Negatif speed modu  (TAMAMLANDI, derlenmedi)
Yapılanlar:
- `Theme.h`: `kMagenta/kLight/kDark/kTeal` artık `const` değil, CANLI palet; normal (`kNormal*`) ve "hell palette" (`kHell*`: kızıl/turuncu/yanık siyah/sıcak krem) sabitleri ve `setHell(t)` eklendi. Hiçbir çizim kodu değişmedi (hepsi zaten bu adları okuyor). Süre/genlik sabitleri: `kShakeMs`, `kShakeAmp`, `kHellInMs`, `kHellOutMs`, `kHellFps`.
- `PluginEditor`: "speed" parametresine `ParameterAttachment`; `< 0` olunca ~450 ms sallanma (çocuklara `setTransform` + arka plan kayması, azalan genlik), ardından 1.2 sn'de palet smoothstep ile hell'e geçer; `>= 0`'a dönünce sallanmadan 0.9 sn'de normale döner. Timer (30 Hz) sadece geçiş sürerken çalışır, bitince `stopTimer()`; yıkıcıda da durur ve palet normale alınır. Editör negatif speed ile açılırsa animasyonsuz doğrudan hell.
- Label renkleri (kurucuda set edilmişlerdi) geçiş karesinde `recolourLabels` ile yenilenir. `ScaleControls.h`: açılır menü renkleri kurucudan çıkıp menü açılırken set ediliyor (güncel palet).

Atlananlar / riskler:
- Palet global (`inline` değişken): aynı işlemde birden fazla plugin örneği/editörü açıksa hepsi aynı paleti paylaşır; birinin speed'i negatifse diğerinin editörü de hell'e boyanır, birinin editörü kapanınca palet normale döner. Örnek başına palet ancak tüm çizim kodunu yeniden yazarak mümkün.
- Geçiş karelerinde editör tam yeniden çizilir (30 Hz, ~1 sn). Sallanma gerçek pencereyi oynatmaz, içeriği oynatır; kenarlar arka planla dolu kalır. Derlenmedi.

Kaldığım yer: yok. Sıradaki: Parça 6B.

## PARÇA 6B: Cila  (TAMAMLANDI, derlenmedi)
Yapılanlar:
- Hover/basılı durumları: açılır kutular, bemol/diyez/harf hücreleri, piyano tuşları, vibrato düğmesi, reference -/+ yarıları, key detect sonucu (teal), knob halkası (hover/sürüklerken parlar ve kalınlaşır). Dolgu `Theme::kHoverAlpha`.
- Yerleşim kontrolü (Theme::L çakışma taraması): giriş etiketi göstergeye biniyordu (455..495'e daraltıldı, metre ekseniyle ortalı), giriş/çıkış metreleri 165 -> 150 yüksekliğe indi (sikicilik etiketine biniyordu), reference etiketi kutuya değiyordu (genişlik 100 -> 90), alt orta yazı kutusu kuruş knob'una biniyordu (420 -> 400). Kasıtlı iç içe olanlar (range kutusu içeriği, değer kutusu/knob 2 px) bırakıldı.
- CctvView: ilk üretim hemen, sonraki boyut değişimleri boyut 2 timer karesi (~130 ms) sabit kalınca; arada eski görsel gerilerek çizilir (boyutlandırma sürerken piksel döngüsü çalışmaz).
- Kazma: PNG'nin görünür piksel ağırlık merkezi (0.4227, 0.4227) görsel ortasından (0.5) kayıktı; dönme artık bu noktadan (`Theme::kPickaxePivot`, KazmaKnob + VibSlider).

Atlananlar / riskler:
- Derlenmedi; hover için `slider.addMouseListener(this,false)` ve `Slider::isMouseOverOrDragging` JUCE davranışına güveniyor. VibSlider kazması hover'da değişmiyor. Boyutlandırma sırasında CCTV birkaç kare bulanık/gergin görünebilir.
- Pivot ağırlık merkezi (kazma baş tarafı yaklaşık merkezde), sap ucu değil; sap ucundan döndürmek istenirse `kPickaxePivot` farklı x/y ile ayrılmalı.

Kaldığım yer: yok. Sıradaki: yok (6 parça tamam).

## Tasarımı sonradan nasıl değiştiririm
- Renk: `Source/Theme.h` en üst, `kNormalMagenta/Light/Dark/Teal` (normal) ve `kHell*` (speed < 0 paleti); sadece hex değeri değiştir.
- Font: `Assets/SFPRODISPLAYBOLD.OTF`'yi aynı adla değiştir; farklı adsa `Theme::kFontRes` (ad, nokta yerine `_`) ve `CMakeLists.txt` `juce_add_binary_data` listesini güncelle.
- Metin: `Theme.h` "metinler" bölümü (başlık, etiketler, alt yazılar `kFootLeft/kFootMid`) ve `kTip*` (tooltip'ler); başka dosyada metin yok.
- Görsel: `Assets/` içindeki dosyayı aynı adla değiştir; yeni ad verirsen `Theme::k*Res` + CMake listesi. Kazma değişirse `kPickaxeTipAngle` ve `kPickaxePivot` da.
- Piksel bayrağı: `Theme::kPixelStyle = true` görselleri yumuşatmadan (piksel piksel) örnekler: kazma, madenciler, CCTV.
- Yerleşim: `Theme::L` kutuları (1160x650 tasarım uzayı, her şey bununla ölçeklenir).

## Notlar sonraki parçalar için
- Alt kontrol eklerken `setBounds` için `Theme::L::x` kutusunu `getWidth()/Theme::kW` ile ölçekle.
- Yeni dosya adı eklenirse hem `Assets/`, hem CMake `SOURCES`, hem Theme.h; BinaryData adı = dosya adı, nokta yerine `_`.

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

## Notlar sonraki parçalar için
- Alt kontrol eklerken `setBounds` için `Theme::L::x` kutusunu `getWidth()/Theme::kW` ile ölçekle.
- Yeni dosya adı eklenirse hem `Assets/`, hem CMake `SOURCES`, hem Theme.h; BinaryData adı = dosya adı, nokta yerine `_`.

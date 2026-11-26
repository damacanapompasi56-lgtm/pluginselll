#pragma once
// Key Detect. Saf C++ (JUCE yok). Ses yolundan ve Engine parametrelerinden (Key/Scale maskesi, Range, Ref,
// düzeltme) tamamen bağımsız: kendi decimate tamponu ve kendi perde izleyicisi var, girişe dokunmaz.
// Akış: ham örnek -> decimate -> MPM perde (sadece voiced + yüksek güven) -> nota SEGMENTLERİ
//   -> cent kalıntılarının dairesel ortalaması (tuning offset) -> 12'li histogram (ağırlık = süre * güven)
//   -> 24 ton Krumhansl-Kessler korelasyonu + tonik ipuçları (en uzun notalar, cümle başı/sonu)
//   -> en iyi 2 aday + yüzde güven (softmax).
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace mt {

struct KeyGuess { int best = -1, alt = -1, bestPct = 0, altPct = 0; };   // 0..11 major, 12..23 minor; best=-1: hiç sesli nota yok

inline std::string keyName(int k)
{
    static const char* N[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return std::string(N[((k % 12) + 12) % 12]) + (k < 12 ? " major" : " minor");
}

// UTF-8 döner (JUCE'ta juce::String::fromUTF8 ile göster). "belirsiz" yok: hep en iyi tahmin + yüzde.
inline std::string keyLabel(const KeyGuess& g)
{
    if (g.best < 0) return "Ses alg\xC4\xB1lanamad\xC4\xB1";
    return keyName(g.best) + " %" + std::to_string(g.bestPct)
         + (g.alt >= 0 ? " \xC2\xB7 alternatif: " + keyName(g.alt) : std::string());
}

class KeyDetector {
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        d = std::max(1, (int) std::lround(sr / 22050.0));
        srD = sr / d;
        hopSec = (float) ((double) kHop * d / sr);
        maxLag = std::min(kMaxLag, (int) std::ceil(srD / 70.0));
        minLag = std::max(2, (int) std::floor(srD / 1000.0));
        active = false;
    }

    void start(float seconds)
    {
        total = (long long) ((double) seconds * sr); fed = 0;
        decN = 0; decCnt = 0; decAcc = 0.f;
        ns = 0; segN = 0; segSum = 0.f; segC = 0.f; gap = 0; fn = 0; peakRms = 0.f;
        result = KeyGuess{};
        active = true;
    }

    float progress() const { return total > 0 ? std::min(1.f, (float) fed / (float) total) : 0.f; }

    // true: süre doldu, result hazır
    bool feed(float x)
    {
        if (!active) return false;
        decAcc += x;
        if (++decCnt == d) {
            ring[(size_t) (decN++ & kRM)] = decAcc / (float) d; decAcc = 0.f; decCnt = 0;
            if (decN % kHop == 0 && decN >= kW) frame();
        }
        if (++fed < total) return false;
        flushSeg(); analyse(); active = false;
        return true;
    }

    KeyGuess result;

private:
    static constexpr int kW = 1536, kHop = 512, kRM = 2047, kMaxLag = 768, kMaxSeg = 512, kSegCap = 256;
    struct Seg { float midi, dur, conf; int startF, endF; };

    double sr = 44100.0, srD = 22050.0;
    int d = 2, minLag = 22, maxLag = 315;
    float hopSec = 0.0232f;
    bool active = false;
    long long total = 0, fed = 0, decN = 0;
    int decCnt = 0;
    float decAcc = 0.f;
    std::array<float, 2048> ring{};
    std::array<float, kW> fr{};
    std::array<float, kMaxLag + 3> nsdf{};

    std::array<Seg, kMaxSeg> segs{};
    int ns = 0;                                    // shortcut: en fazla 512 segment (~dakikalar), fazlası atılır
    std::array<float, kSegCap> segM{};
    int segN = 0, segStart = 0, segEnd = 0, gap = 0, fn = 0;
    float segSum = 0.f, segC = 0.f, segLast = 0.f, peakRms = 0.f;

    // MPM (NSDF): perde (MIDI, A=440 mutlak) + güven
    bool pitch(float& midi, float& conf)
    {
        for (int t = 1; t <= maxLag + 1; ++t) {
            float rr = 0.f, mm = 0.f;
            for (int j = 0; j + t < kW; ++j) { const float a = fr[(size_t) j], b = fr[(size_t) (j + t)]; rr += a * b; mm += a * a + b * b; }
            nsdf[(size_t) t] = mm > 1e-12f ? 2.f * rr / mm : 0.f;
        }
        int keyPos[48]; int nk = 0, pos = 1, cur = 0;
        while (pos < maxLag && nsdf[(size_t) pos] > 0.f) ++pos;
        while (pos < maxLag && nsdf[(size_t) pos] <= 0.f) ++pos;
        while (pos <= maxLag) {
            if (nsdf[(size_t) pos] > nsdf[(size_t) pos - 1] && nsdf[(size_t) pos] >= nsdf[(size_t) pos + 1])
                if (cur == 0 || nsdf[(size_t) pos] > nsdf[(size_t) cur]) cur = pos;
            ++pos;
            if (pos <= maxLag && nsdf[(size_t) pos] <= 0.f) {
                if (cur > 0 && nk < 48) keyPos[nk++] = cur;
                cur = 0;
                while (pos <= maxLag && nsdf[(size_t) pos] <= 0.f) ++pos;
            }
        }
        if (cur > 0 && nk < 48) keyPos[nk++] = cur;
        float hi = 0.f;
        for (int k = 0; k < nk; ++k) if (keyPos[k] >= minLag) hi = std::fmax(hi, nsdf[(size_t) keyPos[k]]);
        if (hi <= 0.f) return false;
        int pick = 0;
        for (int k = 0; k < nk; ++k) if (keyPos[k] >= minLag && nsdf[(size_t) keyPos[k]] >= 0.9f * hi) { pick = keyPos[k]; break; }
        if (pick == 0) return false;
        const float a = nsdf[(size_t) pick - 1], b = nsdf[(size_t) pick], c = nsdf[(size_t) pick + 1], den = a - 2.f * b + c;
        float del = 0.f, pk = b;
        if (den < -1e-9f) { del = std::max(-1.f, std::min(1.f, 0.5f * (a - c) / den)); pk = b - 0.25f * (a - c) * del; }
        midi = 69.f + 12.f * std::log2((float) (srD / ((double) pick + del)) / 440.f);
        conf = std::max(0.f, std::min(1.f, pk));
        return true;
    }

    void frame()
    {
        float mean = 0.f;
        for (int j = 0; j < kW; ++j) { fr[(size_t) j] = ring[(size_t) ((decN - kW + j) & kRM)]; mean += fr[(size_t) j]; }
        mean /= (float) kW;
        float e = 0.f;
        for (float& v : fr) { v -= mean; e += v * v; }
        const float rms = std::sqrt(e / (float) kW);
        peakRms = std::max(rms, peakRms * 0.9995f);

        float m = 0.f, conf = 0.f;
        // sadece voiced + yüksek güven; nefes/gürültü katı (tepe enerjisinin -26 dB altı) elenir
        const bool voiced = rms >= std::max(1e-3f, 0.05f * peakRms) && pitch(m, conf) && conf >= 0.88f;
        ++fn;
        if (voiced) {
            const bool same = segN > 0 && gap <= 3 && segN < kSegCap && std::fabs(m - segLast) <= 0.6f && std::fabs(m - segSum / (float) segN) <= 0.8f;
            if (!same) { flushSeg(); segStart = fn; }
            segM[(size_t) segN++] = m; segSum += m; segC += conf; segLast = m; segEnd = fn; gap = 0;
        } else if (segN > 0 && ++gap > 3) flushSeg();
    }

    void flushSeg()
    {
        if (segN >= 5 && ns < kMaxSeg) {                       // < ~115 ms: nota sayılmaz
            std::array<float, kSegCap> t = segM;
            std::nth_element(t.begin(), t.begin() + segN / 2, t.begin() + segN);
            segs[(size_t) ns++] = Seg{ t[(size_t) (segN / 2)], (float) segN * hopSec, segC / (float) segN, segStart, segEnd };
        }
        segN = 0; segSum = 0.f; segC = 0.f; gap = 0;
    }

    void analyse()
    {
        if (ns == 0) return;
        constexpr float kTwoPi = 6.28318530717958f;
        constexpr float kProfMaj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        constexpr float kProfMin[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

        // tuning offset: cent kalıntılarının (mod 100) ağırlıklı dairesel ortalaması
        float sc = 0.f, ss = 0.f, sw = 0.f;
        for (int i = 0; i < ns; ++i) {
            const float w = segs[(size_t) i].dur * segs[(size_t) i].conf, r = segs[(size_t) i].midi - std::round(segs[(size_t) i].midi);
            sc += w * std::cos(kTwoPi * r); ss += w * std::sin(kTwoPi * r); sw += w;
        }
        // shortcut: kalıntılar dağınıksa (R<0.2) kayma güvenilmez, 0 alınır
        const float off = (std::sqrt(sc * sc + ss * ss) / sw >= 0.2f) ? std::atan2(ss, sc) / kTwoPi : 0.f;

        // histogram + tonik ipuçları
        float hist[12] = {}, hint[12] = {}, hA[12] = {}, hB[12] = {}, hC[12] = {};
        std::array<int, kMaxSeg> pc{}, idx{};
        for (int i = 0; i < ns; ++i) {
            pc[(size_t) i] = ((int) std::lround(segs[(size_t) i].midi - off) % 12 + 12) % 12;
            hist[pc[(size_t) i]] += segs[(size_t) i].dur * segs[(size_t) i].conf;
            idx[(size_t) i] = i;
        }
        const int top = std::min(3, ns);                      // en uzun 3 nota
        std::partial_sort(idx.begin(), idx.begin() + top, idx.begin() + ns, [&](int a, int b) { return segs[(size_t) a].dur > segs[(size_t) b].dur; });
        float sA = 0.f, sB = 0.f, sC = 0.f;
        for (int i = 0; i < top; ++i) { const float w = segs[(size_t) idx[(size_t) i]].dur; hA[pc[(size_t) idx[(size_t) i]]] += w; sA += w; }
        const float phraseGap = 0.3f / hopSec;                // cümle sınırı: > 300 ms boşluk
        for (int i = 0; i < ns; ++i) {
            const float w = segs[(size_t) i].dur * segs[(size_t) i].conf;
            if (i == ns - 1 || (float) (segs[(size_t) i + 1].startF - segs[(size_t) i].endF) > phraseGap) { const float v = i == ns - 1 ? 2.f * w : w; hB[pc[(size_t) i]] += v; sB += v; }
            if (i == 0 || (float) (segs[(size_t) i].startF - segs[(size_t) i - 1].endF) > phraseGap) { hC[pc[(size_t) i]] += w; sC += w; }
        }
        for (int p = 0; p < 12; ++p) hint[p] = 0.5f * hA[p] / std::fmax(sA, 1e-9f) + 0.35f * hB[p] / std::fmax(sB, 1e-9f) + 0.15f * hC[p] / std::fmax(sC, 1e-9f);

        // 24 ton: Pearson(histogram, profil) + tonik ipucu (relatif major/minor ayrımı)
        float mean = 0.f; for (float h : hist) mean += h; mean /= 12.f;
        float score[24];
        for (int k = 0; k < 24; ++k) {
            const float* prof = k < 12 ? kProfMaj : kProfMin;
            float pm = 0.f; for (int i = 0; i < 12; ++i) pm += prof[i]; pm /= 12.f;
            float sxy = 0.f, sxx = 0.f, syy = 0.f;
            for (int i = 0; i < 12; ++i) { const float x = hist[(k % 12 + i) % 12] - mean, y = prof[i] - pm; sxy += x * y; sxx += x * x; syy += y * y; }
            score[k] = sxy / (std::sqrt(sxx * syy) + 1e-12f) + 0.3f * hint[k % 12];
        }
        int b = 0, a = -1;
        for (int k = 1; k < 24; ++k) if (score[k] > score[b]) b = k;
        for (int k = 0; k < 24; ++k) if (k != b && (a < 0 || score[k] > score[a])) a = k;
        float z = 0.f, pb = 0.f, pa = 0.f;
        for (int k = 0; k < 24; ++k) { const float e = std::exp((score[k] - score[b]) / 0.06f); z += e; if (k == b) pb = e; if (k == a) pa = e; }
        result = KeyGuess{ b, a, (int) std::lround(100.f * pb / z), (int) std::lround(100.f * pa / z) };
    }
};

} // namespace mt

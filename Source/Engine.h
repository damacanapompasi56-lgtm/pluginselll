#pragma once
// MADENCITUNE DSP motoru. Saf C++ (JUCE yok) -> CI'da self-test ile doğrudan test edilir.
//
// Akış (mono, örnek örnek):
//   giriş -> DC blocker (yalnız analiz yolu) -> halka tampon + decimate -> her H örnekte: MPM pitch
//   (kaba: decimate, ince: tam hızda, parabolik interpolasyon) -> voiced/unvoiced + median + oktav koruması
//   -> hedef nota (histerezis) -> tolerans / glide / Speed / vibrato -> düzeltme (yarım ton)
//   -> pitch mark'lar: her mark bir öncekinden TAM BİR PERİYOT sonra, dalga şekli korelasyonuyla kilitli
//      (ham tepe seçimi yok, kesirli konum) -> sentez: Hann pencereli 2T taneler, sinc ile kesirli yerleşim,
//      toplama bölerek normalize (acc/accW) -> voiced maskesine göre dry ile crossfade.
//   Düzeltme 0 iken sentez adımı = analiz mark aralığı -> çıkış girişin birebir gecikmeli kopyası.
//   Gecikme sabit ve bildirilir: L = 2048 örnek (>50 kHz: 4096).
#include <atomic>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <vector>
#include "KeyDetector.h"
#include "Params.h"

namespace mt {

using i64 = long long;
constexpr float kPi = 3.14159265358979f;

inline float fclamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int nextPow2(int v) { int p = 1; while (p < v) p <<= 1; return p; }

struct EngineParams {
    float refHz = kRefDefault, speed = 0.f, glideMs = 0.f, tolCents = 0.f, tolMs = 0.f, vibPct = 100.f;
    bool smart = false, vibOn = false;
    int range = kGenericRange;
    bool formantCorrected = kFormantDefault == 0;
    unsigned mask = 0xFFF;  // izinli perde sınıfları, bit0 = C
};

class Engine {
public:
    // UI okur (atomik)
    std::atomic<float> dispMidi{ -1.f };   // algılanan perde (MIDI, ref'e göre), <0 = yok
    std::atomic<int>   dispTarget{ -1 };   // hedef MIDI notası, -1 = yok
    std::atomic<float> dispCorr{ 0.f };    // uygulanan düzeltme (yarım ton); CI eğri testi okur
    std::atomic<bool>  keyRequest{ false };
    std::atomic<int>   keyState{ 0 };      // 0 boşta, 1 dinliyor, 2 bitti
    std::atomic<int>   keyResult{ -1 };    // 0..11 major, 12..23 minor, -1 = hiç sesli nota yok
    std::atomic<float> keyProgress{ 0.f };
    std::atomic<int>   keyConf{ 0 }, keyAlt{ -1 }, keyAltConf{ 0 };   // en iyi tahmin %, 2. aday, 2. aday %

    bool ready() const { return N > 0; }
    int latency() const { return L; }
    void set(const EngineParams& p) { P = p; }

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        d = sr > 60000.0 ? 8 : 4;
        srD = sr / d;
        H = 32 * d;
        L = sr > 50000.0 ? 4096 : 2048;
        TmaxD = (L - H - 32) / (3 * d);          // sentez + analiz payı: L >= 3*Tmax + H + 32
        Tmax = TmaxD * d;                        // en uzun periyot (44.1k: 70 Hz, 96k: 76 Hz)
        Wd = 2 * TmaxD;
        N = nextPow2(8 * Tmax); mask = N - 1;
        in.assign((size_t) N, 0.f); ain.assign((size_t) N, 0.f); acc.assign((size_t) N, 0.f); accW.assign((size_t) N, 0.f); vmask.assign((size_t) N, 0.f);
        const int ND = nextPow2(2 * Wd); decMask = ND - 1; dec.assign((size_t) ND, 0.f);
        yD.assign((size_t) Wd, 0.f); nsdf.assign((size_t) TmaxD + 3, 0.f); nsdfF.assign((size_t) (2 * d + 3), 0.f);
        ncc.assign((size_t) (Tmax + 8), 0.f);
        hopSec = (float) H / (float) sr;
        vibN = std::min(128, std::max(2, (int) std::lround(0.16 / hopSec)));
        vA = 1.f - std::exp(-1.f / (0.006f * (float) sr));      // dry/wet crossfade ~6 ms
        dcR = 1.f - 6.2831853f * 20.f / (float) sr;               // DC blocker ~20 Hz
        aAtk = 1.f - std::exp(-hopSec / 0.3f);
        aRel = 1.f - std::exp(-hopSec / 1.5f);
        n = decN = 0; decAcc = 0.f; decCnt = 0; prevX = 0.f; hopSumSq = hopSumD2 = 0.f; dcX = dcY = 0.f;
        first = true; started = false; cPrev = 0; voiced = false; unv = 0; jumpCnt = 0; jumpM = 0.f; mi = 0;
        haveTarget = hadGoal = false; glideLeft = glideTot = 0; corr = corr1 = 0.f; goal0 = 0.f; rhoPrev = 1.f; outsideMs = 0.f; vibCount = 0;
        runActive = false; markCount = 0; kPtr = 0; sNext = 0.0; lastPos = 0.0; vmix = 0.f; dens = 0.f; medInit = false;
        midiF = 0.f; key.prepare(sr);
    }

    float process(float x)
    {
        keyTick(x);   // Key Detect: ham giriş, her işlemden önce; çıkış/Key/Scale/düzeltme analize karışmaz
        in[(size_t) (n & mask)] = x;
        // DC blocker: yalnız analiz yolunda (DC, NSDF'yi ve korelasyonu bozar); çıkış yolu girişe şeffaf kalır
        float a = x - dcX + dcR * dcY; dcX = x;
        if (std::fabs(a) < 1e-20f) a = 0.f;       // denormal koruması
        dcY = a;
        ain[(size_t) (n & mask)] = a;
        const float dx = x - prevX; prevX = x;
        hopSumSq += x * x; hopSumD2 += dx * dx;
        decAcc += a;
        if (++decCnt == d) { dec[(size_t) (decN & decMask)] = decAcc / (float) d; ++decN; decAcc = 0.f; decCnt = 0; }
        ++n;
        if (n % H == 0) analyse();
        synth();

        const i64 o = n - 1 - L;       // çıkış zamanı (giriş indeksi cinsinden)
        if (o < 0) return 0.f;
        const size_t i = (size_t) (o & mask);
        const float dry = in[i], w = accW[i];
        const float wet = acc[i] / std::fmax(w, 1e-6f);
        vmix += vA * (vmask[i] - vmix);
        if (vmix < 1e-6f) vmix = 0.f; else if (vmix > 0.999999f) vmix = 1.f;   // denormal / tam bypass
        const float m = vmix * std::fmin(w * 8.f, 1.f);   // tane örtüşmesi yoksa (w~0) kuruya düş: delik yok
        acc[i] = 0.f; accW[i] = 0.f;
        return dry + m * (wet - dry);
    }

private:
    static constexpr int kNone = -1000;
    static constexpr int kMarks = 256;
    static constexpr float kJumpSemi = 7.f;   // bundan büyük perde sıçraması kJumpFrames ardışık kare doğrulanmadan kabul edilmez
    static constexpr int   kJumpFrames = 4;
    static constexpr float kJumpPen = 0.3f;   // aday seçiminde: 1 oktav uzaklık = bu kadar nsdf cezası (doğrusal, cent'te)
    struct Mark { double pos; float T0, Ts, Pn, g, b; };   // Pn: bir sonraki mark'a uzaklık, g: bypass ağırlığı, b: formant kayma oranı (1 = kaymaz)
    struct Pitch { float hz, conf; };

    EngineParams P;
    double sr = 44100.0, srD = 11025.0;
    int d = 4, TmaxD = 0, Tmax = 0, Wd = 0, H = 128, L = 0, N = 0, vibN = 55, decMask = 0;
    i64 mask = 0;
    float hopSec = 0.003f, vA = 0.f, aAtk = 0.f, aRel = 0.f;
    std::vector<float> in, ain, acc, accW, vmask, dec, yD, nsdf, nsdfF, ncc;

    i64 n = 0, decN = 0, cPrev = 0;
    float decAcc = 0.f, prevX = 0.f, hopSumSq = 0.f, hopSumD2 = 0.f, dcX = 0.f, dcY = 0.f, dcR = 0.997f;
    int decCnt = 0;

    // yumuşatılmış parametreler
    bool first = true, started = false;
    float sRef = kRefDefault, sSpeed = 0.f, sGlide = 0.f, sTolC = 0.f, sTolT = 0.f, sVib = 100.f, sFmt = 0.f;   // sFmt: 0 Corrected .. 1 Not Corrected

    // perde takibi
    bool voiced = false;
    int unv = 0, jumpCnt = 0, mi = 0;
    float med[5] = { 0, 0, 0, 0, 0 }, track = 0.f, midiF = 0.f, jumpM = 0.f;   // jumpCnt/jumpM: bekleyen büyük sıçrama adayı

    // hedef / düzeltme
    bool haveTarget = false, hadGoal = false;
    int target = 0, lastTarget = 0, glideLeft = 0, glideTot = 0;
    float goal = 0.f, goal0 = 0.f, corr = 0.f, corr1 = 0.f, rhoPrev = 1.f, outsideMs = 0.f;   // corr1: 2. kademe ön süzgeç, rhoPrev: önceki hop'un Ts/T0 oranı

    // vibrato merkezi (kayan ortalama)
    float vibBuf[128] = {}, vibSum = 0.f;
    int vibIdx = 0, vibCount = 0;

    // yoğunluk
    float dens = 0.f, medTrack = 0.f;
    bool medInit = false;

    // PSOLA
    Mark marks[kMarks];
    i64 markCount = 0, kPtr = 0;
    bool runActive = false;
    double sNext = 0.0, lastPos = 0.0;
    float vmix = 0.f;

    KeyDetector key;

    void keyTick(float x)
    {
        if (keyRequest.load(std::memory_order_relaxed) && keyRequest.exchange(false)) { key.start(kKeySeconds); keyProgress.store(0.f); keyState.store(1); }
        if (keyState.load(std::memory_order_relaxed) != 1) return;
        const bool done = key.feed(x);
        keyProgress.store(key.progress());
        if (!done) return;
        keyConf.store(key.result.bestPct); keyAlt.store(key.result.alt); keyAltConf.store(key.result.altPct);
        keyResult.store(key.result.best);
        keyState.store(2);
    }

    Mark& mk(i64 i) { return marks[i & (kMarks - 1)]; }

    bool allowed(int k) const
    {
        const int pc = ((k % 12) + 12) % 12;
        if (!((P.mask >> pc) & 1u)) return false;
        const RangeHz r = kRanges[P.range < 0 ? 0 : (P.range > 6 ? 6 : P.range)];
        const float f = sRef * std::exp2((float) (k - 69) / 12.f);
        return f >= r.lo * 0.99f && f <= r.hi * 1.01f;  // %1 pay: C4=261.6 Soprano'da kalsın
    }

    int nearestAllowed(float m) const
    {
        const int base = (int) std::lround(m);
        int best = kNone; float bd = 1e9f;
        for (int k = base - 12; k <= base + 12; ++k)
            if (allowed(k)) { const float dd = std::fabs((float) k - m); if (dd < bd) { bd = dd; best = k; } }
        return best;
    }

    // MPM: kaba arama decimate edilmiş sinyalde, ince arama tam hızda; parabolik interpolasyon
    Pitch detect()
    {
        Pitch out{ 0.f, 0.f };
        const RangeHz r = kRanges[P.range < 0 ? 0 : (P.range > 6 ? 6 : P.range)];
        const i64 base = decN - Wd;
        if (base < 0) return out;
        for (int j = 0; j < Wd; ++j) yD[(size_t) j] = dec[(size_t) ((base + j) & decMask)];
        const int minLag = std::max(2, (int) std::floor((float) srD / r.hi));
        const int maxLag = std::min(TmaxD, (int) std::ceil((float) srD / r.lo));
        if (maxLag <= minLag + 2) return out;

        for (int t = 1; t <= maxLag + 1; ++t) {
            float rr = 0.f, mm = 0.f;
            for (int j = 0; j + t < Wd; ++j) {
                const float a = yD[(size_t) j], b = yD[(size_t) (j + t)];
                rr += a * b; mm += a * a + b * b;
            }
            nsdf[(size_t) t] = mm > 1e-9f ? 2.f * rr / mm : 0.f;
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
        if (hi < 0.3f) return out;
        int pick = 0;   // klasik MPM seçimi (ilk tepe >= 0.9*hi): güven değeri hep bundan -> voiced kararı değişmez
        for (int k = 0; k < nk; ++k)
            if (keyPos[k] >= minLag && nsdf[(size_t) keyPos[k]] >= 0.9f * hi) { pick = keyPos[k]; break; }
        if (pick == 0) return out;

        // Takip varken: güçlü adaylar arasında önceki perdeye yakın olanı seç (cent mesafesine göre ceza).
        // Eşitlikte kısa lag (küçük indeks) kazanır. Takip yoksa klasik seçim.
        int sel = pick;
        if (voiced && nk > 1) {
            const float vmin = std::fmax(0.3f, 0.5f * hi);
            float bs = -1e9f;
            for (int k = 0; k < nk; ++k) {
                const int p = keyPos[k];
                const float v = nsdf[(size_t) p];
                if (p < minLag || v < vmin) continue;
                const float dm = std::fabs(69.f + 12.f * std::log2((float) srD / lagOf(p) / sRef) - track);
                const float sc = v - kJumpPen * std::fmin(1.f, dm / 12.f);
                if (sc > bs) { bs = sc; sel = p; }
            }
        }

        float Tf, peak;
        refineFine(lagOf(sel) * (float) d, Tf, peak);
        out.hz = (float) sr / Tf;
        out.conf = peak;
        if (sel != pick) { float Tp, pp; refineFine(lagOf(pick) * (float) d, Tp, pp); out.conf = pp; }
        return out;
    }

    // decimate nsdf'de tepenin parabolik interpolasyonlu lag'i
    float lagOf(int p) const
    {
        const float a = nsdf[(size_t) p - 1], b = nsdf[(size_t) p], c = nsdf[(size_t) p + 1];
        const float den = a - 2.f * b + c;
        return (float) p + (den < -1e-9f ? fclamp(0.5f * (a - c) / den, -1.f, 1.f) : 0.f);
    }

    // ince arama (tam hız), pencere = son 2*Tmax örnek; Tc çevresinde +-(d+1) lag
    void refineFine(float Tc, float& Tf, float& conf)
    {
        const int W = 2 * Tmax;
        const i64 s0 = n - W;
        const int Lc = (int) std::lround(Tc);
        const int lo = std::max(2, Lc - d - 1), hi2 = Lc + d + 1;
        float bestV = -2.f; int bestL = lo;
        for (int l = lo; l <= hi2; ++l) {
            float rr = 0.f, mm = 0.f;
            for (int j = 0; j < W - l; ++j) {
                const float a = ain[(size_t) ((s0 + j) & mask)], b = ain[(size_t) ((s0 + j + l) & mask)];
                rr += a * b; mm += a * a + b * b;
            }
            const float v = mm > 1e-9f ? 2.f * rr / mm : 0.f;
            nsdfF[(size_t) (l - lo)] = v;
            if (v > bestV) { bestV = v; bestL = l; }
        }
        Tf = (float) bestL; float peak = bestV;
        if (bestL > lo && bestL < hi2) {
            const float a = nsdfF[(size_t) (bestL - lo - 1)], b = nsdfF[(size_t) (bestL - lo)], c = nsdfF[(size_t) (bestL - lo + 1)];
            const float den = a - 2.f * b + c;
            if (den < -1e-9f) { const float dl = fclamp(0.5f * (a - c) / den, -1.f, 1.f); Tf += dl; peak = b - 0.25f * (a - c) * dl; }
        }
        Tf = fclamp(Tf, 2.f, (float) Tmax);
        conf = fclamp(peak, 0.f, 1.f);
    }

    static float median5(const float* a)
    {
        float t[5] = { a[0], a[1], a[2], a[3], a[4] };
        for (int i = 1; i < 5; ++i) { const float v = t[i]; int j = i; while (j > 0 && t[j - 1] > v) { t[j] = t[j - 1]; --j; } t[j] = v; }
        return t[2];
    }

    void updateDensity(float rmsDb, float bw, bool v)
    {
        const float loud = fclamp((rmsDb + 45.f) / 30.f, 0.f, 1.f);
        const float bright = fclamp((bw - 500.f) / 2500.f, 0.f, 1.f);  // rms-frekans, centroid vekili
        const float height = v ? fclamp(0.5f + (midiF - medTrack) / 6.f, 0.f, 1.f) : 0.5f;
        const float raw = rmsDb < -55.f ? 0.f : 0.5f * loud + 0.2f * bright + 0.3f * height;
        dens += (raw > dens ? aAtk : aRel) * (raw - dens);  // attack hızlı, release yavaş
    }

    void addMark(double pos, float T0, float Ts, float g)
    {
        if (markCount > 0 && runActive) mk(markCount - 1).Pn = (float) (pos - lastPos);
        // Corrected (sFmt 0): b = 1, tane aynen okunur -> formantlar sabit (PSOLA zaten böyle). Not Corrected (sFmt 1): b = T0/Ts,
        // tane perde oranı kadar hızlı okunur -> formantlar perdeyle kayar. Ara değerde log-doğrusal geçiş.
        // shortcut: b >= T0/(Tmax+32), tane yarı genişliği T0/b gecikme payını (L) aşmasın; en düşük seslerde çok büyük aşağı kaymada formant kayması kısmi kalır
        const float b = std::fmax(std::pow(T0 / Ts, sFmt), T0 / (float) (Tmax + 32));
        mk(markCount++) = Mark{ pos, T0, Ts, T0, g, b };
        lastPos = pos;
    }

    // Tutarlı pitch mark'lar: yeni mark = önceki + ölçülmüş periyot. Periyot, önceki mark çevresindeki
    // T0 uzunluklu parçanın T0+-%12 gecikmedeki kopyasıyla normalize korelasyonundan (parabolik) bulunur.
    // Ham örnek tepesi seçilmez -> ardışık taneler aynı dalga fazında, tane dikişi kayması yok.
    void makeMarks(i64 c, float T0, float Ts, float g, bool soft)
    {
        const float rho = Ts / T0;                          // düzeltme oranı (cent alanında log)
        if (!runActive) {                                   // yeni sesli bölge: ilk mark = ilk periyottaki tepe
            i64 best = c; float bv = -1e9f;
            for (i64 p = c; p < c + (i64) T0; ++p) { const float v = ain[(size_t) (p & mask)]; if (v > bv) { bv = v; best = p; } }
            runActive = true; rhoPrev = rho;
            sNext = (double) best;                          // sentez ağı ilk mark'a hizalı başlar (d = 0)
            addMark((double) best, T0, Ts, g);
        }
        const int hw = std::max(4, (int) (T0 * 0.5f)), R = std::max(3, (int) (T0 * 0.12f));
        while (lastPos + (double) T0 <= (double) c) {
            const i64 pi = (i64) std::llround(lastPos);
            const int lc = (int) std::lround(T0), lo = std::max(2, lc - R), hi = std::min(Tmax + 6, lc + R);
            float ea = 0.f;
            for (int j = -hw; j < hw; ++j) { const float a = ain[(size_t) ((pi + j) & mask)]; ea += a * a; }
            int bl = lc; float bv = -2.f;
            for (int l = lo; l <= hi; ++l) {
                float rr = 0.f, eb = 0.f;
                for (int j = -hw; j < hw; ++j) {
                    const float a = ain[(size_t) ((pi + j) & mask)], b = ain[(size_t) ((pi + j + l) & mask)];
                    rr += a * b; eb += b * b;
                }
                const float v = rr / std::sqrt(ea * eb + 1e-12f);
                ncc[(size_t) l] = v;
                if (v > bv) { bv = v; bl = l; }
            }
            double P = (double) T0;                         // zayıf korelasyonda nominal periyot
            if (bv > 0.2f) {
                P = (double) bl;
                if (bl > lo && bl < hi) {
                    const float a = ncc[(size_t) bl - 1], b = ncc[(size_t) bl], cc = ncc[(size_t) bl + 1], den = a - 2.f * b + cc;
                    if (den < -1e-9f) P += (double) fclamp(0.5f * (a - cc) / den, -1.f, 1.f);
                }
            }
            // hop içinde oranı geometrik (= cent'te doğrusal) enterpole et: düzeltme 3 ms'de bir basamaklanmasın.
            // T0'a oran uygulanır (Ts'e değil): oran 1 iken Ts == T0 -> bypass birebir kopya kalır. Speed 0 (soft=false) sert.
            float ro = rho;
            if (soft && rho != rhoPrev) ro = rhoPrev * std::pow(rho / rhoPrev, fclamp((float) ((lastPos + P - (double) (c - H)) / (double) H), 0.f, 1.f));
            addMark(lastPos + P, T0, T0 * ro, g);
        }
        rhoPrev = rho;
    }

    void analyse()
    {
        const float ms = hopSumSq / (float) H;
        const float rmsDb = 10.f * std::log10(ms + 1e-12f);
        const float bw = std::sqrt(hopSumD2 / (hopSumSq + 1e-12f)) * (float) sr / (2.f * kPi);
        hopSumSq = hopSumD2 = 0.f;
        if (n < 2 * (i64) Tmax) return;
        const i64 c = n - Tmax;  // pencerenin merkezi: bu konuma kadar işaretler üretilir
        if (!started) { started = true; cPrev = c - H; }
        const float hopMs = hopSec * 1000.f;

        // parametre yumuşatma (zipper yok): ~20 ms, hop hızında
        const float a = first ? 1.f : 1.f - std::exp(-hopSec / 0.02f);
        const float aF = first ? 1.f : 1.f - std::exp(-hopSec / (kFormantSmoothMs * 0.001f));
        first = false;
        sRef += a * (P.refHz - sRef);   sSpeed += a * (P.speed - sSpeed);
        sGlide += a * (P.glideMs - sGlide); sTolC += a * (P.tolCents - sTolC);
        sTolT += a * (P.tolMs - sTolT); sVib += a * (P.vibPct - sVib);
        sFmt += aF * ((P.formantCorrected ? 0.f : 1.f) - sFmt);
        const float hardness = fclamp(sSpeed / kSpeedMin, 0.f, 1.f);  // 0..1, sadece Speed<0'da > 0

        // 1) perde + voiced
        const Pitch pd = detect();
        const bool strong = pd.hz > 0.f && rmsDb > -55.f && pd.conf >= (voiced ? 0.6f : 0.75f);
        unv = strong ? 0 : (unv < 100 ? unv + 1 : unv);
        const bool vNow = strong || (voiced && unv < 8);   // ~23 ms tutma: kısa güven düşüşlerinde wet/dry titremesi yok
        if (strong) {
            float m = 69.f + 12.f * std::log2(pd.hz / sRef);
            if (!voiced) { for (float& v : med) v = m; jumpCnt = 0; }
            else {
                if (std::fabs(m - track) > kJumpSemi) {    // oktav/büyük sıçrama: ardışık ve birbirine yakın kJumpFrames kare doğrularsa kabul
                    jumpCnt = (jumpCnt > 0 && std::fabs(m - jumpM) < 1.f) ? jumpCnt + 1 : 1;
                    jumpM = m;
                    if (jumpCnt >= kJumpFrames) { jumpCnt = 0; for (float& v : med) v = m; }
                    else m = track;                        // tek/kısa sıçrama: son perdede kal
                } else jumpCnt = 0;
                med[mi] = m; mi = (mi + 1) % 5;
            }
            track = m;
            midiF = median5(med);
        } else jumpCnt = 0;   // güçlü olmayan kare sıçrama serisini böler ("üst üste" şartı); midiF son iyi perdede kalır (voiced tutma <= 8 kare ~ 23 ms)
        if (vNow && !voiced) { corr = corr1 = 0.f; haveTarget = hadGoal = false; vibCount = 0; outsideMs = 0.f; runActive = false; }
        if (!vNow && voiced) { haveTarget = hadGoal = false; runActive = false; }
        voiced = vNow;

        // 3) düzeltme
        bool active = false;
        if (voiced) {
            if (!medInit) { medTrack = midiF; medInit = true; }
            medTrack += (midiF > medTrack ? 1.f : -1.f) * 0.005f;
            updateDensity(rmsDb, bw, true);

            if (vibCount == 0) { for (int i = 0; i < vibN; ++i) vibBuf[i] = midiF; vibSum = midiF * (float) vibN; vibIdx = 0; vibCount = 1; }
            vibSum += midiF - vibBuf[vibIdx]; vibBuf[vibIdx] = midiF; vibIdx = (vibIdx + 1) % vibN;
            const float center = vibSum / (float) vibN;
            // shortcut: vibrato merkezi 160 ms kayan ortalama, 4 Hz altı salınım kısmen merkeze karışır
            const float mSel = (P.vibOn && std::fabs(midiF - center) < 0.7f) ? center : midiF;

            const int cand = nearestAllowed(mSel);
            if (cand == kNone) haveTarget = false;
            else if (!haveTarget || !allowed(target)) { target = cand; haveTarget = true; }
            else if (cand != target && std::fabs(mSel - (float) cand) + 0.25f < std::fabs(mSel - (float) target)) target = cand;  // histerezis

            if (haveTarget) {
                if (!hadGoal) { goal = (float) target; lastTarget = target; glideLeft = glideTot = 0; hadGoal = true; }
                else if (target != lastTarget) {
                    const float g = fclamp(sGlide, kGlideMin, kGlideMax) * (1.f - hardness);  // sertlikle glide -> 0
                    glideLeft = glideTot = (int) std::lround(g / hopMs); goal0 = goal; lastTarget = target;
                }
                // glide: yarım kosinüs (iki ucunda eğim 0), doğrusal rampanın köşeleri yok; süre aynı
                if (glideLeft > 0) { --glideLeft; goal = goal0 + ((float) target - goal0) * 0.5f * (1.f - std::cos(kPi * (1.f - (float) glideLeft / (float) glideTot))); }
                else goal = (float) target;

                // tolerans: cent ölü bölgesi + süre gecikmesi
                const bool inside = P.tolCents >= 0.5f && std::fabs(((float) target - mSel) * 100.f) <= sTolC;
                outsideMs = inside ? 0.f : outsideMs + hopMs;
                const bool hold = inside || (P.tolMs >= 1.f && outsideMs < sTolT);

                float want = 0.f;
                if (!hold) {
                    const float vs = P.vibOn ? (sVib / 100.f) * (1.f - hardness) : 0.f;
                    want = (goal - midiF) + vs * fclamp(midiF - center, -1.f, 1.f);
                    want = fclamp(want, -12.f, 12.f);
                }
                float tau = std::fmax(0.f, sSpeed);              // Speed<0 -> taban 0 ms
                if (P.smart) tau += (1.f - dens) * kSmartPadMs;
                tau = fclamp(tau, 0.f, kRetuneMaxMs);
                // 2 kademeli (kritik sönümlü) süzgeç: geçiş S eğrisiyle başlar (tek kutup ilk hop'ta en dik, kesik duyulur).
                // Kademe başına 0.6*tau -> %63 ve %90 noktaları eski tek kutupla aynı: Speed hissi değişmez. tau=0 -> al=1 -> sert.
                const float al = tau <= 0.f ? 1.f : 1.f - std::exp(-hopMs / (0.6f * tau));
                corr1 += al * (want - corr1);
                corr += al * (corr1 - corr);

                const float fIn = sRef * std::exp2((midiF - 69.f) / 12.f);
                const float fOut = sRef * std::exp2((midiF + corr - 69.f) / 12.f);
                const float T0 = fclamp((float) sr / fIn, (float) sr / 1500.f, (float) Tmax);
                const float Ts = fclamp((float) sr / fOut, 0.5f * T0, 2.f * T0);
                makeMarks(c, T0, Ts, fclamp(1.f - std::fabs(corr) / 0.05f, 0.f, 1.f), tau > 0.f);   // |düzeltme| < 5 cent: giriş periyodunu izle
                active = true;
            } else hadGoal = false;
        } else updateDensity(rmsDb, bw, false);

        if (!active) runActive = false;
        for (i64 p = cPrev + 1; p <= c; ++p) vmask[(size_t) (p & mask)] = active ? 1.f : 0.f;
        cPrev = c;
        dispMidi.store(voiced ? midiF : -1.f);
        dispTarget.store(active ? target : -1);
        dispCorr.store(active ? corr : 0.f);
    }

    // Hann pencereli 2T tane: m.pos çevresinden okunur, s'e (kesirli) yerleştirilir. Kesirli kaydırma 8 noktalı
    // pencereli sinc ile (katsayılar tane başına bir kez); ofset sıfırsa katsayılar delta -> birebir kopya.
    // Formant kaymalı tane (Not Corrected): giriş m.b kat hızlı okunur (kübik Catmull-Rom), pencere çıkış alanında T0/b.
    // b = 1 ise grain() eski yolu kullanır (birebir kopya korunur). shortcut: b > 1'de bant sınırlama yok, >Nyquist/b enerji katlanır; vokalde ihmal edilebilir.
    void grainRs(const Mark& m, double s)
    {
        const float b = m.b, h = std::max(2.f, m.T0 / b);
        const i64 q0 = (i64) std::ceil(s - (double) h), q1 = (i64) std::floor(s + (double) h);
        for (i64 q = q0; q <= q1; ++q) {
            const double x = m.pos + (double) b * ((double) q - s);
            const i64 i0 = (i64) std::floor(x);
            const float t = (float) (x - (double) i0);
            const float y0 = in[(size_t) ((i0 - 1) & mask)], y1 = in[(size_t) (i0 & mask)], y2 = in[(size_t) ((i0 + 1) & mask)], y3 = in[(size_t) ((i0 + 2) & mask)];
            const float v = y1 + 0.5f * t * (y2 - y0 + t * (2.f * y0 - 5.f * y1 + 4.f * y2 - y3 + t * (3.f * (y1 - y2) + y3 - y0)));
            const float w = 0.5f * (1.f + std::cos(kPi * (float) ((double) q - s) / h));
            acc[(size_t) (q & mask)] += w * v; accW[(size_t) (q & mask)] += w;
        }
    }

    void grain(const Mark& m, double s)
    {
        if (std::fabs(m.b - 1.f) > 1e-6f) { grainRs(m, s); return; }
        const float h = std::max(2.f, m.T0);
        const double dl = m.pos - s;
        const i64 p0 = (i64) std::floor(dl);
        const float fr = (float) (dl - (double) p0);
        float cf[8], sum = 0.f;
        for (int j = -3; j <= 4; ++j) {
            const float x = (float) j - fr;
            const float sc = std::fabs(x) < 1e-6f ? 1.f : std::sin(kPi * x) / (kPi * x);
            cf[j + 3] = sc * 0.5f * (1.f + std::cos(kPi * x / 4.f)); sum += cf[j + 3];
        }
        for (float& v : cf) v /= sum;
        const i64 q0 = (i64) std::ceil(s - (double) h), q1 = (i64) std::floor(s + (double) h);
        for (i64 q = q0; q <= q1; ++q) {
            const float w = 0.5f * (1.f + std::cos(kPi * (float) ((double) q - s) / h));
            float v = 0.f;
            for (int j = -3; j <= 4; ++j) v += cf[j + 3] * in[(size_t) ((q + p0 + j) & mask)];
            acc[(size_t) (q & mask)] += w * v; accW[(size_t) (q & mask)] += w;
        }
    }

    void synth()
    {
        const double lim = (double) (cPrev - Tmax);
        while (sNext <= lim) {
            double step = 32.0;                                    // tonsuz boşluk: tane yok
            if (markCount > 0) {
                if (kPtr < markCount - kMarks + 1) kPtr = markCount - kMarks + 1;
                while (kPtr + 1 < markCount && std::fabs(mk(kPtr + 1).pos - sNext) <= std::fabs(mk(kPtr).pos - sNext)) ++kPtr;
                const Mark& m = mk(kPtr);
                if (std::fabs(m.pos - sNext) <= 0.6 * (double) m.T0) {
                    grain(m, sNext);
                    // g=0: düz hedef periyodu Ts; g=1: ölçülmüş mark aralığı / oran (Ts=T0 iken Pn) -> birebir kopya
                    const double rel = std::min(1.25, std::max(0.8, (double) m.Pn / (double) m.T0));
                    step = (double) m.Ts * ((1.0 - (double) m.g) + (double) m.g * rel);
                }
            }
            sNext += step;
        }
    }
};

} // namespace mt

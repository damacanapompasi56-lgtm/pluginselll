// CI self-test (JUCE yok). Başarısızsa exit kodu 1 -> CI kırmızı.
// argv[1] verilirse önce/sonra WAV'larını oraya yazar (CI artifact).
#include "Engine.h"
#include <algorithm>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace mt;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); ++fails; } } while (0)

static const double kPi2 = 6.28318530717958647692;
static double cents(double f, double ref) { return 1200.0 * std::log2(f / ref); }

// ---------- sentetik vokal: glottal darbe -> 3 formant, periyot jitter/shimmer, nefes, tonsuz 'ş', sessizlik ----------
struct Seg { double dur; int kind; double f0a, f0b, vibCents; int vow; };   // kind: 0 sessiz, 1 sesli, 2 tonsuz

static std::vector<float> vocal(double sr, const std::vector<Seg>& segs)
{
    static const double F[2][3] = { { 700, 1220, 2600 }, { 500, 900, 2500 } }, BW[3] = { 90, 110, 160 };
    uint32_t rs = 12345u;
    auto rnd = [&] { rs = rs * 1664525u + 1013904223u; return (double) (rs >> 8) / 8388608.0 - 1.0; };  // -1..1
    std::vector<float> y;
    double ph = 0, up = 0, jit = 1, shim = 1, res[3][2] = {}, tg = 0, n1 = 0;
    for (const Seg& s : segs) {
        const size_t N = (size_t) (s.dur * sr), ramp = (size_t) (0.01 * sr);
        for (size_t k = 0; k < N; ++k, tg += 1.0 / sr) {
            const double env = std::min({ 1.0, (double) k / ramp, (double) (N - k) / ramp });
            double o = 1e-4 * rnd();                                   // taban gürültü
            if (s.kind == 1) {
                const double f0 = (s.f0a + (s.f0b - s.f0a) * (double) k / (double) N) * std::exp2(s.vibCents * std::sin(kPi2 * 5.5 * tg) / 1200.0);
                ph += f0 * jit / sr;
                if (ph >= 1.0) { ph -= 1.0; jit = 1.0 + 0.004 * rnd(); shim = 1.0 + 0.04 * rnd(); }
                const double u = ph < 0.6 ? 0.5 * (1 - std::cos(kPi2 / 2 * ph / 0.6)) : (ph < 0.8 ? std::cos(kPi2 / 4 * (ph - 0.6) / 0.2) : 0.0);
                double e = (u - up) * sr / f0 * 0.1 * shim + 0.02 * rnd(); up = u;
                for (int f = 0; f < 3; ++f) {
                    const double r = std::exp(-kPi2 / 2 * BW[f] / sr), a1 = -2 * r * std::cos(kPi2 * F[s.vow][f] / sr);
                    const double v = (1 - r) * e - a1 * res[f][0] - r * r * res[f][1];
                    res[f][1] = res[f][0]; res[f][0] = v; e = v;
                }
                o += env * e;
            } else if (s.kind == 2) { const double w = rnd(); o += env * 0.03 * (w - n1); n1 = w; }
            y.push_back((float) o);
        }
    }
    float pk = 0.f; for (float v : y) pk = std::max(pk, std::fabs(v));
    for (float& v : y) v *= 0.5f / pk;
    return y;
}

static const std::vector<Seg> kPhrase = { { 0.25, 0, 0, 0, 0, 0 }, { 1.4, 1, 215, 215, 0, 0 }, { 0.12, 2, 0, 0, 0, 0 },
                                          { 1.2, 1, 215, 246, 20, 1 }, { 0.3, 0, 0, 0, 0, 0 } };

// bypass için: perde hep A3'te (220 Hz, vibrato +-30 cent) -> nota geçişi yok, hedef sabit, oran 1.0
static const std::vector<Seg> kOnPitch = { { 0.25, 0, 0, 0, 0, 0 }, { 1.2, 1, 220, 220, 20, 0 }, { 0.12, 2, 0, 0, 0, 0 },
                                           { 1.2, 1, 220, 220, 30, 1 }, { 0.3, 0, 0, 0, 0, 0 } };

static std::vector<float> run(const EngineParams& p, double sr, const std::vector<float>& x)
{
    Engine e; e.prepare(sr); e.set(p);
    std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = e.process(x[i]);
    return y;
}

// bağımsız ölçüm: normalize ACF + parabolik interpolasyon, fc civarı ±%15
static double acfHz(const float* y, size_t n, double sr, double fc)
{
    const int lo = (int) (sr / (fc * 1.15)), hi = (int) (sr / (fc / 1.15)) + 1;
    std::vector<double> r((size_t) hi + 2, 0.0);
    int best = lo;
    for (int l = lo - 1; l <= hi + 1; ++l) {
        double a = 0, b = 0, c = 0;
        for (size_t j = 0; j + (size_t) l < n; ++j) { a += (double) y[j] * y[j + l]; b += (double) y[j] * y[j]; c += (double) y[j + l] * y[j + l]; }
        r[(size_t) l] = a / std::sqrt(b * c + 1e-30);
        if (l >= lo && l <= hi && r[(size_t) l] > r[(size_t) best]) best = l;
    }
    const double A = r[(size_t) best - 1], B = r[(size_t) best], C = r[(size_t) best + 1], den = A - 2 * B + C;
    return sr / ((double) best + (den < -1e-12 ? 0.5 * (A - C) / den : 0.0));
}

static double maxDiff(const std::vector<float>& v) { double m = 0; for (size_t i = 1; i < v.size(); ++i) m = std::max(m, (double) std::fabs(v[i] - v[i - 1])); return m; }

static void wav(const std::string& dir, const std::string& name, const std::vector<float>& v, double sr)
{
    if (dir.empty()) return;
    std::filesystem::create_directories(dir);
    FILE* f = std::fopen((dir + "/" + name).c_str(), "wb"); if (!f) return;
    const uint32_t n = (uint32_t) v.size() * 2, sri = (uint32_t) sr, br = sri * 2, h1 = 36 + n, fmt = 16;
    const uint16_t pcm = 1, ch = 1, ba = 2, bits = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&h1, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f); std::fwrite(&fmt, 4, 1, f);
    std::fwrite(&pcm, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&sri, 4, 1, f); std::fwrite(&br, 4, 1, f);
    std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f); std::fwrite("data", 1, 4, f); std::fwrite(&n, 4, 1, f);
    for (float s : v) { const int16_t q = (int16_t) std::lround(std::max(-1.f, std::min(1.f, s)) * 32767.f); std::fwrite(&q, 2, 1, f); }
    std::fclose(f);
}

// ---------- eski sinüs testleri ----------
static double freqHz(const std::vector<float>& y, size_t a, size_t b, double sr)
{
    double first = 0, last = 0; int cnt = 0;
    for (size_t i = a + 1; i < b; ++i)
        if (y[i - 1] < 0.f && y[i] >= 0.f) {
            const double t = (double) i - 1 + (double) (-y[i - 1]) / (double) (y[i] - y[i - 1]);
            if (cnt == 0) first = t;
            last = t; ++cnt;
        }
    return cnt < 3 ? 0.0 : (cnt - 1) * sr / (last - first);
}

static void checkSine(const char* name, EngineParams p, double sr, bool fm)
{
    const size_t N = (size_t) (3.0 * sr);
    std::vector<float> x(N); double ph = 0;
    for (size_t i = 0; i < N; ++i) {
        const double t = (double) i / sr;
        ph += kPi2 * 435.0 * std::exp2(fm ? 25.0 * std::sin(kPi2 * 6 * t) / 1200.0 : 0.0) / sr;
        x[i] = 0.5f * (float) std::sin(ph);
    }
    const std::vector<float> y = run(p, sr, x);
    const size_t win = (size_t) (0.05 * sr);
    double worst = 0; int n = 0;
    for (size_t a = (size_t) sr; a + win < y.size(); a += win, ++n) {
        const double f = freqHz(y, a, a + win, sr);
        worst = std::max(worst, f > 0 ? std::fabs(cents(f, 440.0)) : 9999.0);
    }
    std::printf("%-30s sr=%6.0f  en kotu sapma = %.2f cent\n", name, sr, worst);
    CHECK(worst <= 10.0, "%s sr=%.0f: %.2f cent > 10", name, sr, worst);
}


// ---------- nota geçişi: A3 -> B3 kayması (A#3'ten geçer); düzeltme eğrisi doğrudan okunur ----------
// Engine'in uyguladığı düzeltme (dispCorr) hop başına (128 örnek, <=48 kHz) bir örnek alınır, cent cinsinden; aktif değilse NaN.
// Sesten ölçmek burada işe yaramaz: perde analizcisi 100 cent'lik sert basamağı ~8 hop'a yayıyor, Speed 0 / 100 ayırt edilemez.
static std::vector<float> runCurve(const EngineParams& p, double sr, const std::vector<float>& x, std::vector<double>& curve)
{
    Engine e; e.prepare(sr); e.set(p);
    std::vector<float> y(x.size()); curve.clear();
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = e.process(x[i]);
        if ((i + 1) % 128 == 0) curve.push_back(e.dispTarget.load() >= 0 ? 100.0 * (double) e.dispCorr.load() : std::nan(""));
    }
    return y;
}
// ardışık iki hop arasındaki en büyük düzeltme sıçraması (cent); yalnız ikisi de aktifse
static double maxJump(const std::vector<double>& c) { double m = 0; for (size_t i = 1; i < c.size(); ++i) if (!std::isnan(c[i]) && !std::isnan(c[i - 1])) m = std::max(m, std::fabs(c[i] - c[i - 1])); return m; }

static void transitionTest(double sr, const std::string& dir, const std::string& tag)
{
    const std::vector<Seg> s = { { 0.2, 0, 0, 0, 0, 0 }, { 1.0, 1, 215, 215, 0, 0 }, { 0.12, 1, 215, 250, 0, 0 },
                                 { 1.0, 1, 250, 250, 0, 0 }, { 0.3, 0, 0, 0, 0, 0 } };
    const std::vector<float> x = vocal(sr, s);
    wav(dir, "once_gecis_" + tag + ".wav", x, sr);
    std::vector<double> c;

    // Speed 0: sert kalmalı (nota basamağı >= 60 cent) ve plato'larda hedefe ±10 cent (A3 220 Hz, B3 246.94 Hz)
    {
        EngineParams p; p.mask = 0xFFF; p.speed = 0.f;
        const std::vector<float> y = runCurve(p, sr, x, c);
        double worst = 0; const size_t win = (size_t) (0.06 * sr);
        for (size_t a = (size_t) (0.7 * sr); a + win < (size_t) (1.2 * sr); a += win / 2) worst = std::max(worst, std::fabs(cents(acfHz(&y[a], win, sr, 220.0), 220.0)));
        for (size_t a = (size_t) (1.6 * sr); a + win < (size_t) (2.28 * sr); a += win / 2) worst = std::max(worst, std::fabs(cents(acfHz(&y[a], win, sr, 246.94), 246.94)));
        const double j = maxJump(c);
        std::printf("gecis Speed 0   sr=%6.0f  hedefe en kotu sapma = %.2f cent, egri sicramasi = %.1f cent/hop\n", sr, worst, j);
        CHECK(worst <= 10.0, "gecis Speed 0 sr=%.0f: hedefe %.2f cent > 10", sr, worst);
        CHECK(j >= 60.0, "gecis Speed 0 sr=%.0f: sert degil, sicrama %.1f cent < 60", sr, j);
        wav(dir, "sonra_gecis_speed0_" + tag + ".wav", y, sr);
    }
    // Speed 100: düzeltme eğrisinde ani sıçrama yok (basamak yok)
    {
        EngineParams p; p.mask = 0xFFF; p.speed = 100.f;
        const std::vector<float> y = runCurve(p, sr, x, c);
        const double j = maxJump(c);
        std::printf("gecis Speed 100 sr=%6.0f  egri sicramasi = %.2f cent/hop\n", sr, j);
        CHECK(j <= 5.0, "gecis Speed 100 sr=%.0f: egri sicramasi %.2f cent/hop > 5", sr, j);
        wav(dir, "sonra_gecis_speed100_" + tag + ".wav", y, sr);
    }
}

// ---------- Key Detect: 24 ton, sentetik melodi; plugin'de FARKLI key seçili + düzeltme AÇIK ----------
// Melodi: ~3 nota/sn, tonik üçlüsüne (özellikle tonik) çekilen basamaklı yürüyüş; cümle başı/sonları çoğunlukla (ama her zaman değil) tonik, 20 cent vibrato,
// tonun kendine göre -20..+20 cent kayması (tuning offset testi).
static std::vector<Seg> melody(int key, double secs, double detCents)
{
    static const int MAJ[7] = { 0, 2, 4, 5, 7, 9, 11 }, MIN[7] = { 0, 2, 3, 5, 7, 8, 10 };
    const int* sc = key < 12 ? MAJ : MIN; const int tonic = key % 12;
    uint32_t rs = 777u + (uint32_t) key * 7919u;
    auto rnd = [&] { rs = rs * 1664525u + 1013904223u; return (double) (rs >> 8) / 16777216.0; };
    auto hz = [&](int deg) {
        const int oct = (int) std::floor(deg / 7.0), dd = ((deg % 7) + 7) % 7;
        return 440.0 * std::exp2((48 + tonic + 12 * oct + sc[dd] + detCents / 100.0 - 69.0) / 12.0);
    };
    std::vector<Seg> s = { { 0.25, 0, 0, 0, 0, 0 } };
    double t = 0;
    while (t < secs) {
        const int len = 8 + (int) (rnd() * 5); int deg = rnd() < 0.5 ? 0 : (rnd() < 0.5 ? 2 : 4);   // cümle başı: %50 tonik
        for (int i = 0; i < len; ++i) {
            const bool last = i == len - 1;
            if (last) {                                          // cümle sonu: %70 tonik, %15 3., %10 5., %5 2. derece
                const double r = rnd(); const int e = r < 0.7 ? 0 : (r < 0.85 ? 2 : (r < 0.95 ? 4 : 1));
                deg = (e == 0 && deg > 3) ? 7 : e;
            } else if (i > 0) {                                  // komşu derecelere adım; tonik üçlüsü (0,2,4) 3 kat çekici
                double w[5], tot = 0; int cand[5], nc = 0;
                for (int st = -2; st <= 2; ++st) {
                    const int c = deg + st; if (st == 0 || c < -1 || c > 7) continue;   // tekrar yok: komşu notalar tek segmentte birleşmesin
                    const int m = ((c % 7) + 7) % 7;
                    w[nc] = (m == 0 ? 6.0 : ((m == 2 || m == 4) ? 3.0 : 1.0)) / (1.0 + std::abs(st)); cand[nc++] = c; tot += w[nc - 1];
                }
                double r = rnd() * tot; int pick = 0;
                while (pick < nc - 1 && r > w[pick]) r -= w[pick++];
                deg = cand[pick];
            }
            const bool tonicNote = (((deg % 7) + 7) % 7) == 0;
            const double dur = last ? 0.8 + 0.4 * rnd() : (0.15 + 0.2 * rnd()) * (tonicNote ? 1.6 : 1.0);   // tonik daha uzun tutulur
            s.push_back({ dur, 1, hz(deg), hz(deg), 20, i & 1 }); t += dur;
        }
        s.push_back({ 0.35, 0, 0, 0, 0, 0 }); t += 0.35;
    }
    return s;
}

static unsigned scaleMask(int tonic) { static const int M[7] = { 0, 2, 4, 5, 7, 9, 11 }; unsigned m = 0; for (int d : M) m |= 1u << ((tonic + d) % 12); return m; }
static int relativeOf(int k) { return k < 12 ? 12 + (k + 9) % 12 : (k - 12 + 3) % 12; }

static Engine* keyRun(Engine& e, double sr, const std::vector<float>& x, const EngineParams& p, int& active)
{
    e.prepare(sr); e.set(p); e.keyRequest.store(true); active = 0;
    for (size_t i = 0; i < x.size(); ++i) { e.process(x[i]); if (i % 512 == 0 && e.dispTarget.load() >= 0) ++active; }
    return &e;
}

static void keyDetectTest(double sr)
{
    int ok = 0, rel = 0;
    for (int k = 0; k < 24; ++k) {
        const std::vector<float> x = vocal(sr, melody(k, (double) kKeySeconds + 2.0, (double) ((k * 7) % 41 - 20)));
        EngineParams p; p.speed = 0.f; p.mask = scaleMask((k % 12 + 6) % 12);   // seçili key = tritone uzakta, düzeltme açık
        Engine e; int act = 0; keyRun(e, sr, x, p, act);
        const int got = e.keyResult.load(), pct = e.keyConf.load(), alt = e.keyAlt.load();
        const bool hit = got == k, isRel = !hit && got == relativeOf(k);
        ok += hit; rel += isRel;
        std::printf("  %-9s -> %-9s %%%-3d alt %-9s %s%s\n", keyName(k).c_str(), got >= 0 ? keyName(got).c_str() : "-", pct,
                    alt >= 0 ? keyName(alt).c_str() : "-", hit ? "OK" : (isRel ? "RELATIF" : "YANLIS"), act > 0 ? "" : "  (duzeltme calismadi!)");
        CHECK(e.keyState.load() == 2, "key detect bitmedi (k=%d)", k);
        CHECK(act > 0, "duzeltme acik degildi, test gecersiz (k=%d)", k);
    }
    std::printf("Key Detect sr=%6.0f  dogru %d/24, relatif karisiklik %d, diger %d\n", sr, ok, rel, 24 - ok - rel);
    CHECK(ok >= 20, "Key Detect sr=%.0f: %d/24 < 20", sr, ok);

    // bagimsizlik: ayni giris, baska key + duzeltme kapali/acik -> sonuc ayni olmali
    const std::vector<float> x = vocal(sr, melody(3, (double) kKeySeconds + 2.0, 12.0));
    int a0 = 0, a1 = 0; Engine e0, e1;
    EngineParams p0; p0.mask = 0xFFF; p0.tolCents = 50.f;
    EngineParams p1; p1.speed = 0.f; p1.mask = scaleMask(1);
    keyRun(e0, sr, x, p0, a0); keyRun(e1, sr, x, p1, a1);
    CHECK(e0.keyResult.load() == e1.keyResult.load() && e0.keyConf.load() == e1.keyConf.load(),
          "secili key/duzeltme analizi etkiliyor: %d %%%d vs %d %%%d", e0.keyResult.load(), e0.keyConf.load(), e1.keyResult.load(), e1.keyConf.load());
}

// ---------- Formant: Corrected (formantlar sabit) / Not Corrected (formantlar perdeyle kayar) ----------
// Bağımsız ölçüm (motor LPC kullanmaz): 4'e bölünmüş sinyalde 10. derece LPC (Levinson, pencere başına ortalama otokorelasyon),
// kutup frekanslarından ilk ikisi = F1, F2. Kutuplar 0..sr/8'de toplansın diye boxcar ile bölünür; yoksa yüksek frekans gürültüsüne harcanır.
static void formantsHz(const std::vector<float>& y0, size_t a0, size_t b0, double sr0, double& f1, double& f2)
{
    std::vector<float> y;
    for (size_t i = a0 / 4; i + 1 < b0 / 4; ++i) { float t = 0; for (int k = 0; k < 4; ++k) t += y0[i * 4 + (size_t) k]; y.push_back(t / 4.f); }
    const double sr = sr0 / 4; const int P = 10, W = (int) (0.03 * sr), hop = W / 2;
    std::vector<double> r((size_t) P + 1, 0.0), w((size_t) W), z((size_t) W);
    for (int i = 0; i < W; ++i) w[(size_t) i] = 0.5 - 0.5 * std::cos(kPi2 * (i + 0.5) / W);
    for (size_t s = 0; s + (size_t) W + 1 <= y.size(); s += (size_t) hop) {
        for (int i = 0; i < W; ++i) z[(size_t) i] = w[(size_t) i] * ((double) y[s + (size_t) i + 1] - 0.97 * (double) y[s + (size_t) i]);
        for (int k = 0; k <= P; ++k) { double t = 0; for (int i = k; i < W; ++i) t += z[(size_t) i] * z[(size_t) (i - k)]; r[(size_t) k] += t; }
    }
    r[0] *= 1.0001;
    std::vector<double> A((size_t) P + 1, 0.0), tmp; A[0] = 1.0; double e = r[0];
    for (int i = 1; i <= P; ++i) {
        double acc = r[(size_t) i]; for (int j = 1; j < i; ++j) acc += A[(size_t) j] * r[(size_t) (i - j)];
        const double k = -acc / e; tmp = A;
        for (int j = 1; j < i; ++j) A[(size_t) j] = tmp[(size_t) j] + k * tmp[(size_t) (i - j)];
        A[(size_t) i] = k; e *= 1.0 - k * k;
    }
    using Cx = std::complex<double>;                       // A(z) kökleri: Durand-Kerner
    std::vector<Cx> q((size_t) P);
    for (int i = 0; i < P; ++i) q[(size_t) i] = std::pow(Cx(0.4, 0.9), i);
    for (int it = 0; it < 500; ++it)
        for (int i = 0; i < P; ++i) {
            Cx num = 1.0, den = 1.0;
            for (int k = 1; k <= P; ++k) num = num * q[(size_t) i] + A[(size_t) k];
            for (int j = 0; j < P; ++j) if (j != i) den *= q[(size_t) i] - q[(size_t) j];
            q[(size_t) i] -= num / den;
        }
    std::vector<double> fr;
    for (const Cx& c : q) if (c.imag() > 0) { const double f = std::atan2(c.imag(), c.real()) * sr / kPi2, bw = -std::log(std::abs(c)) * sr / (kPi2 / 2); if (f > 150 && bw < 700) fr.push_back(f); }
    std::sort(fr.begin(), fr.end());
    f1 = fr.size() > 0 ? fr[0] : 0; f2 = fr.size() > 1 ? fr[1] : 0;
}

static void formantTest(double sr, const std::string& dir, const std::string& tag)
{
    struct Case { const char* name; double f0, tgt; unsigned mask; };
    // yukarı: 170 Hz -> A3 220 Hz (x1.29, +4.4 yt); aşağı: 220 Hz -> E3 164.8 Hz (x0.75, -5 yt). Tek perde sınıfı açık -> hedef kesin.
    const Case cases[] = { { "yukari", 170.0, 220.0, 1u << 9 }, { "asagi", 220.0, 164.8138, 1u << 4 } };
    for (const Case& c : cases) {
        const std::vector<Seg> sg = { { 0.2, 0, 0, 0, 0, 0 }, { 3.0, 1, c.f0, c.f0, 0, 0 }, { 0.2, 0, 0, 0, 0, 0 } };
        const std::vector<float> x = vocal(sr, sg);
        wav(dir, std::string("once_formant_") + c.name + "_" + tag + ".wav", x, sr);
        const size_t a = (size_t) (1.2 * sr), b = (size_t) (3.0 * sr), win = (size_t) (0.06 * sr);
        double i1, i2; formantsHz(x, a, b, sr, i1, i2);
        const double ratio = c.tgt / c.f0;
        for (bool corrected : { true, false }) {
            EngineParams p; p.mask = c.mask; p.speed = 0.f; p.formantCorrected = corrected;
            const std::vector<float> y = run(p, sr, x);
            double o1, o2; formantsHz(y, a, b, sr, o1, o2);
            const double r1 = o1 / i1, r2 = o2 / i2, want = corrected ? 1.0 : ratio;
            const double pc = cents(acfHz(&y[(size_t) (2.0 * sr)], win, sr, c.tgt), c.tgt);
            std::printf("formant %-6s %-13s sr=%6.0f  F1 %.0f->%.0f (x%.3f)  F2 %.0f->%.0f (x%.3f)  beklenen x%.3f  perde %.1f cent\n",
                        c.name, corrected ? "Corrected" : "Not Corrected", sr, i1, o1, r1, i2, o2, r2, want, pc);
            CHECK(i1 > 0 && o1 > 0 && i2 > 0 && o2 > 0, "formant %s sr=%.0f: tepe bulunamadi", c.name, sr);
            CHECK(std::fabs(r1 / want - 1.0) <= 0.05 && std::fabs(r2 / want - 1.0) <= 0.05,
                  "formant %s %s sr=%.0f: F1 x%.3f F2 x%.3f, beklenen x%.3f ±%%5", c.name, corrected ? "Corrected" : "Not Corrected", sr, r1, r2, want);
            CHECK(std::fabs(pc) <= 10.0, "formant %s %s sr=%.0f: perde %.1f cent > 10", c.name, corrected ? "Corrected" : "Not Corrected", sr, pc);
            wav(dir, std::string("sonra_formant_") + c.name + (corrected ? "_corrected_" : "_notcorrected_") + tag + ".wav", y, sr);
        }
    }
}

// Mod geçişi: sesli bölgenin ortasında Corrected -> Not Corrected -> Corrected. Patlama yok: örnek farkı iki sabit modun en kötüsünü %25'ten fazla aşmasın.
static void formantSwitchTest(double sr, const std::string& dir, const std::string& tag)
{
    const std::vector<Seg> sg = { { 0.2, 0, 0, 0, 0, 0 }, { 3.0, 1, 170, 170, 0, 0 }, { 0.2, 0, 0, 0, 0, 0 } };
    const std::vector<float> x = vocal(sr, sg);
    EngineParams p; p.mask = 1u << 9; p.speed = 0.f;
    p.formantCorrected = true;  const double dC = maxDiff(run(p, sr, x));
    p.formantCorrected = false; const double dN = maxDiff(run(p, sr, x));
    Engine e; e.prepare(sr); std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        if (i % 64 == 0) { p.formantCorrected = !(i >= (size_t) (1.2 * sr) && i < (size_t) (2.0 * sr)); e.set(p); }
        y[i] = e.process(x[i]);
    }
    const double d = maxDiff(y), lim = 1.25 * std::max(dC, dN);
    std::printf("formant gecisi  sr=%6.0f  max|dy| = %.4f  (sabit modlar: %.4f / %.4f)\n", sr, d, dC, dN);
    CHECK(d <= lim, "formant gecisi sr=%.0f: max|dy| %.4f > %.4f (patlama)", sr, d, lim);
    wav(dir, "sonra_formant_gecis_" + tag + ".wav", y, sr);
}

// CPU: Not Corrected (en pahalı yol), 10 sn vokal; gerçek zamanın %10'unu geçmesin (CI Release).
static void formantCpuTest(double sr)
{
    const std::vector<Seg> sg = { { 0.2, 0, 0, 0, 0, 0 }, { 5.0, 1, 200, 246, 25, 0 }, { 0.2, 0, 0, 0, 0, 0 }, { 5.0, 1, 300, 180, 25, 1 } };
    const std::vector<float> x = vocal(sr, sg);
    EngineParams p; p.mask = 1u << 0; p.speed = 0.f; p.formantCorrected = false;
    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<float> y = run(p, sr, x);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), cpu = 100.0 * sec / ((double) x.size() / sr);
    std::printf("formant CPU (Not Corrected)  sr=%6.0f  %.2f %% tek cekirdek\n", sr, cpu);
    CHECK(cpu <= 10.0, "formant CPU sr=%.0f: %.2f %% > 10", sr, cpu);
    CHECK(!y.empty() && std::isfinite((double) y[y.size() / 2]), "formant CPU sr=%.0f: cikis gecersiz", sr);
}

int main(int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "";
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        EngineParams p; p.mask = 0xFFF;            // kromatik, A=440
        p.speed = 0.f;   checkSine("Speed 0, sabit 435 Hz", p, sr, false);
        p.speed = -10.f; checkSine("Speed -10, sabit 435 Hz", p, sr, false);
        checkSine("Speed -10, vibratolu giris", p, sr, true);
    }

    for (double sr : { 44100.0, 48000.0 }) {
        const std::vector<float> x = vocal(sr, kPhrase);
        const std::string tag = std::to_string((int) sr);
        Engine probe; probe.prepare(sr); const int lat = probe.latency();

        // 1) sesli vokal 215 Hz -> A3 (220 Hz), Speed 0: ±10 cent
        {
            EngineParams p; p.mask = 0xFFF; p.speed = 0.f;
            std::vector<Seg> s = { { 0.2, 0, 0, 0, 0, 0 }, { 2.6, 1, 215, 215, 0, 0 } };
            const std::vector<float> xs = vocal(sr, s), ys = run(p, sr, xs);
            double worst = 0; const size_t win = (size_t) (0.06 * sr);
            for (size_t a = (size_t) (1.0 * sr); a + win < ys.size(); a += win / 2)
                worst = std::max(worst, std::fabs(cents(acfHz(&ys[a], win, sr, 220.0), 220.0)));
            std::printf("vokal Speed 0 -> 220 Hz         sr=%6.0f  en kotu sapma = %.2f cent\n", sr, worst);
            CHECK(worst <= 10.0, "vokal pitch sr=%.0f: %.2f cent > 10", sr, worst);
        }
        // 2) bypass: düzeltme oranı 1.0 (Tolerance 50 cent = hep tolerans içinde) -> gecikmeli girişle fark < -80 dB
        {
            EngineParams p; p.mask = 0xFFF; p.speed = 0.f; p.tolCents = 50.f;
            const std::vector<float> xb = vocal(sr, kOnPitch), y = run(p, sr, xb);
            double e = 0, d = 0;
            for (size_t i = (size_t) lat; i < xb.size(); ++i) { const double r = (double) y[i] - xb[i - (size_t) lat]; e += r * r; d += (double) xb[i - (size_t) lat] * xb[i - (size_t) lat]; }
            const double db = 10 * std::log10(e / d + 1e-30);
            std::printf("bypass (oran 1.0)               sr=%6.0f  fark = %.1f dB\n", sr, db);
            CHECK(db < -80.0, "bypass sr=%.0f: %.1f dB (> -80)", sr, db);
        }
        transitionTest(sr, dir, tag);

        // 3) süreksizlik: ardışık örnek farkı girişin 1.5 katını geçmesin (Speed 0 ve 40 ms)
        for (float sp : { 0.f, 40.f }) {
            EngineParams p; p.mask = 0xFFF; p.speed = sp;
            const std::vector<float> y = run(p, sr, x);
            const double r = maxDiff(y) / maxDiff(x);
            std::printf("max |dy| / max |dx|, Speed %-3.0f  sr=%6.0f  oran = %.2f\n", sp, sr, r);
            CHECK(r <= 1.5, "sureksizlik sr=%.0f speed=%.0f: oran %.2f > 1.5", sr, sp, r);
            if (sp == 0.f) { wav(dir, "once_giris_" + tag + ".wav", x, sr); wav(dir, "sonra_cikis_speed0_" + tag + ".wav", y, sr); }
            else wav(dir, "sonra_cikis_speed40_" + tag + ".wav", y, sr);
        }
    }
    for (double sr : { 44100.0, 48000.0 }) {
        const std::string tag = std::to_string((int) sr);
        formantTest(sr, dir, tag); formantSwitchTest(sr, dir, tag); formantCpuTest(sr);
    }
    for (double sr : { 44100.0, 48000.0 }) keyDetectTest(sr);
    std::printf(fails ? "SELFTEST FAILED\n" : "SELFTEST OK\n");
    return fails ? 1 : 0;
}

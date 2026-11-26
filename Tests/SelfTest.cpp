// CI self-test (JUCE yok). Başarısızsa exit kodu 1 -> CI kırmızı.
// argv[1] verilirse önce/sonra WAV'larını oraya yazar (CI artifact).
#include "Engine.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
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
    std::printf(fails ? "SELFTEST FAILED\n" : "SELFTEST OK\n");
    return fails ? 1 : 0;
}

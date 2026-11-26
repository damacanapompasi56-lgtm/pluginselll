// CI self-test (JUCE yok). Başarısızsa exit kodu 1 -> CI kırmızı.
#include "Engine.h"
#include <cstdio>
#include <vector>

using namespace mt;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); ++fails; } } while (0)

// pozitif yönlü sıfır geçişlerinden (lineer interpolasyonlu) frekans
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

static double cents(double f, double ref) { return 1200.0 * std::log2(f / ref); }

// 435 Hz sinüs (fm: ±25 cent, 6 Hz vibrato) -> motor
static std::vector<float> run(EngineParams p, double sr, double secs, bool fm)
{
    Engine e; e.prepare(sr); e.set(p);
    const size_t N = (size_t) (secs * sr);
    std::vector<float> y(N);
    double ph = 0;
    for (size_t i = 0; i < N; ++i) {
        const double t = (double) i / sr;
        const double f = 435.0 * std::exp2(fm ? 25.0 * std::sin(2 * 3.14159265358979 * 6 * t) / 1200.0 : 0.0);
        ph += 2 * 3.14159265358979 * f / sr;
        y[i] = e.process(0.5f * (float) std::sin(ph));
    }
    return y;
}

// 1 sn'den sonra, 50 ms pencerelerin hepsi 440 Hz'e ±10 cent içinde olmalı
static void check(const char* name, EngineParams p, double sr, bool fm)
{
    const std::vector<float> y = run(p, sr, 3.0, fm);
    const size_t win = (size_t) (0.05 * sr);
    double worst = 0; int n = 0;
    for (size_t a = (size_t) sr; a + win < y.size(); a += win, ++n) {
        const double f = freqHz(y, a, a + win, sr);
        const double c = f > 0 ? std::fabs(cents(f, 440.0)) : 9999.0;
        if (c > worst) worst = c;
    }
    std::printf("%-28s sr=%6.0f  en kotu sapma = %.2f cent (%d pencere)\n", name, sr, worst, n);
    CHECK(worst <= 10.0, "%s sr=%.0f: %.2f cent > 10", name, sr, worst);
}

int main()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        EngineParams p; p.mask = 0xFFF;           // kromatik, A=440
        p.speed = 0.f;  check("Speed 0, sabit 435 Hz", p, sr, false);
        p.speed = -10.f; check("Speed -10, sabit 435 Hz", p, sr, false);
        check("Speed -10, vibratolu giris", p, sr, true);   // nota icinde sapma olmamali
    }
    std::printf(fails ? "SELFTEST FAILED\n" : "SELFTEST OK\n");
    return fails ? 1 : 0;
}

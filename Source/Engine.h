#pragma once
// MADENCITUNE DSP motoru. Saf C++ (JUCE yok) -> CI'da self-test ile doğrudan test edilir.
//
// Akış (mono, örnek örnek):
//   giriş -> halka tampon + 4x/8x decimate -> her H örnekte (~3 ms): MPM pitch (kaba: decimate,
//   ince: tam hızda) -> voiced/unvoiced + median + oktav koruması -> hedef nota (histerezis)
//   -> tolerans / glide / Speed üstel yaklaşım / vibrato -> analiz işaretleri (T0) ve çıkış periyodu (Ts)
//   -> PSOLA: Hann pencereli 2*T0'lık taneler, Ts aralıkla toplanır (formant korunur)
//   -> voiced maskesine göre dry ile crossfade. Gecikme sabit: L = 3*Tmax + 64 örnek.
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
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
    unsigned mask = 0xFFF;  // izinli perde sınıfları, bit0 = C
};

class Engine {
public:
    // UI okur (atomik)
    std::atomic<float> dispMidi{ -1.f };   // algılanan perde (MIDI, ref'e göre), <0 = yok
    std::atomic<int>   dispTarget{ -1 };   // hedef MIDI notası, -1 = yok
    std::atomic<bool>  keyRequest{ false };
    std::atomic<int>   keyState{ 0 };      // 0 boşta, 1 dinliyor, 2 bitti
    std::atomic<int>   keyResult{ -1 };    // 0..11 major, 12..23 minor, -1 belirsiz
    std::atomic<float> keyProgress{ 0.f };

    bool ready() const { return N > 0; }
    int latency() const { return L; }
    void set(const EngineParams& p) { P = p; }

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        d = sr > 60000.0 ? 8 : 4;
        srD = sr / d;
        TmaxD = (int) std::ceil(srD / kMinDetectHz);
        Tmax = TmaxD * d;
        Wd = 2 * TmaxD;
        H = 32 * d;
        L = 3 * Tmax + 64;
        N = nextPow2(8 * Tmax); mask = N - 1;
        in.assign((size_t) N, 0.f); acc.assign((size_t) N, 0.f); accW.assign((size_t) N, 0.f); vmask.assign((size_t) N, 0.f);
        const int ND = nextPow2(2 * Wd); decMask = ND - 1; dec.assign((size_t) ND, 0.f);
        yD.assign((size_t) Wd, 0.f); nsdf.assign((size_t) TmaxD + 3, 0.f); nsdfF.assign((size_t) (2 * d + 3), 0.f);
        hopSec = (float) H / (float) sr;
        vibN = std::min(128, std::max(2, (int) std::lround(0.16 / hopSec)));
        vA = 1.f - std::exp(-1.f / (0.004f * (float) sr));
        aAtk = 1.f - std::exp(-hopSec / 0.3f);
        aRel = 1.f - std::exp(-hopSec / 1.5f);
        n = decN = 0; decAcc = 0.f; decCnt = 0; prevX = 0.f; hopSumSq = hopSumD2 = 0.f;
        first = true; started = false; cPrev = 0; voiced = false; unv = 0; octCnt = 0; mi = 0;
        haveTarget = hadGoal = false; glideLeft = 0; corr = 0.f; outsideMs = 0.f; vibCount = 0;
        runActive = false; markCount = 0; kPtr = 0; sNext = 0.0; vmix = 0.f; dens = 0.f; medInit = false;
        midiF = 0.f; keyFrames = keyVoiced = 0;
    }

    float process(float x)
    {
        in[(size_t) (n & mask)] = x;
        const float dx = x - prevX; prevX = x;
        hopSumSq += x * x; hopSumD2 += dx * dx;
        decAcc += x;
        if (++decCnt == d) { dec[(size_t) (decN & decMask)] = decAcc / (float) d; ++decN; decAcc = 0.f; decCnt = 0; }
        ++n;
        if (n % H == 0) analyse();
        synth();

        const i64 o = n - 1 - L;       // çıkış zamanı (giriş indeksi cinsinden)
        if (o < 0) return 0.f;
        const size_t i = (size_t) (o & mask);
        const float wet = acc[i] / std::fmax(accW[i], 0.25f);
        vmix += vA * (vmask[i] - vmix);
        const float y = vmix * wet + (1.f - vmix) * in[i];
        acc[i] = 0.f; accW[i] = 0.f;
        return y;
    }

private:
    static constexpr int kNone = -1000;
    static constexpr int kMarks = 256;
    struct Mark { i64 pos; float T0, Ts; };
    struct Pitch { float hz, conf; };

    EngineParams P;
    double sr = 44100.0, srD = 11025.0;
    int d = 4, TmaxD = 0, Tmax = 0, Wd = 0, H = 128, L = 0, N = 0, vibN = 55, decMask = 0;
    i64 mask = 0;
    float hopSec = 0.003f, vA = 0.f, aAtk = 0.f, aRel = 0.f;
    std::vector<float> in, acc, accW, vmask, dec, yD, nsdf, nsdfF;

    i64 n = 0, decN = 0, cPrev = 0;
    float decAcc = 0.f, prevX = 0.f, hopSumSq = 0.f, hopSumD2 = 0.f;
    int decCnt = 0;

    // yumuşatılmış parametreler
    bool first = true, started = false;
    float sRef = kRefDefault, sSpeed = 0.f, sGlide = 0.f, sTolC = 0.f, sTolT = 0.f, sVib = 100.f;

    // perde takibi
    bool voiced = false;
    int unv = 0, octCnt = 0, mi = 0;
    float med[3] = { 0, 0, 0 }, track = 0.f, midiF = 0.f;

    // hedef / düzeltme
    bool haveTarget = false, hadGoal = false;
    int target = 0, lastTarget = 0, glideLeft = 0;
    float goal = 0.f, corr = 0.f, outsideMs = 0.f;

    // vibrato merkezi (kayan ortalama)
    float vibBuf[128] = {}, vibSum = 0.f;
    int vibIdx = 0, vibCount = 0;

    // yoğunluk
    float dens = 0.f, medTrack = 0.f;
    bool medInit = false;

    // PSOLA
    Mark marks[kMarks];
    int markCount = 0, kPtr = 0;
    bool runActive = false;
    i64 nextMark = 0;
    double sNext = 0.0;
    float vmix = 0.f;

    // key detect
    float hist[12] = {};
    int keyFrames = 0, keyVoiced = 0;

    Mark& mk(int i) { return marks[i & (kMarks - 1)]; }

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
        int pick = 0;
        for (int k = 0; k < nk; ++k)
            if (keyPos[k] >= minLag && nsdf[(size_t) keyPos[k]] >= 0.9f * hi) { pick = keyPos[k]; break; }
        if (pick == 0) return out;

        float del = 0.f;
        {
            const float a = nsdf[(size_t) pick - 1], b = nsdf[(size_t) pick], c = nsdf[(size_t) pick + 1];
            const float den = a - 2.f * b + c;
            if (den < -1e-9f) del = fclamp(0.5f * (a - c) / den, -1.f, 1.f);
        }
        const float Tc = ((float) pick + del) * (float) d;

        // ince arama (tam hız), pencere = son 2*Tmax örnek
        const int W = 2 * Tmax;
        const i64 s0 = n - W;
        const int Lc = (int) std::lround(Tc);
        const int lo = std::max(2, Lc - d - 1), hi2 = Lc + d + 1;
        float bestV = -2.f; int bestL = lo;
        for (int l = lo; l <= hi2; ++l) {
            float rr = 0.f, mm = 0.f;
            for (int j = 0; j < W - l; ++j) {
                const float a = in[(size_t) ((s0 + j) & mask)], b = in[(size_t) ((s0 + j + l) & mask)];
                rr += a * b; mm += a * a + b * b;
            }
            const float v = mm > 1e-9f ? 2.f * rr / mm : 0.f;
            nsdfF[(size_t) (l - lo)] = v;
            if (v > bestV) { bestV = v; bestL = l; }
        }
        float Tf = (float) bestL, peak = bestV;
        if (bestL > lo && bestL < hi2) {
            const float a = nsdfF[(size_t) (bestL - lo - 1)], b = nsdfF[(size_t) (bestL - lo)], c = nsdfF[(size_t) (bestL - lo + 1)];
            const float den = a - 2.f * b + c;
            if (den < -1e-9f) { const float dl = fclamp(0.5f * (a - c) / den, -1.f, 1.f); Tf += dl; peak = b - 0.25f * (a - c) * dl; }
        }
        Tf = fclamp(Tf, 2.f, (float) Tmax);
        out.hz = (float) sr / Tf;
        out.conf = fclamp(peak, 0.f, 1.f);
        return out;
    }

    void updateDensity(float rmsDb, float bw, bool v)
    {
        const float loud = fclamp((rmsDb + 45.f) / 30.f, 0.f, 1.f);
        const float bright = fclamp((bw - 500.f) / 2500.f, 0.f, 1.f);  // rms-frekans, centroid vekili
        const float height = v ? fclamp(0.5f + (midiF - medTrack) / 6.f, 0.f, 1.f) : 0.5f;
        const float raw = rmsDb < -55.f ? 0.f : 0.5f * loud + 0.2f * bright + 0.3f * height;
        dens += (raw > dens ? aAtk : aRel) * (raw - dens);  // attack hızlı, release yavaş
    }

    void finishKey()
    {
        int best = -1; float bestR = -2.f;
        if ((float) keyVoiced * hopSec >= 1.5f) {
            float mean = 0.f; for (float h : hist) mean += h; mean /= 12.f;
            for (int k = 0; k < 24; ++k) {
                const float* prof = k < 12 ? kKsMajor : kKsMinor;
                float pm = 0.f; for (int i = 0; i < 12; ++i) pm += prof[i]; pm /= 12.f;
                float sxy = 0.f, sxx = 0.f, syy = 0.f;
                for (int i = 0; i < 12; ++i) {
                    const float x = hist[(k % 12 + i) % 12] - mean, y = prof[i] - pm;
                    sxy += x * y; sxx += x * x; syy += y * y;
                }
                const float rr = sxy / (std::sqrt(sxx * syy) + 1e-12f);
                if (rr > bestR) { bestR = rr; best = k; }
            }
        }
        keyResult.store(bestR >= 0.55f ? best : -1);
        keyState.store(2);
    }

    void makeMarks(i64 c, float T0, float Ts)
    {
        if (!runActive) { nextMark = c; runActive = true; }
        while (nextMark <= c) {
            const int r = (int) (T0 * 0.2f);
            i64 best = nextMark; float bv = -1e9f;
            for (i64 p = nextMark - r; p <= nextMark + r; ++p) {
                const float v = in[(size_t) (p & mask)];
                if (v > bv) { bv = v; best = p; }
            }
            if (markCount > 0 && best <= mk(markCount - 1).pos) best = mk(markCount - 1).pos + 1;
            mk(markCount++) = Mark{ best, T0, Ts };
            nextMark = best + (i64) std::lround(T0);
        }
    }

    void analyse()
    {
        const float ms = hopSumSq / (float) H;
        const float rmsLin = std::sqrt(ms);
        const float rmsDb = 10.f * std::log10(ms + 1e-12f);
        const float bw = std::sqrt(hopSumD2 / (hopSumSq + 1e-12f)) * (float) sr / (2.f * kPi);
        hopSumSq = hopSumD2 = 0.f;
        if (n < 2 * (i64) Tmax) return;
        const i64 c = n - Tmax;  // pencerenin merkezi: bu konuma kadar işaretler üretilir
        if (!started) { started = true; cPrev = c - H; }
        const float hopMs = hopSec * 1000.f;

        // parametre yumuşatma (zipper yok): ~20 ms, hop hızında
        const float a = first ? 1.f : 1.f - std::exp(-hopSec / 0.02f);
        first = false;
        sRef += a * (P.refHz - sRef);   sSpeed += a * (P.speed - sSpeed);
        sGlide += a * (P.glideMs - sGlide); sTolC += a * (P.tolCents - sTolC);
        sTolT += a * (P.tolMs - sTolT); sVib += a * (P.vibPct - sVib);
        const float hardness = fclamp(sSpeed / kSpeedMin, 0.f, 1.f);  // 0..1, sadece Speed<0'da > 0

        // 1) perde + voiced
        const Pitch pd = detect();
        const bool strong = pd.hz > 0.f && rmsDb > -55.f && pd.conf >= (voiced ? 0.6f : 0.75f);
        unv = strong ? 0 : (unv < 100 ? unv + 1 : unv);
        const bool vNow = strong || (voiced && unv < 4);
        if (strong) {
            float m = 69.f + 12.f * std::log2(pd.hz / sRef);
            if (!voiced) { med[0] = med[1] = med[2] = m; octCnt = 0; }
            else {
                if (std::fabs(m - track) > 7.f) {          // oktav/büyük sıçrama koruması
                    if (++octCnt < 10) m = track; else { octCnt = 0; med[0] = med[1] = med[2] = m; }
                } else octCnt = 0;
                med[mi] = m; mi = (mi + 1) % 3;
            }
            track = m;
            midiF = std::fmax(std::fmin(med[0], med[1]), std::fmin(std::fmax(med[0], med[1]), med[2]));
        }
        if (vNow && !voiced) { corr = 0.f; haveTarget = hadGoal = false; vibCount = 0; outsideMs = 0.f; runActive = false; }
        if (!vNow && voiced) { haveTarget = hadGoal = false; runActive = false; }
        voiced = vNow;

        // 2) Key Detect
        if (keyRequest.exchange(false)) {
            for (float& h : hist) h = 0.f;
            keyFrames = keyVoiced = 0; keyProgress.store(0.f); keyState.store(1);
        }
        if (keyState.load() == 1) {
            if (strong) {
                const int pc = ((int) std::lround(midiF) % 12 + 12) % 12;
                hist[pc] += pd.conf * rmsLin;   // confidence * enerji
                ++keyVoiced;
            }
            ++keyFrames;
            keyProgress.store((float) keyFrames * hopSec / kKeySeconds);
            if ((float) keyFrames * hopSec >= kKeySeconds) finishKey();
        }

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
                if (!hadGoal) { goal = (float) target; lastTarget = target; glideLeft = 0; hadGoal = true; }
                else if (target != lastTarget) {
                    const float g = fclamp(sGlide, kGlideMin, kGlideMax) * (1.f - hardness);  // sertlikle glide -> 0
                    glideLeft = (int) std::lround(g / hopMs); lastTarget = target;
                }
                if (glideLeft > 0) { goal += ((float) target - goal) / (float) glideLeft; --glideLeft; }
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
                const float al = tau <= 0.f ? 1.f : 1.f - std::exp(-hopMs / tau);
                corr += al * (want - corr);

                const float fIn = sRef * std::exp2((midiF - 69.f) / 12.f);
                const float fOut = sRef * std::exp2((midiF + corr - 69.f) / 12.f);
                const float T0 = fclamp((float) sr / fIn, (float) sr / 1500.f, (float) Tmax);
                const float Ts = fclamp((float) sr / fOut, 0.5f * T0, 2.f * T0);
                makeMarks(c, T0, Ts);
                active = true;
            } else hadGoal = false;
        } else updateDensity(rmsDb, bw, false);

        if (!active) runActive = false;
        for (i64 p = cPrev + 1; p <= c; ++p) vmask[(size_t) (p & mask)] = active ? 1.f : 0.f;
        cPrev = c;
        dispMidi.store(voiced ? midiF : -1.f);
        dispTarget.store(active ? target : -1);
    }

    void grain(const Mark& m, double s)
    {
        const int h = std::max(2, (int) std::lround(m.T0));
        const i64 sc = (i64) std::floor(s);
        const float fr = (float) (s - (double) sc);   // kesirli yerleşim: periyot jitter'ı yok
        for (int i = -h; i <= h; ++i) {
            const float w = 0.5f * (1.f + std::cos(kPi * (float) i / (float) h));
            const i64 p = m.pos + i;
            const float v = in[(size_t) ((p - 1) & mask)] * fr + in[(size_t) (p & mask)] * (1.f - fr);
            const size_t k = (size_t) ((sc + i) & mask);
            acc[k] += w * v; accW[k] += w;
        }
    }

    void synth()
    {
        const double lim = (double) (cPrev - Tmax);
        while (sNext <= lim) {
            bool placed = false;
            if (markCount > 0) {
                if (kPtr < markCount - kMarks + 1) kPtr = markCount - kMarks + 1;
                while (kPtr + 1 < markCount && std::fabs((double) mk(kPtr + 1).pos - sNext) <= std::fabs((double) mk(kPtr).pos - sNext)) ++kPtr;
                const Mark& m = mk(kPtr);
                if (std::fabs((double) m.pos - sNext) <= 0.6 * m.T0) { grain(m, sNext); sNext += m.Ts; placed = true; }
            }
            if (!placed) sNext += 32.0;   // tonsuz boşluk: tane yok
        }
    }
};

} // namespace mt

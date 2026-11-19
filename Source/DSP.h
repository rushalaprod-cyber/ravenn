#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

// Briques DSP maison : aucune allocation, utilisables sur le thread audio.
namespace gdsp
{
constexpr double pi = 3.14159265358979323846;

//==============================================================================
// Biquad (formules RBJ), forme directe transposee II, en double.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double s1 = 0, s2 = 0;

    void reset() noexcept { s1 = s2 = 0; }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        const double inv = 1.0 / na0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv;
        a1 = na1 * inv; a2 = na2 * inv;
    }

    static double clampF (double fs, double f) noexcept { return std::clamp (f, 1.0, fs * 0.49); }

    void lowpass (double fs, double f, double q) noexcept
    {
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void highpass (double fs, double f, double q) noexcept
    {
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void peak (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    void lowShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        const double sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq),
             2 * A * ((A - 1) - (A + 1) * c),
             A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq,
             -2 * ((A - 1) + (A + 1) * c),
             (A + 1) + (A - 1) * c - sq);
    }

    void highShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        const double sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq),
             -2 * A * ((A - 1) + (A + 1) * c),
             A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq,
             2 * ((A - 1) - (A + 1) * c),
             (A + 1) - (A - 1) * c - sq);
    }
};

//==============================================================================
// Linkwitz-Riley 24 dB/oct (deux Butterworth en cascade).
struct LR4
{
    Biquad a, b;
    void setLowpass (double fs, double f) noexcept  { a.lowpass (fs, f, 0.7071067811865476);  b.lowpass (fs, f, 0.7071067811865476); }
    void setHighpass (double fs, double f) noexcept { a.highpass (fs, f, 0.7071067811865476); b.highpass (fs, f, 0.7071067811865476); }
    inline double process (double x) noexcept { return b.process (a.process (x)); }
    void reset() noexcept { a.reset(); b.reset(); }
};

// Filtre de bande : passe-haut (si lowHz > 0) puis passe-bas (si highHz > 0).
struct BandFilter
{
    LR4 hp, lp;
    bool useHP = false, useLP = false;

    void set (double fs, double lowHz, double highHz) noexcept
    {
        useHP = lowHz > 0;
        useLP = highHz > 0;
        if (useHP) hp.setHighpass (fs, lowHz);
        if (useLP) lp.setLowpass (fs, highHz);
    }

    inline double process (double x) noexcept
    {
        if (useHP) x = hp.process (x);
        if (useLP) x = lp.process (x);
        return x;
    }

    void reset() noexcept { hp.reset(); lp.reset(); }
};

// Bloqueur de DC du 1er ordre.
struct DCBlock
{
    double x1 = 0, y1 = 0, R = 0.9995;
    void set (double fs, double f) noexcept { R = 1.0 - 2.0 * pi * f / fs; }
    inline double process (double x) noexcept
    {
        const double y = x - x1 + R * y1;
        x1 = x; y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0; }
};

// EQ de couleur : tilt (pivot 1 kHz) + low shelf 100 Hz + air 12 kHz.
struct ColorEQ
{
    Biquad tLo, tHi, low, air;

    void set (double fs, double tiltDb, double lowDb, double airDb) noexcept
    {
        tLo.lowShelf (fs, 1000.0, 0.5, -tiltDb * 0.5);
        tHi.highShelf (fs, 1000.0, 0.5, tiltDb * 0.5);
        low.lowShelf (fs, 100.0, 0.707, lowDb);
        air.highShelf (fs, 12000.0, 0.707, airDb);
    }

    inline double process (double x) noexcept { return air.process (low.process (tHi.process (tLo.process (x)))); }
    void reset() noexcept { tLo.reset(); tHi.reset(); low.reset(); air.reset(); }
};

//==============================================================================
// Suiveur d'enveloppe RMS (attaque / relachement) pour les saturations
// "independantes du niveau" : elles mordent pareil a -30 dB qu'a 0 dB.
struct EnvFollower
{
    double e2 = 0, att = 0.01, rel = 0.001;
    void set (double fs, double attackMs, double releaseMs) noexcept
    {
        att = 1.0 - std::exp (-1.0 / (attackMs * 0.001 * fs));
        rel = 1.0 - std::exp (-1.0 / (releaseMs * 0.001 * fs));
    }
    inline double process (double x) noexcept
    {
        const double s = x * x;
        e2 += (s > e2 ? att : rel) * (s - e2);
        return std::sqrt (e2);
    }
    void reset() noexcept { e2 = 0; }
};

// Saturation a niveau constant : le signal est normalise par son enveloppe,
// sature fort, puis remis a son niveau. drive >= 1.
inline double levelSat (double b, double env, double drive, double bias) noexcept
{
    constexpr double floor = 1.0e-3; // -60 dBFS : en dessous, on s'efface (pas de bruit remonte)
    const double ef = std::max (env, floor);
    const double gate = std::min (1.0, env / floor);
    const double tb = std::tanh (bias);
    const double y = (std::tanh (drive * b / ef + bias) - tb) / std::tanh (drive);
    return ef * y * gate + b * (1.0 - gate);
}

//==============================================================================
// Rampe lineaire pour lisser les parametres (pas de "zipper noise").
struct Ramp
{
    double cur = 0, tgt = 0, step = 0;
    bool first = true;

    void set (double t, int n) noexcept
    {
        if (first) { cur = tgt = t; step = 0; first = false; return; }
        tgt = t;
        step = n > 0 ? (tgt - cur) / n : 0.0;
        if (step == 0.0) cur = tgt;
    }
    inline double next() noexcept { cur += step; return cur; }
    void finish() noexcept { cur = tgt; step = 0; }
    void snap() noexcept { first = true; }
};

//==============================================================================
// Fonctions de transfert (sorties bornees a +-1 pour les clippers).
namespace shape
{
    inline double hard (double x) noexcept { return std::clamp (x, -1.0, 1.0); }

    // Lineaire jusqu'a 0.6 puis genou doux vers 1.0
    inline double soft (double x) noexcept
    {
        constexpr double k = 0.6;
        const double a = std::abs (x);
        if (a <= k) return x;
        const double s = k + (1.0 - k) * std::tanh ((a - k) / (1.0 - k));
        return x < 0 ? -s : s;
    }

    // Saturation "a lampe" : tanh legerement asymetrique (harmoniques paires)
    inline double analog (double x) noexcept { return std::tanh (x + 0.12 * x * x / (1.0 + x * x)); }

    // Saturateur asymetrique normalise (pente 1 autour de 0)
    inline double asym (double x, double drive, double bias) noexcept
    {
        const double tb = std::tanh (bias);
        const double slope = 1.0 - tb * tb;
        return (std::tanh (drive * x + bias) - tb) / (drive * slope);
    }
}

//==============================================================================
// Mesure de loudness ITU-R BS.1770 / EBU R128 : momentane (400 ms),
// court terme (3 s) et integre (gating absolu -70 LUFS + relatif -10 LU).
class LoudnessMeter
{
public:
    std::atomic<float> momentary { -100.f }, shortTerm { -100.f }, integrated { -100.f };

    void prepare (double fs, int nch)
    {
        numCh = std::clamp (nch, 1, 2);
        for (auto& f : shelf) f.highShelf (fs, 1681.974450955533, 0.7071752369554196, 3.999843853973347);
        for (auto& f : hp)    f.highpass (fs, 38.13547087602444, 0.5003270373238773);
        blockLen = std::max (1, (int) std::lround (fs * 0.1));
        reset();
    }

    void reset()
    {
        for (auto& f : shelf) f.reset();
        for (auto& f : hp) f.reset();
        acc = 0; cnt = 0; ringPos = 0; numBlocks = 0;
        ring.fill (0); histCount.fill (0); histSum.fill (0);
        momentary = -100.f; shortTerm = -100.f; integrated = -100.f;
    }

    void process (float* const* ch, int nch, int n)
    {
        nch = std::min (nch, numCh);
        for (int i = 0; i < n; ++i)
        {
            double sum = 0;
            for (int c = 0; c < nch; ++c)
            {
                const double y = hp[(size_t) c].process (shelf[(size_t) c].process (ch[c][i]));
                sum += y * y;
            }
            acc += sum;
            if (++cnt >= blockLen)
            {
                push (acc / blockLen);
                acc = 0; cnt = 0;
            }
        }
    }

private:
    static constexpr int ringSize = 30, numBins = 750; // bins de 0.1 LU de -70 a +5

    static float toLufs (double p) noexcept { return p > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (p)) : -100.f; }

    double meanLast (int k) const noexcept
    {
        k = (int) std::min<long long> (k, numBlocks);
        if (k <= 0) return 0;
        double s = 0;
        for (int j = 1; j <= k; ++j)
            s += ring[(size_t) ((ringPos - j + ringSize) % ringSize)];
        return s / k;
    }

    void push (double ms)
    {
        ring[(size_t) ringPos] = ms;
        ringPos = (ringPos + 1) % ringSize;
        ++numBlocks;

        const double m = meanLast (4);
        momentary = toLufs (m);
        shortTerm = toLufs (meanLast (30));

        if (numBlocks >= 4)
        {
            const float l = toLufs (m);
            if (l > -70.f)
            {
                const int b = std::clamp ((int) ((l + 70.f) * 10.f), 0, numBins - 1);
                histCount[(size_t) b] += 1.0;
                histSum[(size_t) b] += m;
            }
        }

        double tc = 0, ts = 0;
        for (int b = 0; b < numBins; ++b) { tc += histCount[(size_t) b]; ts += histSum[(size_t) b]; }
        if (tc <= 0) { integrated = -100.f; return; }

        const float gate = toLufs (ts / tc) - 10.f;
        double rc = 0, rs = 0;
        for (int b = 0; b < numBins; ++b)
        {
            const float lb = -70.f + ((float) b + 0.5f) * 0.1f;
            if (lb >= gate) { rc += histCount[(size_t) b]; rs += histSum[(size_t) b]; }
        }
        integrated = rc > 0 ? toLufs (rs / rc) : -100.f;
    }

    std::array<Biquad, 2> shelf, hp;
    int numCh = 2, blockLen = 4800, cnt = 0, ringPos = 0;
    long long numBlocks = 0;
    double acc = 0;
    std::array<double, ringSize> ring {};
    std::array<double, numBins> histCount {}, histSum {};
};

} // namespace gdsp

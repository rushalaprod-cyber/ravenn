#pragma once
// Modules ajoutes en 1.2 : limiteur, saturation multibande, caractere
// analogique / vinyle, Fresh Air, 3D, petite reverb, presence voix,
// sidechain voix, EQ de correspondance (reference).
// Tout est original : aucun code de plugin tiers n'a ete lu ni copie.
#include "DSP.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>
#include <cstdint>

namespace gdsp
{

//==============================================================================
// Allpass du 2e ordre (RBJ)
inline void makeAllpass (Biquad& b, double fs, double f, double q) noexcept
{
    const double w = 2 * pi * Biquad::clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
    b.set (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
}

//==============================================================================
// LIMITEUR brickwall a anticipation.
// Gain requis -> maintien du minimum sur la fenetre d'anticipation ->
// relachement (dependant du programme) -> moyenne glissante (rampe d'attaque).
// La moyenne de minimums couvrant le pic garantit qu'aucun echantillon ne
// depasse le plafond. Detection true peak par interpolation 4x (Hermite).
struct Limiter
{
    // Styles 8 a 11 : les 4 modes du Stealth Limiter (Tight, Balanced, Harmonics 1 lampe, Harmonics 2 transistor)
    enum Style { Transparent = 0, Punchy, Dynamic, Allround, Aggressive, Modern, Bus, Safe,
                 StealthTight, StealthBalanced, StealthHarm1, StealthHarm2, numStyles };

    struct Settings
    {
        double gainDb = 0, ceilDb = -0.3, lookMs = 2, attackMs = 1, releaseMs = 80;
        double linkT = 0.75, linkR = 0.5, soft = 0;
        int style = 0;
        bool truePeak = true;
    };

    double fs = 48000;
    int maxLen = 1, L = 0, A = 1;
    std::vector<double> dly[2];
    int dPos = 0;

    // min glissant (deque monotone) + moyenne glissante, par canal
    std::vector<double> minVal[2]; std::vector<int> minIdx[2];
    int mHead[2] {}, mTail[2] {};
    std::vector<double> avgBuf[2]; double avgSum[2] {}; int avgPos = 0;
    long long t = 0;

    // Detection true peak : interpolation 4x par sinus cardinal fenetre (16 points).
    // Elle a D echantillons de retard, compenses en allongeant le retard du signal.
    static constexpr int tpTaps = 16, tpD = 8;
    double hist[2][tpTaps] {};
    double tpCoef[3][tpTaps] {};
    int D = 0;
    double gRel[2] { 1, 1 }, gSlow[2] { 1, 1 };
    double relC = 0, relSlowC = 0, ceil = 1, gainLin = 1, preclip = 0, tpMargin = 1;
    int harm = 0;
    Settings cur;
    double lastGr = 1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        maxLen = (int) std::ceil (0.0052 * fs) + 4 + tpD;
        for (int ph = 0; ph < 3; ++ph)
        {
            const double f = 0.25 * (ph + 1);
            double sum = 0;
            for (int j = 0; j < tpTaps; ++j)
            {
                const double d = (double) j - 7.0 - f;
                const double sinc = std::abs (d) < 1e-9 ? 1.0 : std::sin (pi * d) / (pi * d);
                const double w = 0.5 + 0.5 * std::cos (pi * std::clamp (d / 8.5, -1.0, 1.0));
                tpCoef[ph][j] = sinc * w;
                sum += tpCoef[ph][j];
            }
            for (int j = 0; j < tpTaps; ++j) tpCoef[ph][j] /= sum;
        }
        for (int c = 0; c < 2; ++c)
        {
            dly[c].assign ((size_t) maxLen + 2, 0.0);
            minVal[c].assign ((size_t) maxLen + 4, 1.0);
            minIdx[c].assign ((size_t) maxLen + 4, 0);
            avgBuf[c].assign ((size_t) maxLen + 2, 1.0);
        }
        reset();
    }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill (dly[c].begin(), dly[c].end(), 0.0);
            std::fill (avgBuf[c].begin(), avgBuf[c].end(), 1.0);
            avgSum[c] = (double) A;
            mHead[c] = mTail[c] = 0;
            gRel[c] = gSlow[c] = 1.0;
            for (auto& h : hist[c]) h = 0;
        }
        dPos = avgPos = 0; t = 0;
    }

    static int lookFor (double fs, double lookMs) { return (int) std::lround (std::clamp (lookMs, 0.0, 5.0) * 0.001 * fs); }
    // latence totale : anticipation + retard de la detection true peak (0 si pas d'anticipation)
    static int latencyFor (double fs, double lookMs) { const int l = lookFor (fs, lookMs); return l > 0 ? l + tpD : 0; }

    void configure (const Settings& s) noexcept
    {
        const int newL = std::clamp (lookFor (fs, s.lookMs), 0, maxLen - 2 - tpD);
        const int newD = newL > 0 ? tpD : 0;
        if (newD != D) { D = newD; L = -1; }
        double attack = std::min (s.attackMs, s.lookMs);
        static constexpr double relMul[numStyles]  = { 1.4, 0.8, 0.5, 1.0, 0.35, 0.7, 2.0, 3.0, 1.0, 1.7, 1.1, 1.1 };
        static constexpr double atkMul[numStyles]  = { 1.0, 0.45, 0.8, 1.0, 0.3, 0.7, 1.0, 1.0, 0.6, 1.0, 0.7, 0.7 };
        static constexpr double clipAmt[numStyles] = { 0.0, 0.15, 0.0, 0.05, 0.4, 0.25, 0.0, 0.0, 0.6, 0.35, 0.3, 0.3 };
        static constexpr int harmType[numStyles]   = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2 };
        harm = harmType[std::clamp (s.style, 0, numStyles - 1)];
        const int st = std::clamp (s.style, 0, numStyles - 1);
        attack *= atkMul[st];
        const int newA = std::clamp ((int) std::lround (attack * 0.001 * fs), 1, std::max (1, newL));
        if (newL != L || newA != A) { L = newL; A = newA; reset(); }

        const double rel = std::max (1.0, s.releaseMs * relMul[st]);
        relC = 1.0 - std::exp (-1.0 / (rel * 0.001 * fs));
        relSlowC = 1.0 - std::exp (-1.0 / (rel * 4.0 * 0.001 * fs));
        // marge : 3 points interpoles ne voient pas tout a fait le vrai pic (~0,15 dB)
        tpMargin = (st == Safe ? 0.97 : 1.0) * (s.truePeak ? 0.982 : 1.0);
        ceil = std::pow (10.0, s.ceilDb / 20.0) * tpMargin;
        gainLin = std::pow (10.0, s.gainDb / 20.0);
        preclip = std::clamp (clipAmt[st] + s.soft, 0.0, 1.0);
        cur = s;
    }

    // Pic autour de l'echantillon h[7] (= x(t-8)) : lui-meme + 3 points interpoles apres lui
    inline double interPeak (const double* h) const noexcept
    {
        double m = std::abs (h[7]);
        for (int ph = 0; ph < 3; ++ph)
        {
            double y = 0;
            for (int j = 0; j < tpTaps; ++j) y += tpCoef[ph][j] * h[j];
            m = std::max (m, std::abs (y));
        }
        return m;
    }

    inline void pushMin (int c, double v) noexcept
    {
        const int cap = (int) minVal[c].size();
        while (mTail[c] != mHead[c])
        {
            const int last = (mTail[c] - 1 + cap) % cap;
            if (minVal[c][(size_t) last] >= v) mTail[c] = last; else break;
        }
        minVal[c][(size_t) mTail[c]] = v; minIdx[c][(size_t) mTail[c]] = (int) (t & 0x3fffffff);
        mTail[c] = (mTail[c] + 1) % cap;
        while (mHead[c] != mTail[c])
        {
            const int idx = minIdx[c][(size_t) mHead[c]];
            const int age = ((int) (t & 0x3fffffff) - idx) & 0x3fffffff;
            if (age > L) mHead[c] = (mHead[c] + 1) % cap; else break;
        }
    }

    void process (double* x, int nch) noexcept
    {
        const double softRef = ceil * 1.4;
        double req[2] { 1, 1 };
        for (int c = 0; c < nch; ++c)
        {
            double v = x[c] * gainLin;
            if (harm == 1)        // Harmonics 1 : couleur lampe (paires)
                v = ceil * (std::tanh (v / ceil * 0.9 + 0.12 * (v / ceil) * (v / ceil) / (1.0 + (v / ceil) * (v / ceil))) / 0.9);
            else if (harm == 2)   // Harmonics 2 : couleur transistor (impaires)
                v = ceil * std::atan (v / ceil * 1.1) / std::atan (1.1);
            if (preclip > 0)   // arrondit les pointes avant le limiteur
                v += preclip * (softRef * shape::soft (v / softRef) - v);
            x[c] = v;

            double pk;
            if (D > 0)
            {
                auto* h = hist[c];
                for (int j = 0; j < tpTaps - 1; ++j) h[j] = h[j + 1];
                h[tpTaps - 1] = v;
                pk = cur.truePeak ? interPeak (h) : std::abs (h[7]);
            }
            else pk = std::abs (v);
            req[c] = pk > ceil ? ceil / pk : 1.0;
        }
        if (nch == 2)
        {
            const double lk = std::min (req[0], req[1]);
            for (int c = 0; c < 2; ++c) req[c] = cur.linkT * lk + (1.0 - cur.linkT) * req[c];
        }

        double g[2] { 1, 1 };
        for (int c = 0; c < nch; ++c)
        {
            pushMin (c, req[c]);
            const double held = minVal[c][(size_t) mHead[c]];
            // relachement dependant du programme : rapide sur un pic isole,
            // plus lent quand la limitation est dense (moins de pompage)
            gSlow[c] += (held < gSlow[c] ? relC * 0.25 : relSlowC) * (held - gSlow[c]);
            if (held < gRel[c]) gRel[c] = held;
            else
            {
                const double k = gSlow[c] < gRel[c] * 0.97 ? 0.5 * (relC + relSlowC) : relC;
                gRel[c] = std::min (held, gRel[c] + k * (held - gRel[c]));
            }
            g[c] = gRel[c];
        }
        if (nch == 2)
        {
            const double lk = std::min (g[0], g[1]);
            for (int c = 0; c < 2; ++c) g[c] = cur.linkR * lk + (1.0 - cur.linkR) * g[c];
        }

        double grMin = 1;
        const int rd = (dPos - L - D + (int) dly[0].size()) % (int) dly[0].size();
        for (int c = 0; c < nch; ++c)
        {
            // moyenne glissante sur A echantillons
            auto& ab = avgBuf[c];
            const int ap = avgPos % A;
            avgSum[c] += g[c] - ab[(size_t) ap];
            ab[(size_t) ap] = g[c];
            const double gs = std::min (1.0, avgSum[c] / (double) A);

            dly[c][(size_t) dPos] = x[c];
            const double y = dly[c][(size_t) rd] * gs;
            x[c] = std::clamp (y, -ceil, ceil);
            grMin = std::min (grMin, gs);
        }
        avgPos = (avgPos + 1) % A;
        dPos = (dPos + 1) % (int) dly[0].size();
        ++t;
        lastGr = grMin;
    }
};

//==============================================================================
// SATURATION MULTIBANDE (3 bandes, 7 styles)
namespace satshape
{
    inline double apply (double x, int style) noexcept
    {
        switch (style)
        {
            case 0:  return std::tanh (x + 0.08 * x * x) - std::tanh (0.0);                  // CLEAN TUBE
            case 1:  return std::tanh (x + 0.30 * x * x / (1.0 + 0.5 * x * x));                // WARM TUBE
            case 2:  { const double y = x / (1.0 + std::abs (x)); return 1.6 * y - 0.6 * y * y * y; } // TAPE
            case 3:  return (2.0 / pi) * std::atan (1.6 * x) / 0.9;                            // TRANSISTOR
            case 4:  { const double b = x + 0.25; return std::tanh (1.5 * b) - std::tanh (0.375); } // AMP
            case 5:  return x + 0.5 * (std::abs (x) - 0.6366 * std::min (1.0, std::abs (x)));   // RECTIFY (octave)
            case 6:  return std::sin (std::clamp (x, -4.0, 4.0) * 1.2) / 1.2;                    // FOLDBACK
            default: return x;
        }
    }
}

struct MultibandSat
{
    std::array<std::array<LR4, 2>, 2> split;
    std::array<DCBlock, 2> dc;
    double fs = 0, lo = 0, hi = 0;

    void setXover (double sampleRate, double loHz, double hiHz) noexcept
    {
        const bool fsChanged = sampleRate != fs;
        fs = sampleRate;
        if (fsChanged || loHz != lo || hiHz != hi)
        {
            lo = loHz; hi = hiHz;
            for (int c = 0; c < 2; ++c) { split[0][(size_t) c].setLowpass (fs, lo); split[1][(size_t) c].setLowpass (fs, hi); }
        }
        for (auto& d : dc) d.set (fs, 10.0);
    }
    void reset() noexcept { for (auto& s : split) for (auto& f : s) f.reset(); for (auto& d : dc) d.reset(); }

    // drive lineaire par bande (1 = neutre), style par bande, mix global
    inline double process (int c, double x, const double* drive, const int* style, double mix) noexcept
    {
        const double b0 = split[0][(size_t) c].process (x);
        const double rest = x - b0;
        const double b1 = split[1][(size_t) c].process (rest);
        const double band[3] { b0, b1, rest - b1 };
        double y = 0;
        // pente a l'origine de chaque style (pour un niveau coherent a faible drive)
        static constexpr double slope[7] = { 1.0, 1.0, 1.6, 1.132, 1.306, 1.0, 1.0 };
        for (int b = 0; b < 3; ++b)
        {
            if (drive[b] <= 1.0001) { y += band[b]; continue; }
            const int st = std::clamp (style[b], 0, 6);
            // sortie / drive^0,75 : a peu pres le meme volume percu quand on pousse
            // les graves gardent plus de niveau (0,55 au lieu de 0,75) et recoivent des harmoniques
            // paires : on les entend aussi sur de petites enceintes
            double s = satshape::apply (band[b] * drive[b], st) / (slope[st] * std::pow (drive[b], b == 0 ? 0.55 : 0.75));
            if (b == 0)
            {
                const double h = 1.0 - 1.0 / drive[b];
                s += 0.9 * h * s * std::abs (s) + 0.25 * h * std::tanh (3.0 * s);
            }
            y += band[b] + mix * (s - band[b]);
        }
        return dc[(size_t) c].process (y);
    }
};

//==============================================================================
// CARACTERE : bandes, consoles, transfo, vinyle (EQ + saturation propres a
// chaque type). Le pleurage / scintillement est traite a part (Wow).
struct Character
{
    enum Type { Tape15 = 0, Tape30, Cassette, TubeConsole, ClassAConsole, Transformer, Vinyl, numTypes };
    std::array<Biquad, 2> eq1, eq2, eq3, lfLP, lfHP;
    int type = -1; double amt = -1, fs = 0;

    void set (double sampleRate, int t, double amount) noexcept
    {
        if (t == type && amount == amt && sampleRate == fs) return;
        type = t; amt = amount; fs = sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            auto& a = eq1[(size_t) c]; auto& b = eq2[(size_t) c]; auto& d = eq3[(size_t) c];
            switch (t)
            {
                case Tape15:        a.peak (fs, 60, 1.0, 2.0 * amount);  b.highShelf (fs, 12000, 0.7, -1.5 * amount); d.peak (fs, 30, 0.8, -1.5 * amount); break;
                case Tape30:        a.peak (fs, 40, 0.9, 1.2 * amount);  b.highShelf (fs, 16000, 0.7, -0.5 * amount); d.peak (fs, 20, 0.8, -1.0 * amount); break;
                case Cassette:      a.peak (fs, 100, 1.0, 1.0 * amount); b.lowpass (fs, 18000 - 8000 * amount, 0.6);  d.peak (fs, 3500, 1.0, 0.8 * amount); break;
                case TubeConsole:   a.peak (fs, 120, 0.7, 0.8 * amount); b.peak (fs, 3000, 0.8, 0.6 * amount);      d.highShelf (fs, 15000, 0.7, -0.4 * amount); break;
                case ClassAConsole: a.peak (fs, 80, 0.8, 0.7 * amount);  b.peak (fs, 1600, 0.6, 0.4 * amount);      d.highShelf (fs, 14000, 0.7, -0.6 * amount); break;
                case Transformer:   a.lowShelf (fs, 60, 0.7, 0.6 * amount); b.highShelf (fs, 18000, 0.7, -0.8 * amount); d.peak (fs, 200, 0.7, 0.3 * amount); break;
                default:            a.highpass (fs, 15 + 10 * amount, 0.7); b.lowpass (fs, 18000 - 5000 * amount, 0.6); d.peak (fs, 2500, 0.7, 0.8 * amount); break;
            }
            lfLP[(size_t) c].lowpass (fs, 150, 0.7);
            lfHP[(size_t) c].highpass (fs, 150, 0.7);
        }
    }
    void reset() noexcept
    {
        for (auto* arr : { &eq1, &eq2, &eq3, &lfLP, &lfHP })
            for (auto& f : *arr) f.reset();
    }

    // Saturation par modele : drive modere, sortie / drive^0,85 pour garder le
    // volume a peu pres constant (pas de chute de niveau quand on monte AMOUNT).
    inline double process (int c, double x) noexcept
    {
        static constexpr double k[numTypes] = { 0.9, 0.6, 1.4, 1.0, 0.8, 1.2, 0.4 };
        const int t = std::clamp (type, 0, numTypes - 1);
        const double d = 1.0 + k[t] * amt;
        const double norm = std::pow (d, 0.85);
        double y = x;
        switch (t)
        {
            case Tape15: case Tape30:
            {
                // bande : tanh + leger arrondi asymetrique (bias de la bande)
                const double u = d * x;
                y = x + amt * ((std::tanh (u + 0.04 * u * u) - std::tanh (0.0)) / norm - x);
                break;
            }
            case Cassette:      { const double u = d * x; y = x + amt * (std::tanh (1.2 * u) / 1.2 / norm - x); break; }
            case TubeConsole:   { const double u = d * x; y = x + amt * (std::tanh (u + 0.15 * u * u) / norm - x); break; }
            case ClassAConsole: { const double u = d * x; y = x + amt * (std::tanh (u) / norm - x); break; }
            case Transformer:
            {
                const double lo = lfLP[(size_t) c].process (x);
                y = x + amt * (std::tanh (lo * d * 1.5) / (1.5 * norm) - lo);
                break;
            }
            default: { const double u = d * x; y = x + amt * (std::tanh (u) / norm - x); break; }
        }
        return eq3[(size_t) c].process (eq2[(size_t) c].process (eq1[(size_t) c].process (y)));
    }
};

// Pleurage / scintillement (bande, vinyle) : ligne a retard modulee, base fixe.
struct Wow
{
    std::vector<double> buf[2];
    int pos = 0, size = 0;
    double fs = 48000, base = 0, ph1 = 0, ph2 = 0, rnd = 0;
    uint32_t seed = 1234567u;

    static int baseSamples (double fs) { return (int) std::lround (0.0012 * fs); }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        base = (double) baseSamples (fs);
        size = (int) (base * 2 + 8);
        for (auto& b : buf) b.assign ((size_t) size, 0.0);
        pos = 0;
    }
    void reset() noexcept { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0); }

    // depth 0..1, vinyl = true : 0,55 Hz (33 tours), sinon bande 0,8 Hz + 6 Hz
    inline void process (double* x, int nch, double depth, bool vinyl) noexcept
    {
        ph1 += 2 * pi * (vinyl ? 0.55 : 0.8) / fs; if (ph1 > 2 * pi) ph1 -= 2 * pi;
        ph2 += 2 * pi * 6.3 / fs;                  if (ph2 > 2 * pi) ph2 -= 2 * pi;
        seed = seed * 1664525u + 1013904223u;
        rnd += 0.0005 * (((double) (seed >> 8) / 16777216.0 - 0.5) - rnd);
        const double mod = depth * base * (0.85 * std::sin (ph1) + 0.12 * std::sin (ph2) + 0.6 * rnd);
        const double d = std::clamp (base + mod, 1.0, (double) size - 3);
        for (int c = 0; c < nch; ++c)
        {
            auto& b = buf[c];
            b[(size_t) pos] = x[c];
            const double rp = (double) pos - d + (double) size;
            const int i0 = (int) rp % size, i1 = (i0 + 1) % size;
            const double fr = rp - std::floor (rp);
            x[c] = b[(size_t) i0] + fr * (b[(size_t) i1] - b[(size_t) i0]);
        }
        pos = (pos + 1) % size;
    }
};

// Craquements de vinyle (uniquement si l'utilisateur monte NOISE)
struct Crackle
{
    uint32_t seed = 987654321u;
    Biquad tone[2];
    double env = 0;
    void prepare (double fs) { for (auto& t : tone) t.peak (fs, 3000, 0.6, 6.0); }
    inline double rnd() noexcept { seed = seed * 1664525u + 1013904223u; return (double) (seed >> 8) / 16777216.0; }
    inline void process (double* x, int nch, double amount, double fs) noexcept
    {
        if (amount <= 0) return;
        if (rnd() < amount * 18.0 / fs) env = (0.2 + 0.8 * rnd()) * 0.02 * amount;
        env *= 0.92;
        const double pop = env * (rnd() * 2 - 1);
        for (int c = 0; c < nch; ++c) x[c] += tone[c].process (pop * (c == 0 ? 1.0 : 0.8));
    }
};

//==============================================================================
// FRESH AIR : deux etages (MID AIR 5 kHz, HIGH AIR 11 kHz). Shelf dynamique
// (recule sur les sifflantes) + excitation harmonique douce.
struct FreshAir
{
    std::array<Biquad, 2> midShelf, highShelf, midHP, highHP;
    Biquad detHP;
    double envHF = 0, envAll = 0, fs = 0, dyn = 1, aC = 0, rC = 0;
    int cnt = 0; double lastMid = -1, lastHigh = -1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        detHP.highpass (fs, 6500, 0.7);
        for (int c = 0; c < 2; ++c) { midHP[(size_t) c].highpass (fs, 4500, 0.7); highHP[(size_t) c].highpass (fs, 10000, 0.7); }
        aC = 1.0 - std::exp (-1.0 / (0.002 * fs)); rC = 1.0 - std::exp (-1.0 / (0.06 * fs));
        lastMid = lastHigh = -1;
    }
    void reset() noexcept
    {
        for (auto* arr : { &midShelf, &highShelf, &midHP, &highHP }) for (auto& f : *arr) f.reset();
        detHP.reset(); envHF = envAll = 0;
    }

    void process (double* x, int nch, double mid, double high) noexcept
    {
        const double m = nch > 1 ? 0.5 * (x[0] + x[1]) : x[0];
        const double h = detHP.process (m);
        envHF += (h * h > envHF ? aC : rC) * (h * h - envHF);
        envAll += (m * m > envAll ? aC : rC) * (m * m - envAll);

        if (++cnt >= 32 || lastMid < 0)
        {
            cnt = 0;
            const double ratioDb = 10.0 * std::log10 ((envHF + 1e-12) / (envAll + 1e-12));
            const double target = std::clamp (1.0 - std::max (0.0, ratioDb + 14.0) / 10.0, 0.25, 1.0);
            dyn += 0.3 * (target - dyn);
            const double md = mid * 4.0 * dyn, hd = high * 5.0 * dyn;
            if (std::abs (md - lastMid) > 0.01 || std::abs (hd - lastHigh) > 0.01)
            {
                for (int c = 0; c < 2; ++c) { midShelf[(size_t) c].highShelf (fs, 5000, 0.6, md); highShelf[(size_t) c].highShelf (fs, 11000, 0.6, hd); }
                lastMid = md; lastHigh = hd;
            }
        }
        for (int c = 0; c < nch; ++c)
        {
            double v = highShelf[(size_t) c].process (midShelf[(size_t) c].process (x[c]));
            const double em = midHP[(size_t) c].process (x[c]), eh = highHP[(size_t) c].process (x[c]);
            // excitation : harmoniques impaires + paires discretes dans le haut du spectre
            v += mid * (0.6 * (em - std::tanh (2.5 * em) / 2.5) + 1.0 * em * std::abs (em));
            v += high * 4.8 * eh * std::abs (eh);
            x[c] = v;
        }
    }
};

//==============================================================================
// 3D / PROFONDEUR (mono-compatible : on ne touche qu'au Side, sauf une
// legere presence sur le Mid) :
//  - le Side existant est decorrele dans les aigus (allpass) ;
//  - une copie dephasee des aigus du Mid est injectee dans le Side : ca ouvre
//    meme un mix tres mono, et ca disparait parfaitement en mono (L+R) ;
//  - le Mid recoit une petite presence a 3,2 kHz (il "avance").
struct Depth3D
{
    Biquad sideHP, midHP, front, sideLP, midLP;
    std::array<Biquad, 4> ap, ap2;
    double fs = 48000, lastAmt = -1;
    double hiF = 20000, q = 0.6;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        sideHP.highpass (fs, std::max (40.0, cutF * 0.31), q + 0.1);
        midHP.highpass (fs, cutF, q);
        sideLP.lowpass (fs, std::min (hiF, fs * 0.45), q + 0.1);
        midLP.lowpass (fs, std::min (hiF, fs * 0.45), q);
        static constexpr double f[4]  = { 700, 1700, 3900, 8200 };
        static constexpr double f2[4] = { 2300, 4100, 6900, 11000 };
        for (int i = 0; i < 4; ++i) { makeAllpass (ap[(size_t) i], fs, f[i], 0.6); makeAllpass (ap2[(size_t) i], fs, f2[i], 0.5); }
        lastAmt = -1;
    }
    void reset() noexcept { sideHP.reset(); midHP.reset(); sideLP.reset(); midLP.reset(); front.reset(); for (auto& a : ap) a.reset(); for (auto& a : ap2) a.reset(); }
    double cutF = 800;
    // filtre reglable : en dessous, la 3D ne touche a rien (graves solides)
    // plage de la 3D : FROM (passe-haut) - TO (passe-bas), Q = raideur / resonance des bords
    void setRange (double lo, double hi, double qq) noexcept
    {
        if (lo == cutF && hi == hiF && qq == q) return;
        cutF = lo; hiF = hi; q = qq;
        midHP.highpass (fs, lo, q);
        sideHP.highpass (fs, std::max (40.0, lo * 0.31), q + 0.1);
        midLP.lowpass (fs, std::min (hi, fs * 0.45), q);
        sideLP.lowpass (fs, std::min (hi, fs * 0.45), q + 0.1);
    }
    void setFreq (double f) noexcept { setRange (f, hiF, q); }

    inline void process (double& M, double& S, double amt) noexcept
    {
        if (amt != lastAmt) { front.peak (fs, 3200, 0.8, 1.6 * amt); lastAmt = amt; }
        double d = sideHP.process (S);
        if (hiF < 19000) d = sideLP.process (d);
        for (auto& a : ap) d = a.process (d);
        double e = midHP.process (M);
        if (hiF < 19000) e = midLP.process (e);
        for (auto& a : ap2) e = a.process (e);
        S += amt * (0.75 * d + 0.5 * e);
        M = front.process (M);
    }
};

//==============================================================================
// FATTEN : compression ecrasante + saturation (epaissit et colle tout).
// COLOR : pre-accentuation avant la saturation (sombre <-> brillant).
// Conception originale, dans l'esprit des "fatteners" a un bouton.
struct Fattener
{
    std::array<Biquad, 2> pre, post, low;
    double env = 0, aC = 0, rC = 0, fs = 0, lastAmt = -1, lastCol = -1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aC = 1.0 - std::exp (-1.0 / (0.0008 * fs));
        rC = 1.0 - std::exp (-1.0 / (0.06 * fs));
        lastAmt = lastCol = -1;
    }
    void reset() noexcept { for (auto* a : { &pre, &post, &low }) for (auto& f : *a) f.reset(); env = 0; }

    void set (double amt, double col) noexcept
    {
        if (amt == lastAmt && col == lastCol) return;
        lastAmt = amt; lastCol = col;
        const double tilt = (col - 0.5) * 12.0;
        for (int c = 0; c < 2; ++c)
        {
            pre[(size_t) c].highShelf (fs, 1500, 0.5, tilt);
            post[(size_t) c].highShelf (fs, 1500, 0.5, -tilt * 0.8);
            low[(size_t) c].lowShelf (fs, 90, 0.7, 2.5 * amt);
        }
    }

    inline void process (double* x, int nch, double amt) noexcept
    {
        const double drive = std::pow (10.0, 18.0 * amt / 20.0);
        double det = 0;
        for (int c = 0; c < nch; ++c) det = std::max (det, std::abs (x[c]) * drive);
        env += (det > env ? aC : rC) * (det - env);
        constexpr double thr = 0.25, ratio = 8.0;
        const double gr = env > thr ? std::pow (env / thr, -(1.0 - 1.0 / ratio)) : 1.0;
        const double wetAmt = std::min (1.0, amt * 1.5);
        for (int c = 0; c < nch; ++c)
        {
            double v = pre[(size_t) c].process (x[c]) * drive * gr;
            v = std::tanh (v * 1.2) / 1.2;
            v = low[(size_t) c].process (post[(size_t) c].process (v)) * 2.2;
            x[c] += wetAmt * (v - x[c]);
        }
    }
};

//==============================================================================
// IMAGER 3 bandes : largeur du Side par bande (mono-compatible).
struct Imager
{
    std::array<LR4, 2> split;
    double fs = 0, lo = 0, hi = 0;
    void set (double sampleRate, double loHz, double hiHz) noexcept
    {
        if (sampleRate == fs && loHz == lo && hiHz == hi) return;
        fs = sampleRate; lo = loHz; hi = hiHz;
        split[0].setLowpass (fs, lo); split[1].setLowpass (fs, hi);
    }
    void reset() noexcept { for (auto& s : split) s.reset(); }
    inline double process (double S, const double* w) noexcept
    {
        const double b0 = split[0].process (S), rest = S - b0;
        const double b1 = split[1].process (rest), b2 = rest - b1;
        return b0 * w[0] + b1 * w[1] + b2 * w[2];
    }
};

//==============================================================================
// STABILIZER : EQ dynamique qui ramene en douceur l'equilibre par octave vers
// une courbe cible (celle de la reference si elle est chargee).
struct Stabilizer
{
    static constexpr int nb = 10;
    static constexpr double fc[nb] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    std::array<BandFilter, nb> det;
    std::array<double, nb> pw {}, g {}, setG {};
    std::array<std::array<Biquad, 2>, nb> eq;
    double fs = 0, sm = 0;
    int cnt = 0;
    bool active = false;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int b = 0; b < nb; ++b)
        {
            const double lo = fc[b] / std::sqrt (2.0), hi = fc[b] * std::sqrt (2.0);
            det[(size_t) b].set (fs, lo, hi < fs * 0.45 ? hi : 0.0);
            for (auto& f : eq[(size_t) b]) f.peak (fs, fc[b], 1.2, 0.0);
        }
        sm = 1.0 - std::exp (-1.0 / (1.2 * fs));
        reset();
    }
    void reset() noexcept
    {
        for (auto& d : det) d.reset();
        for (auto& a : eq) for (auto& f : a) f.reset();
        pw.fill (0); g.fill (0); setG.fill (0); active = false;
    }

    // target : energie relative par octave en dB (somme = 0 dB)
    inline void process (double* x, int nch, double amt, const float* target) noexcept
    {
        const double m = nch > 1 ? 0.5 * (x[0] + x[1]) : x[0];
        for (int b = 0; b < nb; ++b) { const double v = det[(size_t) b].process (m); pw[(size_t) b] += sm * (v * v - pw[(size_t) b]); }

        if (++cnt >= 64)
        {
            cnt = 0;
            double tot = 0;
            for (auto v : pw) tot += v;
            active = false;
            for (int b = 0; b < nb; ++b)
            {
                double tgt = 0;
                if (tot > 1.0e-7 && amt > 0.0)
                {
                    const double rel = 10.0 * std::log10 (std::max (pw[(size_t) b], 1e-20) / tot);
                    const double lim = (b == 0 || b == nb - 1) ? 3.0 : 6.0;
                    tgt = std::clamp (-(rel - target[b]) * 0.6 * amt, -lim * amt, lim * amt);
                }
                g[(size_t) b] += 0.02 * (tgt - g[(size_t) b]);
                if (std::abs (g[(size_t) b] - setG[(size_t) b]) > 0.05)
                {
                    for (auto& f : eq[(size_t) b]) f.peak (fs, fc[b], 1.2, g[(size_t) b]);
                    setG[(size_t) b] = g[(size_t) b];
                }
                active = active || std::abs (setG[(size_t) b]) > 0.01;
            }
        }
        if (! active) return;
        for (int c = 0; c < nch; ++c)
            for (int b = 0; b < nb; ++b)
                x[c] = eq[(size_t) b][(size_t) c].process (x[c]);
    }
};

//==============================================================================
// VOICE FOCUS (sans sidechain) : fait ressortir la voix d'un mix.
// La voix est au centre : on la pousse sur le Mid (presence), on enleve un peu
// de boue (300 Hz) et on degage le Side dans sa zone (300 Hz - 5 kHz) pour la
// demasquer. Mono-compatible.
struct VoiceFocus
{
    Biquad bell, mud, sHP, sLP;
    double fs = 0, lastAmt = -1, lastF = -1, q = 0.8, lastQ = -1;
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        sHP.highpass (fs, 300, 0.7); sLP.lowpass (fs, 5000, 0.7);
        lastAmt = lastF = -1;
    }
    void reset() noexcept { bell.reset(); mud.reset(); sHP.reset(); sLP.reset(); }
    inline void process (double& M, double& S, double amt, double freq) noexcept
    {
        if (amt != lastAmt || freq != lastF || q != lastQ)
        {
            lastQ = q;
            bell.peak (fs, freq, q, 4.0 * amt);
            mud.peak (fs, 320, 0.9, -2.0 * amt);
            lastAmt = amt; lastF = freq;
        }
        const double vb = sLP.process (sHP.process (S));
        S -= amt * 0.8 * vb;
        M = mud.process (bell.process (M));
    }
};

//==============================================================================
// SPACE : toute petite reverb (FDN 8 lignes) pour la fluidite.
struct SpaceVerb
{
    static constexpr int nL = 8;
    std::array<std::vector<double>, nL> line;
    std::array<int, nL> len {}, pos {};
    std::array<double, nL> lp {};
    std::vector<double> pre; int prePos = 0, preLen = 1;
    Biquad inHP;
    double fs = 48000, fb = 0.7, damp = 0.3;
    double lastSize = -1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < nL; ++i) line[(size_t) i].assign ((size_t) (0.06 * fs) + 4, 0.0);
        preLen = std::max (1, (int) (0.008 * fs));
        pre.assign ((size_t) preLen, 0.0);
        inHP.highpass (fs, lowCut, 0.7);
        lastSize = -1;
        reset();
    }
    void reset() noexcept
    {
        for (auto& l : line) std::fill (l.begin(), l.end(), 0.0);
        std::fill (pre.begin(), pre.end(), 0.0);
        lp.fill (0); pos.fill (0); prePos = 0; inHP.reset();
    }
    double lowCut = 220;
    void setLowCut (double f) noexcept { if (f != lowCut) { lowCut = f; inHP.highpass (fs, f, 0.7); } }
    void setSize (double size) noexcept
    {
        if (size == lastSize) return;
        lastSize = size;
        static constexpr double ms[nL] = { 11.3, 13.7, 17.9, 19.1, 23.3, 27.7, 29.9, 31.7 };
        const double sc = 0.55 + 0.9 * size;
        for (int i = 0; i < nL; ++i)
            len[(size_t) i] = std::clamp ((int) (ms[i] * sc * 0.001 * fs), 8, (int) line[(size_t) i].size() - 2);
        const double rt60 = 0.25 + 0.9 * size;
        const double avg = 21.8 * sc * 0.001;
        fb = std::pow (10.0, -3.0 * avg / rt60);
        damp = 0.35 - 0.15 * size;
    }

    // Renvoie la queue stereo (a ajouter au signal)
    inline void process (double inL, double inR, double& outL, double& outR) noexcept
    {
        const double in = inHP.process (0.5 * (inL + inR));
        const double dIn = pre[(size_t) prePos];
        pre[(size_t) prePos] = in; prePos = (prePos + 1) % preLen;

        std::array<double, nL> o {};
        double sum = 0;
        for (int i = 0; i < nL; ++i)
        {
            auto& l = line[(size_t) i];
            const int rp = (pos[(size_t) i] - len[(size_t) i] + (int) l.size()) % (int) l.size();
            double v = l[(size_t) rp];
            lp[(size_t) i] += (1.0 - damp) * (v - lp[(size_t) i]);
            o[(size_t) i] = lp[(size_t) i];
            sum += o[(size_t) i];
        }
        const double hh = 2.0 / nL * sum;   // Householder
        outL = outR = 0;
        for (int i = 0; i < nL; ++i)
        {
            auto& l = line[(size_t) i];
            const double v = fb * (o[(size_t) i] - hh) + dIn * ((i & 1) ? 0.35 : -0.35);
            l[(size_t) pos[(size_t) i]] = v;
            pos[(size_t) i] = (pos[(size_t) i] + 1) % (int) l.size();
            if (i & 1) outR += o[(size_t) i]; else outL += o[(size_t) i];
        }
        outL *= 0.5; outR *= 0.5;
    }
};

//==============================================================================
// SIDECHAIN VOIX : la voix (entree sidechain) creuse la prod la ou elle chante.
struct VoiceDuck
{
    static constexpr int nb = 6;
    static constexpr double fc[nb] = { 250, 500, 1000, 2000, 4000, 8000 };
    std::array<Biquad, nb> det;
    std::array<std::array<Biquad, 2>, nb> cut;
    std::array<double, nb> env {}, cutDb {};
    double fs = 48000, aC = 0, rC = 0;
    int cnt = 0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int b = 0; b < nb; ++b)
        {
            const double w = 2 * pi * fc[b] / fs, c = std::cos (w), al = std::sin (w) / (2 * 1.4);
            det[(size_t) b].set (al, 0, -al, 1 + al, -2 * c, 1 - al);   // passe-bande
            for (auto& f : cut[(size_t) b]) f.peak (fs, fc[b], 1.4, 0.0);
        }
        aC = 1.0 - std::exp (-1.0 / (0.005 * fs)); rC = 1.0 - std::exp (-1.0 / (0.15 * fs));
        reset();
    }
    void reset() noexcept
    {
        for (auto& d : det) d.reset();
        for (auto& c : cut) for (auto& f : c) f.reset();
        env.fill (0); cutDb.fill (0);
    }

    // sc : echantillon mono de la voix ; x : prod (traitee sur place)
    inline void process (double sc, double* x, int nch, double depth) noexcept
    {
        for (int b = 0; b < nb; ++b)
        {
            const double v = det[(size_t) b].process (sc);
            env[(size_t) b] += (v * v > env[(size_t) b] ? aC : rC) * (v * v - env[(size_t) b]);
        }
        if (++cnt >= 32)
        {
            cnt = 0;
            for (int b = 0; b < nb; ++b)
            {
                const double lv = 10.0 * std::log10 (env[(size_t) b] + 1e-12);
                const double target = -depth * 9.0 * std::clamp ((lv + 50.0) / 35.0, 0.0, 1.0);
                cutDb[(size_t) b] += 0.5 * (target - cutDb[(size_t) b]);
                for (auto& f : cut[(size_t) b]) f.peak (fs, fc[b], 1.4, cutDb[(size_t) b]);
            }
        }
        for (int c = 0; c < nch; ++c)
            for (int b = 0; b < nb; ++b)
                x[c] = cut[(size_t) b][(size_t) c].process (x[c]);
    }
    double maxCut() const noexcept { double m = 0; for (auto d : cutDb) m = std::min (m, d); return m; }
};

//==============================================================================
// EQ de correspondance (10 octaves, 31 Hz - 16 kHz) pour se rapprocher d'une reference
struct MatchEQ
{
    static constexpr int nb = 10;
    static constexpr double fc[nb] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    std::array<std::array<Biquad, 2>, nb> f;
    std::array<double, nb> last {};
    double fs = 0;

    void set (double sampleRate, const float* gainsDb, double amount) noexcept
    {
        const bool fsChanged = sampleRate != fs;
        fs = sampleRate;
        for (int b = 0; b < nb; ++b)
        {
            const double g = gainsDb[b] * amount;
            if (fsChanged || std::abs (g - last[(size_t) b]) > 0.01)
            {
                for (auto& q : f[(size_t) b]) q.peak (fs, fc[b], 1.2, g);
                last[(size_t) b] = g;
            }
        }
    }
    void reset() noexcept { for (auto& a : f) for (auto& q : a) q.reset(); }
    inline double process (int c, double x) noexcept
    {
        for (int b = 0; b < nb; ++b)
            if (last[(size_t) b] != 0.0) x = f[(size_t) b][(size_t) c].process (x);
        return x;
    }
};

//==============================================================================
// COMPRESSEUR MULTIBANDE 4 bandes (conception originale, dans l'esprit des
// multibandes de mastering) : seuil, ratio, attaque, relachement, gain,
// solo / bypass par bande, genou commun, mix parallele. Les bandes se
// resomment exactement quand rien ne compresse.
struct MultibandComp
{
    static constexpr int nb = 4;
    struct Settings
    {
        double thr[nb] {}, ratio[nb] {}, att[nb] {}, rel[nb] {}, gain[nb] {};
        bool solo[nb] {}, byp[nb] {};
        double knee = 6, mix = 1;
    };

    // Vrai arbre Linkwitz-Riley 24 dB/oct : passe-bas + passe-haut a chaque coupure,
    // et passe-tout de compensation sur les bandes basses : la somme est plate
    // (a la phase pres), meme quand les bandes sont compressees differemment.
    std::array<std::array<LR4, 2>, 3> lp, hp;
    std::array<std::array<LR4, 2>, 3> apL, apH;      // passe-tout : [0] x2 sur LOW, [1] x3 sur LOW, [2] x3 sur LOW MID
    std::array<double, 3> xf {};
    std::array<double, nb> env {}, gDb {}, aC {}, rC {}, lastAtt {}, lastRel {};
    std::array<double, nb> grMin {};
    double fs = 0, gS = 0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        xf.fill (0);
        lastAtt.fill (-1); lastRel.fill (-1);
        gS = 1.0 - std::exp (-1.0 / (0.0015 * fs));
        reset();
    }
    void reset() noexcept
    {
        for (auto* arr : { &lp, &hp, &apL, &apH }) for (auto& s : *arr) for (auto& f : s) f.reset();
        env.fill (0); gDb.fill (0); grMin.fill (0);
    }
    void setXover (double a, double b, double c) noexcept
    {
        const double v[3] = { a, std::max (a * 1.2, b), std::max (b * 1.2, c) };
        for (int k = 0; k < 3; ++k)
            if (v[k] != xf[(size_t) k])
            {
                xf[(size_t) k] = v[k];
                for (auto& f : lp[(size_t) k]) f.setLowpass (fs, v[k]);
                for (auto& f : hp[(size_t) k]) f.setHighpass (fs, v[k]);
                // passe-tout : [0] a x2, [1] et [2] a x3
                if (k == 1) { for (auto& f : apL[0]) f.setLowpass (fs, v[k]); for (auto& f : apH[0]) f.setHighpass (fs, v[k]); }
                if (k == 2) for (int a = 1; a <= 2; ++a) { for (auto& f : apL[(size_t) a]) f.setLowpass (fs, v[k]); for (auto& f : apH[(size_t) a]) f.setHighpass (fs, v[k]); }
            }
    }
    void setTimes (const Settings& s) noexcept
    {
        for (int k = 0; k < nb; ++k)
        {
            if (s.att[k] != lastAtt[(size_t) k]) { aC[(size_t) k] = 1.0 - std::exp (-1.0 / (std::max (0.05, s.att[k]) * 0.001 * fs)); lastAtt[(size_t) k] = s.att[k]; }
            if (s.rel[k] != lastRel[(size_t) k]) { rC[(size_t) k] = 1.0 - std::exp (-1.0 / (std::max (5.0, s.rel[k]) * 0.001 * fs)); lastRel[(size_t) k] = s.rel[k]; }
        }
    }

    static double gainComputer (double over, double ratio, double knee) noexcept
    {
        const double slope = 1.0 / std::max (1.0, ratio) - 1.0;
        if (knee > 0.0 && std::abs (over) <= knee * 0.5)
            return slope * (over + knee * 0.5) * (over + knee * 0.5) / (2.0 * knee);
        return over > 0.0 ? slope * over : 0.0;
    }

    inline void process (double* x, int nch, const Settings& s) noexcept
    {
        double b[nb][2] {};
        for (int c = 0; c < nch; ++c)
        {
            const auto C = (size_t) c;
            const double v = x[c];
            double low = lp[0][C].process (v);
            const double h0 = hp[0][C].process (v);
            double lmid = lp[1][C].process (h0);
            const double h1 = hp[1][C].process (h0);
            const double hmid = lp[2][C].process (h1), high = hp[2][C].process (h1);
            low = apL[0][C].process (low) + apH[0][C].process (low);
            low = apL[1][C].process (low) + apH[1][C].process (low);
            lmid = apL[2][C].process (lmid) + apH[2][C].process (lmid);
            b[0][c] = low; b[1][c] = lmid; b[2][c] = hmid; b[3][c] = high;
        }
        bool anySolo = false;
        for (int k = 0; k < nb; ++k) anySolo = anySolo || s.solo[k];

        double g[nb];
        for (int k = 0; k < nb; ++k)
        {
            double det = 0;
            for (int c = 0; c < nch; ++c) det = std::max (det, std::abs (b[k][c]));
            env[(size_t) k] += (det > env[(size_t) k] ? aC[(size_t) k] : rC[(size_t) k]) * (det - env[(size_t) k]);
            const double L = 20.0 * std::log10 (env[(size_t) k] + 1e-9);
            const double t = s.byp[k] ? 0.0 : gainComputer (L - s.thr[k], s.ratio[k], s.knee);
            gDb[(size_t) k] += gS * (t - gDb[(size_t) k]);
            grMin[(size_t) k] = std::min (grMin[(size_t) k], gDb[(size_t) k]);
            g[k] = s.byp[k] ? 1.0 : std::pow (10.0, (gDb[(size_t) k] + s.gain[k]) / 20.0);
        }
        for (int c = 0; c < nch; ++c)
        {
            double y = 0, dry = 0;
            for (int k = 0; k < nb; ++k)
            {
                dry += b[k][c];
                if (anySolo && ! s.solo[k]) continue;
                y += b[k][c] * g[k];
            }
            x[c] = anySolo ? y : dry + s.mix * (y - dry);
        }
    }
};

//==============================================================================
// EQ DYNAMIQUE : 2 bandes flottantes. Quand le niveau dans la bande depasse le
// seuil, la bande se creuse (plage negative) ou se remplit (plage positive),
// jusqu'a la plage complete 12 dB au-dessus du seuil.
struct DynEQ
{
    static constexpr int nf = 2;
    struct Band
    {
        Biquad det;
        std::array<Biquad, 2> pk;
        double env = 0, g = 0, setG = 0, lastF = -1, lastQ = -1;
    };
    std::array<Band, nf> b;
    double fs = 0, aC = 0, rC = 0;
    int cnt = 0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aC = 1.0 - std::exp (-1.0 / (0.003 * fs));
        rC = 1.0 - std::exp (-1.0 / (0.08 * fs));
        for (auto& x : b) { x.lastF = x.lastQ = -1; }
        reset();
    }
    void reset() noexcept
    {
        for (auto& x : b)
        {
            x.det.reset();
            for (auto& f : x.pk) { f.reset(); f.peak (fs > 0 ? fs : 48000.0, 1000.0, 1.0, 0.0); }
            x.env = x.g = x.setG = 0;
        }
    }

    inline void process (double* x, int nch, const double* freq, const double* q, const double* thr, const double* range) noexcept
    {
        const double m = nch > 1 ? 0.5 * (x[0] + x[1]) : x[0];
        const bool upd = ++cnt >= 16;
        if (upd) cnt = 0;
        for (int f = 0; f < nf; ++f)
        {
            auto& B = b[(size_t) f];
            if (range[f] == 0.0 && std::abs (B.g) < 0.01) continue;
            const bool moved = freq[f] != B.lastF || q[f] != B.lastQ;
            if (moved)
            {
                const double w = 2 * pi * Biquad::clampF (fs, freq[f]) / fs, cw = std::cos (w), al = std::sin (w) / (2 * q[f]);
                B.det.set (al, 0, -al, 1 + al, -2 * cw, 1 - al);
                B.lastF = freq[f]; B.lastQ = q[f];
            }
            const double v = B.det.process (m);
            B.env += (std::abs (v) > B.env ? aC : rC) * (std::abs (v) - B.env);
            if (upd)
            {
                const double L = 20.0 * std::log10 (B.env + 1e-9);
                const double target = range[f] * std::clamp ((L - thr[f]) / 12.0, 0.0, 1.0);
                B.g += 0.3 * (target - B.g);
                if (moved || std::abs (B.g - B.setG) > 0.05)
                {
                    for (auto& p : B.pk) p.peak (fs, freq[f], q[f], B.g);
                    B.setG = B.g;
                }
            }
            for (int c = 0; c < nch; ++c) x[c] = B.pk[(size_t) c].process (x[c]);
        }
    }
};

//==============================================================================
// DE-ESSER a bande separee : au-dessus de FREQ, la bande est reduite quand elle
// depasse le seuil (jusqu'a RANGE dB). LISTEN = n'ecouter que ce qui est retire.
struct DeEsser
{
    std::array<LR4, 2> lp, hp, bandLP, bandHP, apL, apH;
    static constexpr int nDet = 5;
    std::array<Biquad, nDet> det;
    std::array<double, nDet> dEnv {}, dSlow {}, dScore {};
    double hiF = 20000, autoFreq = 6500, autoThr = -30, detDb = -100;
    int autoCount = 0;
    double fs = 0, f = 0, env = 0, g = 1, aC = 0, rC = 0, gS = 0, lastGrDb = 0, slow = 0, sC = 0;

    void prepare (double sampleRate)
    {
        fs = sampleRate; f = 0;
        aC = 1.0 - std::exp (-1.0 / (0.0007 * fs));
        rC = 1.0 - std::exp (-1.0 / (0.06 * fs));
        gS = 1.0 - std::exp (-1.0 / (0.0008 * fs));
        sC = 1.0 - std::exp (-1.0 / (1.5 * fs));
        static constexpr double dc[nDet] = { 4500, 5600, 6800, 8200, 10000 };
        for (int k = 0; k < nDet; ++k)
        {
            const double w = 2 * pi * dc[k] / fs, cw = std::cos (w), al = std::sin (w) / (2 * 2.0);
            det[(size_t) k].set (al, 0, -al, 1 + al, -2 * cw, 1 - al);
        }
        hiF = 0; setHi (20000);
        reset();
    }
    void reset() noexcept
    {
        for (auto* a : { &lp, &hp, &bandLP, &bandHP, &apL, &apH }) for (auto& x : *a) x.reset();
        for (auto& d : det) d.reset();
        dEnv.fill (0); dSlow.fill (0); dScore.fill (0);
        env = 0; g = 1; slow = 0;
    }
    void setHi (double f) noexcept
    {
        f = std::min (f, fs * 0.45);
        if (f == hiF) return;
        hiF = f;
        for (auto& x : bandLP) x.setLowpass (fs, f);
        for (auto& x : bandHP) x.setHighpass (fs, f);
        for (auto& x : apL) x.setLowpass (fs, f);
        for (auto& x : apH) x.setHighpass (fs, f);
    }
    void setFreq (double freq) noexcept
    {
        if (freq == f) return;
        f = freq;
        for (auto& x : lp) x.setLowpass (fs, f);
        for (auto& x : hp) x.setHighpass (fs, f);
    }
    // autoMode : le seuil suit tout seul le niveau moyen des aigus (+4 dB) : seules
    // les sifflantes qui depassent nettement leur niveau habituel sont reduites
    inline void process (double* x, int nch, double thrDb, double rangeDb, bool listen, bool autoMode = false) noexcept
    {
        // bande traitee : de FREQ a FREQ HI (au-dessus de FREQ HI : intact)
        const bool useHi = hiF < fs * 0.44;
        double lo[2] {}, hi[2] {}, top[2] {}, det = 0;
        for (int c = 0; c < nch; ++c)
        {
            const auto C = (size_t) c;
            lo[c] = lp[C].process (x[c]);
            hi[c] = hp[C].process (x[c]);
            if (useHi)
            {
                top[c] = bandHP[C].process (hi[c]);
                hi[c] = bandLP[C].process (hi[c]);
                lo[c] = apL[C].process (lo[c]) + apH[C].process (lo[c]);
            }
            det = std::max (det, std::abs (hi[c]));
        }
        // auto : repere la zone la plus "pointue" (sifflante) parmi 4,5 - 10 kHz
        if (autoMode)
        {
            const double m = nch > 1 ? 0.5 * (x[0] + x[1]) : x[0];
            for (int k = 0; k < nDet; ++k)
            {
                const double v = std::abs (this->det[(size_t) k].process (m));
                dEnv[(size_t) k] += (v > dEnv[(size_t) k] ? aC : rC) * (v - dEnv[(size_t) k]);
                dSlow[(size_t) k] += sC * (v - dSlow[(size_t) k]);
            }
            if (++autoCount >= 256)
            {
                autoCount = 0;
                static constexpr double dc[nDet] = { 4500, 5600, 6800, 8200, 10000 };
                int best = 2; double bs = -1;
                for (int k = 0; k < nDet; ++k)
                {
                    const double sc = dEnv[(size_t) k] / (dSlow[(size_t) k] + 1e-9);
                    dScore[(size_t) k] += 0.02 * (sc - dScore[(size_t) k]);
                    if (dScore[(size_t) k] > bs) { bs = dScore[(size_t) k]; best = k; }
                }
                const double target = dc[best] * 0.8;
                autoFreq += 0.05 * (target - autoFreq);
            }
        }
        env += (det > env ? aC : rC) * (det - env);
        slow += sC * (det - slow);
        const double L = 20.0 * std::log10 (env + 1e-9);
        if (autoMode) { thrDb = 20.0 * std::log10 (slow + 1e-9) + 4.0; autoThr = thrDb; }
        detDb = L;
        const double grDb = -std::min (rangeDb, std::max (0.0, (L - thrDb) * 0.75));
        g += gS * (std::pow (10.0, grDb / 20.0) - g);
        lastGrDb = grDb;
        for (int c = 0; c < nch; ++c)
            x[c] = listen ? hi[c] * (1.0 - g) : lo[c] + top[c] + hi[c] * g;
    }
};

//==============================================================================
// MOTEUR SPECTRAL (STFT) : equilibreur de spectre et dompteur de resonances.
//  - BALANCE : ramene l'equilibre (lisse sur 1 octave) vers la courbe cible ;
//  - RESONANCE : repere les pics etroits (1/24 oct) qui depassent leur
//    voisinage (1/3 oct) et les creuse ;
//  - precision ~1/6 d'octave (une soixantaine de zones sur le spectre audible).
// Les gains sont calcules sur la somme L+R et appliques aux deux canaux
// (l'image stereo ne bouge pas). Latence : une taille de FFT.
struct SpectralEngine
{
    int order = 11, N = 2048, hop = 512;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> win, inBuf[2], outBuf[2], frame[2];
    std::vector<double> powS, gainDb, logF, dbNow, cumDb;
    int inPos = 0, outPos = 0, hopCount = 0;
    double fs = 48000;
    bool cutOnly = false;

    static int latencyFor (double fs) { return 1 << (fs <= 50000 ? 11 : (fs <= 100000 ? 12 : 13)); }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        N = latencyFor (fs); order = (int) std::lround (std::log2 ((double) N)); hop = N / 4;
        fft = std::make_unique<juce::dsp::FFT> (order);
        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2 * pi * i / N));
        for (int c = 0; c < 2; ++c)
        {
            inBuf[c].assign ((size_t) N, 0.f); outBuf[c].assign ((size_t) N * 2, 0.f); frame[c].assign ((size_t) N * 2, 0.f);
        }
        const int nb = N / 2 + 1;
        powS.assign ((size_t) nb, 1e-12); gainDb.assign ((size_t) nb, 0.0); dbNow.assign ((size_t) nb, -120.0);
        cumDb.assign ((size_t) nb + 1, 0.0); logF.resize ((size_t) nb);
        for (int k = 0; k < nb; ++k) logF[(size_t) k] = std::log2 (std::max (1.0, k * fs / N));
        inPos = outPos = hopCount = 0;
    }
    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c) { std::fill (inBuf[c].begin(), inBuf[c].end(), 0.f); std::fill (outBuf[c].begin(), outBuf[c].end(), 0.f); }
        std::fill (powS.begin(), powS.end(), 1e-12); std::fill (gainDb.begin(), gainDb.end(), 0.0);
        inPos = outPos = hopCount = 0;
    }

    // moyenne en dB sur +-halfOct octave autour du bin k (sommes cumulees)
    inline double smoothAt (int k, double halfOct) const noexcept
    {
        const int nb = (int) dbNow.size();
        const double f = std::max (1.0, k * fs / N);
        const int a = std::clamp ((int) (f * std::pow (2.0, -halfOct) * N / fs), 1, nb - 1);
        const int b = std::clamp ((int) (f * std::pow (2.0, halfOct) * N / fs) + 1, a + 1, nb);
        return (cumDb[(size_t) b] - cumDb[(size_t) a]) / (double) (b - a);
    }

    // target(log2 f) -> dB relatifs ; balance / resonance 0..1 ; speed 0..1
    template <typename TargetFn>
    void analyse (double balance, double resonance, double speed, TargetFn target)
    {
        const int nb = N / 2 + 1;
        auto* L = frame[0].data(); auto* R = frame[1].data();
        const double tSm = 0.05 + 0.6 * (1.0 - speed);            // lissage temporel du spectre
        for (int k = 0; k < nb; ++k)
        {
            const double pL = (double) L[2 * k] * L[2 * k] + (double) L[2 * k + 1] * L[2 * k + 1];
            const double pR = (double) R[2 * k] * R[2 * k] + (double) R[2 * k + 1] * R[2 * k + 1];
            const double pw = 0.5 * (pL + pR) + 1e-14;
            powS[(size_t) k] += (1.0 - tSm) * (pw - powS[(size_t) k]);
            dbNow[(size_t) k] = 10.0 * std::log10 (powS[(size_t) k]);
        }
        cumDb[0] = 0;
        for (int k = 0; k < nb; ++k) cumDb[(size_t) k + 1] = cumDb[(size_t) k] + dbNow[(size_t) k];

        // decalage pour caler la cible sur le niveau du morceau (100 Hz - 10 kHz)
        double off = 0; int cnt = 0;
        if (balance > 0)
            for (int k = 1; k < nb; ++k)
            {
                const double f = k * fs / N;
                if (f < 100 || f > 10000) continue;
                off += smoothAt (k, 0.5) - target (logF[(size_t) k]); ++cnt;
            }
        if (cnt > 0) off /= cnt;

        const double att = 0.35 + 0.4 * speed, rel = 0.06 + 0.25 * speed;
        for (int k = 1; k < nb; ++k)
        {
            const double f = k * fs / N;
            double g = 0;
            if (f > 25 && f < 18000)
            {
                if (balance > 0)
                {
                    const double dev = target (logF[(size_t) k]) + off - smoothAt (k, 0.5);
                    g += std::clamp (dev * 0.85, -9.0, cutOnly ? 0.0 : 9.0) * balance;
                }
                if (resonance > 0 && f > 80)
                {
                    const double ex = smoothAt (k, 1.0 / 48.0) - smoothAt (k, 1.0 / 6.0) - 3.0;
                    if (ex > 0) g -= std::min (9.0, ex * 1.2) * resonance;
                }
            }
            auto& gd = gainDb[(size_t) k];
            gd += (g < gd ? att : rel) * (g - gd);
        }
        // lissage en frequence du gain (~1/6 oct) pour eviter tout artefact
        for (int k = 0; k < nb; ++k) dbNow[(size_t) k] = gainDb[(size_t) k];
        cumDb[0] = 0;
        for (int k = 0; k < nb; ++k) cumDb[(size_t) k + 1] = cumDb[(size_t) k] + dbNow[(size_t) k];
        for (int k = 1; k < nb; ++k)
        {
            const double gl = std::pow (10.0, smoothAt (k, 1.0 / 12.0) / 20.0);
            for (int c = 0; c < 2; ++c) { frame[c][(size_t) (2 * k)] *= (float) gl; frame[c][(size_t) (2 * k + 1)] *= (float) gl; }
        }
    }

    template <typename TargetFn>
    inline void process (double* x, int nch, double balance, double resonance, double speed, TargetFn target)
    {
        for (int c = 0; c < 2; ++c) inBuf[c][(size_t) inPos] = (float) (c < nch ? x[c] : x[0]);
        for (int c = 0; c < nch; ++c) x[c] = outBuf[c][(size_t) outPos];
        for (int c = 0; c < 2; ++c) outBuf[c][(size_t) outPos] = 0.f;
        inPos = (inPos + 1) % N;
        outPos = (outPos + 1) % N;
        if (++hopCount < hop) return;
        hopCount = 0;

        for (int c = 0; c < 2; ++c)
        {
            auto& fr = frame[c];
            for (int i = 0; i < N; ++i) fr[(size_t) i] = inBuf[c][(size_t) ((inPos + i) % N)] * win[(size_t) i];
            std::fill (fr.begin() + N, fr.end(), 0.f);
            fft->performRealOnlyForwardTransform (fr.data(), true);
        }
        analyse (balance, resonance, speed, target);
        for (int c = 0; c < 2; ++c)
        {
            auto& fr = frame[c];
            fft->performRealOnlyInverseTransform (fr.data());
            // racine de Hann en analyse et en synthese (= Hann), recouvrement 75 % : somme = 2
            for (int i = 0; i < N; ++i)
                outBuf[c][(size_t) ((outPos + i) % N)] += fr[(size_t) i] * win[(size_t) i] * 0.5f;
        }
    }
};

//==============================================================================
// GLUE : compresseur de bus a la maniere des consoles (conception originale).
// Detection stereo liee, sidechain filtre (HPF), genou doux, attaque /
// relachement par crans, relachement AUTO dependant du programme, mix parallele.
struct GlueComp
{
    std::array<Biquad, 2> scHP;
    double fs = 0, env = 0, gDb = 0, aC = 0, rC = 0, rSlowC = 0, envSlow = 0, gSm = 0;
    int lastHpf = -1, lastAtt = -1, lastRel = -1;
    double lastGr = 0;

    void prepare (double sampleRate) { fs = sampleRate; lastHpf = lastAtt = lastRel = -1; gSm = 1.0 - std::exp (-1.0 / (0.0005 * fs)); reset(); }
    void reset() noexcept { for (auto& f : scHP) f.reset(); env = envSlow = gDb = 0; }

    void configure (int hpf, int att, int rel) noexcept
    {
        static constexpr double hpfHz[4] = { 0, 60, 90, 150 };
        static constexpr double attMs[6] = { 0.1, 0.3, 1, 3, 10, 30 };
        static constexpr double relS[5]  = { 0.1, 0.3, 0.6, 1.2, 0.0 };
        if (hpf != lastHpf) { lastHpf = hpf; if (hpf > 0) for (auto& f : scHP) f.highpass (fs, hpfHz[std::clamp (hpf, 0, 3)], 0.7); }
        if (att != lastAtt) { lastAtt = att; aC = 1.0 - std::exp (-1.0 / (attMs[std::clamp (att, 0, 5)] * 0.001 * fs)); }
        if (rel != lastRel)
        {
            lastRel = rel;
            const double r = relS[std::clamp (rel, 0, 4)];
            rC = 1.0 - std::exp (-1.0 / ((r > 0 ? r : 0.12) * fs));
            rSlowC = 1.0 - std::exp (-1.0 / ((r > 0 ? r : 1.5) * fs));
        }
    }

    inline void process (double* x, int nch, double thrDb, double ratio, double makeupDb, double mix, bool autoRel) noexcept
    {
        double d = 0;
        for (int c = 0; c < nch; ++c)
        {
            const double s = lastHpf > 0 ? scHP[(size_t) c].process (x[c]) : x[c];
            d = std::max (d, std::abs (s));
        }
        const double L = 20.0 * std::log10 (d + 1e-9);
        constexpr double knee = 6.0;
        const double over = L - thrDb;
        const double slope = 1.0 / ratio - 1.0;
        double t = 0;
        if (std::abs (over) <= knee * 0.5) t = slope * (over + knee * 0.5) * (over + knee * 0.5) / (2.0 * knee);
        else if (over > 0) t = slope * over;
        // relachement AUTO : rapide sur un pic isole, lent quand ca compresse longtemps
        if (t < env) env += aC * (t - env);
        else env += (autoRel && envSlow < -2.0 ? rSlowC : rC) * (t - env);
        envSlow += rSlowC * (env - envSlow);
        gDb += gSm * (env - gDb);
        lastGr = gDb;
        const double g = std::pow (10.0, (gDb + makeupDb) / 20.0);
        for (int c = 0; c < nch; ++c) x[c] = x[c] * (1.0 - mix) + x[c] * g * mix;
    }
};

} // namespace gdsp